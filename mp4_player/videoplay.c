#include<stdio.h>
#include <unistd.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include "libavformat/avformat.h"

static AVInputFormat *file_iformat;
AVDictionary *format_opts;
//AVDictionary *codec_opts;

//显示
SDL_Window* window;
SDL_Renderer *renderer;

int initSdl()
{
  if (SDL_Init(SDL_INIT_EVERYTHING) == -1)
  {
    printf("SDL_Init\n");
    return -1;
  }

  window = SDL_CreateWindow("Show Image", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480, SDL_WINDOW_ALLOW_HIGHDPI);
  if (NULL == window)
  {
    printf("SDL_CreateWindow failed!\n");
    return -2;
  }

  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (NULL == renderer)
  {
    printf("SDL_CreateRenderer failed!\n");
    SDL_DestroyWindow(window);
    SDL_Quit();
    return -3;
  }

  return 0;
}

int main(int argc, char* argv[])
{
  int err, ret;

  if (argc != 2)
  {
    printf("invalid argument!\n");
    printf("usage: ./videoplay xxx.mp4\n");
  }
  if (initSdl() < 0)
  {
    return -1;
  }

  file_iformat = av_find_input_format(argv[1]);
  AVFormatContext *ic = avformat_alloc_context();
  err = avformat_open_input(&ic, argv[1], file_iformat, &format_opts);
  if (err < 0)
  {
    printf("avformat_open_input failed\n");
    return -1;
  }

  AVCodecContext *avctx = avcodec_alloc_context3(NULL);
  if (!avctx)
  {
    printf("avcodec_alloc_context3 failed\n");
    return -1;
  }

  ret = avcodec_parameters_to_context(avctx, ic->streams[AVMEDIA_TYPE_VIDEO]->codecpar);
  if (ret < 0)
  {
    printf("avcodec_parameters_to_context failed\n");
    return -1;
  }
  avctx->pkt_timebase = ic->streams[AVMEDIA_TYPE_VIDEO]->time_base;
  AVCodec *codec = avcodec_find_decoder(avctx->codec_id);
  printf("codec: %s\n", codec->name);
  avctx->codec_id = codec->id;

  //AVDictionary *opts = filter_codec_opts(codec_opts, avctx->codec_id, ic, ic->streams[AVMEDIA_TYPE_VIDEO], codec);
  if ((ret = avcodec_open2(avctx, codec, NULL)) < 0)
  {
    printf("avcodec_open2 failed\n");
    return -1;
  }

  AVPacket pkt1, *pkt = &pkt1;
  AVFrame *frame = av_frame_alloc();
  SDL_Texture *texture = NULL;
  SDL_Rect rect = {0, 0, 640, 480};
  for (int i = 0; i < 100; i++)
  {
    ret = av_read_frame(ic, pkt);
    if (ret < 0)
    {
      printf("av_read_frame failed\n");
    }

    // 只解视频的
    if (pkt->stream_index == AVMEDIA_TYPE_VIDEO)
    {
      if (avcodec_send_packet(avctx, pkt) == AVERROR(EAGAIN))
      {
        printf("avcodec_send_packet failed\n");
      }
      ret = avcodec_receive_frame(avctx, frame);
      if (ret >= 0)
      {
        printf("receive one frame\n");
      }
      if (!texture && ret >= 0)
      {
        texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, frame->width, frame->height);
        if (!texture)
        {
          printf("SDL_CreateTexture failed\n");
        }
      }

//      if (texture)
//      {
//        if (frame->linesize[0] > 0 && frame->linesize[1] > 0 && frame->linesize[2] > 0)
//        {
//            ret = SDL_UpdateYUVTexture(texture, NULL, frame->data[0], frame->linesize[0],
//                                      frame->data[1], frame->linesize[1],
//                                      frame->data[2], frame->linesize[2]);
//        }
//        else if (frame->linesize[0] < 0 && frame->linesize[1] < 0 && frame->linesize[2] < 0)
//        {
//            ret = SDL_UpdateYUVTexture(*tex, NULL, frame->data[0] + frame->linesize[0] * (frame->height - 1), -frame->linesize[0], frame->data[1] + frame->linesize[1] * (AV_CEIL_RSHIFT(frame->height, 1) - 1), -frame->linesize[1], frame->data[2] + frame->linesize[2] * (AV_CEIL_RSHIFT(frame->height, 1) - 1), -frame->linesize[2]);
//        }
//        else
//        {
//          printf("Mixed negative and positive linesizes are not supported\n");
//        }
//      }

      SDL_RenderCopyEx(renderer, texture, NULL, &rect, 0, NULL, 0);

    }
  }

  while(1)
  {
    usleep(1000*10);
  }

  return 0;
}
