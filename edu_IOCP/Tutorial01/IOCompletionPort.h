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

	}

	void AccepterThread()
	{

	}
};