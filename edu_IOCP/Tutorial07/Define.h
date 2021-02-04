#pragma once

#include <winsock2.h>
#include <WS2tcpip.h>

const UINT32 MAX_SOCKBUF = 256;
const UINT32 MAX_WORKTHREAD = 4;

enum class IOOperation
{
	RECV,
	SEND,
	ACCEPT,
};

struct stOverlappedEx
{
	WSAOVERLAPPED m_wsaOverlapped;
	WSABUF m_wsaBuf;
	IOOperation m_eOperation;
	UINT32 SessionIndex = 0;
};