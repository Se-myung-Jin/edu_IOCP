#pragma once

#include "Define.h"
#include <stdio.h>
#include <mutex>
#include <queue>


class ClientInfo
{
private:
	INT32 mIndex = 0;
	SOCKET mSock;
	stOverlappedEx mRecvOverlappedEx;

	char mRecvBuf[MAX_SOCKBUF];

	std::mutex mSendLock;
	std::queue <stOverlappedEx*> mSendDataQueue;

public:
	ClientInfo()
	{
		ZeroMemory(&mRecvOverlappedEx, sizeof(stOverlappedEx));
		mSock = INVALID_SOCKET;
	}

	void Init(const UINT32 _index)
	{
		mIndex = _index;
	}

	UINT32 GetIndex() { return mIndex; }

	bool IsConnected() { return mSock != INVALID_SOCKET; }

	SOCKET GetSock() { return mSock; }

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

	void Close(bool _bIsForce = false)
	{
		struct linger stLinger = { 0,0 };

		if (_bIsForce == true)
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
	
	}

	bool BindIOCompletionPort(HANDLE _iocpHandle)
	{
		auto hIOCP = CreateIoCompletionPort((HANDLE)mSock,
			_iocpHandle,
			(ULONG_PTR)(this),
			0);

		if (hIOCP == INVALID_HANDLE_VALUE)
		{
			return false;
		}

		return true;
	}

	bool BindRecv()
	{
		DWORD dwRecvNumBytes;
		DWORD dwFlag = 0;

		mRecvOverlappedEx.m_wsaBuf.buf = mRecvBuf;
		mRecvOverlappedEx.m_wsaBuf.len = MAX_SOCKBUF;
		mRecvOverlappedEx.m_eOperation = IOOperation::RECV;

		int nRet = WSARecv(mSock,
			&mRecvOverlappedEx.m_wsaBuf,
			1,
			&dwRecvNumBytes,
			&dwFlag,
			(LPWSAOVERLAPPED)&mRecvOverlappedEx,
			NULL);

		if (nRet == SOCKET_ERROR && WSAGetLastError() != ERROR_IO_PENDING)
		{
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

		std::lock_guard<std::mutex> guard(mSendLock);

		mSendDataQueue.push(sendOverlappedEx);

		if (mSendDataQueue.size() == 1)
		{
			SendIO();
		}

		return true;
	}

	void SendCompleted(const UINT32 _dataSize)
	{
		std::lock_guard<std::mutex> guard(mSendLock);

		delete[] mSendDataQueue.front()->m_wsaBuf.buf;
		delete mSendDataQueue.front();

		mSendDataQueue.pop();

		if (mSendDataQueue.empty() == false)
		{
			SendIO();
		}
	}

private:
	bool SendIO()
	{
		auto sendOverlappedEx = mSendDataQueue.front();

		DWORD dwRecvNumBytes = 0;

		int nRet = WSASend(mSock,
			&sendOverlappedEx->m_wsaBuf,
			1,
			&dwRecvNumBytes,
			0,
			(LPWSAOVERLAPPED)&sendOverlappedEx,
			NULL);

		if (nRet == SOCKET_ERROR && WSAGetLastError() != ERROR_IO_PENDING)
		{
			return false;
		}

		return true;
	}
};