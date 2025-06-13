#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <stdio.h>

uint8_t* g_rgb_frame_buffer = NULL;
uint8_t* g_jpg_frame_buffer = NULL;

AVFrame* covertFrameToFormat(enum AVPixelFormat srcFmt, enum AVPixelFormat dstFmt, AVFrame* frame)
{
  int ret;
  uint8_t* buffer = NULL;
  struct SwsContext* swsContext = NULL;
  AVFrame* rgbFrame = av_frame_alloc();
  if (rgbFrame == NULL)
  {
    printf("av_frame_alloc failed\n");
    return NULL;
  }

  if (srcFmt == dstFmt)
  {
    ret = av_frame_ref(rgbFrame, frame);
    if (ret != 0)
    {
      printf("av_frame_ref failed, %d", ret);
      av_frame_free(&rgbFrame);
      return NULL;
    }
  }
  else
  {
    swsContext = sws_getContext(frame->width, frame->height, srcFmt, frame->width, frame->height, dstFmt, 1, NULL, NULL, NULL);
		if (!swsContext)
		{
			printf("sws_getContext fail\n");
      av_frame_unref(rgbFrame);
      av_frame_free(&rgbFrame);
			return NULL;
		}

    if (g_jpg_frame_buffer == NULL)
    {
      int bufferSize = av_image_get_buffer_size(dstFmt, frame->width, frame->height, 1) * 2;
      g_jpg_frame_buffer = (unsigned char*)av_malloc(bufferSize);
      if (g_jpg_frame_buffer == NULL)
      {
        printf("buffer alloc fail:%d\n", bufferSize);
        av_frame_unref(rgbFrame);
        av_frame_free(&rgbFrame);
        return NULL;
      }
    }

		av_image_fill_arrays(rgbFrame->data, rgbFrame->linesize, g_jpg_frame_buffer, dstFmt, frame->width, frame->height, 1);
		if ((ret = sws_scale(swsContext, (const uint8_t* const*)frame->data, frame->linesize, 0, frame->height, rgbFrame->data, rgbFrame->linesize)) < 0)
		{
			printf("sws_scale error %d\n", ret);
      av_frame_unref(rgbFrame);
      av_frame_free(&rgbFrame);
      return NULL;
		}
		rgbFrame->format = dstFmt;
		rgbFrame->width = frame->width;
		rgbFrame->height = frame->height;
  }

  return rgbFrame;
}

void avframe_to_jpeg_file(AVFrame *frame, AVCodecContext *srcCtx, const char *fileName)
{
  if (frame == NULL)
  {
    return;
  }

  // 创建输出上下文
  AVFormatContext *pFormatCtx = avformat_alloc_context();
  int ret = avformat_alloc_output_context2(&pFormatCtx, NULL, NULL, fileName);
  if (ret < 0)
  {
    printf("avformat_alloc_output_context2 error\n");
    return;
  }

  // 由输出文件jpg格式自动推导编码器类型
  AVCodecContext *pCodecCtx = avcodec_alloc_context3(NULL);
  pCodecCtx->codec_id = pFormatCtx->oformat->video_codec;
  pCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;
  pCodecCtx->pix_fmt = AV_PIX_FMT_YUVJ420P;
  pCodecCtx->width = frame->width;
  pCodecCtx->height = frame->height;
  pCodecCtx->time_base = (AVRational){1, 25};
  pCodecCtx->gop_size = 25;
  pCodecCtx->max_b_frames = 0;
  // 重要参数：压缩效果、码率  设置为高质量
  pCodecCtx->qcompress = 1.0;
  pCodecCtx->bit_rate = frame->width * frame->height * 3;

  // 寻找并打开编码器
  AVCodec *pCodec = avcodec_find_encoder(pCodecCtx->codec_id);
  if (!pCodec)
  {
    printf("avcodec_find_encoder() Failed\n");
    avformat_close_input(&pFormatCtx);
    avformat_free_context(pFormatCtx);
    avcodec_free_context(&pCodecCtx);
    return;
  }
  printf("pix fmt: %d, frame fmt %d\n", (int)*pCodec->pix_fmts, frame->format);

  AVFrame *rgbFrame = covertFrameToFormat(frame->format, *pCodec->pix_fmts, frame);
  if (rgbFrame == NULL)
  {
    printf("covert frame Failed\n");
    avformat_close_input(&pFormatCtx);
    avformat_free_context(pFormatCtx);
    avcodec_free_context(&pCodecCtx);
    return;
  }

  ret = avcodec_open2(pCodecCtx, pCodec, NULL);
  if (ret < 0)
  {
    printf("avcodec_open2() Failed\n");
    avformat_close_input(&pFormatCtx);
    avformat_free_context(pFormatCtx);
    avcodec_free_context(&pCodecCtx);
    return;
  }

  // 进行编码
  ret = avcodec_send_frame(pCodecCtx, rgbFrame);
  if (ret < 0)
  {
    printf("avcodec_send_frame() Failed\n");
    avformat_close_input(&pFormatCtx);
    avformat_free_context(pFormatCtx);
    avcodec_free_context(&pCodecCtx);
    return;
  }

  // 得到编码jpeg数据pkt 由外部使用者释放
  AVPacket *pkt = av_packet_alloc();
  ret = avcodec_receive_packet(pCodecCtx, pkt);
  if (ret < 0)
  {
    printf("avcodec_receive_packet() Failed\n");
    avformat_close_input(&pFormatCtx);
    avformat_free_context(pFormatCtx);
    avcodec_free_context(&pCodecCtx);
    return;
  }

  // 创建流生成本地jpg文件
  AVStream *videoStream = avformat_new_stream(pFormatCtx, 0);
  if (videoStream == NULL)
  {
    printf("avformat_new_stream() Failed\n");
    avcodec_free_context(&pCodecCtx);
    avformat_close_input(&pFormatCtx);
    avformat_free_context(pFormatCtx);
    av_packet_free(&pkt);
    return;
  }

  // 写文件头、数据体、文件尾
  avformat_write_header(pFormatCtx, NULL);
  av_write_frame(pFormatCtx, pkt);
  av_write_trailer(pFormatCtx);

  // 释放资源
  av_frame_unref(rgbFrame);
  av_frame_free(&rgbFrame);
  av_packet_free(&pkt);
  avformat_close_input(&pFormatCtx);
  avformat_free_context(pFormatCtx);
  avcodec_free_context(&pCodecCtx);
  printf("save to file %s success\n", fileName);
}

AVFrame* covert_frame_to_rgb_frame(AVFrame* frame)
{
  int width = frame->width;
  int height = frame->height;
  enum AVPixelFormat srcFmt = frame->format;
  struct SwsContext* swsContext = NULL;
  AVFrame *pFrameRGB = av_frame_alloc();
  swsContext = sws_getContext(frame->width, frame->height, srcFmt, frame->width, frame->height,
                              AV_PIX_FMT_RGB24, SWS_BICUBIC, NULL, NULL, NULL);
  
  if (g_rgb_frame_buffer == NULL)
  {
    int numBytes = avpicture_get_size(AV_PIX_FMT_RGB24, frame->width, frame->height);
    g_rgb_frame_buffer = (uint8_t *) av_malloc(numBytes * sizeof(uint8_t));
  }
  
  avpicture_fill((AVPicture *)pFrameRGB, g_rgb_frame_buffer, AV_PIX_FMT_RGB24,
                 frame->width, frame->height);
  
  frame->data[0] += frame->linesize[0] * (height - 1);
	frame->linesize[0] *= -1;
	frame->data[1] += frame->linesize[1] * (height / 2 - 1);
	frame->linesize[1] *= -1;
	frame->data[2] += frame->linesize[2] * (height / 2 - 1);
	frame->linesize[2] *= -1;
	sws_scale(swsContext, (const uint8_t* const*)frame->data, frame->linesize,
			      0, height, pFrameRGB->data, pFrameRGB->linesize);
  return pFrameRGB;
}

// check if frame is valid
void avframe_to_ppm_file(AVFrame *frame, const char* fileName)
{
  AVFrame *rgbFrame = covert_frame_to_rgb_frame(frame);
  FILE *ppm_file = fopen(fileName, "wb");
  fprintf(ppm_file, "P6\n%d %d\n255\n", rgbFrame->width, rgbFrame->height);
  printf("rbg frame linesize[0]: %d\n", rgbFrame->linesize[0]);
  for (int y = 0; y < rgbFrame->height; y++) {
      fwrite(rgbFrame->data[0] + y * rgbFrame->linesize[0], 1, rgbFrame->width, ppm_file);
  }
  fclose(ppm_file);
  av_frame_unref(rgbFrame);
  av_frame_free(&rgbFrame);
}

int64_t seconds_to_timestamp(int64_t seconds, AVStream *stream) {
    int64_t timestamp = (int64_t)(seconds * av_q2d(stream->time_base) * INT64_C(1000000)) / 1000000;
    return timestamp;
}

int main()
{
  av_register_all();
  avformat_network_init();
  AVFormatContext *fmtCtx = NULL;
  const char *filePath = "oceans.mp4";
  if (avformat_open_input(&fmtCtx, filePath, NULL, NULL) != 0)
  {
    printf("Could not open file %s\n", filePath);
    return -1;
  }

  if (avformat_find_stream_info(fmtCtx, NULL) < 0)
  {
    printf("Could not find stream information\n");
    avformat_close_input(&fmtCtx);
    return -1;
  }

  int videoStreamIndex = -1;
  for (unsigned int i = 0; i < fmtCtx->nb_streams; ++i)
  {
    if (fmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
    {
      videoStreamIndex = i;
      break;
    }
  }

  if (videoStreamIndex == -1)
  {
    printf("No video stream found\n");
    avformat_close_input(&fmtCtx);
    return -1;
  }

  AVCodecContext *codecCtx = avcodec_alloc_context3(NULL);
  if (avcodec_parameters_to_context(
          codecCtx, fmtCtx->streams[videoStreamIndex]->codecpar) < 0)
  {
    printf("Could not copy codec parameters to codec context");
    avformat_close_input(&fmtCtx);
    return -1;
  }

  AVCodec *codec = avcodec_find_decoder(codecCtx->codec_id);
  if (codec == NULL)
  {
    printf("Could not find decoder\n");
    avformat_close_input(&fmtCtx);
    avcodec_free_context(&codecCtx);
    return -1;
  }

  if (avcodec_open2(codecCtx, codec, NULL) < 0)
  {
    printf("Could not open codec");
    avformat_close_input(&fmtCtx);
    avcodec_free_context(&codecCtx);
    return -1;
  }

  int64_t seekTime = 20; // seconds
  int64_t seekTarget = seekTime * AV_TIME_BASE;
  //int64_t seekTarget = seconds_to_timestamp(seekTime, fmtCtx->streams[videoStreamIndex]);
  //printf("seek target %ld\n", seekTarget);
  if (av_seek_frame(fmtCtx, -1, seekTarget, AVSEEK_FLAG_BACKWARD) < 0)
  {
    printf("Seek operation failed");
    avcodec_close(codecCtx);
    avcodec_free_context(&codecCtx);
    avformat_close_input(&fmtCtx);
    return -1;
  }

  AVPacket packet;
  av_init_packet(&packet);
  while (av_read_frame(fmtCtx, &packet) >= 0)
  {
    if (packet.stream_index == videoStreamIndex)
    {
      printf("read frame, pts %ld\n", packet.pts);
      if (packet.flags & AV_PKT_FLAG_KEY)
      {
        printf("Found key frame at seek position, pts %ld\n", packet.pts);
        if (avcodec_send_packet(codecCtx, &packet) == AVERROR(EAGAIN))
        {
          printf("avcodec_send_packet failed\n");
          break;
        }
        AVFrame *frame = av_frame_alloc();
        int ret = avcodec_receive_frame(codecCtx, frame);
        if (ret >= 0)
        {
          printf("decoded key frame success, ret %d\n", ret);
          avframe_to_ppm_file(frame, "output.ppm");
          char jpegFileName[1024] = {0};
          snprintf(jpegFileName, 1023, "%s-%ld.jpg", filePath, seekTime);
          avframe_to_jpeg_file(frame, codecCtx, jpegFileName);
          av_frame_unref(frame);
          av_frame_free(&frame);
        }
        else
        {
          printf("decoded key frame failed, ret = %d\n", ret);
        }
        break;
      }
    }
    av_packet_unref(&packet);
  }

  av_packet_unref(&packet);
  if (g_jpg_frame_buffer != NULL)
  {
    av_free(g_jpg_frame_buffer);
    g_jpg_frame_buffer = NULL;
  }
  if (g_rgb_frame_buffer != NULL)
  {
    av_free(g_rgb_frame_buffer);
    g_rgb_frame_buffer = NULL;
  }

  avcodec_close(codecCtx);
  avcodec_free_context(&codecCtx);
  avformat_close_input(&fmtCtx);

  return 0;
}
