
// mp4_player.h : PROJECT_NAME 应用程序的主头文件
//

#pragma once

#ifndef __AFXWIN_H__
	#error "在包含此文件之前包含“stdafx.h”以生成 PCH 文件"
#endif

#include "resource.h"		// 主符号


// Cmp4_playerApp: 
// 有关此类的实现，请参阅 mp4_player.cpp
//

class Cmp4_playerApp : public CWinApp
{
public:
	Cmp4_playerApp();

// 重写
public:
	virtual BOOL InitInstance();

// 实现

	DECLARE_MESSAGE_MAP()
};

extern Cmp4_playerApp theApp;