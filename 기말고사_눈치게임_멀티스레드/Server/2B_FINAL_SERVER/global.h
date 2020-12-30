#pragma once
#pragma comment(lib, "ws2_32.lib")

#include <stdio.h>
#include <string.h>
#include <winsock2.h>

#define SERVERIP "127.0.0.1"
#define SERVERPORT 9000

#define BUFSIZE 4096   // 버프 배열 크기
#define NICKNAMESIZE 255 // 닉네임 배열 크기

#define GAME_MAX_COUNT 10 // 게임 최대 개수
#define CLIENT_MAX_COUNT 100 // 최대 클라이언트 개수
#define MAX_USER 5 // 게임에 들어갈 수 있는 최대 인원
#define SAME_TIMEING 500 // 동시라고 판단할 시간 

#define NODATA -1
#define INIT 0

#define INTRO_MSG "눈치 게임 프로그램입니다. 닉네임을 입력하세요"
#define NICKNAME_ERROR_MSG "이미있는 닉네임입니다. 다른 닉네임을 입력해 주세요."
#define WAIT_MSG "다른 유저가 접속 중입니다. 잠시만 기다려 주세요..."
#define GAME_START_MSG "눈치 게임을 시작합니다... 1부터 5까지 차례대로 보내주세요."
#define RANGE_ERROR_MSG "잘못 입력 하셨습니다. 1부터 5사이의 숫자를 입력해주세요..."
#define GAME_WIN_MSG "승리 하셨습니다... "
#define GAME_LOSE_MSG "패배 하셨습니다... "

/* 클라이언트 상태 */
enum STATE
{
	INITE_STATE,
	INTRO_STATE, // 인트로
	WAIT_STATE, // 게임 시작 대기 상태
	GAME_PLAY_STATE, // 게임 진행
	GAME_RESULT_STATE, // 게임 결과
	DISCONNECT_STATE // 연결 종료
};

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

struct _GameInfo;

/* 클라 정보 */
struct _ClientInfo
{
	SOCKET sock;
	SOCKADDR_IN clientaddr;
	STATE state; // 클라 상태

	HANDLE timeing_event; // 동시에 보내는것을 확인할 이벤트
	_GameInfo* game_info; // 클라가 속한 게임 정보
	char nickname[NICKNAMESIZE]; // 클라 닉네임
	int number; // 클라가 보낸 숫자
	bool ISanswer; // 2번 보내지 못하게 예외처리용

	char  sendbuf[BUFSIZE];
	char  recvbuf[BUFSIZE];
};

/* 게임 정보 */
struct _GameInfo
{
	HANDLE start_event; // 게임 시작용 이벤트
	HANDLE result_event; // 게임 결과용 이벤트
	HANDLE timing_event; // 동시에 보내는것을 확인할 이벤트
	int same_time_count = 0; // 동시에 입력한 클라 수
	
	_ClientInfo* User[MAX_USER]; // 게임에 들어온 클라이언트

	_ClientInfo* LoseUser[MAX_USER]; // 게임에서 패배한 유저
	int LoseUser_Count = 0; // 패배한 유저 수
	_ClientInfo* TempCliArr[MAX_USER]; // 유저 배열과 패배한 유저 배열의 인덱스를 맞추기 위해 만듬

	_ClientInfo* WinUser[MAX_USER]; // 게임에서 승리한 유저
	int WinUser_Count = 0; // 승리한 유저 수

	int user_count; // 게임 인원이 가득 찼는지 확인할 변수
	bool full; // 게임 인원이 가득 차면 시작하도록 사용할 변수

	int turn_number; // 현재 턴에 보내야 하는 숫자(1로 시작)
	int last_number; // 마지막 숫자(5)
	bool finish; // 게임이 끝나면 처리할 변수

	/* 게임별로 닉네임 리스트 따로 관리 */
	char* NickNameList[MAX_USER];
	int Nick_Count = 0;
};

/* 함수 원형 */

DWORD CALLBACK ProcessClient(LPVOID);

void err_quit(const char* msg);
void err_display(const char* msg);
int recvn(SOCKET s, char* buf, int len, int flags);

void MaKeGameMessage(const char* _nick, int _num, char* _chattmsg);
void MakeEnterMessage(const char* _nick, char* _msg);
void MakeExitMessage(const char* _nick, char* _msg);

_ClientInfo* AddClient(SOCKET sock, SOCKADDR_IN clientaddr);
void RemoveClient(_ClientInfo* ptr);
_GameInfo* AddGameInfo(_ClientInfo* _ptr);
void RemoveGameUser(_ClientInfo* _ptr);
void RemoveGameInfo(_GameInfo* _ptr);

void AddNickName(_ClientInfo* _ptr);
bool NicknameCheck(_ClientInfo* _ptr);
void RemoveNickName(_ClientInfo* _ptr);

bool PacketRecv(SOCKET _sock, char* _buf);

int PackPacket(char* _buf, PROTOCOL _protocol, char** _strlist, int _count);
int PackPacket(char* _buf, PROTOCOL _protocol, const char* _str1);
int PackPacket(char* _buf, PROTOCOL _protocol, int _num, const char* _str1);

PROTOCOL GetProtocol(const char*);

void UnPackPacket(const char*, char*);

void IntroProcess(_ClientInfo* _ptr);
void WaitProcess(_ClientInfo* _ptr);
void GamePlayProcess(_ClientInfo* _ptr);
void GameResultProcess(_ClientInfo* _ptr);
void DisConnectProcess(_ClientInfo* _ptr);
_GameInfo* GameWinLoseProcess(_ClientInfo* _ptr);


#ifdef MAIN
/* 게임 정보 배열 */
_GameInfo* GameInfo[GAME_MAX_COUNT];
int Game_Count = 0;

/* 클라이언트 정보 배열 */
_ClientInfo* ClientInfo[CLIENT_MAX_COUNT];
int Client_Count = 0;

CRITICAL_SECTION cs;
#else
/* 게임 정보 배열 */
extern _GameInfo* GameInfo[GAME_MAX_COUNT];
extern int Game_Count; // 활성화된 게임 개수

/* 클라이언트 정보 배열 */
extern _ClientInfo* ClientInfo[CLIENT_MAX_COUNT];
extern int Client_Count; // 접속한 클라이언트 개수

extern CRITICAL_SECTION cs;
#endif