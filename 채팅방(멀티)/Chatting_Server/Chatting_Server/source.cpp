#pragma comment(lib, "ws2_32.lib")

#include <stdio.h>
#include <string.h>
#include <winsock2.h>
#include <ctype.h> // isdigit 함수 사용 용도

#define SERVERIP "127.0.0.1"
#define SERVERPORT 9000

#define BUFSIZE 4096
#define NICKNAMESIZE 255

#define CLIENT_COUNT 100 // 최대 클라이언트 개수
#define MAXUSER 2 // 채팅방에 들어갈 수 있는 최대 인원
#define MAXROOM 3 // 채팅방 개수

#define NODATA -1
#define INIT 0

#define INTRO_MSG "채팅프로그램입니다. 닉네임을 입력하세요"
#define NICKNAME_ERROR_MSG "이미있는 닉네임입니다. 다른 닉네임을 입력해 주세요."
#define CHATTROOM_INTRO_MSG "1 ~ 3번방 까지 준비 되어 있습니다. 원하시는 방 번호를 입력하세요."
#define CHATTROOM_CODE_ERROR_MSG "숫자 이외의 문자를 입력 하셨습니다. 다시 입력하세요."
#define CHATTROOM_RANGE_ERROR_MSG "방 번호의 범위를 벗어났습니다. 다시 입력하세요."
#define CHATTROOM_FULL_MSG "해당 채팅방은 가득 찼습니다. 다른 채팅방으로 다시 입력해 주세요."

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
	NICKNAME_LIST, // 닉네임 리스트 보낼때 사용할 프로토콜
	CHATT_NICKNAME, // 닉네임 입력할때 보낼 프로토콜
	JOIN_CHATTROOM, // 클라가 접속할 채팅방을 보낼때 사용할 프로토콜
	CONNECT_ERROR, // 접속 오류 일때 에러 코드와 함께 보낼 프로토콜
	INIT_SETTING, // 닉네임, 접속할 채팅방 보낼때 사용할 프로토콜
	USER_ENTER, // 유저가 입장에 성공했을때 사용할 프로토콜
	USER_STATE_CHANGE, // 접속에 성공한 유저의 상태를 변경할때 사용할 프로토콜
	CHATT_MSG, // 채팅 내용 보낼때 사용할 프로토콜
	CHATT_OUT
};

enum ERRORCODE
{
	NICKNAME_EROR, // 이미 있는 닉네임
	CHATTROOM_FULL,  // 가득 찬 채팅방
	CHATTROOM_CODE_EROR, // 방 번호 입력 오류
	CHATTROOM_RANGE_EROR // 방 번호 범위 오류
};

enum CLOSECHATT
{
	LEAVE // 채팅방에서 클라가 나갈때 보내줌
};
struct _ChattRoomInfo;

/* 클라 정보 */
struct _ClientInfo
{
	SOCKET sock;
	SOCKADDR_IN clientaddr;
	STATE state; // 클라 상태
	
	_ChattRoomInfo* room_info; // 클라가 속해진 채팅방 정보
	int room_num; // 클라가 속한 방 번호
	int chattuser_number; // 자기가 속한 방의 몇번 유저인지 확인할때 사용 
	bool chatflag; // 채팅방에 들어 갔는지 체크
	char  nickname[NICKNAMESIZE]; // 클라 닉네임
	char chatt[BUFSIZE]; // 클라가 보낸 채팅 내용
	
	char  sendbuf[BUFSIZE];
	char  recvbuf[BUFSIZE];

};

/* 채팅방 정보 */
struct _ChattRoomInfo
{
	_ClientInfo* User[MAXUSER]; // 채팅방에 들어온 클라
	int chattroom_number; // 방 번호
	int user_count; // 채팅방 인원이 가득 찼는지 확인할 변수
	bool full; // 채팅방 인원이 가득 차면 못들어 오도록 할 변수
	char* NickNameList[MAXUSER]; // 채팅방에 속한 닉네임 리스트
	int Nick_Count = 0; // 닉네임 카운트
};

_ChattRoomInfo* ChattRoomInfo[MAXROOM];
int Room_Count = 0;

_ClientInfo* ClientInfo[CLIENT_COUNT];
int Client_Count = 0;

CRITICAL_SECTION cs;


DWORD CALLBACK ProcessClient(LPVOID);

void err_quit(const char *msg);
void err_display(const char *msg);
int recvn(SOCKET s, char *buf, int len, int flags);

_ClientInfo* SearchClient(const char*); //닉네임으로 유저찾기
void MaKeChattMessage(const char* , const char* , char* );
void MakeEnterMessage(const char* , char* , int);
void MakeExitMessage(const char* , char* , int);


_ClientInfo* AddClient(SOCKET sock, SOCKADDR_IN clientaddr);
void RemoveClient(_ClientInfo* ptr);
_ChattRoomInfo* AddChattRoom(_ClientInfo* _ptr);
void RemoveChattRoom(_ClientInfo* _ptr);
void RemoveChattUser(_ClientInfo* _ptr);

void AddNickName(_ChattRoomInfo* _ptr, const char* _nick);
bool NicknameCheck(_ChattRoomInfo* _ptr, const char* _nick);
void RemoveNickName(_ClientInfo* _ptr);

bool PacketRecv(SOCKET, char*);

int PackPacket(char*, PROTOCOL, const char*); // INTRO, NICKNAME_EROR, CHATT_MSG
int PackPacket(char*, PROTOCOL, char**, int);//NICKNAME_LIST

PROTOCOL GetProtocol(const char*);

void UnPackPacket(const char*, char*);//CHATT_NICKNAME, CHATT_MSG

void NickNameSetting(_ClientInfo*);
void ChattingMessageProcess(_ClientInfo*);
void ChattingOutProcess(_ClientInfo*);
void ChattingEnterProcess(_ClientInfo*);
void ChattRoomSetting(_ClientInfo* _ptr);

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
			if (!PacketRecv(Client_ptr->sock, Client_ptr->recvbuf)) // 클라로부터 닉네임,방 번호 받기
			{
				Client_ptr->state = CONNECT_END_STATE;
				break;
			}

			protocol = GetProtocol(Client_ptr->recvbuf); // 프로토콜 분리

			switch (protocol)
			{
				
			case CHATT_NICKNAME:
				// 이미 있는 닉네임인지 확인하고 유저 추가
				NickNameSetting(Client_ptr);
				break;
				// 유저가 들어갈 채팅방 설정
			case JOIN_CHATTROOM:
				ChattRoomSetting(Client_ptr);
				break;
				// 유저가 종료 선택
			case CHATT_OUT:
				ChattingOutProcess(Client_ptr);
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

int PackPacket(char* _buf, PROTOCOL _protocol, int _num, const char* _str1)
{
	int size = 0;
	char* ptr = _buf + sizeof(size);

	memcpy(ptr, &_protocol, sizeof(_protocol));
	ptr = ptr + sizeof(_protocol);
	size = size + sizeof(size);

	memcpy(ptr, &_num, sizeof(_num));
	ptr = ptr + sizeof(_num);
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

_ChattRoomInfo* AddChattRoom(_ClientInfo* _ptr)
{
	EnterCriticalSection(&cs);
	_ChattRoomInfo* chatt_ptr = nullptr;
	int index = NODATA;

	// null이면 해당방에 처음 들어오는 클라 이므로 밑에 코드에서 새로 생성
	if (ChattRoomInfo[_ptr->room_num - 1] != nullptr)
	{
		// 채팅방 인원이 가득차면
		if (ChattRoomInfo[_ptr->room_num - 1]->user_count == MAXUSER)
		{
			ChattRoomInfo[_ptr->room_num - 1]->full = true;
			LeaveCriticalSection(&cs);
			return ChattRoomInfo[_ptr->room_num - 1];
		}
		// 방 번호가 같고 그 방이 가득차지 않았을 경우에 추가
		if (ChattRoomInfo[_ptr->room_num - 1]->chattroom_number == _ptr->room_num && !ChattRoomInfo[_ptr->room_num - 1]->user_count != MAXUSER)
		{
			ChattRoomInfo[_ptr->room_num - 1]->User[ChattRoomInfo[_ptr->room_num - 1]->user_count++] = _ptr; // 들어오는 클라이언트 순서대로 저장
			_ptr->room_info = ChattRoomInfo[_ptr->room_num - 1]; // 속한 채팅방 정보 저장
			_ptr->chattuser_number = ChattRoomInfo[_ptr->room_num - 1]->user_count; // 자신이 속한 채팅방에서의 번호 저장
			ChattRoomInfo[_ptr->room_num - 1]->full = false;

			chatt_ptr = ChattRoomInfo[_ptr->room_num - 1]; // 갱신된 정보 저장
			index = _ptr->room_num - 1;
			LeaveCriticalSection(&cs);
			return chatt_ptr;
		}
	}
	

	/* 제일 처음 들어온 클라이언트 */
	if (index == NODATA || ChattRoomInfo[_ptr->room_num - 1] == nullptr)
	{
		chatt_ptr = new _ChattRoomInfo;
		memset(chatt_ptr, 0, sizeof(_ChattRoomInfo)); // 초기화

		chatt_ptr->full = false; // 최대 10명으로 설정해둠
		chatt_ptr->chattroom_number = _ptr->room_num; // 방 번호 저장
		chatt_ptr->User[0] = _ptr; // 제일 먼저 들어온 클라 저장
		chatt_ptr->user_count++; // 채팅 참여 클라 +1
		ChattRoomInfo[_ptr->room_num - 1] = chatt_ptr; // 클라가 선택한 방번호 
		Room_Count++;

		_ptr->room_info = chatt_ptr; // 채팅방 정보 저장
		_ptr->chattuser_number = chatt_ptr->user_count; // 자신이 속한 채팅방에서의 번호 저장
	}

	LeaveCriticalSection(&cs);
	return chatt_ptr;
}

void RemoveChattRoom(_ChattRoomInfo* _ptr)
{
	EnterCriticalSection(&cs);
	for (int i = 0; i < Room_Count; i++)
	{
		if (ChattRoomInfo[i] == _ptr) // 해당 채팅방을 찾아서 
		{
			delete _ptr; // 메모리 해제
		}
	}
	Room_Count--;
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
bool NicknameCheck(_ChattRoomInfo* _ptr, const char* _nick)
{
	EnterCriticalSection(&cs);
	for (int i = 0; i < _ptr->Nick_Count; i++)
	{	
		if (!strcmp(_ptr->NickNameList[i], _nick)) // 해당 채팅방의 닉네임 리스트와 닉네임 비교
		{
			LeaveCriticalSection(&cs);
			return false;
		}	
	}
	LeaveCriticalSection(&cs);

	return true; 
}

/* 닉네임리스트 배열에 닉네임을 추가하는 함수 */
void AddNickName(_ChattRoomInfo* _ptr, const char* _nick)
{
	EnterCriticalSection(&cs);
	char* ptr = new char[strlen(_nick) + 1];
	strcpy(ptr, _nick);
	_ptr->NickNameList[_ptr->Nick_Count++] = ptr; // 해당 채팅방의 닉네임 리스트에 추가
	LeaveCriticalSection(&cs);
}

/* 닉네임리스트 배열에서 닉네임을 삭제하는 함수 */
void RemoveNickName(_ClientInfo* _ptr)
{
	EnterCriticalSection(&cs);
	_ChattRoomInfo* chatt_ptr = _ptr->room_info; // 클라가 속한 채팅방 객체 생성

	for (int i = 0; i < chatt_ptr->Nick_Count; i++)
	{
		if (!strcmp(chatt_ptr->NickNameList[i], _ptr->nickname)) // 해당 채팅방 닉네임리스트에서 닉네임을 찾아서 삭제
		{
			for (int j = i; j < chatt_ptr->Nick_Count - 1; j++)
			{
				chatt_ptr->NickNameList[j] = chatt_ptr->NickNameList[j + 1];
			}
			chatt_ptr->Nick_Count--;
			break;
		}
	}
	LeaveCriticalSection(&cs);
}

void RemoveChattUser(_ClientInfo* _ptr)
{
	EnterCriticalSection(&cs);
	_ChattRoomInfo* chatt_info = _ptr->room_info;

	for (int i = 0; i < chatt_info->user_count; i++)
	{
		if (chatt_info->User[i] == _ptr)
		{
			for (int j = i; j < chatt_info->user_count - 1; j++)
			{
				chatt_info->User[j] = chatt_info->User[j + 1]; // 인덱스 앞당겨 줌
			}
			chatt_info->user_count--; // 채팅 유저수 -1
			if (chatt_info->user_count == 0) // 채팅방에 유저가 한명도 없으면 채팅방 삭제
			{
				RemoveChattRoom(chatt_info);
			}
		}
	}

	LeaveCriticalSection(&cs);
}

void MaKeChattMessage(const char* _nick, const char* _msg, char* _chattmsg)
{
	sprintf(_chattmsg, "[ %s ] %s", _nick, _msg);
}

void MakeEnterMessage(const char* _nick, char* _msg, int room_num)
{
	sprintf(_msg, "%s님이 %d번 방에 입장하셨습니다.", _nick, room_num);
}
void MakeExitMessage(const char* _nick, char* _msg, int room_num)
{
	sprintf(_msg, "%s님이 %d번 방에서 퇴장하셨습니다.", _nick, room_num);
}

/* 닉네임이 이미있으면 에러메시지 없으면 flag 켜주는 함수 */
void NickNameSetting(_ClientInfo* _clientinfo)
{
	EnterCriticalSection(&cs);
	
	UnPackPacket(_clientinfo->recvbuf, _clientinfo->nickname); // 닉네임 언패킹

	int size = PackPacket(_clientinfo->sendbuf, USER_STATE_CHANGE, CHATTROOM_INTRO_MSG); // 프로토콜 패킹(채팅방 입력 상태로 변경)
	int retval = send(_clientinfo->sock, _clientinfo->sendbuf, size, 0); // 클라로 전송
	if (retval == SOCKET_ERROR)
	{
		err_display("send()");
		_clientinfo->state = CONNECT_END_STATE;
		LeaveCriticalSection(&cs);
		return;
	}
	
	LeaveCriticalSection(&cs);	
}

void ChattRoomSetting(_ClientInfo* _ptr)
{
	EnterCriticalSection(&cs);
	char unpack_num[10];
	UnPackPacket(_ptr->recvbuf, unpack_num); // 방번호 언패킹

	// 숫자이외의 문자 예외 처리
	for (int i = 0; i < sizeof(unpack_num) / sizeof(char); i++)
	{
		if (!isdigit(unpack_num[i])) // 숫자 이외의 문자가 들어가 있으면 예외 처리
		{
			int size = PackPacket(_ptr->sendbuf, CONNECT_ERROR, CHATTROOM_CODE_EROR, CHATTROOM_CODE_ERROR_MSG); // 에러 코드, 에러 메시지 패킹
			int retval = send(_ptr->sock, _ptr->sendbuf, size, 0); // 코드, 메시지 전송
			if (retval == SOCKET_ERROR)
			{
				err_display("send()");
				_ptr->state = CONNECT_END_STATE;
			}
			LeaveCriticalSection(&cs);
			return;
		}
		// 정상
		else
			break;
	}

	_ptr->room_num = atoi(unpack_num); // 정수로 변환하여 할당
	// 방번호 범위 예외 처리
	if (_ptr->room_num < 1 || _ptr->room_num > MAXROOM)
	{
		int size = PackPacket(_ptr->sendbuf, CONNECT_ERROR, CHATTROOM_RANGE_EROR, CHATTROOM_RANGE_ERROR_MSG); // 에러 코드, 에러 메시지 패킹
		int retval = send(_ptr->sock, _ptr->sendbuf, size, 0); // 코드, 메시지 전송
		if (retval == SOCKET_ERROR)
		{
			err_display("send()");
			_ptr->state = CONNECT_END_STATE;
		}
		LeaveCriticalSection(&cs);
		return;
	}

	else
	{
		_ChattRoomInfo* chatt_ptr = AddChattRoom(_ptr);
	
		// 방이 가득 차면 메시지 전송
		if (chatt_ptr->full)
		{
			int size = PackPacket(_ptr->sendbuf, CONNECT_ERROR, CHATTROOM_FULL, CHATTROOM_FULL_MSG); // 에러 코드, 에러 메시지 패킹
			int retval = send(_ptr->sock, _ptr->sendbuf, size, 0); // 코드, 메시지 전송
			if (retval == SOCKET_ERROR)
			{
				err_display("send()");
				_ptr->state = CONNECT_END_STATE;
			}
			LeaveCriticalSection(&cs);
			return;
		}

		// 이미 있는 닉네임인지 확인
		if (!NicknameCheck(chatt_ptr, _ptr->nickname))
		{
			chatt_ptr->user_count--; // 해당 채팅방에 늘어난 유저수 -1 
			int size = PackPacket(_ptr->sendbuf, CONNECT_ERROR, NICKNAME_EROR, NICKNAME_ERROR_MSG); // 에러 메시지 패킹
			int retval = send(_ptr->sock, _ptr->sendbuf, size, 0); // 클라로 메시지 전송
			if (retval == SOCKET_ERROR)
			{
				err_display("send()");
				_ptr->state = CONNECT_END_STATE;
			}

			LeaveCriticalSection(&cs);
			return;
		}

		AddNickName(chatt_ptr, _ptr->nickname); // 닉네임 리스트에 닉네임 추가

		char msg[BUFSIZE];
		memset(msg, 0, sizeof(msg));

		int size = PackPacket(_ptr->sendbuf, NICKNAME_LIST, chatt_ptr->NickNameList, chatt_ptr->Nick_Count); // 닉네임 리스트 패킹
		int retval = send(_ptr->sock, _ptr->sendbuf, size, 0); // 클라로 전송
		if (retval == SOCKET_ERROR)
		{
			err_display("send()");
			_ptr->state = CONNECT_END_STATE;
			LeaveCriticalSection(&cs);
			return;
		}


		size = PackPacket(_ptr->sendbuf, USER_STATE_CHANGE); // 프로토콜 패킹(채팅방 접속 상태로 변경)
		retval = send(_ptr->sock, _ptr->sendbuf, size, 0); // 클라로 전송
		if (retval == SOCKET_ERROR)
		{
			err_display("send()");
			_ptr->state = CONNECT_END_STATE;
			LeaveCriticalSection(&cs);
			return;
		}

		_ptr->chatflag = true; // 없는 닉네임이고, 채팅방에 접속하면 플래그 on
	}
	LeaveCriticalSection(&cs);
}

void ChattingEnterProcess(_ClientInfo* _clientinfo)
{
	EnterCriticalSection(&cs);

	char msg[BUFSIZE];
	memset(msg, 0, sizeof(msg));

	_ChattRoomInfo* chatt_info = _clientinfo->room_info;

	MakeEnterMessage(_clientinfo->nickname, msg, _clientinfo->room_num); // 입장 메시지 생성
	for (int i = 0; i < chatt_info->user_count; i++)
	{
		int size = PackPacket(chatt_info->User[i]->sendbuf, USER_ENTER, msg); // 입장 메시지 패킹
		int retval = send(chatt_info->User[i]->sock, chatt_info->User[i]->sendbuf, size, 0); // 해당 채팅방에 접속 되어 있는 클라들에게 전송
		if (chatt_info->User[i]->sock == SOCKET_ERROR)
		{
			err_display("send()");
			chatt_info->User[i]->state = CONNECT_END_STATE;
			LeaveCriticalSection(&cs);
			return;
		}
	}

	LeaveCriticalSection(&cs);
}

void ChattingMessageProcess(_ClientInfo* _clientinfo)
{
	EnterCriticalSection(&cs);

	char msg[BUFSIZE];
	memset(msg, 0, sizeof(msg));

	_ChattRoomInfo* chatt_info = _clientinfo->room_info;

	UnPackPacket(_clientinfo->recvbuf, msg); // 클라가 보낸 메시지 언패킹
	MaKeChattMessage(_clientinfo->nickname, msg, _clientinfo->chatt); // 클라가 보낸 채팅 메시지를 다른 클라들에게도 보낼수 있도록 제작

	for (int i = 0; i < chatt_info->user_count; i++)
	{
		int size = PackPacket(chatt_info->User[i]->sendbuf, CHATT_MSG, _clientinfo->chatt); // 채팅 메시지 패킹
		    
		int retval = send(chatt_info->User[i]->sock, chatt_info->User[i]->sendbuf, size, 0); // 해당 채팅방에 접속 되어 있는 클라들에게 전송
		if (chatt_info->User[i]->sock == SOCKET_ERROR)
		{
			err_display("send()");
			chatt_info->User[i]->state = CONNECT_END_STATE;
			LeaveCriticalSection(&cs);
			return;
		}
	}

	LeaveCriticalSection(&cs);
}

void ChattingOutProcess(_ClientInfo* _clientinfo)
{
	EnterCriticalSection(&cs);

	// 클라가 채팅방에 없으면 바로 삭제
	if (_clientinfo->room_info == nullptr)
	{
		_clientinfo->state = CONNECT_END_STATE;
		LeaveCriticalSection(&cs);
		return;
	}

	char msg[BUFSIZE];
	memset(msg, 0, sizeof(msg));
	_ChattRoomInfo* chatt_info = _clientinfo->room_info;

	MakeExitMessage(_clientinfo->nickname, msg, _clientinfo->room_num); // 클라 종료 메시지 제작

	for (int i = 0; i < chatt_info->user_count; i++)
	{
		int size = PackPacket(chatt_info->User[i]->sendbuf, CHATT_OUT, msg); // 종료 메시지 패킹

		// 남은 클라들 에게만 전송
		if (chatt_info->User[i]->nickname != _clientinfo->nickname)
		{
			int retval = send(chatt_info->User[i]->sock, chatt_info->User[i]->sendbuf, size, 0); // 접속 되어 있는 클라들에게 전송
			if (chatt_info->User[i]->sock == SOCKET_ERROR)
			{
				err_display("send()");
				chatt_info->User[i]->state = CONNECT_END_STATE;
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

	RemoveNickName(_clientinfo); // 채팅방에서 닉네임 삭제
	RemoveChattUser(_clientinfo); // 채팅방에서 유저 삭제
	_clientinfo->state = CONNECT_END_STATE;

	LeaveCriticalSection(&cs);
}