#pragma once
#pragma comment(lib, "ws2_32.lib")

#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <conio.h>
#include <stdio.h>

#define BUFSIZE 4096
#define IDSIZE  255
#define PWSIZE  128
#define NICKNAMESIZE 255
#define MAXUSER   10
#define NODATA   -1

#define LOGIN_ERROR_MSG "로그인 정보가 잘못되었습니다."
#define LOGIN_SUCCESS_MSG "로그인에 성공했습니다."

#define JOIN_SUCCESS_MSG "성공적으로 가입되었습니다."
#define JOIN_ERROR_MSG "이미 있는 아이디 입니다."

#define DROP_MSG "회원탈퇴하셨습니다."

#define INPUT_WAIT_BREAK 'a'

enum INITIAL_MENU
{
	INIT_MENU_JOIN = 1,
	INIT_MENU_LOGIN,
	INIT_MENU_EXIT
};

enum CHATTING_MENU
{
	CHAT_MENU_LOGOUT = 1,
	CHAT_STAND_READY,
	CHAT_MENU_SINGLE_CHATTING,
	CHAT_MENU_MULTI_CHATTING,
	CHAT_MENU_DROP,
	CHAT_MENU_REFRESH
};


enum PROTOCOL
{
	JOIN_INFO,
	LOGIN_INFO,
	JOIN_SUCCESS,
	JOIN_ERROR,
	LOGIN_SUCCESS,
	LOGIN_ERROR,
	LOGOUT,
	LOGOUT_SUCCESS,
	USER_LOGIN,
	USER_LOGOUT,
	CHATTING_STAND_READY,
	DROP,
	EXIT,

	LOGIN_LIST_INFO,
	REQ_CHAT,
	CHAT_ERROR_MSG,
	CHAT_MSG,
	CHAT_EXIT,
	MENU_RETURN

};


struct _UserInfo
{
	char id[IDSIZE];
	char pw[PWSIZE];
	char nickname[NICKNAMESIZE];
}MyInfo;

bool PacketRecv(SOCKET, char*);

void PackPacket(char*, PROTOCOL, const char*, const char*, const char*, int&);//id, pw, nickname
void PackPacket(char*, PROTOCOL, const char*, const char*, int&);//id, pw
void PackPacket(char*, PROTOCOL, char*, int&);//id
void PackPacket(char*, PROTOCOL, char**, int, int&);//id list


void GetProtocol(char*, PROTOCOL&);

void UnPackPacket(char*, char*);
void UnPackPacket(char*, char*, char*);
void UnPackPacket(char*, int, _UserInfo**);
void UnPackPacket(char*, int&);
void UnPackPacket(char*, int, char**);


INITIAL_MENU InitMenuShow();
CHATTING_MENU ChattingMenuShow();
void LoginListShow();
void ChattingRequest(int);
void ChatMessageSend();
void ScreenChatClear();

void ChatMessagePrint(char*, char*, char*);

void gotoxy(int x, int y);
COORD getXY();

void Join();
void Login();
void Logout();
void Drop();


void err_quit(const char *msg);
void err_display(const char *msg);
int recvn(SOCKET s, char *buf, int len, int flags);

DWORD CALLBACK RecvThread(LPVOID);
DWORD CALLBACK SendThread(LPVOID);

_UserInfo* LoginLst[MAXUSER];
int Login_User_Count = 0;

_UserInfo* ChatLst[MAXUSER];
int Chat_Count = 0;

SOCKET hSocket;
HANDLE hMenuEvent;

bool Chat_flag = false;
bool login_flag = false;

int main(int argc, char **argv)
{
	WSADATA wsaData;

	int result;

	SOCKADDR_IN servAddr;
	char buf[BUFSIZE];
	int size;
	int gvalue;
	int strsize;
	char str[BUFSIZE];
	PROTOCOL protocol;


	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) /* Load Winsock 2.2 DLL */
		err_quit("WSAStartup() error!");

	hSocket = socket(PF_INET, SOCK_STREAM, 0);
	if (hSocket == INVALID_SOCKET)
		err_quit("socket() error");

	memset(&servAddr, 0, sizeof(servAddr));
	servAddr.sin_family = AF_INET;
	servAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	servAddr.sin_port = htons(9000);

	if (connect(hSocket, (SOCKADDR*)&servAddr, sizeof(servAddr)) == SOCKET_ERROR)
		err_quit("connect() error!");

	hMenuEvent = CreateEvent(NULL, false, true, NULL);


	while (1)
	{
		INITIAL_MENU select = InitMenuShow();

		bool endflag = false;
		switch (select)
		{
		case INIT_MENU_JOIN:
			Join();
			break;
		case INIT_MENU_LOGIN:
			Login();

			CreateThread(nullptr, 0, RecvThread, nullptr, 0, nullptr);

			printf("로그인한 유저리스트를 받고 있습니다. 기다려주십시오...\n");

			while (1)
			{
				WaitForSingleObject(hMenuEvent, INFINITE);

				bool endflag = false;

				LoginListShow();

				CHATTING_MENU select = ChattingMenuShow();

				switch (select)
				{
				case CHAT_MENU_LOGOUT:
					Logout();
					endflag = true;

					break;
				case CHAT_STAND_READY:

					break;
				case CHAT_MENU_SINGLE_CHATTING:
					Chat_flag = true;
					ChattingRequest(1);
					ScreenChatClear();
					ChatMessageSend();

					Chat_flag = false;
					break;
				case CHAT_MENU_MULTI_CHATTING:
					Chat_flag = true;
					{
						printf("몇명과 대화하시겠습니까? ");
						int count;
						scanf("%d", &count);
						ChattingRequest(count);
					}

					ScreenChatClear();
					ChatMessageSend();

					Chat_flag = false;

					break;
				case CHAT_MENU_DROP:
					Drop();
					endflag = true;
					break;
				case CHAT_MENU_REFRESH:
					continue;
					break;
				}

				if (endflag)
				{
					break;
				}
			}
			break;

		case INIT_MENU_EXIT:
			endflag = true;

			break;
		}
		if (endflag)
		{
			break;
		}
	}

	closesocket(hSocket);
	WSACleanup();
	return 0;
}

DWORD CALLBACK RecvThread(LPVOID _ptr)
{
	PROTOCOL protocol;
	int size;
	int count;
	char buf[BUFSIZE];
	char id[IDSIZE];
	char nickname[NICKNAMESIZE];
	char msg[BUFSIZE];

	bool endflag = false;

	while (1)
	{
		if (!PacketRecv(hSocket, buf))
		{
			err_display("recv error()");
			return -1;
		}

		GetProtocol(buf, protocol);

		switch (protocol)
		{
		case USER_LOGIN:
			memset(id, 0, sizeof(id));
			memset(nickname, 0, sizeof(nickname));
			UnPackPacket(buf, id, nickname);
			LoginLst[Login_User_Count] = new _UserInfo;
			memset(LoginLst[Login_User_Count], 0, sizeof(_UserInfo));
			strcpy(LoginLst[Login_User_Count]->id, id);
			strcpy(LoginLst[Login_User_Count]->nickname, nickname);
			Login_User_Count++;

			if (!Chat_flag)
			{
				SetEvent(hMenuEvent);
			}
			break;

		case USER_LOGOUT:
			memset(id, 0, sizeof(id));
			UnPackPacket(buf, id);
			for (int i = 0; i < Login_User_Count; i++)
			{
				if (!strcmp(id, LoginLst[i]->id))
				{
					delete LoginLst[i];
					int j;
					for (j = i; j < i - 1; j++)
					{
						LoginLst[j] = LoginLst[j + 1];
					}
					LoginLst[j] = nullptr;
					break;
				}
			}
			Login_User_Count--;

			if (!Chat_flag)
			{
				SetEvent(hMenuEvent);
			}

			break;
		case LOGIN_LIST_INFO:
			memset(LoginLst, 0, sizeof(LoginLst));

			UnPackPacket(buf, count);
			Login_User_Count = count;

			for (int i = 0; i < Login_User_Count; i++)
			{
				LoginLst[i] = new _UserInfo;
				memset(LoginLst[i], 0, sizeof(_UserInfo));
			}

			UnPackPacket(buf, Login_User_Count, LoginLst);

			SetEvent(hMenuEvent);

			break;
		case REQ_CHAT:
			Chat_flag = true;

			memset(ChatLst, 0, sizeof(ChatLst));

			UnPackPacket(buf, count);
			Chat_Count = count;

			for (int i = 0; i < Chat_Count; i++)
			{
				ChatLst[i] = new _UserInfo;
				memset(ChatLst[i], 0, sizeof(_UserInfo));
			}

			UnPackPacket(buf, Chat_Count, ChatLst);

			for (int i = 0; i < Chat_Count; i++)
			{
				for (int j = 0; j < Login_User_Count; j++)
				{
					if (!strcmp(ChatLst[i]->id, LoginLst[j]->id))
					{
						strcpy(LoginLst[j]->nickname, ChatLst[i]->nickname);
						break;
					}
				}
			}


		case CHAT_MSG:
			memset(id, 0, sizeof(id));
			memset(nickname, 0, sizeof(nickname));
			memset(msg, 0, sizeof(msg));

			UnPackPacket(buf, id, msg);

			for (int i = 0; i < Chat_Count; i++)
			{
				if (!strcmp(ChatLst[i]->id, id))
				{
					strcpy(nickname, ChatLst[i]->nickname);
					break;
				}
			}

			ChatMessagePrint(id, nickname, msg);

			break;
		case CHAT_ERROR_MSG:
			break;

		case LOGOUT_SUCCESS:
			login_flag = false;
			endflag = true;
			break;

		case CHAT_EXIT:
			memset(id, 0, sizeof(id));
			memset(nickname, 0, sizeof(nickname));
			strcpy(msg, "님이 대화방을 나가셨습니다\n");

			UnPackPacket(buf, id);

			for (int i = 0; i < Chat_Count; i++)
			{
				if (!strcmp(ChatLst[i]->id, id))
				{
					strcpy(nickname, ChatLst[i]->nickname);
					for (int j = i; j < Chat_Count - 1; j++)
					{
						ChatLst[j] = ChatLst[j + 1];
					}
					Chat_Count--;
					break;
				}
			}

			ChatMessagePrint(id, nickname, msg);

			break;
		}

		if (endflag)
		{
			break;
		}

	}

	return 0;
}

void err_quit(const char *msg)
{
	LPVOID lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	MessageBox(NULL, (LPCTSTR)lpMsgBuf, msg, MB_ICONERROR);
	LocalFree(lpMsgBuf);
	exit(-1);
}

int recvn(SOCKET s, char *buf, int len, int flags)
{
	int received;
	char *ptr = buf;
	int left = len;

	while (left > 0) {
		received = recv(s, ptr, left, flags);
		if (received == SOCKET_ERROR)
			return SOCKET_ERROR;
		else if (received == 0)
			break;
		left -= received;
		ptr += received;
	}

	return (len - left);
}

// 소켓 함수 오류 출력
void err_display(const char *msg)
{
	LPVOID lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	printf("[%s] %s", msg, (LPCTSTR)lpMsgBuf);
	LocalFree(lpMsgBuf);
}

bool PacketRecv(SOCKET _sock, char* _buf)
{
	int size;

	int retval = recvn(_sock, (char*)&size, sizeof(size), 0);
	if (retval == SOCKET_ERROR)
	{
		err_display("gvalue recv error()");
		return false;
	}
	else if (retval == 0)
	{
		return false;
	}

	retval = recvn(_sock, _buf, size, 0);
	if (retval == SOCKET_ERROR)
	{
		err_display("gvalue recv error()");
		return false;

	}
	else if (retval == 0)
	{
		return false;
	}

	return true;
}

void PackPacket(char* _buf, PROTOCOL _protocol, const char* _str1, const char* _str2, const char* _str3, int& _size)//id, pw, nickname
{
	char* ptr = _buf;
	int strsize1 = strlen(_str1);
	int strsize2 = strlen(_str2);
	int strsize3 = strlen(_str3);
	ptr = ptr + sizeof(_size);
	_size = 0;


	memcpy(ptr, &_protocol, sizeof(_protocol));
	ptr = ptr + sizeof(_protocol);
	_size = _size + sizeof(_protocol);

	memcpy(ptr, &strsize1, sizeof(strsize1));
	ptr = ptr + sizeof(strsize1);
	_size = _size + sizeof(strsize1);

	memcpy(ptr, _str1, strsize1);
	ptr = ptr + strsize1;
	_size = _size + strsize1;

	memcpy(ptr, &strsize2, sizeof(strsize2));
	ptr = ptr + sizeof(strsize2);
	_size = _size + sizeof(strsize2);

	memcpy(ptr, _str2, strsize2);
	ptr = ptr + strsize2;
	_size = _size + strsize2;

	memcpy(ptr, &strsize3, sizeof(strsize3));
	ptr = ptr + sizeof(strsize3);
	_size = _size + sizeof(strsize3);

	memcpy(ptr, _str3, strsize3);
	ptr = ptr + strsize3;
	_size = _size + strsize3;

	ptr = _buf;
	memcpy(ptr, &_size, sizeof(_size));


}
void PackPacket(char* _buf, PROTOCOL _protocol, const char* _str1, const char* _str2, int& _size)//id, pw
{
	char* ptr = _buf;
	int strsize1 = strlen(_str1);
	int strsize2 = strlen(_str2);
	ptr = ptr + sizeof(_size);
	_size = 0;

	memcpy(ptr, &_protocol, sizeof(_protocol));
	ptr = ptr + sizeof(_protocol);
	_size = _size + sizeof(_protocol);

	memcpy(ptr, &strsize1, sizeof(strsize1));
	ptr = ptr + sizeof(strsize1);
	_size = _size + sizeof(strsize1);

	memcpy(ptr, _str1, strsize1);
	ptr = ptr + strsize1;
	_size = _size + strsize1;

	memcpy(ptr, &strsize2, sizeof(strsize2));
	ptr = ptr + sizeof(strsize2);
	_size = _size + sizeof(strsize2);

	memcpy(ptr, _str2, strsize2);
	ptr = ptr + strsize2;
	_size = _size + strsize2;

	ptr = _buf;
	memcpy(ptr, &_size, sizeof(_size));

}
void PackPacket(char* _buf, PROTOCOL _protocol, const char* _str1, int& _size)//id
{
	char* ptr = _buf;
	int strsize1 = strlen(_str1);

	ptr = ptr + sizeof(_size);
	_size = 0;

	memcpy(ptr, &_protocol, sizeof(_protocol));
	ptr = ptr + sizeof(_protocol);
	_size = _size + sizeof(_protocol);

	memcpy(ptr, &strsize1, sizeof(strsize1));
	ptr = ptr + sizeof(strsize1);
	_size = _size + sizeof(strsize1);

	memcpy(ptr, _str1, strsize1);
	ptr = ptr + strsize1;
	_size = _size + strsize1;

	ptr = _buf;
	memcpy(ptr, &_size, sizeof(_size));

}
void PackPacket(char* _buf, PROTOCOL _protocol, char** _strlist, int _count, int& _size)
{
	char* ptr = _buf;
	int strsize;

	ptr = ptr + sizeof(_size);
	_size = 0;

	memcpy(ptr, &_protocol, sizeof(_protocol));
	ptr = ptr + sizeof(_protocol);
	_size = _size + sizeof(_protocol);

	memcpy(ptr, &_count, sizeof(_count));
	ptr = ptr + sizeof(_count);
	_size = _size + sizeof(_count);

	for (int i = 0; i < _count; i++)
	{
		strsize = strlen(_strlist[i]);

		memcpy(ptr, &strsize, sizeof(strsize));
		ptr = ptr + sizeof(strsize);
		_size = _size + sizeof(strsize);

		memcpy(ptr, _strlist[i], strsize);
		ptr = ptr + strsize;
		_size = _size + strsize;
	}

	ptr = _buf;

	memcpy(ptr, &_size, sizeof(_size));
}

void GetProtocol(char* _ptr, PROTOCOL& _protocol)
{
	memcpy(&_protocol, _ptr, sizeof(PROTOCOL));

}

void UnPackPacket(char* _buf, char* _str1)
{
	int strsize;
	char* ptr = _buf + sizeof(PROTOCOL);

	memcpy(&strsize, ptr, sizeof(strsize));
	ptr = ptr + sizeof(strsize);

	memcpy(_str1, ptr, strsize);
	ptr = ptr + strsize;
}

void UnPackPacket(char* _buf, int& _count)
{
	char* ptr = _buf + sizeof(PROTOCOL);

	memcpy(&_count, ptr, sizeof(_count));
	ptr = ptr + sizeof(_count);
}

void UnPackPacket(char* _buf, char* _str1, char* _str2)
{
	int strsize1, strsize2;

	char* ptr = _buf + sizeof(PROTOCOL);

	memcpy(&strsize1, ptr, sizeof(strsize1));
	ptr = ptr + sizeof(strsize1);

	memcpy(_str1, ptr, strsize1);
	ptr = ptr + strsize1;

	memcpy(&strsize2, ptr, sizeof(strsize2));
	ptr = ptr + sizeof(strsize2);

	memcpy(_str2, ptr, strsize2);
	ptr = ptr + strsize2;
}

void UnPackPacket(char* _buf, int _count, _UserInfo** _userlist)
{
	int idsize, nicknamesize;
	char* ptr = _buf + sizeof(PROTOCOL) + sizeof(_count);


	for (int i = 0; i < _count; i++)
	{
		memcpy(&idsize, ptr, sizeof(idsize));
		ptr = ptr + sizeof(idsize);

		memcpy(_userlist[i]->id, ptr, idsize);
		ptr = ptr + idsize;

		memcpy(&nicknamesize, ptr, sizeof(nicknamesize));
		ptr = ptr + sizeof(nicknamesize);

		memcpy(_userlist[i]->nickname, ptr, nicknamesize);
		ptr = ptr + nicknamesize;
	}

}

void UnPackPacket(char* _buf, int _count, char** _strlist)
{
	int strsize;
	char* ptr = _buf + sizeof(PROTOCOL) + sizeof(_count);

	for (int i = 0; i < _count; i++)
	{
		memcpy(&strsize, ptr, sizeof(strsize));
		ptr = ptr + sizeof(strsize);

		memcpy(_strlist[i], ptr, strsize);
		ptr = ptr + strsize;
	}

}

INITIAL_MENU InitMenuShow()
{
	while (1)
	{
		system("cls");
		printf("<< 메뉴 >>\n");
		printf("1. 회원가입\n");
		printf("2. 로그인\n");
		printf("3. 종료\n");
		printf("선택:");

		INITIAL_MENU select;
		scanf("%d", &select);

		if (!(select == INIT_MENU_JOIN || select == INIT_MENU_LOGIN || select == INIT_MENU_EXIT))
		{
			printf("잘못선택했습니다.\n");
			continue;
		}

		return select;
	}
}


CHATTING_MENU  ChattingMenuShow()
{
	while (1)
	{
		printf("<< 채팅메뉴 >>\n");
		printf("1. 로그아웃\n");
		printf("2. 채팅 대기\n");
		printf("3. 일대일 채팅\n");
		printf("4. 일대다 채팅\n");
		printf("5. 회원탈퇴\n");
		printf("6. 로그인리스트새로고침\n");
		printf("============================================\n");
		printf("선택:");

		CHATTING_MENU select;
		scanf("%d", &select);

		if (select < CHAT_MENU_LOGOUT || select > CHAT_MENU_REFRESH)
		{
			printf("잘못선택했습니다.\n");
			continue;
		}

		return select;
	}

}

void gotoxy(int _x, int _y)
{
	COORD Pos;
	Pos.X = _x;
	Pos.Y = _y;
	SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), Pos);
}

COORD getXY()
{
	COORD Cur;
	CONSOLE_SCREEN_BUFFER_INFO a;

	GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &a);
	Cur.X = a.dwCursorPosition.X;
	Cur.Y = a.dwCursorPosition.Y;
	return Cur;
}

void Join()
{
	char id[IDSIZE];
	char pw[PWSIZE];
	char nickname[NICKNAMESIZE];
	char buf[BUFSIZE];
	int  size;
	PROTOCOL protocol;

	while (1)
	{
		system("cls");
		printf("id:");
		scanf("%s", id);
		printf("pw:");
		scanf("%s", pw);
		printf("nickname:");
		scanf("%s", nickname);

		//memset(buf, 0, sizeof(buf));
		PackPacket(buf, JOIN_INFO, id, pw, nickname, size);

		if (send(hSocket, buf, size + sizeof(size), 0) == SOCKET_ERROR)
		{
			err_quit("Join Info Send() Error");

		}

		if (!PacketRecv(hSocket, buf))
		{
			err_quit("Join result recv() Error");
		}

		GetProtocol(buf, protocol);

		char msg[BUFSIZE] = { 0 };

		UnPackPacket(buf, msg);

		printf("%s\n", msg);

		if (protocol == JOIN_SUCCESS)
		{
			break;
		}

		system("pause");
	}

}
void Login()
{
	char id[IDSIZE];
	char pw[PWSIZE];
	char nickname[NICKNAMESIZE];
	char buf[BUFSIZE];
	char msg[BUFSIZE];
	int  size;
	PROTOCOL protocol;

	while (1)
	{
		system("cls");
		printf("id:");
		scanf("%s", id);
		printf("pw:");
		scanf("%s", pw);

		PackPacket(buf, LOGIN_INFO, id, pw, size);

		if (send(hSocket, buf, size + sizeof(size), 0) == SOCKET_ERROR)
		{
			err_quit("Login Info Send() Error");

		}

		if (!PacketRecv(hSocket, buf))
		{
			err_quit("Login result recv() Error");
		}

		GetProtocol(buf, protocol);
		memset(msg, 0, sizeof(msg));
		switch (protocol)
		{
		case LOGIN_SUCCESS:
			UnPackPacket(buf, msg, nickname);
			printf("%s\n", msg);
			system("pause");
			strcmp(MyInfo.id, id);
			strcmp(MyInfo.nickname, nickname);
			login_flag = true;
			return;

		case LOGIN_ERROR:
			UnPackPacket(buf, msg);
			printf("%s\n", msg);
			system("pause");
			break;
		}
	}

}
void Logout()
{
	char buf[BUFSIZE];
	int  size;

	PROTOCOL protocol;

	PackPacket(buf, LOGOUT, MyInfo.id, size);

	if (send(hSocket, buf, size + sizeof(size), 0) == SOCKET_ERROR)
	{
		err_quit("Login Info Send() Error");
	}


}
void Drop()
{
	char buf[BUFSIZE];
	int size;
	PackPacket(buf, DROP, MyInfo.id, size);

	if (send(hSocket, buf, size + sizeof(size), 0) == SOCKET_ERROR)
	{
		err_quit("Login Info Send() Error");
	}

}


void LoginListShow()
{
	system("cls");

	printf("<로그인 리스트>\n");

	for (int i = 0; i < Login_User_Count; i++)
	{
		printf("%d. %s : %s \n", i + 1, LoginLst[i]->id, LoginLst[i]->nickname);
	}

	printf("======================================\n");
}


void ChattingRequest(int _count)
{
	char** id = nullptr;
	id = new char*[_count];

	for (int i = 0; i < _count; i++)
	{
		id[i] = new char[IDSIZE];
	}

	char buf[BUFSIZE];
	int size;

	for (int i = 0; i < _count; i++)
	{
		printf("%d 번째 대화상대의 아이디를 입력하세요:", i + 1);
		scanf("%s", id[i]);
	}


	PackPacket(buf, REQ_CHAT, id, _count, size);
	if (send(hSocket, buf, size + sizeof(size), 0) == SOCKET_ERROR)
	{
		err_quit("Chatting Request Send() Error");
	}


	for (int i = 0; i < _count; i++)
	{
		delete id[i];
	}

	delete[] id;
}

void ChatMessageSend()
{
	while (1)
	{
		gotoxy(1, 25);
		printf("=========================================================================\n");

		gotoxy(1, 26);
		printf("%s:", MyInfo.nickname);

		char msg[BUFSIZE];
		char buf[BUFSIZE];
		int size;

		gets_s(msg, sizeof(msg));

		if (!strcmp(msg, "QUIT"))
		{
			PackPacket(buf, CHAT_EXIT, MyInfo.id, size);

			if (send(hSocket, buf, size + sizeof(size), 0) == SOCKET_ERROR)
			{
				err_quit("Chatting Request Send() Error");
			}

			break;
		}

		PackPacket(buf, CHAT_MSG, MyInfo.id, msg, size);

		if (send(hSocket, buf, size + sizeof(size), 0) == SOCKET_ERROR)
		{
			err_quit("Chatting Request Send() Error");
		}

	}
}

void ChatMessagePrint(char* _id, char* _nickname, char* _msg)
{
	static int y = 3;
	bool return_point = false;

	COORD point = getXY();
	if (point.Y == 26)
	{
		return_point = true;
	}

	if (y == 3 || y == 25)
	{
		ScreenChatClear();
		y = 3;
	}

	y++;

	gotoxy(1, y);

	printf("[%s(%s)]:%s\n", _nickname, _id, _msg);

	if (return_point)
	{
		gotoxy(strlen(MyInfo.nickname) + 1, 26);
	}
}

void ScreenChatClear()
{
	system("cls");
	printf("<<채팅 메시지 창>>\n");
	printf("=========================================================================\n");
}
