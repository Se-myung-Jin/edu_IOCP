#include "IOCompletionPort.h"

const UINT32 MAX_CLIENTCNT = 100;
const UINT32 SERVER_PORT = 9000;

int main()
{
	IOCompletionPort ioCompletionPort;

	ioCompletionPort.InitSocket();

	ioCompletionPort.BindSocket(SERVER_PORT);

	ioCompletionPort.StartServer(MAX_CLIENTCNT);

	printf("Enter Any Key\n");
	getchar();

	ioCompletionPort.DestroyThread();
	return 0;
}