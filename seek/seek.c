#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <jpeglib.h>
#include <stdio.h>


void avframe_to_jpeg(const AVFrame* frame, const char* filename, int quality) {
  struct jpeg_compress_struct cinfo;
  struct jpeg_error_mgr jerr;
  JSAMPROW row_pointer[1];
  FILE* outfile;
  int row_stride;

  // 初始化JPEG压缩对象
  cinfo.err = jpeg_std_error(&jerr);
  jpeg_create_compress(&cinfo);

  // 打开输出文件
  if ((outfile = fopen(filename, "wb")) == NULL) {
      printf("Can't open output file: %s\n", filename);
      jpeg_destroy_compress(&cinfo);
      return;
  }

  jpeg_stdio_dest(&cinfo, outfile);

  // 设置压缩参数
  cinfo.image_width = frame->width;
  cinfo.image_height = frame->height;
  cinfo.input_components = 3;
  cinfo.in_color_space = JCS_RGB;
  jpeg_set_defaults(&cinfo);
  jpeg_set_quality(&cinfo, quality, TRUE);

  // 开始压缩
  jpeg_start_compress(&cinfo, TRUE);

  row_stride = frame->width * 3;
  while (cinfo.next_scanline < cinfo.image_height) {
      row_pointer[0] = (JSAMPROW)&frame->data[0][cinfo.next_scanline * row_stride];
      jpeg_write_scanlines(&cinfo, row_pointer, 1);
      printf("write scanline, line %u, image height %u\n", cinfo.next_scanline, cinfo.image_height);
  }

  // 完成压缩
  jpeg_finish_compress(&cinfo);
  fclose(outfile);
  jpeg_destroy_compress(&cinfo);

  printf("save to jpeg image file: %s\n", filename);
}

int main() {
  av_register_all();
  avformat_network_init();
  AVFormatContext *fmtCtx = NULL;
  const char *filePath = "oceans.mp4";
  if (avformat_open_input(&fmtCtx, filePath, NULL, NULL) != 0) {
    printf("Could not open file %s\n", filePath);
    return -1;
  }

  if (avformat_find_stream_info(fmtCtx, NULL) < 0) {
    printf("Could not find stream information\n");
    avformat_close_input(&fmtCtx);
    return -1;
  }

  int videoStreamIndex = -1;
  for (unsigned int i = 0; i < fmtCtx->nb_streams; ++i) {
    if (fmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
      videoStreamIndex = i;
      break;
    }
  }
  
  if (videoStreamIndex == -1) {
    printf("No video stream found\n");
    avformat_close_input(&fmtCtx);
    return -1;
  }

  AVCodecContext *codecCtx = avcodec_alloc_context3(NULL);
  if (avcodec_parameters_to_context(
          codecCtx, fmtCtx->streams[videoStreamIndex]->codecpar) < 0) {
    printf("Could not copy codec parameters to codec context");
    avformat_close_input(&fmtCtx);
    return -1;
  }

  AVCodec *codec = avcodec_find_decoder(codecCtx->codec_id);
  if (codec == NULL) {
    printf("Could not find decoder\n");
    avformat_close_input(&fmtCtx);
    avcodec_free_context(&codecCtx);
    return -1;
  }

  if (avcodec_open2(codecCtx, codec, NULL) < 0) {
    printf("Could not open codec");
    avformat_close_input(&fmtCtx);
    avcodec_free_context(&codecCtx);
    return -1;
  }

  double seekTime = 10.0;
  int64_t seekTarget =
      (int64_t)(seekTime *
                av_q2d(fmtCtx->streams[videoStreamIndex]->time_base));
  if (av_seek_frame(fmtCtx, videoStreamIndex, seekTarget,
                    AVSEEK_FLAG_BACKWARD) < 0) {
    printf("Seek operation failed");
    avcodec_close(codecCtx);
    avcodec_free_context(&codecCtx);
    avformat_close_input(&fmtCtx);
    return -1;
  }

  AVPacket packet;
  av_init_packet(&packet);
  while (av_read_frame(fmtCtx, &packet) >= 0) {
    if (packet.stream_index == videoStreamIndex) {
      if (packet.flags & AV_PKT_FLAG_KEY) {
        printf("Found key frame at seek position\n");
        if (avcodec_send_packet(codecCtx, &packet) == AVERROR(EAGAIN)) {
          printf("avcodec_send_packet failed\n");
          break;
        }
        AVFrame *frame = av_frame_alloc();
        int ret = avcodec_receive_frame(codecCtx, frame);
        if (ret >= 0) {
          printf("decoded key frame success\n");
          char jpegFileName[1024] = {0};
          snprintf(jpegFileName, 1023, "%s-%f.jpg", filePath, seekTime);
          avframe_to_jpeg(frame, jpegFileName, 90);
          av_frame_free(&frame);
        } else {
          printf("decoded key frame failed, ret = %d\n", ret);
        }
        break;
      }
    }
    av_packet_unref(&packet);
  }

  av_packet_unref(&packet);
  avcodec_close(codecCtx);
  avcodec_free_context(&codecCtx);
  avformat_close_input(&fmtCtx);

  return 0;
}
