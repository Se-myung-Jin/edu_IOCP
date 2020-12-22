#pragma once
#pragma comment(lib, "ws2_32")
#include <winsock2.h>
#include <Ws2tcpip.h>

#include <thread>
#include <vector>

#define MAX_SOCKBUF 1024 // 패킷 크기
#define MAX_WORKERTHREAD 4 // 스레드 수

enum class IOOperation
{
	RECV,
	SEND,
};

/*
*	@author: jsm
*	WSAOVERLAPPED구조체를 확장 시켜서 필요한 정보를 더 넣음
*/
struct stOverlappedEx
{
	WSAOVERLAPPED	m_wsaOverlapped;
	SOCKET			m_socketClient;
	WSABUF			m_wsaBuf;
	char			m_szBuf[MAX_SOCKBUF];
	IOOperation		m_eOperation;
};

/*
*	@author: jsm
*	클라이언트 정보를 담기위한 구조체
*/
struct stClientInfo
{
	SOCKET			m_socketClient;
	stOverlappedEx	m_stRecvOverlappedEx;
	stOverlappedEx	m_stSendOverlappedEx;

	stClientInfo()
	{
		ZeroMemory(&m_stRecvOverlappedEx, sizeof(stOverlappedEx));
		ZeroMemory(&m_stSendOverlappedEx, sizeof(stOverlappedEx));
		m_socketClient = INVALID_SOCKET;
	}
};

class IOCompletionPort
{
private:
	// 클라이언트 정보 저장 컨테이너
	std::vector<stClientInfo> mClientInfos;

	// 클라이언트 접속을 받기위한 리슨 소켓
	SOCKET mListenSocket = INVALID_SOCKET;

	// 접속한 클라이언트 수
	int mClientCnt = 0;

	// IO Worker 스레드
	std::vector<std::thread> mIOWorkerThreads;

	// Accept 스레드
	std::thread mAccepterThread;

	// CompletionPort 객체 핸들
	HANDLE mIOCPHandle = INVALID_HANDLE_VALUE;

	// 스레드 동작 플래그
	bool mIsWorkerRun = false;
	bool mIsAccepterRun = false;

	// 소켓 버퍼
	char mSocketBuf[MAX_SOCKBUF] = { 0, };

public:
	IOCompletionPort() {}

	~IOCompletionPort() 
	{
		WSACleanup();
	}

	// 소켓 초기화 함수
	bool InitSocket() 
	{
		WSADATA wsaData;

		int nRet = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (nRet != 0)
		{
			printf("error msg:[%d]\n", WSAGetLastError());
			return false;
		}

		mListenSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, NULL, WSA_FLAG_OVERLAPPED);

		if (mListenSocket == INVALID_SOCKET)
		{
			printf("error msg:[%d]\n", WSAGetLastError());
			return false;
		}

		printf("initialize socket completely\n");
		return true;
	}

	// 소켓 바인딩용 함수
	bool BindSocket(int _port)
	{
		SOCKADDR_IN stServerAddr;
		stServerAddr.sin_family = AF_INET;
		stServerAddr.sin_port = htons(_port);
		stServerAddr.sin_addr.s_addr = htonl(INADDR_ANY);

		int nRet = bind(mListenSocket, (SOCKADDR*)&stServerAddr, sizeof(SOCKADDR_IN));
		if (nRet != 0)
		{
			printf("error msg:[%d]\n", WSAGetLastError());
			return false;
		}

		nRet = listen(mListenSocket, 5);
		if (nRet != 0)
		{
			printf("error msg:[%d]\n", WSAGetLastError());
			return false;
		}

		printf("bind socket completely\n");
		return true;
	}

	bool StartServer(const UINT32 _maxClientCnt)
	{
		CreateClient(_maxClientCnt);

		mIOCPHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, MAX_WORKERTHREAD);
		if (mIOCPHandle == NULL)
		{
			printf("error msg:[%d]\n", GetLastError());
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

		if (mAccepterThread.joinable()) // 스레드 활성여부 판단
		{
			mAccepterThread.join(); // 스레드가 종료될 때가지 기다림
		}
	}

private:
	void CreateClient(const UINT32 _maxClientCnt)
	{
		for (UINT i = 0; i < _maxClientCnt; ++i)
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

		printf("WorkerThread start\n");
		return true;
	}

	bool CreateAccepterThread()
	{
		mAccepterThread = std::thread([this]() {AccepterThread(); });

		printf("AccepterThread start\n");
		return true;
	}

	void WorkerThread()
	{
		stClientInfo* pClientInfo = NULL;
		BOOL bSuccess = TRUE;
		DWORD dwIoSize = 0;
		LPOVERLAPPED lpOverlapped = NULL;

		while (mIsWorkerRun)
		{
			bSuccess = GetQueuedCompletionStatus(mIOCPHandle,
				&dwIoSize,										// 실제 전송된 바이트 수
				(PULONG_PTR)&pClientInfo,						// Completion Key
				&lpOverlapped,									// Overlapped IO 
				INFINITE);

			// 사용자 스레드 종료 메세지
			if (bSuccess == TRUE && dwIoSize == 0 && lpOverlapped == NULL)
			{
				mIsWorkerRun = false;
				continue;
			}

			// 클라이언트 접속 종료 메세지
			if (bSuccess == FALSE || (dwIoSize == 0 && bSuccess == TRUE))
			{
				printf("socket(%d) disconnected\n", (int)pClientInfo->m_socketClient);
				CloseSocket(pClientInfo);
				continue;
			}

			if (NULL == lpOverlapped)
			{
				continue;
			}

			stOverlappedEx* pOverlappedEx = (stOverlappedEx*)lpOverlapped;

			if (pOverlappedEx->m_eOperation == IOOperation::RECV)
			{
				pOverlappedEx->m_szBuf[dwIoSize] = NULL;
				printf("[RECV] bytes : %d, msg : %s\n", dwIoSize, pOverlappedEx->m_szBuf);

				SendMsg(pClientInfo, pOverlappedEx->m_szBuf, dwIoSize);
				BindRecv(pClientInfo);
			}
			else if (pOverlappedEx->m_eOperation == IOOperation::SEND)
			{
				printf("[SEND] bytes : %d, msg : %s\n", dwIoSize, pOverlappedEx->m_szBuf);
			}
			else
			{
				printf("[Exception] socket : %d\n", (int)pClientInfo->m_socketClient);
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

			// 초기 받기 수행
			bRet = BindRecv(pClientInfo);
			if (bRet == false)
			{
				return;
			}

			char clientIP[32] = { 0, };
			inet_ntop(AF_INET, &(stClientAddr.sin_addr), clientIP, 32 - 1);
			printf("[Accept] : IP(%s) SOCKET(%d)\n", clientIP, (int)pClientInfo->m_socketClient);

			++mClientCnt;
		}
	}

	void CloseSocket(stClientInfo* _pClientInfo, bool _bIsForce = false)
	{
		struct linger stLinger = { 0, 0 };

		if (_bIsForce == true)
		{
			stLinger.l_onoff = 1; // (1 = 설정 & 송신 버퍼의 자료가 모두 전송된 것이 확인될 때까지 지연, 0 = 해제 & 의미없음)
		}

		shutdown(_pClientInfo->m_socketClient, SD_BOTH);

		setsockopt(_pClientInfo->m_socketClient, SOL_SOCKET, SO_LINGER, (char*)&stLinger, sizeof(stLinger));

		closesocket(_pClientInfo->m_socketClient);

		_pClientInfo->m_socketClient = INVALID_SOCKET;
	}

	bool BindRecv(stClientInfo* _pClientInfo)
	{
		DWORD dwFlag = 0;
		DWORD dwRecvNumBytes = 0;

		_pClientInfo->m_stRecvOverlappedEx.m_wsaBuf.len = MAX_SOCKBUF;
		_pClientInfo->m_stRecvOverlappedEx.m_wsaBuf.buf = _pClientInfo->m_stRecvOverlappedEx.m_szBuf; // buf 포인터가 szBuf를 가리킴
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

		CopyMemory(_pClientInfo->m_stSendOverlappedEx.m_szBuf, _pMsg, _nLen);

		_pClientInfo->m_stSendOverlappedEx.m_wsaBuf.len = _nLen;
		_pClientInfo->m_stSendOverlappedEx.m_wsaBuf.buf = _pClientInfo->m_stSendOverlappedEx.m_szBuf; // buf 포인터가 szBuf를 가리킴
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
		auto hIOCP = CreateIoCompletionPort((HANDLE)_pClientInfo->m_socketClient
			, mIOCPHandle
			, (ULONG_PTR)(_pClientInfo)
			, 0);

		if (hIOCP == NULL || mIOCPHandle != hIOCP)
		{
			printf("error msg : %s\n", GetLastError());
			return false;
		}

		return true;
	}
};