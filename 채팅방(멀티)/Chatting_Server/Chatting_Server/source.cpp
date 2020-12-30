#pragma comment(lib, "ws2_32.lib")

#include <stdio.h>
#include <string.h>
#include <winsock2.h>
#include <ctype.h> // isdigit �Լ� ��� �뵵

#define SERVERIP "127.0.0.1"
#define SERVERPORT 9000

#define BUFSIZE 4096
#define NICKNAMESIZE 255

#define CLIENT_COUNT 100 // �ִ� Ŭ���̾�Ʈ ����
#define MAXUSER 2 // ä�ù濡 �� �� �ִ� �ִ� �ο�
#define MAXROOM 3 // ä�ù� ����

#define NODATA -1
#define INIT 0

#define INTRO_MSG "ä�����α׷��Դϴ�. �г����� �Է��ϼ���"
#define NICKNAME_ERROR_MSG "�̹��ִ� �г����Դϴ�. �ٸ� �г����� �Է��� �ּ���."
#define CHATTROOM_INTRO_MSG "1 ~ 3���� ���� �غ� �Ǿ� �ֽ��ϴ�. ���Ͻô� �� ��ȣ�� �Է��ϼ���."
#define CHATTROOM_CODE_ERROR_MSG "���� �̿��� ���ڸ� �Է� �ϼ̽��ϴ�. �ٽ� �Է��ϼ���."
#define CHATTROOM_RANGE_ERROR_MSG "�� ��ȣ�� ������ ������ϴ�. �ٽ� �Է��ϼ���."
#define CHATTROOM_FULL_MSG "�ش� ä�ù��� ���� á���ϴ�. �ٸ� ä�ù����� �ٽ� �Է��� �ּ���."

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
	NICKNAME_LIST, // �г��� ����Ʈ ������ ����� ��������
	CHATT_NICKNAME, // �г��� �Է��Ҷ� ���� ��������
	JOIN_CHATTROOM, // Ŭ�� ������ ä�ù��� ������ ����� ��������
	CONNECT_ERROR, // ���� ���� �϶� ���� �ڵ�� �Բ� ���� ��������
	INIT_SETTING, // �г���, ������ ä�ù� ������ ����� ��������
	USER_ENTER, // ������ ���忡 ���������� ����� ��������
	USER_STATE_CHANGE, // ���ӿ� ������ ������ ���¸� �����Ҷ� ����� ��������
	CHATT_MSG, // ä�� ���� ������ ����� ��������
	CHATT_OUT
};

enum ERRORCODE
{
	NICKNAME_EROR, // �̹� �ִ� �г���
	CHATTROOM_FULL,  // ���� �� ä�ù�
	CHATTROOM_CODE_EROR, // �� ��ȣ �Է� ����
	CHATTROOM_RANGE_EROR // �� ��ȣ ���� ����
};

enum CLOSECHATT
{
	LEAVE // ä�ù濡�� Ŭ�� ������ ������
};
struct _ChattRoomInfo;

/* Ŭ�� ���� */
struct _ClientInfo
{
	SOCKET sock;
	SOCKADDR_IN clientaddr;
	STATE state; // Ŭ�� ����
	
	_ChattRoomInfo* room_info; // Ŭ�� ������ ä�ù� ����
	int room_num; // Ŭ�� ���� �� ��ȣ
	int chattuser_number; // �ڱⰡ ���� ���� ��� �������� Ȯ���Ҷ� ��� 
	bool chatflag; // ä�ù濡 ��� ������ üũ
	char  nickname[NICKNAMESIZE]; // Ŭ�� �г���
	char chatt[BUFSIZE]; // Ŭ�� ���� ä�� ����
	
	char  sendbuf[BUFSIZE];
	char  recvbuf[BUFSIZE];

};

/* ä�ù� ���� */
struct _ChattRoomInfo
{
	_ClientInfo* User[MAXUSER]; // ä�ù濡 ���� Ŭ��
	int chattroom_number; // �� ��ȣ
	int user_count; // ä�ù� �ο��� ���� á���� Ȯ���� ����
	bool full; // ä�ù� �ο��� ���� ���� ����� ������ �� ����
	char* NickNameList[MAXUSER]; // ä�ù濡 ���� �г��� ����Ʈ
	int Nick_Count = 0; // �г��� ī��Ʈ
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

_ClientInfo* SearchClient(const char*); //�г������� ����ã��
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
	// ���� �ʱ�ȭ
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

	// ������ ��ſ� ����� ����		
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

			/* �г��� ���� */
		case CHATT_INITE_STATE:
			if (!PacketRecv(Client_ptr->sock, Client_ptr->recvbuf)) // Ŭ��κ��� �г���,�� ��ȣ �ޱ�
			{
				Client_ptr->state = CONNECT_END_STATE;
				break;
			}

			protocol = GetProtocol(Client_ptr->recvbuf); // �������� �и�

			switch (protocol)
			{
				
			case CHATT_NICKNAME:
				// �̹� �ִ� �г������� Ȯ���ϰ� ���� �߰�
				NickNameSetting(Client_ptr);
				break;
				// ������ �� ä�ù� ����
			case JOIN_CHATTROOM:
				ChattRoomSetting(Client_ptr);
				break;
				// ������ ���� ����
			case CHATT_OUT:
				ChattingOutProcess(Client_ptr);
				break;
			}

			// �г��� ����Ʈ�� ���� �г����̸� ����Ʈ�� �߰��ϰ� Ŭ�� ���� ���·� ����
			if (Client_ptr->chatflag &&	Client_ptr->state != CONNECT_END_STATE)
			{
				ChattingEnterProcess(Client_ptr);
				Client_ptr->state = CHATTING_STATE;
			}
			break;

			/* ä�� ���ڿ� �޾Ƽ� Ŭ��鿡�� ������ */
		case CHATTING_STATE:
			if (!PacketRecv(Client_ptr->sock, Client_ptr->recvbuf)) // Ŭ��� ���� ä�� ���� �ޱ�
			{
				Client_ptr->state = CONNECT_END_STATE;
				break;
			}

			protocol = GetProtocol(Client_ptr->recvbuf); // �������� �и�

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

// ���� �Լ� ���� ���
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
	printf("\nClient ����: IP �ּ�=%s, ��Ʈ ��ȣ=%d\n", inet_ntoa(clientaddr.sin_addr),
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

	printf("\nClient ����: IP �ּ�=%s, ��Ʈ ��ȣ=%d\n",
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

	// null�̸� �ش�濡 ó�� ������ Ŭ�� �̹Ƿ� �ؿ� �ڵ忡�� ���� ����
	if (ChattRoomInfo[_ptr->room_num - 1] != nullptr)
	{
		// ä�ù� �ο��� ��������
		if (ChattRoomInfo[_ptr->room_num - 1]->user_count == MAXUSER)
		{
			ChattRoomInfo[_ptr->room_num - 1]->full = true;
			LeaveCriticalSection(&cs);
			return ChattRoomInfo[_ptr->room_num - 1];
		}
		// �� ��ȣ�� ���� �� ���� �������� �ʾ��� ��쿡 �߰�
		if (ChattRoomInfo[_ptr->room_num - 1]->chattroom_number == _ptr->room_num && !ChattRoomInfo[_ptr->room_num - 1]->user_count != MAXUSER)
		{
			ChattRoomInfo[_ptr->room_num - 1]->User[ChattRoomInfo[_ptr->room_num - 1]->user_count++] = _ptr; // ������ Ŭ���̾�Ʈ ������� ����
			_ptr->room_info = ChattRoomInfo[_ptr->room_num - 1]; // ���� ä�ù� ���� ����
			_ptr->chattuser_number = ChattRoomInfo[_ptr->room_num - 1]->user_count; // �ڽ��� ���� ä�ù濡���� ��ȣ ����
			ChattRoomInfo[_ptr->room_num - 1]->full = false;

			chatt_ptr = ChattRoomInfo[_ptr->room_num - 1]; // ���ŵ� ���� ����
			index = _ptr->room_num - 1;
			LeaveCriticalSection(&cs);
			return chatt_ptr;
		}
	}
	

	/* ���� ó�� ���� Ŭ���̾�Ʈ */
	if (index == NODATA || ChattRoomInfo[_ptr->room_num - 1] == nullptr)
	{
		chatt_ptr = new _ChattRoomInfo;
		memset(chatt_ptr, 0, sizeof(_ChattRoomInfo)); // �ʱ�ȭ

		chatt_ptr->full = false; // �ִ� 10������ �����ص�
		chatt_ptr->chattroom_number = _ptr->room_num; // �� ��ȣ ����
		chatt_ptr->User[0] = _ptr; // ���� ���� ���� Ŭ�� ����
		chatt_ptr->user_count++; // ä�� ���� Ŭ�� +1
		ChattRoomInfo[_ptr->room_num - 1] = chatt_ptr; // Ŭ�� ������ ���ȣ 
		Room_Count++;

		_ptr->room_info = chatt_ptr; // ä�ù� ���� ����
		_ptr->chattuser_number = chatt_ptr->user_count; // �ڽ��� ���� ä�ù濡���� ��ȣ ����
	}

	LeaveCriticalSection(&cs);
	return chatt_ptr;
}

void RemoveChattRoom(_ChattRoomInfo* _ptr)
{
	EnterCriticalSection(&cs);
	for (int i = 0; i < Room_Count; i++)
	{
		if (ChattRoomInfo[i] == _ptr) // �ش� ä�ù��� ã�Ƽ� 
		{
			delete _ptr; // �޸� ����
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

/* ���� �г����� �ִ��� üũ�ϴ� �Լ� */
bool NicknameCheck(_ChattRoomInfo* _ptr, const char* _nick)
{
	EnterCriticalSection(&cs);
	for (int i = 0; i < _ptr->Nick_Count; i++)
	{	
		if (!strcmp(_ptr->NickNameList[i], _nick)) // �ش� ä�ù��� �г��� ����Ʈ�� �г��� ��
		{
			LeaveCriticalSection(&cs);
			return false;
		}	
	}
	LeaveCriticalSection(&cs);

	return true; 
}

/* �г��Ӹ���Ʈ �迭�� �г����� �߰��ϴ� �Լ� */
void AddNickName(_ChattRoomInfo* _ptr, const char* _nick)
{
	EnterCriticalSection(&cs);
	char* ptr = new char[strlen(_nick) + 1];
	strcpy(ptr, _nick);
	_ptr->NickNameList[_ptr->Nick_Count++] = ptr; // �ش� ä�ù��� �г��� ����Ʈ�� �߰�
	LeaveCriticalSection(&cs);
}

/* �г��Ӹ���Ʈ �迭���� �г����� �����ϴ� �Լ� */
void RemoveNickName(_ClientInfo* _ptr)
{
	EnterCriticalSection(&cs);
	_ChattRoomInfo* chatt_ptr = _ptr->room_info; // Ŭ�� ���� ä�ù� ��ü ����

	for (int i = 0; i < chatt_ptr->Nick_Count; i++)
	{
		if (!strcmp(chatt_ptr->NickNameList[i], _ptr->nickname)) // �ش� ä�ù� �г��Ӹ���Ʈ���� �г����� ã�Ƽ� ����
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
				chatt_info->User[j] = chatt_info->User[j + 1]; // �ε��� �մ�� ��
			}
			chatt_info->user_count--; // ä�� ������ -1
			if (chatt_info->user_count == 0) // ä�ù濡 ������ �Ѹ� ������ ä�ù� ����
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
	sprintf(_msg, "%s���� %d�� �濡 �����ϼ̽��ϴ�.", _nick, room_num);
}
void MakeExitMessage(const char* _nick, char* _msg, int room_num)
{
	sprintf(_msg, "%s���� %d�� �濡�� �����ϼ̽��ϴ�.", _nick, room_num);
}

/* �г����� �̹������� �����޽��� ������ flag ���ִ� �Լ� */
void NickNameSetting(_ClientInfo* _clientinfo)
{
	EnterCriticalSection(&cs);
	
	UnPackPacket(_clientinfo->recvbuf, _clientinfo->nickname); // �г��� ����ŷ

	int size = PackPacket(_clientinfo->sendbuf, USER_STATE_CHANGE, CHATTROOM_INTRO_MSG); // �������� ��ŷ(ä�ù� �Է� ���·� ����)
	int retval = send(_clientinfo->sock, _clientinfo->sendbuf, size, 0); // Ŭ��� ����
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
	UnPackPacket(_ptr->recvbuf, unpack_num); // ���ȣ ����ŷ

	// �����̿��� ���� ���� ó��
	for (int i = 0; i < sizeof(unpack_num) / sizeof(char); i++)
	{
		if (!isdigit(unpack_num[i])) // ���� �̿��� ���ڰ� �� ������ ���� ó��
		{
			int size = PackPacket(_ptr->sendbuf, CONNECT_ERROR, CHATTROOM_CODE_EROR, CHATTROOM_CODE_ERROR_MSG); // ���� �ڵ�, ���� �޽��� ��ŷ
			int retval = send(_ptr->sock, _ptr->sendbuf, size, 0); // �ڵ�, �޽��� ����
			if (retval == SOCKET_ERROR)
			{
				err_display("send()");
				_ptr->state = CONNECT_END_STATE;
			}
			LeaveCriticalSection(&cs);
			return;
		}
		// ����
		else
			break;
	}

	_ptr->room_num = atoi(unpack_num); // ������ ��ȯ�Ͽ� �Ҵ�
	// ���ȣ ���� ���� ó��
	if (_ptr->room_num < 1 || _ptr->room_num > MAXROOM)
	{
		int size = PackPacket(_ptr->sendbuf, CONNECT_ERROR, CHATTROOM_RANGE_EROR, CHATTROOM_RANGE_ERROR_MSG); // ���� �ڵ�, ���� �޽��� ��ŷ
		int retval = send(_ptr->sock, _ptr->sendbuf, size, 0); // �ڵ�, �޽��� ����
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
	
		// ���� ���� ���� �޽��� ����
		if (chatt_ptr->full)
		{
			int size = PackPacket(_ptr->sendbuf, CONNECT_ERROR, CHATTROOM_FULL, CHATTROOM_FULL_MSG); // ���� �ڵ�, ���� �޽��� ��ŷ
			int retval = send(_ptr->sock, _ptr->sendbuf, size, 0); // �ڵ�, �޽��� ����
			if (retval == SOCKET_ERROR)
			{
				err_display("send()");
				_ptr->state = CONNECT_END_STATE;
			}
			LeaveCriticalSection(&cs);
			return;
		}

		// �̹� �ִ� �г������� Ȯ��
		if (!NicknameCheck(chatt_ptr, _ptr->nickname))
		{
			chatt_ptr->user_count--; // �ش� ä�ù濡 �þ ������ -1 
			int size = PackPacket(_ptr->sendbuf, CONNECT_ERROR, NICKNAME_EROR, NICKNAME_ERROR_MSG); // ���� �޽��� ��ŷ
			int retval = send(_ptr->sock, _ptr->sendbuf, size, 0); // Ŭ��� �޽��� ����
			if (retval == SOCKET_ERROR)
			{
				err_display("send()");
				_ptr->state = CONNECT_END_STATE;
			}

			LeaveCriticalSection(&cs);
			return;
		}

		AddNickName(chatt_ptr, _ptr->nickname); // �г��� ����Ʈ�� �г��� �߰�

		char msg[BUFSIZE];
		memset(msg, 0, sizeof(msg));

		int size = PackPacket(_ptr->sendbuf, NICKNAME_LIST, chatt_ptr->NickNameList, chatt_ptr->Nick_Count); // �г��� ����Ʈ ��ŷ
		int retval = send(_ptr->sock, _ptr->sendbuf, size, 0); // Ŭ��� ����
		if (retval == SOCKET_ERROR)
		{
			err_display("send()");
			_ptr->state = CONNECT_END_STATE;
			LeaveCriticalSection(&cs);
			return;
		}


		size = PackPacket(_ptr->sendbuf, USER_STATE_CHANGE); // �������� ��ŷ(ä�ù� ���� ���·� ����)
		retval = send(_ptr->sock, _ptr->sendbuf, size, 0); // Ŭ��� ����
		if (retval == SOCKET_ERROR)
		{
			err_display("send()");
			_ptr->state = CONNECT_END_STATE;
			LeaveCriticalSection(&cs);
			return;
		}

		_ptr->chatflag = true; // ���� �г����̰�, ä�ù濡 �����ϸ� �÷��� on
	}
	LeaveCriticalSection(&cs);
}

void ChattingEnterProcess(_ClientInfo* _clientinfo)
{
	EnterCriticalSection(&cs);

	char msg[BUFSIZE];
	memset(msg, 0, sizeof(msg));

	_ChattRoomInfo* chatt_info = _clientinfo->room_info;

	MakeEnterMessage(_clientinfo->nickname, msg, _clientinfo->room_num); // ���� �޽��� ����
	for (int i = 0; i < chatt_info->user_count; i++)
	{
		int size = PackPacket(chatt_info->User[i]->sendbuf, USER_ENTER, msg); // ���� �޽��� ��ŷ
		int retval = send(chatt_info->User[i]->sock, chatt_info->User[i]->sendbuf, size, 0); // �ش� ä�ù濡 ���� �Ǿ� �ִ� Ŭ��鿡�� ����
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

	UnPackPacket(_clientinfo->recvbuf, msg); // Ŭ�� ���� �޽��� ����ŷ
	MaKeChattMessage(_clientinfo->nickname, msg, _clientinfo->chatt); // Ŭ�� ���� ä�� �޽����� �ٸ� Ŭ��鿡�Ե� ������ �ֵ��� ����

	for (int i = 0; i < chatt_info->user_count; i++)
	{
		int size = PackPacket(chatt_info->User[i]->sendbuf, CHATT_MSG, _clientinfo->chatt); // ä�� �޽��� ��ŷ
		    
		int retval = send(chatt_info->User[i]->sock, chatt_info->User[i]->sendbuf, size, 0); // �ش� ä�ù濡 ���� �Ǿ� �ִ� Ŭ��鿡�� ����
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

	// Ŭ�� ä�ù濡 ������ �ٷ� ����
	if (_clientinfo->room_info == nullptr)
	{
		_clientinfo->state = CONNECT_END_STATE;
		LeaveCriticalSection(&cs);
		return;
	}

	char msg[BUFSIZE];
	memset(msg, 0, sizeof(msg));
	_ChattRoomInfo* chatt_info = _clientinfo->room_info;

	MakeExitMessage(_clientinfo->nickname, msg, _clientinfo->room_num); // Ŭ�� ���� �޽��� ����

	for (int i = 0; i < chatt_info->user_count; i++)
	{
		int size = PackPacket(chatt_info->User[i]->sendbuf, CHATT_OUT, msg); // ���� �޽��� ��ŷ

		// ���� Ŭ��� ���Ը� ����
		if (chatt_info->User[i]->nickname != _clientinfo->nickname)
		{
			int retval = send(chatt_info->User[i]->sock, chatt_info->User[i]->sendbuf, size, 0); // ���� �Ǿ� �ִ� Ŭ��鿡�� ����
			if (chatt_info->User[i]->sock == SOCKET_ERROR)
			{
				err_display("send()");
				chatt_info->User[i]->state = CONNECT_END_STATE;
				LeaveCriticalSection(&cs);
				return;
			}
		}
	}

	int size = PackPacket(_clientinfo->sendbuf, CHATT_OUT, LEAVE); // ������ Ŭ�󿡰� ���� �������� ��ŷ
	int retval = send(_clientinfo->sock, _clientinfo->sendbuf, size, 0); // ������ Ŭ�󿡰� ����
	if (retval == SOCKET_ERROR)
	{
		err_display("send()");
		_clientinfo->state = CONNECT_END_STATE;
		LeaveCriticalSection(&cs);
		return;
	}

	RemoveNickName(_clientinfo); // ä�ù濡�� �г��� ����
	RemoveChattUser(_clientinfo); // ä�ù濡�� ���� ����
	_clientinfo->state = CONNECT_END_STATE;

	LeaveCriticalSection(&cs);
}