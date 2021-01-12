#pragma once
#pragma comment(lib, "ws2_32")

#include "Define.h"
#include <thread>
#include <vector>

class IOCPServer
{
private:
	std::vector<stClientInfo> mClientInfos;

	SOCKET mListenSocket = INVALID_SOCKET;

	int mClientCnt = 0;

	std::vector<std::thread> mIOWorkerThreads;
	
	std::thread mAccepterThread;

	HANDLE mIOCPHandle = INVALID_HANDLE_VALUE;

	bool mIsWorkerRun = true;

	bool mIsAccepterRun = true;

	char mSocketBuf[1024] = { 0, };
public:
	IOCPServer() {}
	virtual ~IOCPServer() 
	{
		WSACleanup();
	}

	bool InitSocket()
	{
		WSAData wsaData;

		int nRet = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (nRet != 0)
		{
			printf("[error] msg : %d", WSAGetLastError());
			return false;
		}

		mListenSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, NULL, WSA_FLAG_OVERLAPPED);

		if (mListenSocket == INVALID_SOCKET)
		{
			printf("[error] msg : %d", WSAGetLastError());
			return false;
		}

		printf("Init socket completely\n");
		return true;
	}

	bool BindSocket(int _port)
	{
		SOCKADDR_IN stServerAddr;
		stServerAddr.sin_family = AF_INET;
		stServerAddr.sin_port = _port;
		stServerAddr.sin_addr.s_addr = htonl(INADDR_ANY);

		int nRet = bind(mListenSocket, (SOCKADDR*)&stServerAddr, sizeof(SOCKADDR_IN));
		if (nRet != 0)
		{
			printf("[error] msg : %d\n", WSAGetLastError());
			return false;
		}

		nRet = listen(mListenSocket, 5);
		if (nRet != 0)
		{
			printf("[error] msg : %d\n", WSAGetLastError());
			return false;
		}

		printf("bind socket completely\n");
		return true;
	}

	bool StartServer(const UINT32 _maxClientCnt)
	{
		CreateClient(_maxClientCnt);

		mIOCPHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 0);
		if (mIOCPHandle == NULL)
		{
			printf("[error] msg : %d\n", GetLastError());
			return false;
		}

		bool bRet = CreateWorkerThread();
		if (bRet == false)
		{
			return false;
		}

		bRet = CreateAccepterThread();
		if (bRet == false)
		{
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

	virtual void OnConnect(const UINT32 _clientIndex) {}

	virtual void OnClose(const UINT32 _clientIndex) {}

	virtual void OnReceive(const UINT32 _clientIndex, const UINT32 _size, char* _pData) {}

private:
	void CreateClient(const UINT32 _maxClientCnt)
	{
		for (UINT32 i = 0; i < _maxClientCnt; ++i)
		{
			mClientInfos.emplace_back();

			mClientInfos[i].mIndex = i;
		}
	}

	bool CreateWorkerThread()
	{
		unsigned int uiThreadId = 0;
		for (int i = 0; i < MAX_WORKTHREAD; ++i)
		{
			mIOWorkerThreads.emplace_back([this]() {WorkerThread(); });
		}

		printf("start worker threads\n");
		return true;
	}

	bool CreateAccepterThread()
	{
		mAccepterThread = std::thread([this]() {AccepterThread(); });

		printf("start accepter thread\n");
		return true;
	}

	stClientInfo* GetEmptyClientInfo()
	{
		for (auto& client : mClientInfos)
		{
			if (client.m_socketClient == INVALID_SOCKET)
			{
				return &client;
			}
		}

		return nullptr;
	}

	bool BindIOCompletionPort(stClientInfo* _pClientInfo)
	{
		auto hIOCP = CreateIoCompletionPort((HANDLE)_pClientInfo->m_socketClient,
			mIOCPHandle,
			(ULONG_PTR)_pClientInfo,
			0);

		if (hIOCP == NULL || mIOCPHandle != hIOCP)
		{
			printf("[error] msg : %d", GetLastError());
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
			(LPWSAOVERLAPPED)&_pClientInfo->m_stRecvOverlappedEx,
			NULL);

		if (nRet == SOCKET_ERROR && (WSAGetLastError() != ERROR_IO_PENDING))
		{
			printf("[error] msg : %d\n", WSAGetLastError());
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
			(LPWSAOVERLAPPED)&(_pClientInfo->m_stSendOverlappedEx),
			NULL);

		if (nRet == SOCKET_ERROR && (WSAGetLastError() != ERROR_IO_PENDING))
		{
			printf("[error] msg : %d\n", WSAGetLastError());
			return false;
		}

		return true;
	}

	void WorkerThread()
	{
		stClientInfo* pClientInfo = nullptr;
		bool bSuccess = true;
		DWORD dwIoSize = 0;
		LPOVERLAPPED lpOverlapped = NULL;

		while (mIsWorkerRun)
		{
			bSuccess = GetQueuedCompletionStatus(mIOCPHandle,
				&dwIoSize,
				(PULONG_PTR)&pClientInfo,
				&lpOverlapped,
				INFINITE);

			if (bSuccess == true && dwIoSize == 0 && lpOverlapped == NULL)
			{
				mIsWorkerRun = false;
				continue;
			}

			if (lpOverlapped == NULL)
			{
				continue;
			}

			if (bSuccess == false || (bSuccess == true && dwIoSize == 0))
			{
				CloseSocket(pClientInfo);
				continue;
			}

			auto pOverlappedEx = (stOverlappedEx*)lpOverlapped;

			if (pOverlappedEx->m_eOperation == IOOperation::RECV)
			{
				OnReceive(pClientInfo->mIndex, dwIoSize, pClientInfo->mRecvBuf);

				SendMsg(pClientInfo, pClientInfo->mRecvBuf, dwIoSize);

				BindRecv(pClientInfo);
			}

			else if (pOverlappedEx->m_eOperation == IOOperation::SEND)
			{
				printf("[send] bytes : %d , msg : %s\n", dwIoSize, pClientInfo->mSendBuf);
			}

			else
			{
				printf("[exception] %d", (int)pClientInfo->m_socketClient);
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
				printf("[error] client full\n");
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

			OnConnect(pClientInfo->mIndex);

			++mClientCnt;
		}
	}

	void CloseSocket(stClientInfo* _pClientInfo, bool _bIsForce = false)
	{
		auto clientIndex = _pClientInfo->mIndex;

		struct linger stLinger = { 0, 0 };

		if (_bIsForce == true)
		{
			stLinger.l_onoff = 1;
		}

		shutdown(_pClientInfo->m_socketClient, SD_BOTH);

		setsockopt(_pClientInfo->m_socketClient, SOL_SOCKET, SO_LINGER, (char*)&stLinger, sizeof(stLinger));

		closesocket(_pClientInfo->m_socketClient);

		_pClientInfo->m_socketClient = INVALID_SOCKET;

		OnClose(clientIndex);
	}
};