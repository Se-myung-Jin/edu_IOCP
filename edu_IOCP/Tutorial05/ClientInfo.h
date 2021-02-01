#pragma once

#include "Define.h"
#include <stdio.h>
#include <mutex>

class ClientInfo
{
private:
	INT32 mIndex = 0;
	SOCKET mSock;
	stOverlappedEx mRecvOverlappedEx;
	stOverlappedEx mSendOverlappedEx;

	char mRecvBuf[MAX_SOCKBUF];

	std::mutex mSendLock;
	bool mIsSending = false;
	UINT64 mSendPos = 0;
	char mSendBuf[MAX_SOCKBUF];
	char mSendingBuf[MAX_SOCKBUF];

public:
	ClientInfo()
	{
		ZeroMemory(&mRecvOverlappedEx, sizeof(stOverlappedEx));
		ZeroMemory(&mSendOverlappedEx, sizeof(stOverlappedEx));
		mSock = INVALID_SOCKET;
	}
	~ClientInfo() = default;

	void Init(const UINT32 _index)
	{
		mIndex = _index;
	}

	UINT32 GetIndex() { return mIndex; }

	bool IsConnected() { return mSock != INVALID_SOCKET; }

	SOCKET GetSocket() { return mSock; }

	char* RecvBuffer() { return mRecvBuf; }

	bool OnConnect(HANDLE _iocpHandle, SOCKET _socket)
	{
		mSock = _socket;

		Clear();

		if (BindIOCompletionPort(_iocpHandle) == false)
		{
			return false;
		}

		return BindRecv();
	}

	void Close(bool bIsForce = false)
	{
		struct linger stLinger = { 0, 0 };

		if (bIsForce == true)
		{
			stLinger.l_onoff = 1;
		}

		shutdown(mSock, SD_BOTH);

		setsockopt(mSock, SOL_SOCKET, SO_LINGER, (char*)&stLinger, sizeof(stLinger));

		closesocket(mSock);

		mSock = INVALID_SOCKET;
	}

	void Clear()
	{
		mSendPos = 0;
		mIsSending = false;
	}

	bool BindIOCompletionPort(HANDLE _iocpHandle)
	{
		auto hIOCP = CreateIoCompletionPort((HANDLE)GetSocket(),
			_iocpHandle,
			(ULONG_PTR)(this),
			0);

		if (hIOCP == INVALID_HANDLE_VALUE)
		{
			printf("error msg : %d\n", GetLastError());
			return false;
		}

		return true;
	}

	bool BindRecv()
	{
		DWORD dwFlag = 0;
		DWORD dwRecvNumBytes = 0;

		mRecvOverlappedEx.m_wsaBuf.buf = mRecvBuf;
		mRecvOverlappedEx.m_wsaBuf.len = MAX_SOCKBUF;
		mRecvOverlappedEx.m_eOperation = IOOperation::RECV;

		int nRet = WSARecv(mSock,
			&(mRecvOverlappedEx.m_wsaBuf),
			1,
			&dwRecvNumBytes,
			&dwFlag,
			(LPWSAOVERLAPPED)&mRecvOverlappedEx,
			NULL);

		if (nRet == SOCKET_ERROR && (WSAGetLastError() != ERROR_IO_PENDING))
		{
			printf("error msg : %d\n", WSAGetLastError());
			return false;
		}

		return true;
	}

	bool SendMsg(const UINT32 _dataSize, char* _pMsg)
	{
		std::lock_guard<std::mutex> guard(mSendLock);

		if ((mSendPos + _dataSize) > MAX_SOCKBUF)
		{
			mSendPos = 0;
		}

		auto pSendBuf = &mSendBuf[mSendPos];

		CopyMemory(pSendBuf, _pMsg, _dataSize);
		mSendPos += _dataSize;

		return true;
	}

	bool SendIO()
	{
		if (mSendPos <= 0 || mIsSending)
		{
			return true;
		}

		std::lock_guard<std::mutex> guard(mSendLock);

		mIsSending = true;

		CopyMemory(mSendingBuf, &mSendBuf[0], mSendPos);

		mSendOverlappedEx.m_wsaBuf.buf = &mSendingBuf[0];
		mSendOverlappedEx.m_wsaBuf.len = mSendPos;
		mSendOverlappedEx.m_eOperation = IOOperation::SEND;

		DWORD dwSendNumBytes = 0;

		int nRet = WSASend(mSock,
			&(mSendOverlappedEx.m_wsaBuf),
			1,
			&dwSendNumBytes,
			0,
			(LPWSAOVERLAPPED)&(mSendOverlappedEx),
			NULL);

		if (nRet == SOCKET_ERROR && (WSAGetLastError() != ERROR_IO_PENDING))
		{
			printf("error msg : %d\n", WSAGetLastError());
			return false;
		}

		mSendPos = 0;
		return true;
	}

	void SendComplete(const UINT32 _dataSize)
	{
		mIsSending = false;
		printf("send complete : %d bytes\n", _dataSize);
	}
};