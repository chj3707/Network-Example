#define MAIN
#include "global.h"

// ���� �Լ�
int main(int argc, char** argv)
{
	InitializeCriticalSection(&cs);

	// ���� �ʱ�ȭ
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return -1;

	// socket()
	SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_sock == INVALID_SOCKET) err_quit("socket()");

	// bind()
	SOCKADDR_IN serveraddr;
	ZeroMemory(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(SERVERPORT);
	serveraddr.sin_addr.s_addr = inet_addr(SERVERIP);
	int retval = bind(listen_sock, (SOCKADDR*)&serveraddr, sizeof(serveraddr));
	if (retval == SOCKET_ERROR) err_quit("bind()");

	// listen()
	retval = listen(listen_sock, SOMAXCONN);
	if (retval == SOCKET_ERROR) err_quit("listen()");

	// ������ ��ſ� ����� ����		
	int addrlen;
	SOCKET sock;
	SOCKADDR_IN clientaddr;

	while (1)
	{
		addrlen = sizeof(clientaddr);

		sock = accept(listen_sock, (SOCKADDR*)&clientaddr, &addrlen);
		if (sock == INVALID_SOCKET)
		{
			err_display("accept()");
			continue;
		}

		_ClientInfo* ptr = AddClient(sock, clientaddr); // ���� Ŭ���̾�Ʈ �߰�

		HANDLE hThread = CreateThread(NULL, 0, ProcessClient, ptr, 0, nullptr); // ������ ����, ���� Ŭ���̾�Ʈ �Լ��� ����
		if (hThread != NULL)
		{
			CloseHandle(hThread);
		}

	}

	closesocket(listen_sock);
	DeleteCriticalSection(&cs);
	WSACleanup();
	return 0;
}

// ������ �Լ�
DWORD CALLBACK ProcessClient(LPVOID  _ptr)
{
	_ClientInfo* Client_ptr = (_ClientInfo*)_ptr; // ������ Ŭ���̾�Ʈ �Ҵ�

	int size;
	PROTOCOL protocol;

	bool breakflag = false;

	while (1)
	{
		// Ŭ���̾�Ʈ ���¿� ���� ���μ���
		switch (Client_ptr->state)
		{
		case INITE_STATE:
			Client_ptr->state = INTRO_STATE;
			break;

			/* ��Ʈ�� �޽��� ���� */
		case INTRO_STATE:
			IntroProcess(Client_ptr);
			break;

			/* Ŭ��� ���� �г��� �ް� ��� �޽��� ���� */
		case WAIT_STATE:
			WaitProcess(Client_ptr);
			break;

			/* ���� ���� ���� */
		case GAME_PLAY_STATE:
			GamePlayProcess(Client_ptr);
			break;

			/* ���� ���� ó��, ��� ���� */
		case GAME_RESULT_STATE:
			GameResultProcess(Client_ptr);
			break;

			/* ���� ���� */
		case DISCONNECT_STATE:
			DisConnectProcess(Client_ptr);
			breakflag = true;
			break;
		}

		if (breakflag)
		{
			break;
		}

	}

	return 0;
}