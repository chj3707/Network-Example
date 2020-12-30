#pragma comment(lib, "ws2_32.lib")

#include <stdio.h>
#include <string.h>
#include <winsock2.h>

#define SERVERIP "127.0.0.1"
#define SERVERPORT 9000

#define BUFSIZE 4096
#define NICKNAMESIZE 255
#define MAXUSER   10
#define NODATA   -1

#define INTRO_MSG "채팅프로그램입니다. 닉네임을 입력하세요"
#define NICKNAME_ERROR_MSG "이미있는 닉네임입니다."


enum STATE
{
	INITE_STATE,
	INTRO_STATE,
	CHATT_INITE_STATE,
	CHATTING_STATE,
	CONNECT_END_STATE
};

enum PROTOCOL
{
	INTRO,
	CHATT_NICKNAME, // 닉네임 입력할때 보낼 프로토콜
	NICKNAME_EROR, // 닉네임 에러일때 사용할 프로토콜
	NICKNAME_LIST, // 닉네임 리스트 보낼때 사용할 프로토콜
	USER_ENTER, // 유저가 입장에 성공했을때 사용할 프로토콜
	USER_STATE_CHANGE, // 접속에 성공한 유저의 상태를 변경할때 사용할 프로토콜
	CHATT_MSG, // 채팅 내용 보낼때 사용할 프로토콜
	CHATT_OUT,
};

enum CLOSECHATT
{
	LEAVE,
};

struct _ClientInfo
{
	SOCKET sock;
	SOCKADDR_IN clientaddr;
	STATE state;	
	bool chatflag;
	char  nickname[NICKNAMESIZE];
	char chatt[BUFSIZE];
	char  sendbuf[BUFSIZE];
	char  recvbuf[BUFSIZE];
	
};

DWORD CALLBACK ProcessClient(LPVOID);

void err_quit(const char *msg);
void err_display(const char *msg);
int recvn(SOCKET s, char *buf, int len, int flags);

_ClientInfo* SearchClient(const char*); //닉네임으로 유저찾기
bool NicknameCheck(const char*);// 닉네임 중복 체크
void MaKeChattMessage(const char* , const char* , char* );
void MakeEnterMessage(const char* , char* );
void MakeExitMessage(const char* , char* );


_ClientInfo* AddClient(SOCKET sock, SOCKADDR_IN clientaddr);
void RemoveClient(_ClientInfo* ptr);


void AddNickName(const char*);
void RemoveNickName(const char*);

bool PacketRecv(SOCKET, char*);

int PackPacket(char*, PROTOCOL, const char*); // INTRO, NICKNAME_EROR, CHATT_MSG
int PackPacket(char*, PROTOCOL, char**, int);//NICKNAME_LIST

PROTOCOL GetProtocol(const char*);

void UnPackPacket(const char*, char*);//CHATT_NICKNAME, CHATT_MSG

void NickNameSetting(_ClientInfo*);
void ChattingMessageProcess(_ClientInfo*);
void ChattingOutProcess(_ClientInfo*);
void ChattingEnterProcess(_ClientInfo*);

_ClientInfo* ClientInfo[MAXUSER];
char* NickNameList[MAXUSER];

int Client_Count = 0;
int Nick_Count = 0;

CRITICAL_SECTION cs;

int main(int argc, char **argv)
{
	// 윈속 초기화
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return -1;
	InitializeCriticalSection(&cs);
	// socket()
	SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_sock == INVALID_SOCKET) err_quit("socket()");

	// bind()
	SOCKADDR_IN serveraddr;
	ZeroMemory(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(SERVERPORT);
	serveraddr.sin_addr.s_addr = inet_addr(SERVERIP);
	int retval = bind(listen_sock, (SOCKADDR *)&serveraddr, sizeof(serveraddr));
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

		sock = accept(listen_sock, (SOCKADDR *)&clientaddr, &addrlen);
		if (sock == INVALID_SOCKET)
		{
			err_display("accept()");
			continue;
		}

		_ClientInfo* ptr = AddClient(sock, clientaddr);
		
		HANDLE hThread=CreateThread(NULL, 0, ProcessClient, ptr, 0, nullptr);	
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


DWORD CALLBACK ProcessClient(LPVOID  _ptr)
{
	_ClientInfo* Client_ptr = (_ClientInfo*)_ptr;
		
	int size;	
	PROTOCOL protocol;

	bool breakflag = false;

	while (1)
	{

		switch (Client_ptr->state)
		{
		case INITE_STATE:
			Client_ptr->state = INTRO_STATE;
			break;
		case INTRO_STATE:
			size = PackPacket(Client_ptr->sendbuf, INTRO, INTRO_MSG);
			if (send(Client_ptr->sock, Client_ptr->sendbuf, size, 0) == SOCKET_ERROR)
			{
				err_display("intro Send()");
				Client_ptr->state = CONNECT_END_STATE;	
				break;
			}
			Client_ptr->state = CHATT_INITE_STATE;
			break;

			/* 닉네임 설정 */
		case CHATT_INITE_STATE:
			if (!PacketRecv(Client_ptr->sock, Client_ptr->recvbuf)) // 클라로부터 닉네임 받기
			{
				Client_ptr->state = CONNECT_END_STATE;
				break;
			}

			protocol = GetProtocol(Client_ptr->recvbuf); // 프로토콜 분리

			switch (protocol)
			{
				
			case CHATT_NICKNAME:
				// 이미 있는 닉네임인지 확인
				NickNameSetting(Client_ptr);
				break;
			}

			// 닉네임 리스트에 없는 닉네임이면 리스트에 추가하고 클라 접속 상태로 변경
			if (Client_ptr->chatflag &&	Client_ptr->state != CONNECT_END_STATE)
			{
				ChattingEnterProcess(Client_ptr);
				Client_ptr->state = CHATTING_STATE;
			}
			break;

			/* 채팅 문자열 받아서 클라들에게 보내기 */
		case CHATTING_STATE:
			if (!PacketRecv(Client_ptr->sock, Client_ptr->recvbuf)) // 클라로 부터 채팅 내용 받기
			{
				Client_ptr->state = CONNECT_END_STATE;
				break;
			}

			protocol = GetProtocol(Client_ptr->recvbuf); // 프로토콜 분리

			switch (protocol)
			{
			case CHATT_MSG:
				ChattingMessageProcess(Client_ptr);
				break;
			case CHATT_OUT:
				ChattingOutProcess(Client_ptr);
				break;
			}
			break;
		case CONNECT_END_STATE:
			RemoveClient(Client_ptr);
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

bool PacketRecv(SOCKET _sock, char* _buf)
{
	int size;

	int retval = recvn(_sock, (char*)&size, sizeof(size), 0);
	if (retval == SOCKET_ERROR)
	{
		err_display("packetsize recv error()");
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

int PackPacket(char* _buf, PROTOCOL _protocol, int _num)
{
	int size = 0;
	char* ptr = _buf + sizeof(size);

	memcpy(ptr, &_protocol, sizeof(_protocol));
	ptr = ptr + sizeof(_protocol);
	size = size + sizeof(size);

	memcpy(ptr, &_num, sizeof(_num));
	ptr = ptr + sizeof(_num);
	size = size + sizeof(size);

	ptr = _buf;

	memcpy(ptr, &size, sizeof(size));
	size = size + sizeof(size);
	return size;
}


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

int PackPacket(char* _buf, PROTOCOL _protocol, char** _strlist, int _count)
{
	char* ptr = _buf;
	int strsize;
	int size = 0;

	ptr = ptr + sizeof(size);

	memcpy(ptr, &_protocol, sizeof(_protocol));
	ptr = ptr + sizeof(_protocol);
	size = size + sizeof(_protocol);

	memcpy(ptr, &_count, sizeof(_count));
	ptr = ptr + sizeof(_count);
	size = size + sizeof(_count);

	for (int i = 0; i < _count; i++)
	{
		strsize = strlen(_strlist[i]);

		memcpy(ptr, &strsize, sizeof(strsize));
		ptr = ptr + sizeof(strsize);
		size = size + sizeof(strsize);

		memcpy(ptr, _strlist[i], strsize);
		ptr = ptr + strsize;
		size = size + strsize;
	}

	ptr = _buf;

	memcpy(ptr, &size, sizeof(size));

	size = size + sizeof(size);

	return size;
}

PROTOCOL GetProtocol(const char* _ptr)
{
	PROTOCOL protocol;
	memcpy(&protocol, _ptr, sizeof(PROTOCOL));

	return protocol;
}


void UnPackPacket(const char* _buf, char* _str)
{
	int strsize;
	const char* ptr = _buf + sizeof(PROTOCOL);

	memcpy(&strsize, ptr, sizeof(strsize));
	ptr = ptr + sizeof(strsize);

	memcpy(_str, ptr, strsize);
	ptr = ptr + strsize;
}

void err_quit(const char *msg)
{
	LPVOID lpMsgbuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgbuf, 0, NULL);
	MessageBox(NULL, (LPCTSTR)lpMsgbuf, msg, MB_ICONERROR);
	LocalFree(lpMsgbuf);
	exit(-1);
}

int recvn(SOCKET s, char *buf, int len, int flags)
{
	int received;
	char *ptr = buf;
	int left = len;

	while (left > 0){
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
	LPVOID lpMsgbuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgbuf, 0, NULL);
	printf("[%s] %s", msg, (LPCTSTR)lpMsgbuf);
	LocalFree(lpMsgbuf);
}


_ClientInfo* AddClient(SOCKET sock, SOCKADDR_IN clientaddr)
{
	printf("\nClient 접속: IP 주소=%s, 포트 번호=%d\n", inet_ntoa(clientaddr.sin_addr),
		ntohs(clientaddr.sin_port));

	EnterCriticalSection(&cs);
	_ClientInfo* ptr = new _ClientInfo;
	ZeroMemory(ptr, sizeof(_ClientInfo));
	ptr->sock = sock;
	memcpy(&(ptr->clientaddr), &clientaddr, sizeof(clientaddr));
	ptr->state = INITE_STATE;	
	ptr->chatflag = false;
	ClientInfo[Client_Count++] = ptr;
	
	LeaveCriticalSection(&cs);
	return ptr;
}

void RemoveClient(_ClientInfo* ptr)
{
	closesocket(ptr->sock);

	printf("\nClient 종료: IP 주소=%s, 포트 번호=%d\n",
		inet_ntoa(ptr->clientaddr.sin_addr),
		ntohs(ptr->clientaddr.sin_port));

	EnterCriticalSection(&cs);

	
	for (int i = 0; i < Client_Count; i++)
	{
		if (ClientInfo[i] == ptr)
		{			
			delete ptr;
			int j;
			for (j = i; j < Client_Count - 1; j++)
			{
				ClientInfo[j] = ClientInfo[j + 1];
			}
			ClientInfo[j] = nullptr;
			break;
		}
	}

	Client_Count--;
	LeaveCriticalSection(&cs);
}

_ClientInfo* SearchClient(const char* _nick)
{
	EnterCriticalSection(&cs);
	for (int i = 0; i < Client_Count; i++)
	{
		if (!strcmp(ClientInfo[i]->nickname, _nick))
		{
			LeaveCriticalSection(&cs);
			return ClientInfo[i];
		}
	}

	LeaveCriticalSection(&cs);

	return nullptr;
}

/* 같은 닉네임이 있는지 체크하는 함수 */
bool NicknameCheck(const char* _nick)
{
	EnterCriticalSection(&cs);
	for (int i = 0; i < Nick_Count; i++)
	{
		if (!strcmp(NickNameList[i], _nick)) 
		{
			LeaveCriticalSection(&cs);
			return false; 
		}
	}
	LeaveCriticalSection(&cs);

	return true; 
}

/* 닉네임리스트 배열에 닉네임을 추가하는 함수 */
void AddNickName(const char* _nick)
{
	EnterCriticalSection(&cs);
	char* ptr = new char[strlen(_nick) + 1];
	strcpy(ptr, _nick);
	NickNameList[Nick_Count++] = ptr;
	LeaveCriticalSection(&cs);
}

/* 닉네임리스트 배열에서 닉네임을 삭제하는 함수 */
void RemoveNickName(const char* _nick)
{
	EnterCriticalSection(&cs);
	for (int i = 0; i < Nick_Count; i++)
	{
		if (!strcmp(NickNameList[i], _nick))
		{
			delete[] NickNameList[i];

			for (int j = i; j < Nick_Count - 1; j++)
			{
				NickNameList[j] = NickNameList[j + 1];
			}
			NickNameList[Nick_Count--] = nullptr;			
			break;
		}
	}
	LeaveCriticalSection(&cs);
}

void MaKeChattMessage(const char* _nick, const char* _msg, char* _chattmsg)
{
	sprintf(_chattmsg, "[ %s ] %s", _nick, _msg);
}

void MakeEnterMessage(const char* _nick, char* _msg)
{
	sprintf(_msg, "%s님이 입장하셨습니다.", _nick);
}
void MakeExitMessage(const char* _nick, char* _msg)
{
	sprintf(_msg, "%s님이 퇴장하셨습니다.", _nick);
}

/* 닉네임이 이미있으면 에러메시지 없으면 flag 켜주는 함수 */
void NickNameSetting(_ClientInfo* _clientinfo)
{
	EnterCriticalSection(&cs);
	
	UnPackPacket(_clientinfo->recvbuf, _clientinfo->nickname); // 닉네임 언패킹
	
	if (!NicknameCheck(_clientinfo->nickname)) // 이미 있는 닉네임인지 확인
	{
		int size = PackPacket(_clientinfo->sendbuf, NICKNAME_EROR, NICKNAME_ERROR_MSG); // 에러 메시지 패킹
		int retval = send(_clientinfo->sock, _clientinfo->sendbuf, size, 0); // 클라로 메시지 전송
		if (retval == SOCKET_ERROR)
		{
			err_display("send()");
			_clientinfo->state = CONNECT_END_STATE;
			LeaveCriticalSection(&cs);
			return;
		}

		LeaveCriticalSection(&cs);
		return;
	}

	else
	{
		_clientinfo->chatflag = true; // 없는 닉네임이면 flag on
	}

	LeaveCriticalSection(&cs);	
}

void ChattingEnterProcess(_ClientInfo* _clientinfo)
{
	EnterCriticalSection(&cs);

	char msg[BUFSIZE];
	memset(msg, 0, sizeof(msg));

	AddNickName(_clientinfo->nickname); // 닉네임 리스트에 닉네임 추가

	int size = PackPacket(_clientinfo->sendbuf, NICKNAME_LIST, NickNameList, Nick_Count); // 닉네임 리스트 패킹
	int retval = send(_clientinfo->sock, _clientinfo->sendbuf, size, 0); // 클라로 전송
	if (retval == SOCKET_ERROR)
	{
		err_display("send()");
		_clientinfo->state = CONNECT_END_STATE;
		LeaveCriticalSection(&cs);
		return;
	}

	MakeEnterMessage(_clientinfo->nickname, msg); // 입장 메시지 생성
	for (int i = 0; i < Client_Count; i++)
	{
		int size = PackPacket(_clientinfo->sendbuf, USER_ENTER, msg); // 입장 메시지 패킹
		int retval = send(ClientInfo[i]->sock, _clientinfo->sendbuf, size, 0); // 접속 되어 있는 클라들에게 전송
		if (ClientInfo[i]->sock == SOCKET_ERROR)
		{
			err_display("send()");
			ClientInfo[i]->state = CONNECT_END_STATE;
			LeaveCriticalSection(&cs);
			return;
		}
	}

	size = PackPacket(_clientinfo->sendbuf, USER_STATE_CHANGE); // 프로토콜 패킹
	retval = send(_clientinfo->sock, _clientinfo->sendbuf, size, 0); // 클라로 전송
	if (retval == SOCKET_ERROR)
	{
		err_display("send()");
		_clientinfo->state = CONNECT_END_STATE;
		LeaveCriticalSection(&cs);
		return;
	}

	LeaveCriticalSection(&cs);
}

void ChattingMessageProcess(_ClientInfo* _clientinfo)
{
	EnterCriticalSection(&cs);

	char msg[BUFSIZE];
	memset(msg, 0, sizeof(msg));

	UnPackPacket(_clientinfo->recvbuf, msg); // 클라가 보낸 메시지 언패킹
	MaKeChattMessage(_clientinfo->nickname, msg, _clientinfo->chatt); // 클라가 보낸 채팅 메시지를 다른 클라들에게도 보낼수 있도록 제작

	for (int i = 0; i < Client_Count; i++)
	{
		int size = PackPacket(_clientinfo->sendbuf, CHATT_MSG, _clientinfo->chatt); // 채팅 메시지 패킹
		    
		int retval = send(ClientInfo[i]->sock, _clientinfo->sendbuf, size, 0); // 접속 되어 있는 클라들에게 전송
		if (ClientInfo[i]->sock == SOCKET_ERROR)
		{
			err_display("send()");
			ClientInfo[i]->state = CONNECT_END_STATE;
			LeaveCriticalSection(&cs);
			return;
		}
	}



	LeaveCriticalSection(&cs);
}

void ChattingOutProcess(_ClientInfo* _clientinfo)
{
	EnterCriticalSection(&cs);

	char msg[BUFSIZE];
	memset(msg, 0, sizeof(msg));

	MakeExitMessage(_clientinfo->nickname, msg); // 클라 종료 메시지 제작

	for (int i = 0; i < Client_Count; i++)
	{
		int size = PackPacket(_clientinfo->sendbuf, CHATT_OUT, msg); // 종료 메시지 패킹

		// 남은 클라들 에게만 전송
		if (ClientInfo[i]->nickname != _clientinfo->nickname)
		{
			int retval = send(ClientInfo[i]->sock, _clientinfo->sendbuf, size, 0); // 접속 되어 있는 클라들에게 전송
			if (ClientInfo[i]->sock == SOCKET_ERROR)
			{
				err_display("send()");
				ClientInfo[i]->state = CONNECT_END_STATE;
				LeaveCriticalSection(&cs);
				return;
			}
		}
	}

	int size = PackPacket(_clientinfo->sendbuf, CHATT_OUT, LEAVE); // 나가는 클라에게 보낼 프로토콜 패킹
	int retval = send(_clientinfo->sock, _clientinfo->sendbuf, size, 0); // 나가는 클라에게 전송
	if (retval == SOCKET_ERROR)
	{
		err_display("send()");
		_clientinfo->state = CONNECT_END_STATE;
		LeaveCriticalSection(&cs);
		return;
	}

	RemoveNickName(_clientinfo->nickname); // 닉네임 삭제
	_clientinfo->state = CONNECT_END_STATE;

	LeaveCriticalSection(&cs);
}