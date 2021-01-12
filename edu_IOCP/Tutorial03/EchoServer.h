#pragma once

#include "IOCPServer.h"

class EchoServer : public IOCPServer
{
	virtual void OnConnect(const UINT32 _clientIndex) override
	{
		printf("[OnConnect] : Index(%d)\n", _clientIndex);
	}

	virtual void OnClose(const UINT32 _clientIndex) override
	{
		printf("[OnClose] : Index(%d)\n", _clientIndex);
	}

	virtual void OnReceive(const UINT32 _clientIndex, const UINT32 _size, char* _pData) override
	{
		printf("[OnReceive] : Index(%d), dataSize(%d)\n", _clientIndex, _size);
	}
};