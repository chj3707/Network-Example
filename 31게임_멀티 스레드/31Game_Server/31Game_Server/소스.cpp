#pragma comment(lib, "ws2_32")
#include <winsock2.h>
#include <stdlib.h>
#include <stdio.h>
#include <ws2tcpip.h> 
#include <time.h>

/* ��ȣ��� ���� */
#define SERVERPORT 9000
#define BUFSIZE    4096
#define CLIENT_COUNT 100 // ���� ������ Ŭ���̾�Ʈ ��
#define PLAYER_COUNT 3 // �� ���ӿ� ���� ������ ����

#define GAME_NUMBER_SIZE 31

#define WAIT_MSG "�ٸ� ������ ���� ���Դϴ�. ��ø� ��ٷ� �ּ���.\n"
#define INTRO_MSG "31 ���ڰ��� �Դϴ�. 1���� �����մϴ�. \n������ʸ� ��ٷ� �ּ���.\n"
#define CLIENT_TURN_MSG "��������Դϴ�. �ѹ��� 3������ ������ �� �ֽ��ϴ�. �����ϼ���:"
#define DATA_RANGE_ERROR_MSG "�߸� �����߽��ϴ�. 1~3������ ���ð����մϴ�.\n"
#define GAME_ESCAPE_MSG "�� Player�� �������ϴ�.\n"
#define GAME_CLOSE_MSG "����� �÷��̾ �����ϴ�. ������ �����ϰڽ��ϴ�.\n"
#define WIN_MSG "�� Player�� �����ϴ�. �¸� �ϼ̽��ϴ�.\n"
#define LOSE_MSG "����� �����ϴ�.\n"

CRITICAL_SECTION cs;

/* Ŭ���̾�Ʈ ���� */
enum CLIENT_STATE
{
	INIT_STATE = 1,
	CLIENT_TURN_STATE, // Ŭ���̾�Ʈ ��
	OTHER_TURN_STATE, // �ٸ� �÷��̾� ��
	GAME_RESULT_STATE, // ���� ����
	DISCONNECTED_STATE // ���� ����
};

/* ���� ��� */
enum RESULT_VALUE
{
	INIT = -1,
	WIN = 1,  // �¸�
	LOSE  // �й�
};

/* �������� */
enum PROTOCOL
{
	WAIT = 1, // �ٸ� ������ ��ٸ��� ����� ��������
	INTRO, // ��Ʈ�ο� ����� ��������
	PLAYER_INFO, // ��� �÷��̾����� �˷��ٶ� ����� ��������
	SELECT_NUM, // Ŭ�� ���� �����ؼ� ������ ����� ��������
	COUNT_VALUE, // Ŭ�� �Է��� ���ڸ� ����ؼ� ������ ����� ��������
	CLIENT_TURN, // Ŭ���̾�Ʈ ���ʸ� �˷��ٶ� ����� ��������
	PLAYER_ESCAPE, // �÷��̾ �߰��� ������ �� ���� ��������
	GAME_CLOSE, // ���� ���� �÷��̾ ������ �����Ҷ� ���� ��������
	DATA_ERROR, // �����϶� ���� �������� 
	GAME_RESULT // ���� ����� �������� Ŭ�󿡰� ���� ��������
};

/* ���� �ڵ� */
enum ERROR_CODE
{
	DATA_RANGE_ERROR = 1 // ���� ����
};

/* ���� ���� */
enum GAME_STATE
{
	G_WAIT_STATE = 1, // ��� ����
	G_PLAYING_STATE, // ���� �������� ����
	G_GAME_OVER_STATE // ������ ���� ����
};

struct _ClientInfo; // Ŭ������ ����ü ����

/* ���� ���� ����ü(Ŭ�� �׷� ����) */
struct _GameInfo
{
	int num_count; // ���� ���� ����� ������� ������ ����
	int currect_num[GAME_NUMBER_SIZE]; // ����� ���ڱ��� ������ �迭

	HANDLE start_event; // ���� ���� �̺�Ʈ(����)
	GAME_STATE state; // ���� ���� ������

	_ClientInfo* players[PLAYER_COUNT]; // ���ӿ� ������ Ŭ���̾�Ʈ(PLAYER_COUNT��ŭ ��������)
	_ClientInfo* cur_player; // ���� ������ �÷��̾�
	_ClientInfo* lose_player; // ������ �� �÷��̾�

	int player_count; // ���� �÷��� �ο��� ���� á���� Ȯ���� ����
	bool full; // �÷��� �ο��� ���� ���� ���ӽ����� ������ ����
};

/* Ŭ������ ����ü */
struct _ClientInfo
{
	SOCKET sock; // Ŭ�� ����
	SOCKADDR_IN addr; // Ŭ�� �ּ�

	CLIENT_STATE state; // Ŭ�� ���� ������
	
	HANDLE turn_event; // �ڱ� ���� ��ٸ��� �̺�Ʈ
	_GameInfo* game_info; // �ڱⰡ ������ ��������
	int player_number; // ���� ������ ��� �÷��̾����� ������ ����
	RESULT_VALUE result; // ���� ���

	char recv_buf[BUFSIZE]; // ������ ����� �۾��뿪��
	char send_buf[BUFSIZE]; // ������ ����� �۾��뿪��
};

_GameInfo* GameInfo[CLIENT_COUNT]; // ���� ������ ����Ǿ� �ִ� ����ü �迭
int GameCount = 0; // ���ӿ� ������ Ŭ���̾�Ʈ ��

_ClientInfo* ClientInfo[CLIENT_COUNT]; // Ŭ�� ������ ����Ǿ� �ִ� ����ü �迭
int Count = 0; // ���� Ŭ���̾�Ʈ ��

/* �Լ� ���� */
void EscapePlayer(_ClientInfo* _ptr);
DWORD WINAPI ProcessClient(LPVOID arg);
void GameResultProcess(_ClientInfo* _ptr);
void OtherTurnProcess(_ClientInfo* _ptr);
void ClientTurnProcess(_ClientInfo* _ptr);
_ClientInfo* NextTurnClient(_ClientInfo* _ptr);
bool GameContinueCheak(_ClientInfo* _ptr, int num_count);
void InitProcess(_ClientInfo* _ptr);
bool ChecKDataRange(int _data);
void UnPacking(char* _buf, int& _data);
int Packing(char* _buf, PROTOCOL  _protocol, int _data, const char* _str);
int Packing(char* _buf, PROTOCOL  _protocol, const char* _str);
PROTOCOL GetProtocol(char* _buf);
bool PacketRecv(SOCKET _sock, char* _buf);
int recvn(SOCKET s, char* buf, int len, int flags);
void ReMoveGamePlayer(_ClientInfo* _ptr);
void ReMoveGameInfo(_GameInfo* _ptr);
_GameInfo* AddGameInfo(_ClientInfo* _ptr);
void ReMoveClientInfo(_ClientInfo* ptr);
_ClientInfo* AddClientInfo(SOCKET sock, SOCKADDR_IN addr);

// ���� �Լ� ���� ��� �� ����
void err_quit(const char* msg)
{
	LPVOID lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	MessageBox(NULL, (LPCTSTR)lpMsgBuf, msg, MB_ICONERROR);
	LocalFree(lpMsgBuf);
	exit(1);
}

// ���� �Լ� ���� ���
void err_display(const char* msg)
{
	LPVOID lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	printf("[%s] %s", msg, (char*)lpMsgBuf);
	LocalFree(lpMsgBuf);
}

/* Ŭ�� ���� �߰� */
_ClientInfo* AddClientInfo(SOCKET sock, SOCKADDR_IN addr)
{
	EnterCriticalSection(&cs); // ������ ��ȣ

	/* ���� Ŭ���̾�Ʈ �ʱⰪ ���� */
	_ClientInfo* ptr = new _ClientInfo;
	ZeroMemory(ptr, sizeof(_ClientInfo));
	ptr->sock = sock;
	memcpy(&ptr->addr, &addr, sizeof(addr));
	ptr->state = INIT_STATE;
	ptr->result = INIT;
	ptr->player_number = INIT;

	ptr->turn_event = CreateEvent(nullptr, false/* �ڵ� */, false/* ���ȣ ���� */, nullptr); //�̺�Ʈ ���� 

	ClientInfo[Count++] = ptr;
	
	LeaveCriticalSection(&cs);

	printf("\n[TCP ����] Ŭ���̾�Ʈ ����: IP �ּ�=%s, ��Ʈ ��ȣ=%d\n",
		inet_ntoa(ptr->addr.sin_addr), ntohs(ptr->addr.sin_port));

	return ptr;
}

/* Ŭ�� ���� ���� */
void ReMoveClientInfo(_ClientInfo* ptr)
{
	printf("\n[TCP ����] Ŭ���̾�Ʈ ����: IP �ּ�=%s, ��Ʈ ��ȣ=%d\n",
		inet_ntoa(ptr->addr.sin_addr), ntohs(ptr->addr.sin_port));

	EnterCriticalSection(&cs);
	for (int i = 0; i < Count; i++)
	{
		if (ClientInfo[i] == ptr) // Ŭ�� ���� �迭���� �ݺ��� ���� �ش� Ŭ�� ã�Ƽ�
		{
			closesocket(ptr->sock); // Ŭ�� ���� ����
			CloseHandle(ptr->turn_event); // Ŭ���� �̺�Ʈ �ڵ� ����
			delete ptr; // �޸� ����
			for (int j = i; j < Count - 1; j++)
			{
				ClientInfo[j] = ClientInfo[j + 1]; // ������ �ε��� �������� �մ�� ����
			}
			break;
		}
	}

	Count--; // Ŭ�� ���� ���� -1
	LeaveCriticalSection(&cs);
}

/* ���� ���� �߰�(�ʱⰪ ����, �ʱ�ȭ) */
_GameInfo* AddGameInfo(_ClientInfo* _ptr)
{
	EnterCriticalSection(&cs); // ������ ��ȣ
	_GameInfo* game_ptr = nullptr; // ��ü ����
	int index = INIT;

	for (int i = 0; i < GameCount; i++)
	{
		if (!GameInfo[i]->full) // ���� �ο��� �������� �ʾ��� ���
		{
			GameInfo[i]->players[GameInfo[i]->player_count++] = _ptr; // ������ Ŭ���̾�Ʈ ������� ����
			_ptr->game_info = GameInfo[i]; // ���� ���� ���� ����
			_ptr->player_number = GameInfo[i]->player_count; // �÷��̾� ��ȣ ����
			if (GameInfo[i]->player_count == PLAYER_COUNT) // ���� �ο��� ���� ����
			{
				GameInfo[i]->full = true; 
				GameInfo[i]->state = G_PLAYING_STATE; // ���� ���� ���·� ����
				GameInfo[i]->cur_player = GameInfo[i]->players[0]; // 1�� �÷��̾���� ����
				SetEvent(GameInfo[i]->start_event); // �̺�Ʈ ��ȣ ���·� ����
			}
			game_ptr = GameInfo[i];
			index = i;
			break;
		}
	}

	/* ���� ���� ���� Ŭ���̾�Ʈ(1�� �÷��̾�) */
	if (index == INIT)
	{
		game_ptr = new _GameInfo;
		ZeroMemory(game_ptr, sizeof(_GameInfo)); // �������� ����ü ������ �ʱ�ȭ
		
		game_ptr->full = false;
		game_ptr->start_event = CreateEvent(nullptr, true/* ���� */, false/* ���ȣ */, nullptr); // ���� ���� �̺�Ʈ ����
		game_ptr->players[0] = _ptr; // ���� ���� ���� Ŭ���̾�Ʈ ����
		/* ���� ������ ������ �ʱ�ȭ */
		ZeroMemory(game_ptr->currect_num, sizeof(game_ptr->currect_num));
		game_ptr->num_count = INIT;
		game_ptr->cur_player = nullptr;
		game_ptr->lose_player = nullptr;

		game_ptr->player_count++; // ���� ���� �÷��̾� +1
		game_ptr->state = G_WAIT_STATE; // ���� ���� ��� ����
		GameInfo[GameCount++] = game_ptr; // ���� ���� �迭�� ����, GameCount +1

		_ptr->game_info = game_ptr; // ���� ���� ���� ����
		_ptr->player_number = game_ptr->player_count; // �÷��̾� ��ȣ ����
	}

	LeaveCriticalSection(&cs);

	return game_ptr;
}

/* ���� ���� ���� */
void ReMoveGameInfo(_GameInfo* _ptr)
{
	EnterCriticalSection(&cs);
	for (int i = 0; i < GameCount; i++)
	{
		if (GameInfo[i] == _ptr) // ���� ���� �迭���� �ݺ��� ���� �ش� ������ ã�Ƽ�
		{
			CloseHandle(_ptr->start_event); // �ش� ���� �̺�Ʈ �ڵ� ����
			delete _ptr; // �ش� ���� ���� �޸� ����
			for (int j = i; j < GameCount - 1; j++)
			{
				GameInfo[j] = GameInfo[j + 1]; // ������ �ε��� �������� �մ�� ����
			}
		}
	}
	GameCount--; // ���� ���� -1
	LeaveCriticalSection(&cs);
}

/* ���ӿ� ���� �÷��̾�(Ŭ��) ���� */
void ReMoveGamePlayer(_ClientInfo* _ptr)
{
	EnterCriticalSection(&cs);
	_GameInfo* game_info = _ptr->game_info; // Ŭ�� ���� �������� �Ҵ�

	for (int i = 0; i < game_info->player_count; i++) // �ش� ������ �÷��̾� ������ŭ �ݺ�
	{
		if (game_info->players[i] == _ptr) // �ݺ��� ���� �÷��̾�(Ŭ��)�� ã�Ƽ�
		{
			for (int j = i; j < game_info->player_count - 1; j++)
			{
				game_info->players[j] = game_info->players[j + 1]; // ������ �ε��� �������� �մ�� ����
			}
			game_info->player_count--; // �÷��̾�� -1

			if (game_info->player_count == 0)
			{
				ReMoveGameInfo(game_info); // ������ �÷��̾ 0���� �Ǹ� �ش� �������� ���� 
			}
		}
	}
	LeaveCriticalSection(&cs);
}

/* �÷��̾ ���� ���� ������ �� ó���� �Լ� */
void EscapePlayer(_ClientInfo* _ptr)
{
	char msg[BUFSIZE]; // �޽��� ������ ����
	_GameInfo* game_info = _ptr->game_info; // ���ӿ��� ���� �÷��̾ ���� �������� ��ü ����

	if (game_info->player_count >= 2) // �÷��̾ 2�� �̻��϶���
	{
		for (int i = 0; i < game_info->player_count; i++)
		{
			if (_ptr != game_info->players[i]) // ���� �÷��̾� �鿡�� �޽��� ������
			{
				sprintf(msg, "%d%s", _ptr->player_number, GAME_ESCAPE_MSG);

				int size = Packing(_ptr->send_buf, PLAYER_ESCAPE, msg);
				int retval = send(game_info->players[i]->sock, _ptr->send_buf, size, 0); // ��ŷ�� �޽��� Ŭ��� ������
				if (retval == SOCKET_ERROR)
				{
					if (game_info->players[i]->sock == INVALID_SOCKET) // ������ ���� Ŭ��(������ Ŭ���̾�Ʈ)�� ã�Ƽ� ���� ���� ���·� ����
					{
						err_display("send()");
						game_info->players[i]->state = DISCONNECTED_STATE;
						return;
					}
				}
			}
		}
	
		/* ���� ���� �÷��̾ ������ ��쿡�� �� ���� (���� ���� �ƴ� �÷��̾ ������ ��쿡�� �� ���� x) */
		if (_ptr == game_info->cur_player) 
		{
			_ClientInfo* next_ptr = NextTurnClient(_ptr); // �������Ƿ� ���� ���� �÷��̾� ������ ����
			SetEvent(next_ptr->turn_event); // �̺�Ʈ ��ȣ ���·� ����(�ڵ�:�ִ� ����)
		}
	}
}

int recvn(SOCKET s, char* buf, int len, int flags)
{
	int received;
	char* ptr = buf;
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

/* recvn 2ȸ */
bool PacketRecv(SOCKET _sock, char* _buf)
{
	int size;

	// �뷮 
	int retval = recvn(_sock, (char*)&size, sizeof(size), 0);
	// closesocket ȣ�� ���� ���� (���� ����) = SOCKET_ERROR
	if (retval == SOCKET_ERROR)
	{
		err_display("gvalue recv error()");
		return false;
	}
	// (��밡)closesocket ȣ�� �Ͽ� ���� (���� ����) �ҽ� return 0
	else if (retval == 0)
	{
		return false;
	}

	// �뷮��ŭ ������ �޾ƿ�
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

/* ������ ���� �������� �и��ϴ� �κ� */
PROTOCOL GetProtocol(char* _buf)
{
	PROTOCOL protocol;
	memcpy(&protocol, _buf, sizeof(PROTOCOL));

	return protocol;
}

// INIT, INTRO, PLAYER_INFO, CLIENT_TURN,
int Packing(char* _buf, PROTOCOL  _protocol, const char* _str)
{
	int size = 0;
	char* ptr = _buf;
	int strsize = strlen(_str);


	ptr = ptr + sizeof(size);

	memcpy(ptr, &_protocol, sizeof(_protocol));
	ptr = ptr + sizeof(_protocol);
	size = size + sizeof(_protocol);


	memcpy(ptr, &strsize, sizeof(strsize));
	ptr = ptr + sizeof(strsize);
	size = size + sizeof(strsize);

	memcpy(ptr, _str, strsize);
	ptr = ptr + strsize;
	size = size + strsize;

	ptr = _buf;
	memcpy(ptr, &size, sizeof(size));

	size = size + sizeof(size);

	return size;
}

// DATA_ERROR, WIN_MSG, LOSE_MSG
int Packing(char* _buf, PROTOCOL  _protocol, int _data, const char* _str)
{
	char* ptr = _buf;
	int strsize = strlen(_str);
	int size = 0;

	ptr = ptr + sizeof(size);

	memcpy(ptr, &_protocol, sizeof(_protocol));
	ptr = ptr + sizeof(_protocol);
	size = size + sizeof(_protocol);

	memcpy(ptr, &_data, sizeof(_data));
	ptr = ptr + sizeof(_data);
	size = size + sizeof(_data);

	memcpy(ptr, &strsize, sizeof(strsize));
	ptr = ptr + sizeof(strsize);
	size = size + sizeof(strsize);

	memcpy(ptr, _str, strsize);
	ptr = ptr + strsize;
	size = size + strsize;

	ptr = _buf;
	memcpy(ptr, &size, sizeof(size));

	size = size + sizeof(size);

	return size;
}


// SELECT_NUM
void UnPacking(char* _buf, int& _data)
{
	char* ptr = _buf + sizeof(PROTOCOL);

	memcpy(&_data, ptr, sizeof(_data));
	ptr = ptr + sizeof(_data);
}

/* (Ŭ��κ���)���� ������ ���� �˻� */
bool ChecKDataRange(int _data)
{

	if (_data < 1 || _data > 3) // 1~3
	{
		return false;
	}

	return true;
}

/* INIT_STATE ���μ��� */
void InitProcess(_ClientInfo* _ptr)
{
	_GameInfo* game_info = AddGameInfo(_ptr);

	/* ��� ���� */
	if (game_info->state == G_WAIT_STATE)
	{
		int size = Packing(_ptr->send_buf, WAIT, WAIT_MSG); // ��� �޽��� ��ŷ

		int retval = send(_ptr->sock, _ptr->send_buf, size, 0);
		if (retval == SOCKET_ERROR) // �۽� ���۰� ������
		{
			err_display("send()");
			_ptr->state = DISCONNECTED_STATE;
			return;
		}
	}

	WaitForSingleObject(game_info->start_event, INFINITE); // ���� ���� �̺�Ʈ(���� �̺�Ʈ)�� ��ȣ �ٶ����� ���

	/* ���� ���� ��ȣ ���� �� ---> */
	int size = Packing(_ptr->send_buf, INTRO, INTRO_MSG); // ��Ʈ�� �޽��� ��ŷ
	int retval = send(_ptr->sock, _ptr->send_buf, size, 0); // �޽��� ������
	if (retval == SOCKET_ERROR) // �۽� ���۰� ������
	{
		err_display("send()");
		_ptr->state = DISCONNECTED_STATE;
		return;
	}

	char msg[BUFSIZE]; // �޽��� ������ ����

	sprintf(msg, "����� %d�� Player �Դϴ�.\n", _ptr->player_number);

	size = Packing(_ptr->send_buf, PLAYER_INFO, msg); // ��� �÷��̾� ���� �˷��� �޽��� ��ŷ
	retval = send(_ptr->sock, _ptr->send_buf, size, 0);
	if (retval == SOCKET_ERROR) // �۽� ���۰� ������
	{
		err_display("send()");
		_ptr->state = DISCONNECTED_STATE;
		return;
	}

	if (_ptr->player_number == 1) // 1�� �÷��̾�
	{
		_ptr->state = CLIENT_TURN_STATE;
	}
	else // 1�� �÷��̾ �ƴ� �÷��̾�
	{
		_ptr->state = OTHER_TURN_STATE;
	}
}

/* ���� �����ص� �Ǵ��� Ȯ���ϴ� �Լ� */
bool GameContinueCheak(_ClientInfo* _ptr, int num_count)
{
	EnterCriticalSection(&cs); // ������ ��ȣ

	_GameInfo* game_info = _ptr->game_info; // Ŭ�� ���� ���� ���� ��ü ����

	if (num_count == GAME_NUMBER_SIZE) // 31�� �Ǹ� ���� ����
	{
		/* �й��� �÷��̾� */
		_ptr->result = LOSE; // Ŭ�� ���Ӱ�� �й�� ����
		_ptr->state = GAME_RESULT_STATE; // Ŭ�� ���Ӱ�� ���·� ���� 
		game_info->lose_player = _ptr; // �ش� Ŭ�� �ش� ������ lose�÷��̾�� ����
		game_info->state = G_GAME_OVER_STATE; // ���� ���� ���·� ����
		
		/* �¸��� ������ �÷��̾� */
		for (int i = 0; i < game_info->player_count; i++)
		{
			if (game_info->players[i] != _ptr) // �й��� �÷��̾ �ƴ� �÷��̾��
			{
				game_info->players[i]->result = WIN; // Ŭ�� ���Ӱ�� �¸��� ����
				SetEvent(game_info->players[i]->turn_event); // �̺�Ʈ �ڵ��� ��ȣ ���·� �����ؼ� �¸��� �÷��̾� ���� ���� ��� ���μ����� ���� �ֵ��� ����
			}
		}
		LeaveCriticalSection(&cs);
		return false;
	}
	LeaveCriticalSection(&cs);
	return true; // ���� ���ڰ� 31�� �ȵǾ����� ���� ����
}

/* ���� ������ Ŭ���̾�Ʈ �������ִ� �Լ� */
_ClientInfo* NextTurnClient(_ClientInfo* _ptr)
{
	int index; // ������ �÷��̾��� �ε���

	EnterCriticalSection(&cs);
	_GameInfo* game_info = _ptr->game_info; // �ش� �������� ��ü ����
	_ClientInfo* ptr = nullptr;

	/* �÷��̾� ���� ���� �ٸ��� ���� */
	switch (game_info->player_count)
	{
		/* �÷��̾ 3���� ���(�ƹ��� ������ ���� ���) */
	case 3: 
		index = _ptr->player_number % game_info->player_count; // 1 % 3 = 1 , 2 % 3 = 2, 3 % 3 = 0
		ptr = game_info->players[index];
		break;

		/* �÷��̾ 2���� ���(1���� �÷��̾ ���� ���) */
	case 2:
		for (int i = 0; i < game_info->player_count; i++) // 2�� �ݺ�
		{
			if (_ptr != game_info->players[i]) // ���� �÷��̾ �ƴ� �÷��̾�
			{
				index = i; // �ε����� �ٲ㼭 ���� ���� �÷��̾� ���� ���� �ѱ�
			}
		}
		ptr = game_info->players[index];
		break;
	}
	LeaveCriticalSection(&cs);
	return ptr; // ���� ���� Ŭ���̾�Ʈ ����
}

/* CLIENT_TURN ���μ��� */
void ClientTurnProcess(_ClientInfo* _ptr)
{
	_GameInfo* game_info = _ptr->game_info; // Ŭ�� ���� ���� ����

	/* �÷��̾ 1�� ������ ���(�������� x) */
	if (game_info->player_count == 1)
	{
		int size = Packing(_ptr->send_buf, GAME_CLOSE, GAME_CLOSE_MSG); // ���� �޽��� ��ŷ
		int retval = send(_ptr->sock, _ptr->send_buf, size, 0);
		if (retval == SOCKET_ERROR) // �۽� ���۰� ������
		{
			err_display("send()");
			_ptr->state = DISCONNECTED_STATE;
			return;
		}

		_ptr->state = DISCONNECTED_STATE;
		return;
	}

	if (!GameContinueCheak(_ptr, game_info->num_count + 1)) // ���� ������ �÷��̾ 30���� �Է��� ���
	{
		return;
	}

	int size = Packing(_ptr->send_buf, CLIENT_TURN, CLIENT_TURN_MSG); // �ڱ� ���ʶ�� �˷��ִ� �޽���
	int retval = send(_ptr->sock, _ptr->send_buf, size, 0); 
	if (retval == SOCKET_ERROR) // �۽� ���۰� ������
	{
		err_display("send()");
		_ptr->state = DISCONNECTED_STATE;
		return;
	}

	if (!PacketRecv(_ptr->sock, _ptr->recv_buf)) // ������ �ޱ�(Ŭ�� �Է��� ����)
	{
		_ptr->state = DISCONNECTED_STATE;
		return;
	}

	PROTOCOL protocol = GetProtocol(_ptr->recv_buf); // ���� ������ �������� �Ҵ�

	switch (protocol)
	{
	case SELECT_NUM:
		int client_num; // Ŭ�� ������ ���� 

		UnPacking(_ptr->recv_buf, client_num); // Ŭ�� ������ ���� ����ŷ�ؼ� ������ ����

		if (!ChecKDataRange(client_num)) // �Է� ������ ��� ��� ����ó��
		{
			int size = Packing(_ptr->send_buf, DATA_ERROR, DATA_RANGE_ERROR, DATA_RANGE_ERROR_MSG); // ������ ����ٰ� �˷��ִ� �޽��� ��ŷ
			int retval = send(_ptr->sock, _ptr->send_buf, size, 0); // ������ ����ٰ� �޽��� ����
			if (retval == SOCKET_ERROR)
			{
				err_display("send()");
				_ptr->state = DISCONNECTED_STATE;
				return;
			}
			return;
		}
	
		for (int i = game_info->num_count; i < game_info->num_count + client_num; i++)
		{
			game_info->currect_num[i] = i + 1; // �迭�� �Է��� ����ŭ ����
		}

		char msg[BUFSIZE];
		ZeroMemory(msg, sizeof(msg));

		switch (client_num)
		{
		case 1:
			sprintf(msg, "%d�� Player�� ������ ���ڴ�\t%d\t�Դϴ�.\n", _ptr->player_number, game_info->currect_num[game_info->num_count]);
			break;
		case 2:
			sprintf(msg, "%d�� Player�� ������ ���ڴ�\t%d\t%d\t�Դϴ�.\n", _ptr->player_number, game_info->currect_num[game_info->num_count]
				, game_info->currect_num[game_info->num_count + 1]);
			break;
		case 3:
			sprintf(msg, "%d�� Player�� ������ ���ڴ�\t%d\t%d\t%d\t�Դϴ�.\n", _ptr->player_number, game_info->currect_num[game_info->num_count]
				, game_info->currect_num[game_info->num_count + 1]
				, game_info->currect_num[game_info->num_count + 2]);
			break;
		}
	
		game_info->num_count += client_num; // Ŭ�� ������ ����ŭ ī��Ʈ

		if (!GameContinueCheak(_ptr, game_info->num_count)) // ���� ��� �̾�� �Ǵ��� Ȯ��
		{
			return;
		}

		int size = Packing(_ptr->send_buf, COUNT_VALUE, msg); // Ŭ�� �Է��� �� ��ŷ
		for (int i = 0; i < game_info->player_count; i++)
		{
			int retval = send(game_info->players[i]->sock, _ptr->send_buf, size, 0); // ���ӿ� ������ Ŭ��鿡�� ���� �޽��� ������
			if (retval == SOCKET_ERROR)
			{
				if (game_info->players[i]->sock == INVALID_SOCKET) // ������ ���� Ŭ��(������ Ŭ���̾�Ʈ)�� ã�Ƽ� ���� ���� ���·� ����
				{
					err_display("send()");
					game_info->players[i]->state = DISCONNECTED_STATE; // ������ Ŭ�� ���� ����
					return;
				}
			}
		}
	}
	_ClientInfo* next_ptr = NextTurnClient(_ptr); // ���� ������ Ŭ���̾�Ʈ �Ҵ�
	if (next_ptr == nullptr)
	{
		return;
	}
	SetEvent(next_ptr->turn_event); // ���� ���� Ŭ���̾�Ʈ�� �̺�Ʈ ��ȣ���·� ����

	_ptr->state = OTHER_TURN_STATE;
}

/* �ڱ� ���ʰ� �ƴҶ� */
void OtherTurnProcess(_ClientInfo* _ptr)
{
	WaitForSingleObject(_ptr->turn_event, INFINITE); // �̺�Ʈ ��ȣ ���(�ڵ� �̺�Ʈ): �ڱ� ���ʰ� �ö����� ��� Ű�� �ٷ� ����

	_GameInfo* game_info = _ptr->game_info; // �ش� Ŭ�� ���� ���� ���� �Ҵ�
	
	switch (game_info->state)
	{
		/* ���� ������ */
	case G_PLAYING_STATE:
		game_info->cur_player = _ptr; // ���� �÷��̾ �ش� Ŭ��� ����
		_ptr->state = CLIENT_TURN_STATE; // �ش� Ŭ���� ������ ����
		break;

		/* ���� ���� */
	case G_GAME_OVER_STATE:
		_ptr->state = GAME_RESULT_STATE; // Ŭ�� ���� ���Ӱ�� ���·� ����
		break;
	}

}
/* ���� ���  */
void GameResultProcess(_ClientInfo* _ptr)
{
	int size;
	int retval;
	_GameInfo* game_info = _ptr->game_info; // Ŭ�� ���� ���� ���� �Ҵ�

	switch (_ptr->result) 
	{
		/* �¸� */
	case WIN: 
		char msg[BUFSIZE]; // �޽��� ������ ����

		sprintf(msg, "%d%s", game_info->lose_player->player_number, WIN_MSG); // ������ ���� �޽��� ����

		size = Packing(_ptr->send_buf, GAME_RESULT, WIN, msg); // ���ۿ� �¸� �޽��� ��ŷ
		break;

		/* �й� */
	case LOSE: 
		size = Packing(_ptr->send_buf, GAME_RESULT, LOSE, LOSE_MSG); // ���ۿ� �й� �޽��� ��ŷ
		
		break;
	}

	retval = send(_ptr->sock, _ptr->send_buf, size, 0); // ��ŷ�� �޽��� Ŭ��� ������
	if (retval == SOCKET_ERROR)
	{
		err_display("send()");
		_ptr->state = DISCONNECTED_STATE;
		return;
	}

	// Ŭ�� ���� ���
	if (!PacketRecv(_ptr->sock, _ptr->recv_buf))
	{
		_ptr->state = DISCONNECTED_STATE;
		return;
	}

}

/* ���� ���� ���μ��� */
void DisConnectedProcess(_ClientInfo* _ptr)
{
	EscapePlayer(_ptr); // ���� �÷��̾�鿡�� �޽��� ����, �� ����
	ReMoveGamePlayer(_ptr); // ������ Ŭ���̾�Ʈ�� ���ӿ��� ����
	ReMoveClientInfo(_ptr); // ������ Ŭ���̾�Ʈ ���� ����
}

/* ������ �Լ� */
DWORD WINAPI ProcessClient(LPVOID arg)
{
	_ClientInfo* ptr = (_ClientInfo*)arg; // ������ Ŭ���̾�Ʈ ����
	bool endflag = false;

	while (1)
	{
		switch (ptr->state) // Ŭ���̾�Ʈ�� ����
		{
		case INIT_STATE:
			InitProcess(ptr); 
			break;
		case CLIENT_TURN_STATE:
			ClientTurnProcess(ptr);
			break;
		case OTHER_TURN_STATE:
			OtherTurnProcess(ptr);
			break;
		case GAME_RESULT_STATE:
			GameResultProcess(ptr);
			break;
		case DISCONNECTED_STATE:
			DisConnectedProcess(ptr);
			endflag = true;
			break;
		}

		if (endflag)
		{
			break;
		}
	}
	
	return 0;
}

int main(int argc, char* argv[])
{
	InitializeCriticalSection(&cs); // �ʱ�ȭ

	int retval;

	// ���� �ʱ�ȭ
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return 1;

	// socket()
	SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_sock == INVALID_SOCKET) err_quit("socket()");

	// bind()
	SOCKADDR_IN serveraddr;
	ZeroMemory(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons(SERVERPORT);
	retval = bind(listen_sock, (SOCKADDR*)&serveraddr, sizeof(serveraddr));
	if (retval == SOCKET_ERROR) err_quit("bind()");

	// listen()
	retval = listen(listen_sock, SOMAXCONN);
	if (retval == SOCKET_ERROR) err_quit("listen()");

	// ������ ��ſ� ����� ����
	SOCKET client_sock;
	SOCKADDR_IN clientaddr;
	int addrlen;

	while (1)
	{
		// accept()
		addrlen = sizeof(clientaddr);
		client_sock = accept(listen_sock, (SOCKADDR*)&clientaddr, &addrlen);
		if (client_sock == INVALID_SOCKET)
		{
			err_display("accept()");
			break;
		}

		_ClientInfo* ClientPtr = AddClientInfo(client_sock, clientaddr);

		HANDLE hThread = CreateThread(NULL, 0, ProcessClient, ClientPtr, 0, NULL); // ������ ����
		if (hThread == NULL) // ���� ó��
		{
			ReMoveClientInfo(ClientPtr);
			continue;
		}
		else
		{
			CloseHandle(hThread);
		}
	}
	// closesocket()
	closesocket(listen_sock);

	DeleteCriticalSection(&cs); // ũ��Ƽ�ü��� ����

	// ���� ����
	WSACleanup();
	return 0;
}