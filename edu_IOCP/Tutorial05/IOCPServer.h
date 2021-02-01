#pragma once
#pragma comment(lib, "ws2_32")

#include "ClientInfo.h"
#include "Define.h"
#include <vector>
#include <thread>


class IOCPServer
{
private:
	std::vector<ClientInfo*> mClientInfos;

	SOCKET mListenSocket = INVALID_SOCKET;

	int mClientCnt = 0;

	bool mIsWorkerRun = false;
	bool mIsAccepterRun = false;

	std::vector<std::thread> mIOWorkerThreads;
	std::thread mAccepterThread;

	bool mIsSenderRun = false;
	std::thread mSendThread;

	HANDLE mIOCPHandle = INVALID_HANDLE_VALUE;

public:
	IOCPServer() {}

	virtual ~IOCPServer()
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

	bool BindandListen(int _port)
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

		printf("bind socket completely\n");
		return true;
	}

	bool StartServer(const UINT32 _maxClientCnt)
	{
		CreateClient(_maxClientCnt);

		mIOCPHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE,
			NULL, NULL, MAX_WORKTHREAD);
		if (mIOCPHandle == NULL)
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

		CreateSendThread();

		printf("server start...\n");
		return true;
	}

	void DestroyThread()
	{
		mIsSenderRun = false;

		if (mSendThread.joinable())
		{
			mSendThread.join();
		}

		mIsAccepterRun = false;
		closesocket(mListenSocket);
		if (mAccepterThread.joinable())
		{
			mAccepterThread.join();
		}

		mIsWorkerRun = false;
		CloseHandle(mIOCPHandle);

		for (auto &thr : mIOWorkerThreads)
		{
			if (thr.joinable())
			{
				thr.join();
			}
		}
	}

	bool SendMsg(const UINT32 _sessionIndex, const UINT32 _dataSize, char* _pData)
	{
		auto pClient = GetClientInfo(_sessionIndex);
		return pClient->SendMsg(_dataSize, _pData);
	}

	virtual void OnConnect(const UINT32 _clientIndex) {}

	virtual void OnClose(const UINT32 _clientIndex) {}

	virtual void OnReceive(const UINT32 _clientIndex, const UINT32 _size, char* _pData) {}

private:
	void CreateClient(const UINT32 _maxClientCnt)
	{
		for (UINT32 i = 0; i < _maxClientCnt; ++i)
		{
			auto client = new ClientInfo();
			client->Init(i);

			mClientInfos.push_back(client);
		}
	}

	bool CreateWorkerThread()
	{
		mIsWorkerRun = true;

		for (int i = 0; i < MAX_WORKTHREAD; ++i)
		{
			mIOWorkerThreads.emplace_back([this]() {WorkerThread(); });
		}

		return true;
	}

	bool CreateAccepterThread()
	{
		mIsAccepterRun = true;

		mAccepterThread = std::thread([this]() {AccepterThread(); });

		return true;
	}

	void CreateSendThread()
	{
		mIsSenderRun = true;
		mSendThread = std::thread([this]() {SendThread(); });
	}

	ClientInfo* GetClientInfo(const UINT32 _sessionIndex)
	{
		return mClientInfos[_sessionIndex];
	}

	ClientInfo* GetEmptyClientInfo()
	{
		for (auto client : mClientInfos)
		{
			if (client->IsConnected() == false)
			{
				return client;
			}
		}
		
		return nullptr;
	}

	void CloseSocket(ClientInfo* _pClientInfo, bool _bIsForce = false)
	{
		auto clientIndex = _pClientInfo->GetIndex();

		_pClientInfo->Close(_bIsForce);

		OnClose(clientIndex);
	}

	void WorkerThread()
	{
		ClientInfo* pClientInfo = nullptr;
		bool bSuccess = true;
		DWORD dwIoSize = 0;
		LPOVERLAPPED lpOverlapped = NULL;

		while (mIsWorkerRun)
		{
			bSuccess = GetQueuedCompletionStatus(mIOCPHandle,
				(LPDWORD)&dwIoSize,
				(PULONG_PTR)&pClientInfo,
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

			if (bSuccess == FALSE || (bSuccess == TRUE && dwIoSize == 0))
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
				pClientInfo->SendComplete(dwIoSize);
			}
			else
			{
				printf("exception client : %d\n", pClientInfo->GetIndex());
			}
		}
	}

	void AccepterThread()
	{
		SOCKADDR_IN stClientAddr;
		int nAddrLen = sizeof(SOCKADDR_IN);

		while (mIsAccepterRun)
		{
			ClientInfo* pClientInfo = GetEmptyClientInfo();
			if (pClientInfo == NULL)
			{
				printf("client full\n");
				return;
			}

			auto newSocket = accept(mListenSocket, (SOCKADDR*)&stClientAddr, &nAddrLen);
			if (newSocket == INVALID_SOCKET)
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

	void SendThread()
	{
		while (mIsSenderRun)
		{
			for (auto client : mClientInfos)
			{
				if (client->IsConnected() == false)
				{
					continue;
				}

				client->SendIO();
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
	}
};