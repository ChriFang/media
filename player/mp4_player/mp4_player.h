
// mp4_player.h : PROJECT_NAME Ӧ�ó������ͷ�ļ�
//

#pragma once

#ifndef __AFXWIN_H__
	#error "�ڰ������ļ�֮ǰ������stdafx.h�������� PCH �ļ�"
#endif

#include "resource.h"		// ������


// Cmp4_playerApp: 
// �йش����ʵ�֣������ mp4_player.cpp
//

class Cmp4_playerApp : public CWinApp
{
public:
	Cmp4_playerApp();

// ��д
public:
	virtual BOOL InitInstance();

// ʵ��

	DECLARE_MESSAGE_MAP()
};

extern Cmp4_playerApp theApp;