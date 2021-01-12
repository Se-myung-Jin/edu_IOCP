#pragma once

#include <winsock2.h>
#include <WS2tcpip.h>

const UINT32 MAX_SOCKBUF = 256;		// 패킷 크기
const UINT32 MAX_WORKTHREAD = 4;	// 스레드 수

enum class IOOperation
{
	RECV,
	SEND,
};

struct stOverlappedEx
{
	WSAOVERLAPPED m_wsaOverlapped;
	SOCKET m_socketClient;
	WSABUF m_wsaBuf;
	IOOperation m_eOperation;
};

struct stClientInfo
{
	INT32 mIndex = 0;
	SOCKET m_socketClient;
	stOverlappedEx m_stRecvOverlappedEx;
	stOverlappedEx m_stSendOverlappedEx;

	char mRecvBuf[MAX_SOCKBUF];
	char mSendBuf[MAX_SOCKBUF];

	stClientInfo()
	{
		ZeroMemory(&m_stRecvOverlappedEx, sizeof(stOverlappedEx));
		ZeroMemory(&m_stSendOverlappedEx, sizeof(stOverlappedEx));
		m_socketClient = INVALID_SOCKET;
	}
};