#pragma once
#pragma comment(lib, "ws2_32.lib")

#include <stdio.h>
#include <string.h>
#include <winsock2.h>

#define SERVERIP "127.0.0.1"
#define SERVERPORT 9000

#define BUFSIZE 4096   // ���� �迭 ũ��
#define NICKNAMESIZE 255 // �г��� �迭 ũ��

#define GAME_MAX_COUNT 10 // ���� �ִ� ����
#define CLIENT_MAX_COUNT 100 // �ִ� Ŭ���̾�Ʈ ����
#define MAX_USER 5 // ���ӿ� �� �� �ִ� �ִ� �ο�
#define SAME_TIMEING 500 // ���ö�� �Ǵ��� �ð� 

#define NODATA -1
#define INIT 0

#define INTRO_MSG "��ġ ���� ���α׷��Դϴ�. �г����� �Է��ϼ���"
#define NICKNAME_ERROR_MSG "�̹��ִ� �г����Դϴ�. �ٸ� �г����� �Է��� �ּ���."
#define WAIT_MSG "�ٸ� ������ ���� ���Դϴ�. ��ø� ��ٷ� �ּ���..."
#define GAME_START_MSG "��ġ ������ �����մϴ�... 1���� 5���� ���ʴ�� �����ּ���."
#define RANGE_ERROR_MSG "�߸� �Է� �ϼ̽��ϴ�. 1���� 5������ ���ڸ� �Է����ּ���..."
#define GAME_WIN_MSG "�¸� �ϼ̽��ϴ�... "
#define GAME_LOSE_MSG "�й� �ϼ̽��ϴ�... "

/* Ŭ���̾�Ʈ ���� */
enum STATE
{
	INITE_STATE,
	INTRO_STATE, // ��Ʈ��
	WAIT_STATE, // ���� ���� ��� ����
	GAME_PLAY_STATE, // ���� ����
	GAME_RESULT_STATE, // ���� ���
	DISCONNECT_STATE // ���� ����
};

/* �������� */
enum PROTOCOL
{
	INTRO, // (���� -> Ŭ��) ��Ʈ�� �޽����� ������ ����� ��������

	WAIT, // (���� -> Ŭ��) ��� �޽����� ������ ����� ��������

	NICKNAME_LIST, // (���� -> Ŭ��) �г��� ����Ʈ ������ ����� ��������

	NICKNAME, // (Ŭ�� -> ����) �г��� �Է��Ҷ� ���� ��������

	DATA_ERROR, // (���� -> Ŭ��) ������ ������ ���� �ڵ�� �Բ� ���� ��������

	USER_ENTER, // (���� -> Ŭ��) ������ ���忡 ���������� ����� ��������

	GAME_START, // (���� -> Ŭ��) ���� ���� �޽����� ������ ����� ��������

	GAME_RESULT, // (���� -> Ŭ��) ���� ��� ������ ����� ��������

	GAME_MSG, // ���� ���� ������ ����� ��������
	
	GAME_OUT // ������ ���ӿ��� �����ٰ� ������ ����� ��������
};

/* ���� ��� */
enum GAMERESULT
{
	WIN = 0,
	LOSE
};

/* �����ڵ� */
enum ERRORCODE
{
	NICKNAME_EROR, // �̹� �ִ� �г���
	RANGE_ERROR, // ���� ����
};

struct _GameInfo;

/* Ŭ�� ���� */
struct _ClientInfo
{
	SOCKET sock;
	SOCKADDR_IN clientaddr;
	STATE state; // Ŭ�� ����

	HANDLE timeing_event; // ���ÿ� �����°��� Ȯ���� �̺�Ʈ
	_GameInfo* game_info; // Ŭ�� ���� ���� ����
	char nickname[NICKNAMESIZE]; // Ŭ�� �г���
	int number; // Ŭ�� ���� ����
	bool ISanswer; // 2�� ������ ���ϰ� ����ó����

	char  sendbuf[BUFSIZE];
	char  recvbuf[BUFSIZE];
};

/* ���� ���� */
struct _GameInfo
{
	HANDLE start_event; // ���� ���ۿ� �̺�Ʈ
	HANDLE result_event; // ���� ����� �̺�Ʈ
	HANDLE timing_event; // ���ÿ� �����°��� Ȯ���� �̺�Ʈ
	int same_time_count = 0; // ���ÿ� �Է��� Ŭ�� ��
	
	_ClientInfo* User[MAX_USER]; // ���ӿ� ���� Ŭ���̾�Ʈ

	_ClientInfo* LoseUser[MAX_USER]; // ���ӿ��� �й��� ����
	int LoseUser_Count = 0; // �й��� ���� ��
	_ClientInfo* TempCliArr[MAX_USER]; // ���� �迭�� �й��� ���� �迭�� �ε����� ���߱� ���� ����

	_ClientInfo* WinUser[MAX_USER]; // ���ӿ��� �¸��� ����
	int WinUser_Count = 0; // �¸��� ���� ��

	int user_count; // ���� �ο��� ���� á���� Ȯ���� ����
	bool full; // ���� �ο��� ���� ���� �����ϵ��� ����� ����

	int turn_number; // ���� �Ͽ� ������ �ϴ� ����(1�� ����)
	int last_number; // ������ ����(5)
	bool finish; // ������ ������ ó���� ����

	/* ���Ӻ��� �г��� ����Ʈ ���� ���� */
	char* NickNameList[MAX_USER];
	int Nick_Count = 0;
};

/* �Լ� ���� */

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
/* ���� ���� �迭 */
_GameInfo* GameInfo[GAME_MAX_COUNT];
int Game_Count = 0;

/* Ŭ���̾�Ʈ ���� �迭 */
_ClientInfo* ClientInfo[CLIENT_MAX_COUNT];
int Client_Count = 0;

CRITICAL_SECTION cs;
#else
/* ���� ���� �迭 */
extern _GameInfo* GameInfo[GAME_MAX_COUNT];
extern int Game_Count; // Ȱ��ȭ�� ���� ����

/* Ŭ���̾�Ʈ ���� �迭 */
extern _ClientInfo* ClientInfo[CLIENT_MAX_COUNT];
extern int Client_Count; // ������ Ŭ���̾�Ʈ ����

extern CRITICAL_SECTION cs;
#endif