#pragma comment(lib, "ws2_32")
#include <winsock2.h>
#include <stdlib.h>
#include <stdio.h>
#include <ws2tcpip.h> 
#include <time.h>

/* 기호상수 정의 */
#define SERVERPORT 9000
#define BUFSIZE    4096
#define CLIENT_COUNT 100 // 접속 가능한 클라이언트 수
#define PLAYER_COUNT 3 // 한 게임에 참여 가능한 개수

#define GAME_NUMBER_SIZE 31

#define WAIT_MSG "다른 유저가 접속 중입니다. 잠시만 기다려 주세요.\n"
#define INTRO_MSG "31 숫자게임 입니다. 1부터 시작합니다. \n당신차례를 기다려 주세요.\n"
#define CLIENT_TURN_MSG "당신차례입니다. 한번에 3개까지 선택할 수 있습니다. 선택하세요:"
#define DATA_RANGE_ERROR_MSG "잘못 선택했습니다. 1~3개까지 선택가능합니다.\n"
#define GAME_ESCAPE_MSG "번 Player가 나갔습니다.\n"
#define GAME_CLOSE_MSG "상대할 플레이어가 없습니다. 게임을 종료하겠습니다.\n"
#define WIN_MSG "번 Player가 졌습니다. 승리 하셨습니다.\n"
#define LOSE_MSG "당신이 졌습니다.\n"

CRITICAL_SECTION cs;

/* 클라이언트 상태 */
enum CLIENT_STATE
{
	INIT_STATE = 1,
	CLIENT_TURN_STATE, // 클라이언트 턴
	OTHER_TURN_STATE, // 다른 플레이어 턴
	GAME_RESULT_STATE, // 게임 종료
	DISCONNECTED_STATE // 연결 종료
};

/* 게임 결과 */
enum RESULT_VALUE
{
	INIT = -1,
	WIN = 1,  // 승리
	LOSE  // 패배
};

/* 프로토콜 */
enum PROTOCOL
{
	WAIT = 1, // 다른 유저를 기다릴때 사용할 프로토콜
	INTRO, // 인트로에 사용할 프로토콜
	PLAYER_INFO, // 몇번 플레이어인지 알려줄때 사용할 프로토콜
	SELECT_NUM, // 클라가 숫자 선택해서 보낼때 사용할 프로토콜
	COUNT_VALUE, // 클라가 입력한 숫자를 출력해서 보낼때 사용할 프로토콜
	CLIENT_TURN, // 클라이언트 차례를 알려줄때 사용할 프로토콜
	PLAYER_ESCAPE, // 플레이어가 중간에 나갔을 때 보낼 프로토콜
	GAME_CLOSE, // 게임 도중 플레이어가 나가서 종료할때 보낼 프로토콜
	DATA_ERROR, // 에러일때 보낼 프로토콜 
	GAME_RESULT // 게임 종료시 서버에서 클라에게 보낼 프로토콜
};

/* 에러 코드 */
enum ERROR_CODE
{
	DATA_RANGE_ERROR = 1 // 범위 에러
};

/* 게임 상태 */
enum GAME_STATE
{
	G_WAIT_STATE = 1, // 대기 상태
	G_PLAYING_STATE, // 게임 진행중인 상태
	G_GAME_OVER_STATE // 게임이 끝난 상태
};

struct _ClientInfo; // 클라정보 구조체 원형

/* 게임 정보 구조체(클라 그룹 정보) */
struct _GameInfo
{
	int num_count; // 현재 숫자 몇까지 진행된지 저장할 변수
	int currect_num[GAME_NUMBER_SIZE]; // 진행된 숫자까지 저장할 배열

	HANDLE start_event; // 게임 시작 이벤트(수동)
	GAME_STATE state; // 게임 상태 조절용

	_ClientInfo* players[PLAYER_COUNT]; // 게임에 참여할 클라이언트(PLAYER_COUNT만큼 참여가능)
	_ClientInfo* cur_player; // 현재 차례인 플레이어
	_ClientInfo* lose_player; // 게임을 진 플레이어

	int player_count; // 게임 플레이 인원이 가득 찼는지 확인할 변수
	bool full; // 플레이 인원이 가득 차면 게임시작을 제어할 변수
};

/* 클라정보 구조체 */
struct _ClientInfo
{
	SOCKET sock; // 클라 소켓
	SOCKADDR_IN addr; // 클라 주소

	CLIENT_STATE state; // 클라 상태 조절용
	
	HANDLE turn_event; // 자기 차례 기다리는 이벤트
	_GameInfo* game_info; // 자기가 속해진 게임정보
	int player_number; // 속한 게임의 몇번 플레이어인지 저장할 변수
	RESULT_VALUE result; // 게임 결과

	char recv_buf[BUFSIZE]; // 받을때 사용할 작업대역할
	char send_buf[BUFSIZE]; // 보낼때 사용할 작업대역할
};

_GameInfo* GameInfo[CLIENT_COUNT]; // 게임 정보가 저장되어 있는 구조체 배열
int GameCount = 0; // 게임에 참여한 클라이언트 수

_ClientInfo* ClientInfo[CLIENT_COUNT]; // 클라 정보가 저장되어 있는 구조체 배열
int Count = 0; // 접속 클라이언트 수

/* 함수 원형 */
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

// 소켓 함수 오류 출력 후 종료
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

// 소켓 함수 오류 출력
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

/* 클라 정보 추가 */
_ClientInfo* AddClientInfo(SOCKET sock, SOCKADDR_IN addr)
{
	EnterCriticalSection(&cs); // 데이터 보호

	/* 접속 클라이언트 초기값 설정 */
	_ClientInfo* ptr = new _ClientInfo;
	ZeroMemory(ptr, sizeof(_ClientInfo));
	ptr->sock = sock;
	memcpy(&ptr->addr, &addr, sizeof(addr));
	ptr->state = INIT_STATE;
	ptr->result = INIT;
	ptr->player_number = INIT;

	ptr->turn_event = CreateEvent(nullptr, false/* 자동 */, false/* 비신호 상태 */, nullptr); //이벤트 생성 

	ClientInfo[Count++] = ptr;
	
	LeaveCriticalSection(&cs);

	printf("\n[TCP 서버] 클라이언트 접속: IP 주소=%s, 포트 번호=%d\n",
		inet_ntoa(ptr->addr.sin_addr), ntohs(ptr->addr.sin_port));

	return ptr;
}

/* 클라 정보 삭제 */
void ReMoveClientInfo(_ClientInfo* ptr)
{
	printf("\n[TCP 서버] 클라이언트 종료: IP 주소=%s, 포트 번호=%d\n",
		inet_ntoa(ptr->addr.sin_addr), ntohs(ptr->addr.sin_port));

	EnterCriticalSection(&cs);
	for (int i = 0; i < Count; i++)
	{
		if (ClientInfo[i] == ptr) // 클라 정보 배열에서 반복을 통해 해당 클라를 찾아서
		{
			closesocket(ptr->sock); // 클라 소켓 종료
			CloseHandle(ptr->turn_event); // 클라의 이벤트 핸들 종료
			delete ptr; // 메모리 해제
			for (int j = i; j < Count - 1; j++)
			{
				ClientInfo[j] = ClientInfo[j + 1]; // 삭제된 인덱스 기준으로 앞당겨 저장
			}
			break;
		}
	}

	Count--; // 클라 정보 개수 -1
	LeaveCriticalSection(&cs);
}

/* 게임 정보 추가(초기값 설정, 초기화) */
_GameInfo* AddGameInfo(_ClientInfo* _ptr)
{
	EnterCriticalSection(&cs); // 데이터 보호
	_GameInfo* game_ptr = nullptr; // 객체 생성
	int index = INIT;

	for (int i = 0; i < GameCount; i++)
	{
		if (!GameInfo[i]->full) // 게임 인원이 가득차지 않았을 경우
		{
			GameInfo[i]->players[GameInfo[i]->player_count++] = _ptr; // 들어오는 클라이언트 순서대로 저장
			_ptr->game_info = GameInfo[i]; // 속한 게임 정보 저장
			_ptr->player_number = GameInfo[i]->player_count; // 플레이어 번호 지정
			if (GameInfo[i]->player_count == PLAYER_COUNT) // 게임 인원이 가득 차면
			{
				GameInfo[i]->full = true; 
				GameInfo[i]->state = G_PLAYING_STATE; // 게임 시작 상태로 변경
				GameInfo[i]->cur_player = GameInfo[i]->players[0]; // 1번 플레이어부터 시작
				SetEvent(GameInfo[i]->start_event); // 이벤트 신호 상태로 변경
			}
			game_ptr = GameInfo[i];
			index = i;
			break;
		}
	}

	/* 제일 먼저 들어온 클라이언트(1번 플레이어) */
	if (index == INIT)
	{
		game_ptr = new _GameInfo;
		ZeroMemory(game_ptr, sizeof(_GameInfo)); // 게임정보 구조체 포인터 초기화
		
		game_ptr->full = false;
		game_ptr->start_event = CreateEvent(nullptr, true/* 수동 */, false/* 비신호 */, nullptr); // 게임 시작 이벤트 생성
		game_ptr->players[0] = _ptr; // 제일 먼저 들어온 클라이언트 저장
		/* 게임 시작전 변수들 초기화 */
		ZeroMemory(game_ptr->currect_num, sizeof(game_ptr->currect_num));
		game_ptr->num_count = INIT;
		game_ptr->cur_player = nullptr;
		game_ptr->lose_player = nullptr;

		game_ptr->player_count++; // 게임 참여 플레이어 +1
		game_ptr->state = G_WAIT_STATE; // 게임 시작 대기 상태
		GameInfo[GameCount++] = game_ptr; // 게임 정보 배열에 저장, GameCount +1

		_ptr->game_info = game_ptr; // 속한 게임 정보 저장
		_ptr->player_number = game_ptr->player_count; // 플레이어 번호 지정
	}

	LeaveCriticalSection(&cs);

	return game_ptr;
}

/* 게임 정보 삭제 */
void ReMoveGameInfo(_GameInfo* _ptr)
{
	EnterCriticalSection(&cs);
	for (int i = 0; i < GameCount; i++)
	{
		if (GameInfo[i] == _ptr) // 게임 정보 배열에서 반복을 통해 해당 게임을 찾아서
		{
			CloseHandle(_ptr->start_event); // 해당 게임 이벤트 핸들 종료
			delete _ptr; // 해당 게임 정보 메모리 해제
			for (int j = i; j < GameCount - 1; j++)
			{
				GameInfo[j] = GameInfo[j + 1]; // 삭제된 인덱스 기준으로 앞당겨 저장
			}
		}
	}
	GameCount--; // 게임 정보 -1
	LeaveCriticalSection(&cs);
}

/* 게임에 속한 플레이어(클라) 삭제 */
void ReMoveGamePlayer(_ClientInfo* _ptr)
{
	EnterCriticalSection(&cs);
	_GameInfo* game_info = _ptr->game_info; // 클라가 속한 게임정보 할당

	for (int i = 0; i < game_info->player_count; i++) // 해당 게임의 플레이어 개수만큼 반복
	{
		if (game_info->players[i] == _ptr) // 반복을 통해 플레이어(클라)를 찾아서
		{
			for (int j = i; j < game_info->player_count - 1; j++)
			{
				game_info->players[j] = game_info->players[j + 1]; // 삭제된 인덱스 기준으로 앞당겨 저장
			}
			game_info->player_count--; // 플레이어수 -1

			if (game_info->player_count == 0)
			{
				ReMoveGameInfo(game_info); // 게임의 플레이어가 0명이 되면 해당 게임정보 삭제 
			}
		}
	}
	LeaveCriticalSection(&cs);
}

/* 플레이어가 게임 도중 나갔을 때 처리할 함수 */
void EscapePlayer(_ClientInfo* _ptr)
{
	char msg[BUFSIZE]; // 메시지 저장할 변수
	_GameInfo* game_info = _ptr->game_info; // 게임에서 나간 플레이어가 속한 게임정보 객체 생성

	if (game_info->player_count >= 2) // 플레이어가 2명 이상일때만
	{
		for (int i = 0; i < game_info->player_count; i++)
		{
			if (_ptr != game_info->players[i]) // 남은 플레이어 들에게 메시지 보내기
			{
				sprintf(msg, "%d%s", _ptr->player_number, GAME_ESCAPE_MSG);

				int size = Packing(_ptr->send_buf, PLAYER_ESCAPE, msg);
				int retval = send(game_info->players[i]->sock, _ptr->send_buf, size, 0); // 패킹된 메시지 클라로 보내기
				if (retval == SOCKET_ERROR)
				{
					if (game_info->players[i]->sock == INVALID_SOCKET) // 소켓이 없는 클라(종료한 클라이언트)를 찾아서 연결 종료 상태로 변경
					{
						err_display("send()");
						game_info->players[i]->state = DISCONNECTED_STATE;
						return;
					}
				}
			}
		}
	
		/* 현재 턴인 플레이어가 나갔을 경우에만 턴 변경 (현재 턴이 아닌 플레이어가 나갔을 경우에는 턴 변경 x) */
		if (_ptr == game_info->cur_player) 
		{
			_ClientInfo* next_ptr = NextTurnClient(_ptr); // 나갔으므로 다음 차례 플레이어 턴으로 변경
			SetEvent(next_ptr->turn_event); // 이벤트 신호 상태로 변경(자동:켯다 꺼짐)
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

/* recvn 2회 */
bool PacketRecv(SOCKET _sock, char* _buf)
{
	int size;

	// 용량 
	int retval = recvn(_sock, (char*)&size, sizeof(size), 0);
	// closesocket 호출 없이 종료 (강제 종료) = SOCKET_ERROR
	if (retval == SOCKET_ERROR)
	{
		err_display("gvalue recv error()");
		return false;
	}
	// (상대가)closesocket 호출 하여 종료 (정상 종료) 할시 return 0
	else if (retval == 0)
	{
		return false;
	}

	// 용량만큼 데이터 받아옴
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

/* 데이터 받은 프로토콜 분리하는 부분 */
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

/* (클라로부터)받은 데이터 범위 검사 */
bool ChecKDataRange(int _data)
{

	if (_data < 1 || _data > 3) // 1~3
	{
		return false;
	}

	return true;
}

/* INIT_STATE 프로세스 */
void InitProcess(_ClientInfo* _ptr)
{
	_GameInfo* game_info = AddGameInfo(_ptr);

	/* 대기 상태 */
	if (game_info->state == G_WAIT_STATE)
	{
		int size = Packing(_ptr->send_buf, WAIT, WAIT_MSG); // 대기 메시지 패킹

		int retval = send(_ptr->sock, _ptr->send_buf, size, 0);
		if (retval == SOCKET_ERROR) // 송신 버퍼가 없을때
		{
			err_display("send()");
			_ptr->state = DISCONNECTED_STATE;
			return;
		}
	}

	WaitForSingleObject(game_info->start_event, INFINITE); // 게임 시작 이벤트(수동 이벤트)가 신호 줄때까지 대기

	/* 게임 시작 신호 받은 후 ---> */
	int size = Packing(_ptr->send_buf, INTRO, INTRO_MSG); // 인트로 메시지 패킹
	int retval = send(_ptr->sock, _ptr->send_buf, size, 0); // 메시지 보내기
	if (retval == SOCKET_ERROR) // 송신 버퍼가 없을때
	{
		err_display("send()");
		_ptr->state = DISCONNECTED_STATE;
		return;
	}

	char msg[BUFSIZE]; // 메시지 저장할 변수

	sprintf(msg, "당신은 %d번 Player 입니다.\n", _ptr->player_number);

	size = Packing(_ptr->send_buf, PLAYER_INFO, msg); // 몇번 플레이어 인지 알려줄 메시지 패킹
	retval = send(_ptr->sock, _ptr->send_buf, size, 0);
	if (retval == SOCKET_ERROR) // 송신 버퍼가 없을때
	{
		err_display("send()");
		_ptr->state = DISCONNECTED_STATE;
		return;
	}

	if (_ptr->player_number == 1) // 1번 플레이어
	{
		_ptr->state = CLIENT_TURN_STATE;
	}
	else // 1번 플레이어가 아닌 플레이어
	{
		_ptr->state = OTHER_TURN_STATE;
	}
}

/* 게임 진행해도 되는지 확인하는 함수 */
bool GameContinueCheak(_ClientInfo* _ptr, int num_count)
{
	EnterCriticalSection(&cs); // 데이터 보호

	_GameInfo* game_info = _ptr->game_info; // 클라가 속한 게임 정보 객체 생성

	if (num_count == GAME_NUMBER_SIZE) // 31이 되면 게임 종료
	{
		/* 패배한 플레이어 */
		_ptr->result = LOSE; // 클라 게임결과 패배로 변경
		_ptr->state = GAME_RESULT_STATE; // 클라 게임결과 상태로 변경 
		game_info->lose_player = _ptr; // 해당 클라를 해당 게임의 lose플레이어로 저장
		game_info->state = G_GAME_OVER_STATE; // 게임 종료 상태로 변경
		
		/* 승리한 나머지 플레이어 */
		for (int i = 0; i < game_info->player_count; i++)
		{
			if (game_info->players[i] != _ptr) // 패배한 플레이어가 아닌 플레이어들
			{
				game_info->players[i]->result = WIN; // 클라 게임결과 승리로 변경
				SetEvent(game_info->players[i]->turn_event); // 이벤트 핸들을 신호 상태로 변경해서 승리한 플레이어 들이 게임 결과 프로세스로 갈수 있도록 설정
			}
		}
		LeaveCriticalSection(&cs);
		return false;
	}
	LeaveCriticalSection(&cs);
	return true; // 아직 숫자가 31이 안되었으면 게임 진행
}

/* 다음 차례의 클라이언트 지정해주는 함수 */
_ClientInfo* NextTurnClient(_ClientInfo* _ptr)
{
	int index; // 리턴할 플레이어의 인덱스

	EnterCriticalSection(&cs);
	_GameInfo* game_info = _ptr->game_info; // 해당 게임정보 객체 생성
	_ClientInfo* ptr = nullptr;

	/* 플레이어 수에 따라 다르게 설정 */
	switch (game_info->player_count)
	{
		/* 플레이어가 3명인 경우(아무도 나가지 않은 경우) */
	case 3: 
		index = _ptr->player_number % game_info->player_count; // 1 % 3 = 1 , 2 % 3 = 2, 3 % 3 = 0
		ptr = game_info->players[index];
		break;

		/* 플레이어가 2명인 경우(1명의 플레이어가 나간 경우) */
	case 2:
		for (int i = 0; i < game_info->player_count; i++) // 2번 반복
		{
			if (_ptr != game_info->players[i]) // 현재 플레이어가 아닌 플레이어
			{
				index = i; // 인덱스값 바꿔서 다음 차례 플레이어 에게 턴을 넘김
			}
		}
		ptr = game_info->players[index];
		break;
	}
	LeaveCriticalSection(&cs);
	return ptr; // 다음 차례 클라이언트 리턴
}

/* CLIENT_TURN 프로세스 */
void ClientTurnProcess(_ClientInfo* _ptr)
{
	_GameInfo* game_info = _ptr->game_info; // 클라가 속한 게임 정보

	/* 플레이어가 1명만 남았을 경우(게임진행 x) */
	if (game_info->player_count == 1)
	{
		int size = Packing(_ptr->send_buf, GAME_CLOSE, GAME_CLOSE_MSG); // 종료 메시지 패킹
		int retval = send(_ptr->sock, _ptr->send_buf, size, 0);
		if (retval == SOCKET_ERROR) // 송신 버퍼가 없을때
		{
			err_display("send()");
			_ptr->state = DISCONNECTED_STATE;
			return;
		}

		_ptr->state = DISCONNECTED_STATE;
		return;
	}

	if (!GameContinueCheak(_ptr, game_info->num_count + 1)) // 이전 차례의 플레이어가 30까지 입력한 경우
	{
		return;
	}

	int size = Packing(_ptr->send_buf, CLIENT_TURN, CLIENT_TURN_MSG); // 자기 차례라고 알려주는 메시지
	int retval = send(_ptr->sock, _ptr->send_buf, size, 0); 
	if (retval == SOCKET_ERROR) // 송신 버퍼가 없을때
	{
		err_display("send()");
		_ptr->state = DISCONNECTED_STATE;
		return;
	}

	if (!PacketRecv(_ptr->sock, _ptr->recv_buf)) // 데이터 받기(클라가 입력한 숫자)
	{
		_ptr->state = DISCONNECTED_STATE;
		return;
	}

	PROTOCOL protocol = GetProtocol(_ptr->recv_buf); // 받은 데이터 프로토콜 할당

	switch (protocol)
	{
	case SELECT_NUM:
		int client_num; // 클라가 선택한 숫자 

		UnPacking(_ptr->recv_buf, client_num); // 클라가 선택한 숫자 언패킹해서 변수에 저장

		if (!ChecKDataRange(client_num)) // 입력 범위를 벗어난 경우 예외처리
		{
			int size = Packing(_ptr->send_buf, DATA_ERROR, DATA_RANGE_ERROR, DATA_RANGE_ERROR_MSG); // 범위를 벗어났다고 알려주는 메시지 패킹
			int retval = send(_ptr->sock, _ptr->send_buf, size, 0); // 범위를 벗어났다고 메시지 보냄
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
			game_info->currect_num[i] = i + 1; // 배열에 입력한 수만큼 저장
		}

		char msg[BUFSIZE];
		ZeroMemory(msg, sizeof(msg));

		switch (client_num)
		{
		case 1:
			sprintf(msg, "%d번 Player가 선택한 숫자는\t%d\t입니다.\n", _ptr->player_number, game_info->currect_num[game_info->num_count]);
			break;
		case 2:
			sprintf(msg, "%d번 Player가 선택한 숫자는\t%d\t%d\t입니다.\n", _ptr->player_number, game_info->currect_num[game_info->num_count]
				, game_info->currect_num[game_info->num_count + 1]);
			break;
		case 3:
			sprintf(msg, "%d번 Player가 선택한 숫자는\t%d\t%d\t%d\t입니다.\n", _ptr->player_number, game_info->currect_num[game_info->num_count]
				, game_info->currect_num[game_info->num_count + 1]
				, game_info->currect_num[game_info->num_count + 2]);
			break;
		}
	
		game_info->num_count += client_num; // 클라가 선택한 수만큼 카운트

		if (!GameContinueCheak(_ptr, game_info->num_count)) // 게임 계속 이어가도 되는지 확인
		{
			return;
		}

		int size = Packing(_ptr->send_buf, COUNT_VALUE, msg); // 클라가 입력한 값 패킹
		for (int i = 0; i < game_info->player_count; i++)
		{
			int retval = send(game_info->players[i]->sock, _ptr->send_buf, size, 0); // 게임에 참여한 클라들에게 각각 메시지 보내기
			if (retval == SOCKET_ERROR)
			{
				if (game_info->players[i]->sock == INVALID_SOCKET) // 소켓이 없는 클라(종료한 클라이언트)를 찾아서 연결 종료 상태로 변경
				{
					err_display("send()");
					game_info->players[i]->state = DISCONNECTED_STATE; // 종료한 클라 상태 변경
					return;
				}
			}
		}
	}
	_ClientInfo* next_ptr = NextTurnClient(_ptr); // 다음 차례의 클라이언트 할당
	if (next_ptr == nullptr)
	{
		return;
	}
	SetEvent(next_ptr->turn_event); // 다음 차례 클라이언트의 이벤트 신호상태로 변경

	_ptr->state = OTHER_TURN_STATE;
}

/* 자기 차례가 아닐때 */
void OtherTurnProcess(_ClientInfo* _ptr)
{
	WaitForSingleObject(_ptr->turn_event, INFINITE); // 이벤트 신호 대기(자동 이벤트): 자기 차례가 올때까지 대기 키고 바로 꺼짐

	_GameInfo* game_info = _ptr->game_info; // 해당 클라가 속한 게임 정보 할당
	
	switch (game_info->state)
	{
		/* 게임 진행중 */
	case G_PLAYING_STATE:
		game_info->cur_player = _ptr; // 현재 플레이어를 해당 클라로 지정
		_ptr->state = CLIENT_TURN_STATE; // 해당 클라의 턴으로 변경
		break;

		/* 게임 종료 */
	case G_GAME_OVER_STATE:
		_ptr->state = GAME_RESULT_STATE; // 클라 상태 게임결과 상태로 변경
		break;
	}

}
/* 게임 결과  */
void GameResultProcess(_ClientInfo* _ptr)
{
	int size;
	int retval;
	_GameInfo* game_info = _ptr->game_info; // 클라가 속한 게임 정보 할당

	switch (_ptr->result) 
	{
		/* 승리 */
	case WIN: 
		char msg[BUFSIZE]; // 메시지 저장할 변수

		sprintf(msg, "%d%s", game_info->lose_player->player_number, WIN_MSG); // 변수에 보낼 메시지 저장

		size = Packing(_ptr->send_buf, GAME_RESULT, WIN, msg); // 버퍼에 승리 메시지 패킹
		break;

		/* 패배 */
	case LOSE: 
		size = Packing(_ptr->send_buf, GAME_RESULT, LOSE, LOSE_MSG); // 버퍼에 패배 메시지 패킹
		
		break;
	}

	retval = send(_ptr->sock, _ptr->send_buf, size, 0); // 패킹된 메시지 클라로 보내기
	if (retval == SOCKET_ERROR)
	{
		err_display("send()");
		_ptr->state = DISCONNECTED_STATE;
		return;
	}

	// 클라 종료 대기
	if (!PacketRecv(_ptr->sock, _ptr->recv_buf))
	{
		_ptr->state = DISCONNECTED_STATE;
		return;
	}

}

/* 연결 종료 프로세스 */
void DisConnectedProcess(_ClientInfo* _ptr)
{
	EscapePlayer(_ptr); // 남은 플레이어들에게 메시지 전송, 턴 조절
	ReMoveGamePlayer(_ptr); // 종료한 클라이언트를 게임에서 삭제
	ReMoveClientInfo(_ptr); // 종료한 클라이언트 정보 삭제
}

/* 스레드 함수 */
DWORD WINAPI ProcessClient(LPVOID arg)
{
	_ClientInfo* ptr = (_ClientInfo*)arg; // 접속한 클라이언트 정보
	bool endflag = false;

	while (1)
	{
		switch (ptr->state) // 클라이언트의 상태
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
	InitializeCriticalSection(&cs); // 초기화

	int retval;

	// 윈속 초기화
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

	// 데이터 통신에 사용할 변수
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

		HANDLE hThread = CreateThread(NULL, 0, ProcessClient, ClientPtr, 0, NULL); // 스레드 생성
		if (hThread == NULL) // 에러 처리
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

	DeleteCriticalSection(&cs); // 크리티컬섹션 삭제

	// 윈속 종료
	WSACleanup();
	return 0;
}