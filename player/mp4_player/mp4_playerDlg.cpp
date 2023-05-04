
// mp4_playerDlg.cpp : 实现文件
//

#include "stdafx.h"
#include "mp4_player.h"
#include "mp4_playerDlg.h"
#include "afxdialogex.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// 用于应用程序“关于”菜单项的 CAboutDlg 对话框

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

// 实现
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// Cmp4_playerDlg 对话框



Cmp4_playerDlg::Cmp4_playerDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(IDD_MP4_PLAYER_DIALOG, pParent)
	, m_strPlayUrl(_T(""))
	, m_frameRGB(NULL)
  , m_picBuf(NULL)
	, m_imgCtx(NULL)
	, m_audioSwrCtx(NULL)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void Cmp4_playerDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT_URL, m_strPlayUrl);
}

BEGIN_MESSAGE_MAP(Cmp4_playerDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_BROWSE, &Cmp4_playerDlg::OnBnClickedBrowse)
	ON_BN_CLICKED(IDC_PLAY, &Cmp4_playerDlg::OnBnClickedPlay)
	ON_BN_CLICKED(IDC_STOP, &Cmp4_playerDlg::OnBnClickedStop)
	ON_WM_CLOSE()
END_MESSAGE_MAP()


// Cmp4_playerDlg 消息处理程序

BOOL Cmp4_playerDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 将“关于...”菜单项添加到系统菜单中。

	// IDM_ABOUTBOX 必须在系统命令范围内。
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// 设置此对话框的图标。  当应用程序主窗口不是对话框时，框架将自动
	//  执行此操作
	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标

	initUI();
	initDecoder();
	initRender();

	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

void Cmp4_playerDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// 如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。  对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。

void Cmp4_playerDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 用于绘制的设备上下文

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 使图标在工作区矩形中居中
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 绘制图标
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR Cmp4_playerDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

static int64_t getTickCount()
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now().time_since_epoch()).count();
}

static AVRational AvTimeBaseQ() {
  AVRational ret = { 1, AV_TIME_BASE };
  return ret;
}

void Cmp4_playerDlg::initUI()
{
	m_strPlayUrl = _T("..\\resource\\big_buck_bunny.mp4");
	UpdateData(FALSE);
}

void Cmp4_playerDlg::initDecoder()
{
	avcodec_register_all();
	av_register_all();

	resetDecoder();
}

void Cmp4_playerDlg::resetDecoder()
{
	m_picBytes = 0;
	if (m_imgCtx)
	{
		sws_freeContext(m_imgCtx);
		m_imgCtx = NULL;
	}
	if (m_frameRGB != NULL)
	{
		av_frame_free(&m_frameRGB);
		m_frameRGB = NULL;
	}

	if (m_picBuf != NULL)
	{
		delete[] m_picBuf;
		m_picBuf = NULL;
	}

	if (m_audioSwrCtx)
	{
		swr_free(&m_audioSwrCtx);
		m_audioSwrCtx = NULL;
	}

	m_vstream_index = 0;
	m_astream_index = 0;
}

void Cmp4_playerDlg::initRender()
{
	m_canvasWidth = 0;
	m_canvasHeight = 0;

	m_firstPlayAudio = true;

	m_DrawDib = DrawDibOpen();

	if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_TIMER)) 
	{
		TRACE("Could not initialize SDL - %s\n", SDL_GetError());
	}
}

void Cmp4_playerDlg::deInitRender()
{
	m_firstPlayAudio = true;
	DrawDibClose(m_DrawDib);
	SDL_Quit();
}

unsigned __stdcall Cmp4_playerDlg::DemuxerProc(void *pVoid)
{
	Cmp4_playerDlg* pDlg = (Cmp4_playerDlg*)pVoid;
	pDlg->DemuxerWorker();
	return 0;
}

unsigned __stdcall Cmp4_playerDlg::PlayProc(void *pVoid)
{
	Cmp4_playerDlg* pDlg = (Cmp4_playerDlg*)pVoid;
	pDlg->PlayerWorker();
	return 0;
}

void Cmp4_playerDlg::DemuxerWorker()
{
	int ret;
	bool isBufferFull;
	const int maxDecodedListSize = 100; // 解码队列中的音视频帧最大个数
	AVPacket pkt1, *pkt = &pkt1;
	while (av_read_frame(m_fmtCtx, pkt) >= 0)
	{
    if (m_bStopThread)
    {
      break;
    }

		isBufferFull = false;
		// 视频
		if (pkt->stream_index == m_vstream_index)
		{
      AVStream *pStream = m_fmtCtx->streams[pkt->stream_index];
      pkt->pts = av_rescale_q(pkt->pts, pStream->time_base, AvTimeBaseQ()) / 1000;
      pkt->dts = av_rescale_q(pkt->dts, pStream->time_base, AvTimeBaseQ()) / 1000;

			if (avcodec_send_packet(m_vCodecCtx, pkt) == AVERROR(EAGAIN))
			{
				TRACE("[video] avcodec_send_packet failed\n");
			}
			AVFrame *frame = av_frame_alloc();
			ret = avcodec_receive_frame(m_vCodecCtx, frame);
			if (ret >= 0)
			{
				TRACE("[video] receive one video frame\n");
				std::lock_guard<std::mutex> lock(m_dListMtx);
				m_vdFrameList.push_back(frame);
				if (m_vdFrameList.size() > maxDecodedListSize 
					&& m_adFrameList.size() > maxDecodedListSize)
				{
					isBufferFull = true;
				}
			}
			else
			{
				av_frame_free(&frame);
			}
		}
		else if (pkt->stream_index == m_astream_index)
		{
			if (avcodec_send_packet(m_aCodecCtx, pkt) == AVERROR(EAGAIN))
			{
				TRACE("[audio] avcodec_send_packet failed\n");
			}
			AVFrame *frame = av_frame_alloc();
			ret = avcodec_receive_frame(m_aCodecCtx, frame);
			if (ret >= 0)
			{
				TRACE("[audio] receive one audio frame\n");
				std::lock_guard<std::mutex> lock(m_dListMtx);
				m_adFrameList.push_back(frame);
				if (m_adFrameList.size() > maxDecodedListSize 
					&& m_vdFrameList.size() > maxDecodedListSize)
				{
					isBufferFull = true;
				}
			}
			else
			{
				av_frame_free(&frame);
			}
		}

		if (isBufferFull)
		{
			Sleep(100);
		}
	}

  SetEvent(m_hThreadEvent[0]);
  m_bDemuxing = false;
}

void Cmp4_playerDlg::PlayerWorker()
{
	m_firstFramePts = 0;
	m_firstFrameTick = 0;
	while (true)
	{
    if (m_bStopThread)
    {
      break;
    }

		// 获取队列中的第一帧，如果该帧到了播放时机则进行渲染，然后删除队列中的帧
		// 如果未到播放时机则等下次peek
		AVFrame* aframe = peekOneAudioFrame();
		AVFrame* vframe = peekOneVideoFrame();
		if (renderOneAudioFrame(aframe))
		{
			popOneAudioFrame();
		}
		if (renderOneVideoFrame(vframe))
		{
			popOneVideoFrame();
		}
		Sleep(10);
	}

  SetEvent(m_hThreadEvent[1]);
  m_bPlaying = false;
}

bool Cmp4_playerDlg::renderOneAudioFrame(AVFrame* frame)
{
	if (frame == NULL)
	{
		return false;
	}
  // 音频通过SDL拉模式播放，不用做播放控制
	playAudio(frame);
	return true;
}

bool Cmp4_playerDlg::renderOneVideoFrame(AVFrame* frame)
{
	if (frame == NULL)
	{
		return false;
	}
	if (isTimeToRender(frame->pts))
	{
		playVideo(frame);
		if (m_firstFramePts == 0)
		{
			m_firstFramePts = frame->pts;
			m_firstFrameTick = getTickCount();
		}
		return true;
	}
	return false;
}

AVFrame* Cmp4_playerDlg::peekOneAudioFrame()
{
	std::lock_guard<std::mutex> lock(m_dListMtx);
	if (m_adFrameList.empty())
	{
		return NULL;
	}
	return m_adFrameList.front();
}

AVFrame* Cmp4_playerDlg::peekOneVideoFrame()
{
	std::lock_guard<std::mutex> lock(m_dListMtx);
	if (m_vdFrameList.empty())
	{
		return NULL;
	}
	return m_vdFrameList.front();
}

void Cmp4_playerDlg::popOneAudioFrame()
{
	std::lock_guard<std::mutex> lock(m_dListMtx);
	if (!m_adFrameList.empty())
	{
		AVFrame* frame = m_adFrameList.front();
		av_frame_free(&frame);
		m_adFrameList.pop_front();
	}
}

void Cmp4_playerDlg::popOneVideoFrame()
{
	std::lock_guard<std::mutex> lock(m_dListMtx);
	if (!m_vdFrameList.empty())
	{
		AVFrame* frame = m_vdFrameList.front();
		av_frame_free(&frame);
		m_vdFrameList.pop_front();
	}
}

void Cmp4_playerDlg::clearFrameList()
{
  // 清空渲染队列
  {
    std::lock_guard<std::mutex> lock(m_dListMtx);
    while (!m_vdFrameList.empty())
    {
      AVFrame* frame = m_vdFrameList.front();
      av_frame_free(&frame);
      m_vdFrameList.pop_front();
    }

    while (!m_adFrameList.empty())
    {
      AVFrame* frame = m_adFrameList.front();
      av_frame_free(&frame);
      m_adFrameList.pop_front();
    }
  }

  // 清空音频pending队列
  {
    std::lock_guard<std::mutex> lock(m_apListMtx);
    while (!m_aPendingList.empty())
    {
      ARFrame* frame = m_aPendingList.front();
      delete frame;
      m_aPendingList.pop_front();
    }
  }
}

// 播放逻辑控制，按音视频中的pts来播放
bool Cmp4_playerDlg::isTimeToRender(int64_t pts)
{
	if (0 == m_firstFramePts)
	{
		return true; // 首帧直接播放
	}
	int64_t ptsDelta = pts - m_firstFramePts;
	int64_t tickDelta = getTickCount() - m_firstFrameTick;
	if (tickDelta >= ptsDelta)
	{
		return true;
	}
	return false;
}

void Cmp4_playerDlg::OnBnClickedBrowse()
{
	CFileDialog dlgFile(TRUE, NULL, NULL, OFN_HIDEREADONLY, _T("Describe Files (*.mp4)|*.mp4|All Files (*.*)|*.*||"), NULL);

	if (dlgFile.DoModal() == IDOK)
	{
		m_strPlayUrl = dlgFile.GetPathName();
	}
	UpdateData(FALSE);
}


void Cmp4_playerDlg::OnBnClickedPlay()
{
	char filename[MAX_PATH] = { 0 };
	strncpy_s(filename, m_strPlayUrl.GetString(), MAX_PATH - 1);
	AVInputFormat *inFmt = av_find_input_format("mp4");
	m_fmtCtx = avformat_alloc_context();

	AVDictionary *format_opts = NULL;
	int ret = avformat_open_input(&m_fmtCtx, filename, inFmt, &format_opts);
	if (ret < 0)
	{
		MessageBox("avformat_open_input failed");
		return;
	}

	m_vCodecCtx = avcodec_alloc_context3(NULL);
	m_aCodecCtx = avcodec_alloc_context3(NULL);
	if (!m_vCodecCtx || !m_aCodecCtx)
	{
		MessageBox("avcodec_alloc_context3 failed");
		return;
	}

	// 为了简化逻辑，需要同时有音视频，否则接下来的逻辑需要做更多的异常判断
	m_vstream_index = av_find_best_stream(m_fmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
	m_astream_index = av_find_best_stream(m_fmtCtx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
	if (m_vstream_index < 0 || m_astream_index < 0)
	{
		MessageBox("Cannot find audio or video index");
		return;
	}
	ret = avcodec_parameters_to_context(m_vCodecCtx, m_fmtCtx->streams[m_vstream_index]->codecpar);
	if (ret < 0)
	{
		MessageBox("[video] avcodec_parameters_to_context failed");
		return;
	}
	ret = avcodec_parameters_to_context(m_aCodecCtx, m_fmtCtx->streams[m_astream_index]->codecpar);
	if (ret < 0)
	{
		MessageBox("[audio] avcodec_parameters_to_context failed");
		return;
	}
	av_codec_set_pkt_timebase(m_vCodecCtx, m_fmtCtx->streams[m_vstream_index]->time_base);
	av_codec_set_pkt_timebase(m_aCodecCtx, m_fmtCtx->streams[m_astream_index]->time_base);

	AVCodec *vCodec = avcodec_find_decoder(m_vCodecCtx->codec_id);
	AVCodec *aCodec = avcodec_find_decoder(m_aCodecCtx->codec_id);
	if (vCodec != NULL)
	{
		TRACE("video codec: %s\n", vCodec->name);
		m_vCodecCtx->codec_id = vCodec->id;
	}
	if (aCodec != NULL)
	{
		TRACE("audio codec: %s\n", aCodec->name);
		m_aCodecCtx->codec_id = aCodec->id;
	}

	if ((ret = avcodec_open2(m_vCodecCtx, vCodec, NULL)) < 0)
	{
		MessageBox("[video] avcodec_open2 failed");
		return;
	}

	if ((ret = avcodec_open2(m_aCodecCtx, aCodec, NULL)) < 0)
	{
		MessageBox("[audio] avcodec_open2 failed");
		return;
	}

  m_hThreadEvent[0] = CreateEvent(NULL, TRUE, FALSE, NULL);
  m_hThreadEvent[1] = CreateEvent(NULL, TRUE, FALSE, NULL);
  m_bStopThread = false;
  m_bDemuxing = true;
  m_bPlaying = true;

	// 创建解封装和解码线程
	unsigned int demuxerThreadAddr;
	m_dwDemuxerThread = _beginthreadex(
		NULL,			// Security
		0,				// Stack size
		DemuxerProc,		// Function address
		this,			// Argument
		0,				// Init flag
		&demuxerThreadAddr);	// Thread address
	if (!m_dwDemuxerThread)
	{
		MessageBox("Could not create demuxer thread");
		return;
	}

	// 创建播放线程
	unsigned int playThreadAddr;
	m_dwPlayThread = _beginthreadex(
		NULL,			// Security
		0,				// Stack size
		PlayProc,		// Function address
		this,			// Argument
		0,				// Init flag
		&playThreadAddr);	// Thread address
	if (!m_dwPlayThread)
	{
		MessageBox("Could not create play thread");
	}

  GetDlgItem(IDC_PLAY)->EnableWindow(FALSE);
  GetDlgItem(IDC_STOP)->EnableWindow(TRUE);
}

void Cmp4_playerDlg::playVideo(AVFrame *frame)
{
	if (m_picBytes == 0)
	{
		m_picBytes = avpicture_get_size(AV_PIX_FMT_BGR24, m_vCodecCtx->width, m_vCodecCtx->height);
		m_picBuf = new uint8_t[m_picBytes];
		m_frameRGB = av_frame_alloc();
		avpicture_fill((AVPicture *)m_frameRGB, m_picBuf, AV_PIX_FMT_BGR24,
						m_vCodecCtx->width, m_vCodecCtx->height);
	}

	if (!m_imgCtx)
	{
		m_imgCtx = sws_getContext(m_vCodecCtx->width, m_vCodecCtx->height,
								  m_vCodecCtx->pix_fmt, m_vCodecCtx->width,
								  m_vCodecCtx->height, AV_PIX_FMT_BGR24,
								  SWS_BICUBIC, NULL, NULL, NULL);
	}

	frame->data[0] += frame->linesize[0] * (m_vCodecCtx->height - 1);
	frame->linesize[0] *= -1;
	frame->data[1] += frame->linesize[1] * (m_vCodecCtx->height / 2 - 1);
	frame->linesize[1] *= -1;
	frame->data[2] += frame->linesize[2] * (m_vCodecCtx->height / 2 - 1);
	frame->linesize[2] *= -1;
	sws_scale(m_imgCtx, (const uint8_t* const*)frame->data, frame->linesize,
			  0, m_vCodecCtx->height, m_frameRGB->data, m_frameRGB->linesize);

	displayPicture(m_frameRGB->data[0], m_vCodecCtx->width, m_vCodecCtx->height);
}

void Cmp4_playerDlg::displayPicture(uint8_t* data, int width, int height)
{
	CRect rc;
	CWnd* PlayWnd = GetDlgItem(IDC_VIDEO_CANVAS);
	HDC hdc = PlayWnd->GetDC()->GetSafeHdc();
	PlayWnd->GetClientRect(&rc);

	init_bm_head(width, height);

	DrawDibDraw(m_DrawDib,
				hdc,
				rc.left,
				rc.top,
        rc.Width(),			// 拉伸到和窗口一样的尺寸
        rc.Height(),
				&m_bm_info.bmiHeader,
				(void*)data,
				0,
				0,
				width,
				height,
				0);
}

void Cmp4_playerDlg::init_bm_head(int width, int height)
{
	m_bm_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	m_bm_info.bmiHeader.biWidth = width;
	m_bm_info.bmiHeader.biHeight = height;
	m_bm_info.bmiHeader.biPlanes = 1;
	m_bm_info.bmiHeader.biBitCount = 24;
	m_bm_info.bmiHeader.biCompression = BI_RGB;
	m_bm_info.bmiHeader.biSizeImage = 0;
	m_bm_info.bmiHeader.biClrUsed = 0;
	m_bm_info.bmiHeader.biClrImportant = 0;
}

void Cmp4_playerDlg::playAudio(AVFrame *frame)
{
  if (m_firstPlayAudio)
  {
    openSdlAudio(frame->sample_rate, frame->channels, frame->nb_samples);
    m_firstPlayAudio = false;
  }

	// 重采样
  if (m_audioSwrCtx == NULL)
  {
    m_audioSwrCtx = swr_alloc_set_opts(m_audioSwrCtx,
                                      frame->channel_layout,
                                      AV_SAMPLE_FMT_S16,
                                      frame->sample_rate,
                                      frame->channel_layout,
                                      (AVSampleFormat)frame->format,
                                      frame->sample_rate,
                                      0,
                                      NULL);
    swr_init(m_audioSwrCtx);
  }

  int dataLen = frame->channels * frame->nb_samples * 2;
  ARFrame* rframe = new ARFrame(dataLen);
	swr_convert(m_audioSwrCtx, &rframe->m_data, frame->nb_samples, (const uint8_t **)frame->data, frame->nb_samples);
  {
    std::lock_guard<std::mutex> lock(m_apListMtx);
    m_aPendingList.push_back(rframe);
    // TRACE("push one audio frame to render list\n");
  }
}

void Cmp4_playerDlg::openSdlAudio(int sampleRate, int channels, int samples)
{
	SDL_AudioSpec spec;
	spec.freq = sampleRate;
	spec.format = AUDIO_S16SYS;
	spec.channels = channels;
	spec.silence = 0;
	spec.samples = samples;
	spec.callback = fillAudio;
	spec.userdata = this;
	if (SDL_OpenAudio(&spec, NULL))
	{
		TRACE("Failed to open audio device, %s\n", SDL_GetError());
	}

	SDL_PauseAudio(0);
}

void Cmp4_playerDlg::closeSdlAudio()
{
  SDL_CloseAudio();
}

void Cmp4_playerDlg::fillAudio(void* udata, Uint8* stream, int len)
{
	Cmp4_playerDlg* thiz = (Cmp4_playerDlg*)udata;
	thiz->innerFillAudio(stream, len);
}

void Cmp4_playerDlg::innerFillAudio(Uint8* stream, int len)
{
	SDL_memset(stream, 0, len);
  {
    std::lock_guard<std::mutex> lock(m_apListMtx);
    if (m_aPendingList.empty())
    {
      // TRACE("audio pull empty\n");
      return;
    }
    ARFrame* rframe = m_aPendingList.front();
    m_aPendingList.pop_front();
    int copySize = min(len, (int)rframe->m_length);
    SDL_MixAudioFormat(stream, rframe->m_data, AUDIO_S16SYS, copySize, 100);
    delete rframe;
    // TRACE("fill one audio frame, len %d\n", copySize);
  }
}

void Cmp4_playerDlg::OnBnClickedStop()
{
  m_bStopThread = true;
  if (m_bDemuxing)
  {
    WaitForSingleObject(m_hThreadEvent[0], INFINITE);
    CloseHandle(m_hThreadEvent[0]);
    m_hThreadEvent[0] = NULL;
    m_bDemuxing = false;
  }
  if (m_bPlaying)
  {
    WaitForSingleObject(m_hThreadEvent[1], INFINITE);
    CloseHandle(m_hThreadEvent[1]);
    m_hThreadEvent[1] = NULL;
    m_bPlaying = false;
  }
  clearFrameList();
  resetDecoder();
  closeSdlAudio();
  m_firstPlayAudio = true;

  GetDlgItem(IDC_PLAY)->EnableWindow(TRUE);
  GetDlgItem(IDC_STOP)->EnableWindow(FALSE);
}


void Cmp4_playerDlg::OnClose()
{
  OnBnClickedStop();
	deInitRender();

	CDialogEx::OnClose();
}
