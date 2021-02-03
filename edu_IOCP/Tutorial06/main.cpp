﻿#include "EchoServer.h"
#include <iostream>
#include <string>

const UINT16 SERVER_PORT = 9000;
const UINT16 MAX_CLIENT = 100;

int main()
{
	EchoServer server;

	//소켓을 초기화
	server.InitSocket();

	//소켓과 서버 주소를 연결하고 등록 시킨다.
	server.BindSocket(SERVER_PORT);

	server.Run(MAX_CLIENT);

	printf("아무 키나 누를 때까지 대기합니다\n");
	while (true)
	{
		std::string inputCmd;
		std::getline(std::cin, inputCmd);

		if (inputCmd == "quit")
		{
			break;
		}
	}

	server.End();
	return 0;
}