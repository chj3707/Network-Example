#pragma comment(lib, "ws2_32")
#include <winsock2.h>
#include <stdlib.h>
#include <stdio.h>
#include "resource.h"

#define SERVERIP   "127.0.0.1"
#define SERVERPORT 9000
#define BUFSIZE 4096 // 버퍼 배열 크기
#define NICKNAMESIZE 255 // 닉네임 배열 크기
#define THREAD_COUNT 2 // 쓰레드 개수

/* 프로토콜 */
enum PROTOCOL
{
	INTRO, // (서버 -> 클라) 인트로 메시지를 보낼때 사용할 프로토콜

	WAIT, // (서버 -> 클라) 대기 메시지를 보낼때 사용할 프로토콜

	NICKNAME_LIST, // (서버 -> 클라) 닉네임 리스트 보낼때 사용할 프로토콜

	NICKNAME, // (클라 -> 서버) 닉네임 입력할때 보낼 프로토콜

	DATA_ERROR, // (서버 -> 클라) 오류가 있을때 에러 코드와 함께 보낼 프로토콜

	USER_ENTER, // (서버 -> 클라) 유저가 입장에 성공했을때 사용할 프로토콜

	GAME_START, // (서버 -> 클라) 게임 시작 메시지를 보낼떄 사용할 프로토콜

	GAME_RESULT, // (서버 -> 클라) 게임 결과 보낼때 사용할 프로토콜

	GAME_MSG, // 게임 내용 보낼때 사용할 프로토콜

	GAME_OUT // 유저가 게임에서 나갔다고 보낼때 사용할 프로토콜
};

/* 게임 결과 */
enum GAMERESULT
{
	WIN = 0,
	LOSE
};

/* 에러코드 */
enum ERRORCODE
{
	NICKNAME_EROR, // 이미 있는 닉네임
	RANGE_ERROR, // 범위 에러
};


enum STATE
{
	INITE_STATE,
	GAME_INITE_STATE, // 닉네임 설정
	GAME_WAIT_STATE, // 게임 시작까지 대기 상태
	GAME_PLAY_STATE, // 게임 진행
	GAME_CLOSE_STATE // 게임 종료
};

struct _MyInfo
{
	SOCKET sock;
	STATE state;
	char sendbuf[BUFSIZE];
	char recvbuf[BUFSIZE];
}*MyInfo;

bool PacketRecv(SOCKET, char*);

// 대화상자 프로시저
BOOL CALLBACK DlgProc(HWND, UINT, WPARAM, LPARAM);
// 편집 컨트롤 출력 함수
void DisplayText(char *fmt, ...);
// 오류 출력 함수
void err_quit(const char*);
void err_display(const char*);
// 사용자 정의 데이터 수신 함수
int recvn(SOCKET , char*, int, int);
// 소켓 통신 스레드 함수
DWORD WINAPI ClientMain(LPVOID);
DWORD CALLBACK RecvThread(LPVOID);

PROTOCOL GetProtocol(const char*);
int PackPacket(char*, PROTOCOL, const char*);
int PackPacket(char* _buf, PROTOCOL _protocol);
void UnPackPacket(const char*, char*, int&);
void UnPackPacket(const char*, char*);
void UnPackPacket(const char* _buf, int& _num, char* _str);

char buf[BUFSIZE+1]; // 데이터 송수신 버퍼
HANDLE hReadEvent, hWriteEvent; // 이벤트
HWND hSendButton; // 보내기 버튼
HWND hEdit1, hEdit2; // 편집 컨트롤
//HANDLE hClientMain, hRecvThread;
HANDLE hThread[THREAD_COUNT];

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow)
{
	// 이벤트 생성
	hReadEvent = CreateEvent(NULL, FALSE, TRUE, NULL); // 자동, 신호 상태 이벤트
	if(hReadEvent == NULL) return 1;
	hWriteEvent = CreateEvent(NULL, FALSE, FALSE, NULL); // 자동, 비신호 상태 이벤트
	if(hWriteEvent == NULL) return 1;
		
	// 소켓 통신 스레드 생성	
	// 대화상자 생성

	MyInfo = new _MyInfo; // 동적 할당
	memset(MyInfo, 0, sizeof(_MyInfo)); // 메모리 초기화

	hThread[0]= CreateThread(NULL, 0, ClientMain, NULL, 0, NULL); // 네트워크 담당 스레드

	DialogBox(hInstance, MAKEINTRESOURCE(IDD_DIALOG1), NULL, DlgProc); // 대화상자 생성

	WaitForMultipleObjects(THREAD_COUNT, hThread, TRUE, INFINITE); // 설정해둔 쓰레드 개수만큼 대기

	// 이벤트 제거
	CloseHandle(hWriteEvent);
	CloseHandle(hReadEvent);
	CloseHandle(hThread[0]);
	CloseHandle(hThread[1]);


	// closesocket()
	closesocket(MyInfo->sock);

	delete MyInfo; // 메모리 해제

	// 윈속 종료
	WSACleanup();
	return 0;
}

// 대화상자 프로시저
BOOL CALLBACK DlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg){
	case WM_INITDIALOG:
		hEdit1 = GetDlgItem(hDlg, IDC_EDIT1); // 입력 핸들
		hEdit2 = GetDlgItem(hDlg, IDC_EDIT2); // 출력 핸들
		hSendButton = GetDlgItem(hDlg, IDOK); // 보내기 버튼
		SendMessage(hEdit1, EM_SETLIMITTEXT, BUFSIZE, 0); // 입력 에디트 컨트롤 에서 입력할수 있는 최대크기 설정
		hThread[1] = CreateThread(NULL, 0, RecvThread, NULL, 0, NULL); // recv용 쓰레드 생성
		MyInfo->state = INITE_STATE;
		return TRUE;

	case WM_COMMAND:
		switch(LOWORD(wParam)){
		case IDOK:
			EnableWindow(hSendButton, FALSE); // 보내기 버튼 비활성화
			WaitForSingleObject(hReadEvent, INFINITE); // 읽기 완료 기다리기
			GetDlgItemText(hDlg, IDC_EDIT1, buf, BUFSIZE + 1); // 입력 내용 buf에 저장
			SetEvent(hWriteEvent); // 쓰기 완료 알리기
			SetWindowText(hEdit1, "");
			SetFocus(hEdit1);			
			return TRUE;

		case IDCANCEL:
			WaitForSingleObject(hReadEvent, INFINITE); // 읽기 완료 기다리기
			MyInfo->state = GAME_CLOSE_STATE;
			SetEvent(hWriteEvent); // 쓰기 완료 알리기
			EndDialog(hDlg, IDCANCEL);
			return TRUE;
		}
		return FALSE;
	}
	return FALSE;
}

// 편집 컨트롤 출력 함수
void DisplayText(char *fmt, ...)
{
	va_list arg; // 가변 인자 리스트 메모리 주소를 저장하는 포인터
	va_start(arg, fmt); // 문자열 시작 리스트에 세팅

	char cbuf[BUFSIZE+256];
	vsprintf(cbuf, fmt, arg); // 조립된 문자열 저장

	int nLength = GetWindowTextLength(hEdit2);
	SendMessage(hEdit2, EM_SETSEL, nLength, nLength);
	SendMessage(hEdit2, EM_REPLACESEL, FALSE, (LPARAM)cbuf);

	va_end(arg);
}

// 소켓 함수 오류 출력 후 종료
void err_quit(const char *msg)
{
	LPVOID lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	MessageBox(NULL, (LPCTSTR)lpMsgBuf, msg, MB_ICONERROR);
	LocalFree(lpMsgBuf);
	exit(1);
}

// 소켓 함수 오류 출력
void err_display(const char *msg)
{
	LPVOID lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	DisplayText("[%s] %s", msg, (char *)lpMsgBuf);
	LocalFree(lpMsgBuf);
}

// 사용자 정의 데이터 수신 함수
int recvn(SOCKET s, char *buf, int len, int flags)
{
	int received;
	char *ptr = buf;
	int left = len;

	while(left > 0){
		received = recv(s, ptr, left, flags);
		if(received == SOCKET_ERROR)
			return SOCKET_ERROR;
		else if(received == 0)
			break;
		left -= received;
		ptr += received;
	}

	return (len - left);
}

// TCP 클라이언트 시작 부분
DWORD WINAPI ClientMain(LPVOID arg)
{
	int retval;
	int size;

	// 윈속 초기화
	WSADATA wsa;
	if(WSAStartup(MAKEWORD(2,2), &wsa) != 0)
		return 1;

	// socket()
	MyInfo->sock = socket(AF_INET, SOCK_STREAM, 0);
	if(MyInfo->sock == INVALID_SOCKET) err_quit("socket()");

	
	// connect()
	SOCKADDR_IN serveraddr;
	ZeroMemory(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = inet_addr(SERVERIP);
	serveraddr.sin_port = htons(SERVERPORT);
	retval = connect(MyInfo->sock, (SOCKADDR *)&serveraddr, sizeof(serveraddr));
	if(retval == SOCKET_ERROR) err_quit("connect()");

	bool endflag = false;

	while(1)
	{		
		WaitForSingleObject(hWriteEvent, INFINITE); // 쓰기 완료 기다리기

		// 문자열 길이가 0이면 보내지 않음
		if(MyInfo->state!= GAME_CLOSE_STATE && strlen(buf) == 0)
		{
			EnableWindow(hSendButton, TRUE); // 보내기 버튼 활성화
			SetEvent(hReadEvent); // 읽기 완료 알리기
			continue;
		}

		switch (MyInfo->state)
		{		
			// 닉네임 설정
		case GAME_INITE_STATE:
			size = PackPacket(MyInfo->sendbuf, NICKNAME, buf);
			retval = send(MyInfo->sock, MyInfo->sendbuf, size, 0);
			if (retval == SOCKET_ERROR)
			{
				err_display("send()");
				MyInfo->state = GAME_CLOSE_STATE;
				break;
			}
			break;

			// 대기 상태
		case GAME_WAIT_STATE:
			break;

			// 게임 문자열 전송
		case GAME_PLAY_STATE:
			size = PackPacket(MyInfo->sendbuf, GAME_MSG, buf);
			retval = send(MyInfo->sock, MyInfo->sendbuf, size, 0);
			if (retval == SOCKET_ERROR)
			{
				err_display("send()");
				MyInfo->state = GAME_CLOSE_STATE;
				break;
			}
			break;

			// 접속 종료 프로토콜 전송하고 종료
		case GAME_CLOSE_STATE:
			size = PackPacket(MyInfo->sendbuf, GAME_OUT);
			retval = send(MyInfo->sock, MyInfo->sendbuf, size, 0);
			if (retval == SOCKET_ERROR)
			{
				err_display("send()");
			}
			endflag = true; // 쓰레드 종료
			break;

		}
		
		EnableWindow(hSendButton, TRUE); // 보내기 버튼 활성화
		SetEvent(hReadEvent); // 읽기 완료 알리기

		if (endflag)
		{
			break;
		}
	}

	return 0;
}

bool PacketRecv(SOCKET _sock, char* _buf)
{
	int size;

	int retval = recvn(_sock, (char*)&size, sizeof(size), 0);
	if (retval == SOCKET_ERROR)
	{
		err_display("recv error()");
		return false;
	}
	else if (retval == 0)
	{
		return false;
	}

	retval = recvn(_sock, _buf, size, 0);
	if (retval == SOCKET_ERROR)
	{
		err_display("recv error()");
		return false;

	}
	else if (retval == 0)
	{
		return false;
	}

	return true;
}

DWORD CALLBACK RecvThread(LPVOID _ptr)
{
	PROTOCOL protocol;

	char msg[BUFSIZE]; // 서버로부터 받은 메시지 언패킹하여 출력용
	int count; // 서버가 보내는 닉네임 개수

	bool endflag = false;
	while (1)
	{
		if (!PacketRecv(MyInfo->sock, MyInfo->recvbuf))
		{
			err_display("recv error()");
			return -1;
		}

		protocol = GetProtocol(MyInfo->recvbuf);

		switch (protocol)
		{
			/* 인트로 */
		case INTRO:
			memset(msg, 0, sizeof(msg));
			UnPackPacket(MyInfo->recvbuf, msg);
			DisplayText("%s\r\n", msg);
			MyInfo->state = GAME_INITE_STATE; // 인트로 메시지를 받고 상태 변경
			break;

			/* 에러 메시지 */
		case DATA_ERROR:
			int code;
			memset(msg, 0, sizeof(msg));
			UnPackPacket(MyInfo->recvbuf, code, msg);
			switch (code)
			{
			case NICKNAME_EROR: // 닉네임 중복 에러
				DisplayText("%s\r\n", msg);
				MyInfo->state = GAME_INITE_STATE; // 닉네임 다시 입력
				break;
			case RANGE_ERROR: // 범위 에러
				DisplayText("%s\r\n", msg);
				MyInfo->state = GAME_PLAY_STATE; // 숫자 다시 입력
				break;
			}
			break;

			/* 접속 리스트 출력 */
		case NICKNAME_LIST:
			memset(msg, 0, sizeof(msg));
			UnPackPacket(MyInfo->recvbuf, msg, count);
			DisplayText("%s\r\n", msg); // 닉네임 리스트 출력
			break;

			/* 접속 메시지, 게임 메시지 출력 */
		case USER_ENTER:
		case GAME_MSG:
			memset(msg, 0, sizeof(msg));
			UnPackPacket(MyInfo->recvbuf, msg);
			DisplayText("%s\r\n", msg);
			break;

			/* 대기 메시지 출력후 상태 변경 */
		case WAIT:
			memset(msg, 0, sizeof(msg));
			UnPackPacket(MyInfo->recvbuf, msg);
			DisplayText("%s\r\n", msg);
			MyInfo->state = GAME_WAIT_STATE;
			break;

			/* 게임 시작 메시지 출력, 상태 변경 */
		case GAME_START:
			memset(msg, 0, sizeof(msg));
			UnPackPacket(MyInfo->recvbuf, msg);
			DisplayText("%s\r\n", msg);
			MyInfo->state = GAME_PLAY_STATE;
			break;

			/* 게임 결과 출력, 상태 변경 */
		case GAME_RESULT:
			int result;
			memset(msg, 0, sizeof(msg));
			UnPackPacket(MyInfo->recvbuf, result, msg);
			switch (result)
			{
			case WIN:
			case LOSE:
				DisplayText("%s\r\n", msg);
				MyInfo->state = GAME_CLOSE_STATE;
				break;
			}
			endflag = true; // 쓰레드 종료
			break;
		}

		if (endflag)
		{
			break;
		}
	}

	return 0;
}

PROTOCOL GetProtocol(const char* _ptr)
{
	PROTOCOL protocol;
	memcpy(&protocol, _ptr, sizeof(PROTOCOL));

	return protocol;
}

/* GAME_OUT */
int PackPacket(char* _buf, PROTOCOL _protocol)
{
	int size = 0;
	char* ptr = _buf + sizeof(size);

	memcpy(ptr, &_protocol, sizeof(_protocol));
	ptr = ptr + sizeof(_protocol);
	size = size + sizeof(size);

	ptr = _buf;

	memcpy(ptr, &size, sizeof(size));
	size = size + sizeof(size);
	return size;
}

/* NICKNAME, GAME_MSG */
int PackPacket(char* _buf, PROTOCOL _protocol, const char* _str1)
{
	char* ptr = _buf;
	int size = 0;
	ptr = ptr + sizeof(size);

	memcpy(ptr, &_protocol, sizeof(_protocol));
	ptr = ptr + sizeof(_protocol);
	size = size + sizeof(size);

	int strsize1 = strlen(_str1);
	memcpy(ptr, &strsize1, sizeof(strsize1));
	ptr = ptr + sizeof(strsize1);
	size = size + sizeof(strsize1);

	memcpy(ptr, _str1, strsize1);
	ptr = ptr + strsize1;
	size = size + strsize1;

	ptr = _buf;

	memcpy(ptr, &size, sizeof(size));
	size = size + sizeof(size);
	return size;
}

/* GAME_RESULT, DATA_ERROR */
void UnPackPacket(const char* _buf, int& _num, char* _str)
{
	int strsize;
	const char* ptr = _buf + sizeof(PROTOCOL);

	memcpy(&_num, ptr, sizeof(_num));
	ptr = ptr + sizeof(_num);

	memcpy(&strsize, ptr, sizeof(strsize));
	ptr = ptr + sizeof(strsize);

	memcpy(_str, ptr, strsize);
	ptr = ptr + strsize;
}

/* INTRO, USER_ENTER, GAME_MSG, WAIT, GAME_START */
void UnPackPacket(const char* _buf, char* _str)
{
	int strsize;
	const char* ptr = _buf + sizeof(PROTOCOL);

	memcpy(&strsize, ptr, sizeof(strsize));
	ptr = ptr + sizeof(strsize);

	memcpy(_str, ptr, strsize);
	ptr = ptr + strsize;
}

/* NICKNAME_LIST */
void UnPackPacket(const char* _buf, char* _str, int& _count)
{	
	const char* ptr = _buf + sizeof(PROTOCOL);

	memcpy(&_count, ptr, sizeof(_count));
	ptr = ptr + sizeof(_count);

	for (int i = 0; i < _count; i++)
	{
		int strsize;
		memcpy(&strsize, ptr, sizeof(strsize));
		ptr = ptr + sizeof(strsize);

		memcpy(_str, ptr, strsize);
		ptr = ptr + strsize;
		_str = _str + strsize;
		strcat(_str, ",");
		_str++;
	}

}