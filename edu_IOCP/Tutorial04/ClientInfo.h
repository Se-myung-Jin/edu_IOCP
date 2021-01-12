#pragma once

#include "Define.h"
#include <stdio.h>

class ClientInfo
{
private:
	SOCKET m_socketClient;
	
	stOverlappedEx m_stRecvOverlappedEx;
	
	char m_RecvBuf[MAX_SOCKBUF];

	INT32 mIndex = 0;
public:
	ClientInfo() 
	{
		ZeroMemory(&m_stRecvOverlappedEx, sizeof(stOverlappedEx));
		m_socketClient = INVALID_SOCKET;
	}
	~ClientInfo(){}

	void Init(const UINT32 _index)
	{
		mIndex = _index;
	}

	UINT32 GetIndex() { return mIndex; }

	bool IsConnected() { return m_socketClient != INVALID_SOCKET; }

	SOCKET GetSock() { return m_socketClient; }

	char* RecvBuffer() { return m_RecvBuf; }

	void Clear()
	{

	}

	void Close(bool _bIsForce = false)
	{
		struct linger stLinger = { 0,0 };

		if (_bIsForce == true)
		{
			stLinger.l_onoff = 1;
		}

		shutdown(m_socketClient, SD_BOTH);

		setsockopt(m_socketClient, SOL_SOCKET, SO_LINGER, (char*)&stLinger, sizeof(stLinger));

		closesocket(m_socketClient);
		m_socketClient = INVALID_SOCKET;
	}

	bool OnConnect(HANDLE _iocpHandle, SOCKET _socket)
	{
		m_socketClient = _socket;

		Clear();

		if (BindIOCompletionPort(_iocpHandle) == false)
		{
			return false;
		}

		return BindRecv();
	}

	bool BindIOCompletionPort(HANDLE _iocpHandle)
	{
		auto hIOCP = CreateIoCompletionPort((HANDLE)m_socketClient,
			_iocpHandle,
			(ULONG_PTR)this,
			0);

		if (hIOCP == INVALID_HANDLE_VALUE)
		{
			printf("[error] msg : %d\n", GetLastError());
			return false;
		}
		
		return true;
	}

	bool BindRecv()
	{
		DWORD dwRecvNumBytes;
		DWORD dwFlag;

		m_stRecvOverlappedEx.m_wsaBuf.len = MAX_SOCKBUF;
		m_stRecvOverlappedEx.m_wsaBuf.buf = m_RecvBuf;
		m_stRecvOverlappedEx.m_eOperation = IOOperation::RECV;

		int nRet = WSARecv(m_socketClient,
			(LPWSABUF)&m_stRecvOverlappedEx.m_wsaBuf,
			1,
			&dwRecvNumBytes,
			&dwFlag,
			(LPWSAOVERLAPPED)&m_stRecvOverlappedEx,
			NULL);

		if (nRet == SOCKET_ERROR && WSAGetLastError() != ERROR_IO_PENDING)
		{
			printf("[error] msg : %d\n", WSAGetLastError());
			return false;
		}

		return true;
	}

	bool SendMsg(const UINT32 _dataSize, char* _pMsg)
	{
		auto sendOverlappedEx = new stOverlappedEx;
		ZeroMemory(&sendOverlappedEx, sizeof(stOverlappedEx));
		sendOverlappedEx->m_wsaBuf.len = _dataSize;
		sendOverlappedEx->m_wsaBuf.buf = new char[_dataSize];
		CopyMemory(&sendOverlappedEx->m_wsaBuf.buf, _pMsg, _dataSize);
		sendOverlappedEx->m_eOperation = IOOperation::SEND;

		DWORD dwSendNumBytes = 0;

		int nRet = WSASend(m_socketClient,
			(LPWSABUF)&sendOverlappedEx->m_wsaBuf,
			1,
			&dwSendNumBytes,
			0,
			(LPWSAOVERLAPPED)&sendOverlappedEx,
			NULL);

		if (nRet == SOCKET_ERROR && WSAGetLastError() != ERROR_IO_PENDING)
		{
			printf("error msg : %d\n", WSAGetLastError());
			return false;
		}

		return true;
	}

	void SendCompleted(const UINT32 _dataSize)
	{
		printf("[send completed] bytes : %d\n", _dataSize);
	}
};