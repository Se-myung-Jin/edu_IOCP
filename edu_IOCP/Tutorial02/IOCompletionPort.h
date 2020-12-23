#pragma once
#pragma comment(lib, "ws2_32")

#include "Define.h"
#include <vector>
#include <thread>

class IOCompletionPort
{
private:
	std::vector<stClientInfo> mClientInfos;

	SOCKET mListenSocket;

	int mClientCnt = 0;

	std::vector<std::thread> mIOWorkerThreads;

	std::thread mAccepterThread;

	HANDLE mIOCPHandle = INVALID_HANDLE_VALUE;

	bool mIsWorkerRun = false;
	
	bool mIsAccepterRun = false;

	char mSocketBuf[1024] = { 0, };

public:
	IOCompletionPort() {}
	
	~IOCompletionPort()
	{
		WSACleanup();
	}

	bool InitSocket()
	{
		WSADATA wsaData;

		int nRet = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (nRet != 0)
		{
			printf("error msg : %d\n", WSAGetLastError());
			return false;
		}

		mListenSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, NULL, WSA_FLAG_OVERLAPPED);
		if (mListenSocket == INVALID_SOCKET)
		{
			printf("error msg : %d\n", WSAGetLastError());
			return false;
		}

		printf("Initialize socket completely\n");
		return true;
	}

	bool BindSocket(int _port)
	{
		SOCKADDR_IN stServerAddr;
		stServerAddr.sin_family = AF_INET;
		stServerAddr.sin_port = htons(_port);
		stServerAddr.sin_addr.s_addr = htonl(INADDR_ANY);

		int nRet = bind(mListenSocket, (SOCKADDR*)&stServerAddr, sizeof(SOCKADDR_IN));
		if (nRet != 0)
		{
			printf("error msg : %d\n", WSAGetLastError());
			return false;
		}

		nRet = listen(mListenSocket, 5);
		if (nRet != 0)
		{
			printf("error msg : %d\n", WSAGetLastError());
			return false;
		}

		printf("Bind socket completely\n");
		return true;
	}

	bool StartServer(const UINT32 _maxClientCnt)
	{
		CreateClient(_maxClientCnt);

		mIOCPHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, MAX_WORKERTHREAD);
		if (mIOCPHandle == NULL)
		{
			printf("error msg : %d\n", GetLastError());
			return false;
		}

		bool bRet = CreateWorkerThread();
		if (false == bRet) {
			return false;
		}

		bRet = CreateAccepterThread();
		if (false == bRet) {
			return false;
		}

		printf("Start server completely\n");
		return true;
	}

	void DestroyThread()
	{
		mIsWorkerRun = false;
		CloseHandle(mIOCPHandle);

		for (auto& thr : mIOWorkerThreads)
		{
			if (thr.joinable())
			{
				thr.join();
			}
		}

		mIsAccepterRun = false;
		closesocket(mListenSocket);

		if (mAccepterThread.joinable())
		{
			mAccepterThread.join();
		}
	}

private:
	void CreateClient(const UINT32 _maxClientCnt)
	{
		for (int i = 0; i < _maxClientCnt; ++i)
		{
			mClientInfos.emplace_back();
		}
	}

	bool CreateWorkerThread()
	{
		unsigned int uiThreadId = 0;
		for (int i = 0; i < MAX_WORKERTHREAD; ++i)
		{
			mIOWorkerThreads.emplace_back([this]() { WorkerThread(); });
		}

		printf("Create worker threads completely\n");
		return true;
	}

	bool CreateAccepterThread()
	{
		mAccepterThread = std::thread([this]() {AccepterThread(); });

		printf("Create accepter thread completely\n");
		return true;
	}

	void WorkerThread()
	{
		stClientInfo* pClientInfo;
		DWORD dwIoSize = 0;
		BOOL bSuccess = TRUE;
		LPOVERLAPPED lpOverlapped = NULL;

		while (mIsWorkerRun)
		{
			bSuccess = GetQueuedCompletionStatus(mIOCPHandle,
				&dwIoSize,
				(PULONG_PTR)&pClientInfo,
				&lpOverlapped,
				INFINITE);

			if (bSuccess == TRUE && dwIoSize == 0 && lpOverlapped == NULL)
			{
				mIsWorkerRun = false;
				continue;
			}

			if (bSuccess == FALSE || (bSuccess == TRUE && dwIoSize == 0))
			{
				printf("socket(%d) disconnected\n", (int)pClientInfo->m_socketClient);
				CloseSocket(pClientInfo);
				continue;
			}

			if (lpOverlapped == NULL)
			{
				continue;
			}

			auto pOverlappedEx = (stOverlappedEx*)lpOverlapped;

			if (IOOperation::RECV == pOverlappedEx->m_eOperation)
			{
				pClientInfo->mRecvBuf[dwIoSize] = '\0';
				printf("[RECV] bytes : %d , msg : %s\n", dwIoSize, pClientInfo->mRecvBuf);

				SendMsg(pClientInfo, pClientInfo->mRecvBuf, dwIoSize);

				BindRecv(pClientInfo);
			}
			else if (IOOperation::SEND == pOverlappedEx->m_eOperation)
			{
				printf("[SEND] bytes : %d , msg : %s\n", dwIoSize, pClientInfo->mSendBuf);
			}
			else
			{
				printf("[Exception] socket(%d)\n", (int)pClientInfo->m_socketClient);
			}
		}
	}

	void AccepterThread()
	{
		SOCKADDR_IN stClientAddr;
		int nAddrLen = sizeof(SOCKADDR_IN);

		while (mIsAccepterRun)
		{
			stClientInfo* pClientInfo = GetEmptyClientInfo();
			if (pClientInfo == NULL)
			{
				printf("[Error] Client Full\n");
				return;
			}

			pClientInfo->m_socketClient = accept(mListenSocket, (SOCKADDR*)&stClientAddr, &nAddrLen);
			if (pClientInfo->m_socketClient == INVALID_SOCKET)
			{
				continue;
			}

			bool bRet = BindIOCompletionPort(pClientInfo);
			if (bRet == false)
			{
				return;
			}

			bRet = BindRecv(pClientInfo);
			if (bRet == false)
			{
				return;
			}

			char clientIP[32] = { 0, };
			inet_ntop(AF_INET, &(stClientAddr.sin_addr), clientIP, 32 - 1);
			printf("connect client : IP(%s) SOCKET(%d)\n", clientIP, (int)pClientInfo->m_socketClient);

			++mClientCnt;
		}
	}

	stClientInfo* GetEmptyClientInfo()
	{
		for (auto& client : mClientInfos)
		{
			if (INVALID_SOCKET == client.m_socketClient)
			{
				return &client;
			}
		}

		return nullptr;
	}

	bool BindIOCompletionPort(stClientInfo* _pClientInfo)
	{
		auto hIOCP = CreateIoCompletionPort((HANDLE)_pClientInfo->m_socketClient
			, mIOCPHandle
			, (ULONG_PTR)(_pClientInfo), 0);

		if (NULL == hIOCP || mIOCPHandle != hIOCP)
		{
			printf("error msg : %d\n", GetLastError());
			return false;
		}

		return true;
	}

	bool BindRecv(stClientInfo* _pClientInfo)
	{
		DWORD dwFlag = 0;
		DWORD dwRecvNumBytes = 0;

		_pClientInfo->m_stRecvOverlappedEx.m_wsaBuf.len = MAX_SOCKBUF;
		_pClientInfo->m_stRecvOverlappedEx.m_wsaBuf.buf = _pClientInfo->mRecvBuf;
		_pClientInfo->m_stRecvOverlappedEx.m_eOperation = IOOperation::RECV;

		int nRet = WSARecv(_pClientInfo->m_socketClient,
			&(_pClientInfo->m_stRecvOverlappedEx.m_wsaBuf),
			1,
			&dwRecvNumBytes,
			&dwFlag,
			(LPWSAOVERLAPPED) & (_pClientInfo->m_stRecvOverlappedEx),
			NULL);

		if (nRet == SOCKET_ERROR && (WSAGetLastError() != ERROR_IO_PENDING))
		{
			printf("error msg : %d\n", WSAGetLastError());
			return false;
		}

		return true;
	}

	bool SendMsg(stClientInfo* _pClientInfo, char* _pMsg, int _nLen)
	{
		DWORD dwSendNumBytes = 0;

		CopyMemory(_pClientInfo->mSendBuf, _pMsg, _nLen);
		_pClientInfo->mSendBuf[_nLen] = '\0';

		_pClientInfo->m_stSendOverlappedEx.m_wsaBuf.len = _nLen;
		_pClientInfo->m_stSendOverlappedEx.m_wsaBuf.buf = _pClientInfo->mSendBuf;
		_pClientInfo->m_stSendOverlappedEx.m_eOperation = IOOperation::SEND;

		int nRet = WSASend(_pClientInfo->m_socketClient,
			&(_pClientInfo->m_stSendOverlappedEx.m_wsaBuf),
			1,
			&dwSendNumBytes,
			0,
			(LPWSAOVERLAPPED) & (_pClientInfo->m_stSendOverlappedEx),
			NULL);

		if (nRet == SOCKET_ERROR && (WSAGetLastError() != ERROR_IO_PENDING))
		{
			printf("error msg : %d\n", WSAGetLastError());
			return false;
		}
		return true;
	}

	void CloseSocket(stClientInfo* pClientInfo, bool bIsForce = false)
	{
		struct linger stLinger = { 0, 0 };

		if (true == bIsForce)
		{
			stLinger.l_onoff = 1;
		}

		shutdown(pClientInfo->m_socketClient, SD_BOTH);

		setsockopt(pClientInfo->m_socketClient, SOL_SOCKET, SO_LINGER, (char*)&stLinger, sizeof(stLinger));

		closesocket(pClientInfo->m_socketClient);

		pClientInfo->m_socketClient = INVALID_SOCKET;
	}
};