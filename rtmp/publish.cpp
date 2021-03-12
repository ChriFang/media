#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "librtmp/rtmp.h"
#include "librtmp/log.h"



#define HTON16(x)  ((x>>8&0xff) | (x<<8&0xff00))
#define HTON24(x)  ((x>>16&0xff) | (x<<16&0xff0000) | (x&0xff00))
#define HTON32(x)  ((x>>24&0xff) | (x>>8&0xff00) | (x<<8&0xff0000) | (x<<24&0xff000000))
#define HTONTIME(x) ((x>>16&0xff) | (x<<16&0xff0000) | (x&0xff00) | (x&0xff000000))

/*read 1 byte*/
int ReadU8(uint32_t *u8, FILE* fp)
{
  if (fread(u8, 1, 1, fp) != 1)
  {
    return 0;
  }
  return 1;
}

/*read 2 byte*/
int ReadU16(uint32_t *u16, FILE* fp)
{
  if (fread(u16, 2, 1, fp) != 1)
  {
    return 0;
  }
  *u16 = HTON16(*u16);
  return 1;
}

/*read 3 byte*/
int ReadU24(uint32_t *u24, FILE* fp)
{
  if (fread(u24, 3, 1, fp) != 1)
  {
    return 0;
  }
  *u24 = HTON24(*u24);
  return 1;
}

/*read 4 byte*/
int ReadU32(uint32_t *u32, FILE* fp)
{
  if (fread(u32, 4, 1, fp) != 1)
  {
    return 0;
  }
  *u32 = HTON32(*u32);
  return 1;
}

/*read 1 byte,and loopback 1 byte at once*/
int PeekU8(uint32_t *u8, FILE* fp)
{
  if (fread(u8, 1, 1, fp) != 1)
  {
    return 0;
  }
  fseek(fp, -1, SEEK_CUR);
  return 1;
}

/*read 4 byte and convert to time format*/
int ReadTime(uint32_t *utime, FILE* fp)
{
  if (fread(utime, 4, 1, fp) != 1)
  {
    return 0;
  }
  *utime = HTONTIME(*utime);
  return 1;
}

void DumpBuffer(const char* sendBuf, uint32_t bufLength)
{
  static uint32_t dump_count = 0;
  if (dump_count++ >= 6)
  {
    return;
  }
  printf("dump %u data, size %u--------------------------------\n", dump_count, bufLength);

  uint32_t lineSize = 16;
  for (uint32_t i = 0; i < bufLength; i++)
  {
    if (i > 0 && i % lineSize == 0)
    {
      printf("\n");
    }
    uint32_t v = sendBuf[i] & 0x000000ff;
    printf("0x%02x, ", v);
  }

  printf("\n\n");
}


int main(int argc, char* argv[])
{
  uint32_t start_time = 0;
  uint32_t now_time = 0;
  uint32_t pre_frame_time = 0;
  uint32_t last_time = 0;
//  int bNextIsKey = 0;
  char* pFileBuf = NULL;
  uint32_t bufLength = 0;

  uint32_t type = 0;
  uint32_t dataLength = 0;
  uint32_t timestamp = 0;

  char rtmp_url[] = "rtmp://mcs.rtmp.yy.com/newpublish/";
  char play_path[] = "1351120983_1351120983_15013_50019867_0?secret=03ad2d3be4593f850a5c5dc84d9ddbab&t=1614609882&cfgid=15013_1200_800&ex_videosrc=20000&extmeta55=1&src_param=webyy";

  FILE* fp = fopen("xuehua.flv", "rb");
  if (!fp)
  {
    RTMP_LogPrintf("Open File Error.\n");
    return -1;
  }

  RTMP* rtmp = RTMP_Alloc();
  RTMP_Init(rtmp);
  rtmp->Link.timeout = 5;
  if (!RTMP_SetupURL(rtmp, rtmp_url))
  {
    RTMP_Log(RTMP_LOGERROR,"SetupURL Err\n");
    RTMP_Free(rtmp);
    return -1;
  }

  RTMP_EnableWrite(rtmp);
  RTMP_SetBufferMS(rtmp, 3600*1000);
  rtmp->Link.playpath.av_val = play_path;
  rtmp->Link.playpath.av_len = strlen(play_path);
  if (!RTMP_Connect(rtmp, NULL))
  {
    RTMP_Log(RTMP_LOGERROR,"Connect Err\n");
    RTMP_Free(rtmp);
    return -1;
  }
  if (!RTMP_ConnectStream(rtmp, 0))
  {
    RTMP_Log(RTMP_LOGERROR,"ConnectStream Err\n");
    RTMP_Close(rtmp);
    RTMP_Free(rtmp);
    return -1;
  }

  RTMP_LogPrintf("Start to send data ...\n");

  fseek(fp, 9, SEEK_SET);
  fseek(fp, 4, SEEK_CUR);
  start_time = RTMP_GetTime();
  RTMP_LogPrintf("start_time %u\n", start_time);
  while(1)
  {
    now_time = RTMP_GetTime() - start_time;
    if (now_time < pre_frame_time)
    {
      if (pre_frame_time > last_time)
      {
        //RTMP_LogPrintf("TimeStamp:%u ms\n", pre_frame_time);
        last_time = pre_frame_time;
      }
      usleep((pre_frame_time - now_time) * 1000);
      continue;
    }

    // jump over type
    fseek(fp, 1, SEEK_CUR);
    if (!ReadU24(&dataLength, fp))
    {
      break;
    }
    if (!ReadTime(&timestamp, fp))
    {
      break;
    }

    // jump back
    fseek(fp, -8, SEEK_CUR);

    bufLength = 11 + dataLength + 4;
    pFileBuf = (char*)malloc(bufLength);
    memset(pFileBuf, 0, bufLength);
    if (fread(pFileBuf, 1, bufLength, fp) != bufLength)
    {
      break;
    }

    pre_frame_time = timestamp;

    if (!RTMP_IsConnected(rtmp))
    {
      RTMP_Log(RTMP_LOGERROR, "rtmp is not connect\n");
      break;
    }
    DumpBuffer(pFileBuf, bufLength);
    if (!RTMP_Write(rtmp, pFileBuf, bufLength))
    {
      RTMP_Log(RTMP_LOGERROR, "Rtmp Write Error\n");
      break;
    }

    free(pFileBuf);
    pFileBuf = NULL;

    if (!PeekU8(&type, fp))
    {
      break;
    }
//    if (type == 0x09)
//    {
//      if (fseek(fp, 11, SEEK_CUR) != 0)
//      {
//        break;
//      }
//      if (!PeekU8(&type, fp))
//      {
//        break;
//      }
//      bNextIsKey = (type == 0x17 ? 1 : 0);
//      fseek(fp, -11, SEEK_CUR);
//    }
  }



  RTMP_LogPrintf("\nSend Data Over\n");

  fclose(fp);
  RTMP_Close(rtmp);
  RTMP_Free(rtmp);
  rtmp = NULL;
  if (pFileBuf)
  {
    free(pFileBuf);
    pFileBuf = NULL;
  }

  return 0;
}

