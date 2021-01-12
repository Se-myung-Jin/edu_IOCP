#pragma once
#pragma comment(lib, "ws2_32")

#include "ClientInfo.h"

#include <vector>
#include <thread>

class IOCPServer
{
private:
	std::vector<ClientInfo> mClientInfos;

	SOCKET mListenSocket = INVALID_SOCKET;

	std::vector<std::thread> mIOWorkerThreads;

	std::thread mAccepterThread;

	bool mIsWorkerRun = true;

	bool mIsAccepterRun = true;

	int mClientCnt = 0;

	HANDLE mIOCPHandle = INVALID_HANDLE_VALUE;

public:
	IOCPServer() {}

	~IOCPServer()
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

		printf("init socket completely\n");
		return true;
	}

	bool BindSocket(int _port)
	{
		SOCKADDR_IN stServerAddr;

		stServerAddr.sin_family = AF_INET;
		stServerAddr.sin_port = htons(_port);
		stServerAddr.sin_addr.s_addr = htonl(INADDR_ANY);

		int nRet = bind(mListenSocket, (sockaddr*)&stServerAddr, sizeof(SOCKADDR_IN));
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

		printf("bind socket completely\n");
		return true;
	}

	bool StartServer(const UINT32 _maxClientCnt)
	{
		CreateClient(_maxClientCnt);

		mIOCPHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 0);

		if (mIOCPHandle == INVALID_HANDLE_VALUE)
		{
			printf("error msg : %d\n", GetLastError());
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

		printf("start server completely\n");
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

	bool SendMsg(const UINT32 _sessionIndex, const UINT32 _dataSize, char* _pData)
	{
		auto pClient = GetClientInfo(_sessionIndex);
		return pClient->SendMsg(_dataSize, _pData);
	}

	virtual void OnConnect(const UINT32 clientIndex_) {}

	virtual void OnClose(const UINT32 clientIndex_) {}

	virtual void OnReceive(const UINT32 clientIndex_, const UINT32 size_, char* pData_) {}

private:
	void CreateClient(const UINT32 _maxClientCnt)
	{
		for (UINT32 i = 0; i < _maxClientCnt; ++i)
		{
			mClientInfos.emplace_back();

			mClientInfos[i].Init(i);
		}
	}

	ClientInfo* GetEmptyClientInfo()
	{
		for (auto& client : mClientInfos)
		{
			if (client.IsConnected() == false)
			{
				return &client;
			}
		}

		return nullptr;
	}

	ClientInfo* GetClientInfo(const UINT32 _sessionIndex)
	{
		return &mClientInfos[_sessionIndex];
	}

	bool CreateWorkerThread()
	{
		unsigned int uiThreadId = 0;

		for (int i = 0; i < MAX_WORKTHREAD; ++i)
		{
			mIOWorkerThreads.emplace_back([this]() {WorkerThread(); });
		}

		printf("worker thread start\n");
		return true;
	}

	bool CreateAccepterThread()
	{
		mAccepterThread = std::thread([this]() {AccepterThread(); });

		printf("accepter thread start\n");
		return true;
	}

	void WorkerThread()
	{
		DWORD dwIoSize = 0;
		bool bSuccess;
		ClientInfo* pClientInfo = nullptr;
		LPOVERLAPPED lpOverlapped = NULL;

		while (mIsWorkerRun)
		{
			bSuccess = GetQueuedCompletionStatus(mIOCPHandle,
				&dwIoSize,
				(PULONG_PTR)pClientInfo,
				&lpOverlapped,
				INFINITE);

			if (TRUE == bSuccess && 0 == dwIoSize && NULL == lpOverlapped)
			{
				mIsWorkerRun = false;
				continue;
			}

			if (NULL == lpOverlapped)
			{
				continue;
			}

			if (FALSE == bSuccess || (0 == dwIoSize && TRUE == bSuccess))
			{
				CloseSocket(pClientInfo);
				continue;
			}

			auto pOverlappedEx = (stOverlappedEx*)lpOverlapped;

			if (pOverlappedEx->m_eOperation == IOOperation::RECV)
			{
				OnReceive(pClientInfo->GetIndex(), dwIoSize, pClientInfo->RecvBuffer());

				pClientInfo->BindRecv();
			}
			else if (pOverlappedEx->m_eOperation == IOOperation::SEND)
			{
				delete[] pOverlappedEx->m_wsaBuf.buf;
				delete pOverlappedEx;
				pClientInfo->SendCompleted(dwIoSize);
			}
			else
			{
				printf("Client Index(%d)에서 예외상황\n", pClientInfo->GetIndex());
			}
		}
	}

	void AccepterThread()
	{
		SOCKADDR_IN		stClientAddr;
		int nAddrLen = sizeof(SOCKADDR_IN);

		while (mIsAccepterRun)
		{
			ClientInfo* pClientInfo = GetEmptyClientInfo();
			if (NULL == pClientInfo)
			{
				printf("[에러] Client Full\n");
				return;
			}

			auto newSocket = accept(mListenSocket, (SOCKADDR*)&stClientAddr, &nAddrLen);
			if (INVALID_SOCKET == newSocket)
			{
				continue;
			}

			if (pClientInfo->OnConnect(mIOCPHandle, newSocket) == false)
			{
				pClientInfo->Close(true);
				return;
			}

			OnConnect(pClientInfo->GetIndex());

			++mClientCnt;
		}
	}

	void CloseSocket(ClientInfo* _pClientInfo, bool _bIsForce = false)
	{
		auto clientIndex = _pClientInfo->GetIndex();

		_pClientInfo->Close(_bIsForce);

		OnClose(clientIndex);
	}
};