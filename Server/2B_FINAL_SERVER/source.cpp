#define MAIN
#include "global.h"

// 메인 함수
int main(int argc, char** argv)
{
	InitializeCriticalSection(&cs);

	// 윈속 초기화
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

	// 데이터 통신에 사용할 변수		
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

		_ClientInfo* ptr = AddClient(sock, clientaddr); // 접속 클라이언트 추가

		HANDLE hThread = CreateThread(NULL, 0, ProcessClient, ptr, 0, nullptr); // 쓰레드 생성, 들어온 클라이언트 함수로 보냄
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

// 쓰레드 함수
DWORD CALLBACK ProcessClient(LPVOID  _ptr)
{
	_ClientInfo* Client_ptr = (_ClientInfo*)_ptr; // 접속한 클라이언트 할당

	int size;
	PROTOCOL protocol;

	bool breakflag = false;

	while (1)
	{
		// 클라이언트 상태에 따른 프로세스
		switch (Client_ptr->state)
		{
		case INITE_STATE:
			Client_ptr->state = INTRO_STATE;
			break;

			/* 인트로 메시지 전송 */
		case INTRO_STATE:
			IntroProcess(Client_ptr);
			break;

			/* 클라로 부터 닉네임 받고 대기 메시지 전송 */
		case WAIT_STATE:
			WaitProcess(Client_ptr);
			break;

			/* 게임 진행 관리 */
		case GAME_PLAY_STATE:
			GamePlayProcess(Client_ptr);
			break;

			/* 게임 승패 처리, 결과 전송 */
		case GAME_RESULT_STATE:
			GameResultProcess(Client_ptr);
			break;

			/* 연결 종료 */
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