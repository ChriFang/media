
// mp4_playerDlg.h : ͷ�ļ�
//

#pragma once

extern "C" {
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"
}

#include "SDL.h"

#include<vfw.h>
#include<list>
#include<mutex>

// audio render frame
struct ARFrame
{
  ARFrame(int len)
  {
    m_data = new uint8_t[len];
    m_length = len;
  }

  ~ARFrame()
  {
    if (m_data != NULL)
    {
      delete[] m_data;
      m_length = 0;
    }
  }

  uint8_t* m_data;
  int m_length;
};

// Cmp4_playerDlg �Ի���
class Cmp4_playerDlg : public CDialogEx
{
// ����
public:
	Cmp4_playerDlg(CWnd* pParent = NULL);	// ��׼���캯��

// �Ի�������
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_MP4_PLAYER_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV ֧��


// ʵ��
protected:
	HICON m_hIcon;

	// ���ɵ���Ϣӳ�亯��
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg void OnClose();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnBnClickedBrowse();
	afx_msg void OnBnClickedPlay();
	afx_msg void OnBnClickedStop();
	DECLARE_MESSAGE_MAP()

private:
	void initUI();
	void initDecoder();
	void resetDecoder();
	void initRender();
	void deInitRender();

	void playVideo(AVFrame *frame);
	void displayPicture(uint8_t* data, int width, int height);
	void init_bm_head(int width, int height);
	void getCanvasSize();

	void playAudio(AVFrame *frame);
	void openSdlAudio(int sampleRate, int channels, int samples);

private:
	static void fillAudio(void* udata, Uint8* stream, int len);
	void innerFillAudio(Uint8* stream, int len);

private:
	static unsigned __stdcall DemuxerProc(void *pVoid);
	static unsigned __stdcall PlayProc(void *pVoid);
	void DemuxerWorker();
	void PlayerWorker();

	bool renderOneAudioFrame(AVFrame* frame);
	bool renderOneVideoFrame(AVFrame* frame);

	AVFrame* peekOneAudioFrame();
	AVFrame* peekOneVideoFrame();
	void popOneAudioFrame();
	void popOneVideoFrame();

	bool isTimeToRender(int64_t pts);

public:
	CString m_strPlayUrl;
	
private:
	AVFormatContext *m_fmtCtx;
	AVCodecContext *m_vCodecCtx;
	AVCodecContext *m_aCodecCtx;
	AVFrame *m_frameRGB;

	int m_picBytes;
	uint8_t* m_picBuf;
	struct SwsContext *m_imgCtx;

	struct SwrContext *m_audioSwrCtx;

	int m_vstream_index;
	int m_astream_index;

private: // render
	// video
	BITMAPINFO m_bm_info;
	HDRAWDIB m_DrawDib;
	int m_canvasWidth;
	int m_canvasHeight;

	// audio
	bool m_firstPlayAudio;

	std::list<AVFrame*> m_vdFrameList;
	std::list<AVFrame*> m_adFrameList;
	std::mutex m_dListMtx;

	std::list<ARFrame*> m_aPendingList; // ����Ⱦ��Ƶ֡
	std::mutex m_apListMtx;

	int64_t m_firstFramePts;
	int64_t m_firstFrameTick;

private: // thread
	DWORD m_dwDemuxerThread;
	DWORD m_dwPlayThread;
};
