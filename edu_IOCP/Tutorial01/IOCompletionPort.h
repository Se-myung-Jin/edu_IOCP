#pragma once
#pragma comment(lib, "ws2_32")
#include <winsock2.h>
#include <Ws2tcpip.h>

#include <thread>
#include <vector>

#define MAX_SOCKBUF 1024 // ��Ŷ ũ��
#define MAX_WORKERTHREAD 4 // ������ ��

enum class IOOperation
{
	RECV,
	SEND,
};

/*
*	@author: jsm
*	WSAOVERLAPPED����ü�� Ȯ�� ���Ѽ� �ʿ��� ������ �� ����
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
*	Ŭ���̾�Ʈ ������ ������� ����ü
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
	// Ŭ���̾�Ʈ ���� ���� �����̳�
	std::vector<stClientInfo> mClientInfos;

	// Ŭ���̾�Ʈ ������ �ޱ����� ���� ����
	SOCKET mListenSocket = INVALID_SOCKET;

	// ������ Ŭ���̾�Ʈ ��
	int mClientCnt = 0;

	// IO Worker ������
	std::vector<std::thread> mIOWorkerThreads;

	// Accept ������
	std::thread mAccepterThread;

	// CompletionPort ��ü �ڵ�
	HANDLE mIOCPHandle = INVALID_HANDLE_VALUE;

	// ������ ���� �÷���
	bool mIsWorkerRun = false;
	bool mIsAccepterRun = false;

	// ���� ����
	char mSocketBuf[MAX_SOCKBUF] = { 0, };

public:
	IOCompletionPort() {}

	~IOCompletionPort() 
	{
		WSACleanup();
	}

	// ���� �ʱ�ȭ �Լ�
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

	// ���� ���ε��� �Լ�
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