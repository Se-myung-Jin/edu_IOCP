#include "IOCompletionPort.h"

const UINT16 SERVER_PORT = 9000;
const UINT16 MAX_CLIENT = 100;

int main()
{
	IOCompletionPort ioCompletionPort;

	ioCompletionPort.InitSocket();

	ioCompletionPort.BindSocket(SERVER_PORT);

	ioCompletionPort.StartServer(MAX_CLIENT);

	printf("Enter Any Key\n");
	getchar();

	ioCompletionPort.DestroyThread();
	return 0;
}