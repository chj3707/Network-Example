#pragma comment(lib, "ws2_32.lib")
#include <winsock2.h>
#include <stdlib.h>
#include <stdio.h>

#define BUFSIZE 4096
#define FILENAMESIZE 256
#define CLIENTMAX 100
#define FILETRANSMAX 10
#define NODATA -1

#define INTRO_MSG "전송할 파일명을 입력하세요"
#define FILE_DUPLICATE_MSG "전송하고자 하는 파일은 이미 서버에 존재하는 파일입니다."
#define FILE_TRANS_WAIT_MSG "같은 파일을 이미 보내고 있습니다. 대기해 주세요.\n"
#define FILE_RESEND_MSG "대기중인 클라이언트가 이어서 전송합니다.\n"
CRITICAL_SECTION cs;

/* 파일 전송에 사용할 프로토콜 */
enum PROTOCOL 
{ 
	INTRO=1, // 서버 -> 클라 메시지
	FILE_INFO,  // 클라 -> 서버 패킷 프로토콜(파일 이름,총 크기)
	FILE_TRANS_DENY, // 전송 받는걸 거부 (이미 파일이 있음) 용량과 이름이 같음
	FILE_TRANS_START_POINT, // 어디서 부터 보낼지 알려줄 프로토콜
	FILE_TRANS_WAIT, // 파일 전송 대기하라고 메시지 보낼때 사용할 프로토콜
	FILE_RESEND, // 파일 재전송 메시지 보낼떄 사용할 프로토콜
	FILE_TRANS_DATA // 클라 -> 서버 패킷 프로토콜(파일 데이터)
};

/* 클라이언트 상태 */
enum STATE
{
	INIT_STATE=1, // 인트로
	FILE_TRANS_READY_STATE, // 파일 전송 준비 단계
	FILE_TRANS_STATE, // 파일을 받아서 쓰는 단계
	FILE_TRANS_WAIT_STATE, // 다른 클라이언트가 전송을 끝내길 기다리는 상태
	FILE_TRANS_END_STATE, // 전송이 끊어지거나 완료된 단계
	DISCONNECTED_STATE
};

enum DENY_CODE
{
	FILEEXIST = -1
};

struct _FileInfo
{
	char filename[FILENAMESIZE]; // 파일 이름
	int  filesize;	// 파일 총 용량
	int  nowsize; // 현재까지 받은 용량
};

struct _ClientInfo;

/* 파일 전송 정보 구조체 */
struct _FileTransInfo
{
	_ClientInfo* trans_client[CLIENTMAX]; // 같은 파일을 전송하는 클라이언트
	_ClientInfo* cur_trans_client; // 현재 전송중인 클라이언트
	int client_count; // 같은 파일을 전송하는 클라이언트 수
};

/* 클라이언트 정보 구조체 */
struct _ClientInfo
{
	SOCKET sock;
	SOCKADDR_IN addr;
	STATE	state; // 클라이언트 상태
	_FileInfo file_info; // 파일 정보

	_FileTransInfo* trans_info; // 클라가 속한 파일 전송 정보
	HANDLE wait_event; // 대기 이벤트 핸들
	int trans_turn_number; // 전송 순서 정하는 용도

	char recv_buf[BUFSIZE];
	char send_buf[BUFSIZE];
};

_FileTransInfo* TransInfo[FILETRANSMAX];
int TransCount = 0;
_ClientInfo* ClientInfo[CLIENTMAX];
int count;

/* 함수 원형 */
void err_quit(const char*);
void err_display(const char*);

int recvn(SOCKET, char*, int, int);

_ClientInfo* AddClientInfo(SOCKET sock, SOCKADDR_IN addr);
void ReMoveClientInfo(_ClientInfo*);

bool SearchFile(const char*);
bool PacketRecv(SOCKET, char*);
PROTOCOL GetProtocol(const char*);
int PackPacket(char*, PROTOCOL, const char*); // INTRO
int PackPacket(char*, PROTOCOL, int, const char*); // FILE_TRANS_DNEY
int PackPacket(char*, PROTOCOL, int); // FILE_TRANS_START_POINT

void UnPackPacket(const char* _buf, char* _str1, int& _data1); // FILE_INFO
void UnPackPacket(const char*, int&, char*); // FILE_TRANS_DATA

void InitProcess(_ClientInfo*);
void ReadyProcess(_ClientInfo*);
void FileTransProcess(_ClientInfo*);
void FileTransEndProcess(_ClientInfo*);

DWORD WINAPI ProcessClient(LPVOID arg);


_FileTransInfo* AddFileTransInfo(_ClientInfo* _ptr);
void ReMoveFileTransClient(_ClientInfo* _ptr);
void ReMoveFileTransInfo(_FileTransInfo* _ptr);

int main(int argc, char* argv[])
{
	InitializeCriticalSection(&cs); // 초기화

	int retval;

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
	serveraddr.sin_port = htons(9000);
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	retval = bind(listen_sock, (SOCKADDR *)&serveraddr, sizeof(serveraddr));
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
		addrlen = sizeof(clientaddr);
		client_sock = accept(listen_sock, (SOCKADDR *)&clientaddr, &addrlen);
		if (client_sock == INVALID_SOCKET) {
			err_display("accept()");
			continue;
		}	

		_ClientInfo* ClientPtr = AddClientInfo(client_sock, clientaddr); // 클라정보 추가		
		
		HANDLE hThread = CreateThread(nullptr, 0, ProcessClient, ClientPtr, 0, nullptr); // 스레드 생성
		if (hThread == nullptr)
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

	DeleteCriticalSection(&cs);

	// 윈속 종료
	WSACleanup();
	return 0;
}

// 소켓 함수 오류 출력 후 종료
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


_ClientInfo* AddClientInfo(SOCKET sock, SOCKADDR_IN addr)
{
	EnterCriticalSection(&cs);
	_ClientInfo* ptr = new _ClientInfo;
	ZeroMemory(ptr, sizeof(_ClientInfo));
	ptr->sock = sock;
	memcpy(&ptr->addr, &addr, sizeof(addr));
	ptr->state = INIT_STATE;
	ptr->trans_turn_number = NODATA;
	ptr->wait_event = CreateEvent(nullptr, false/* 자동 */, false/* 비신호 상태 */, nullptr); // 이벤트 생성

	ClientInfo[count++] = ptr;
	
	LeaveCriticalSection(&cs);

	printf("\nFileSender 접속: IP 주소=%s, 포트 번호=%d\n",
		inet_ntoa(ptr->addr.sin_addr), ntohs(ptr->addr.sin_port));

	return ptr;
}

void ReMoveClientInfo(_ClientInfo* ptr)
{
	
	printf("FileSender 종료: IP 주소=%s, 포트 번호=%d\n",
		inet_ntoa(ptr->addr.sin_addr), ntohs(ptr->addr.sin_port));

	EnterCriticalSection(&cs);

	for (int i = 0; i<count; i++)
	{
		if (ClientInfo[i] == ptr)
		{
			closesocket(ptr->sock); // 소켓 닫기
			CloseHandle(ptr->wait_event); // 이벤트 핸들 닫기
			delete ptr; // 메모리 해제
			for (int j = i; j<count - 1; j++)
			{
				ClientInfo[j] = ClientInfo[j + 1];
			}
			break;
		}
	}

	count--;
	LeaveCriticalSection(&cs);
}

/* 파일 전송 정보 추가 */
_FileTransInfo* AddFileTransInfo(_ClientInfo* _ptr)
{
	EnterCriticalSection(&cs); // 데이터 보호

	_FileTransInfo* trans_ptr = nullptr; // 객체 생성
	int index = NODATA;

	for (int i = 0; i < TransCount; i++)
	{
		TransInfo[i]->trans_client[TransInfo[i]->client_count++] = _ptr; // 들어오는 클라 저장
		_ptr->trans_info = TransInfo[i]; // 속한 파일 전송 정보 저장
		_ptr->trans_turn_number = TransInfo[i]->client_count; // 순서 지정
		TransInfo[i]->cur_trans_client = TransInfo[i]->trans_client[0];

		trans_ptr = TransInfo[i];
		index = i;
		break;
	}

	/* 제일 먼저 들어온 클라이언트 */
	if (index == NODATA)
	{
		trans_ptr = new _FileTransInfo;
		ZeroMemory(trans_ptr, sizeof(_FileTransInfo)); // 초기화

		trans_ptr->trans_client[0] = _ptr; // 제일 먼저 들어온 클라 저장
		trans_ptr->cur_trans_client = _ptr; // 제일 먼저 들어온 클라먼저 전송
		trans_ptr->client_count++; // 접속 카운트 +1
		TransInfo[TransCount++] = trans_ptr; // 전송 정보 배열에 저장

		_ptr->trans_info = trans_ptr; // 속한 파일 전송 정보 저장
		_ptr->trans_turn_number = trans_ptr->client_count; // 순서 지정(1번)
	}

	LeaveCriticalSection(&cs);
	return trans_ptr;
}

/* 파일 전송하다가 나간 클라이언트 삭제 */
void ReMoveFileTransClient(_ClientInfo* _ptr)
{
	EnterCriticalSection(&cs);
	_FileTransInfo* trans_info = _ptr->trans_info; // 클라가 속한 전송정보 객체 생성

	for (int i = 0; i < trans_info->client_count; i++) // 클라 개수 만큼 반복
	{
		if (_ptr == trans_info->trans_client[i]) // 클라 배열에서 종료한 클라를 찾아서
		{
			for (int j = i; j < trans_info->client_count - 1; j++)
			{
				trans_info->trans_client[j] = trans_info->trans_client[j + 1]; // 인덱스 앞당겨 주기
			}

			trans_info->client_count--; // 클라 개수 -1

			if (trans_info->client_count == 0) // 클라가 하나도 없으면
			{
				ReMoveFileTransInfo(trans_info); // 전송정보 삭제
			}
		}
	}

	LeaveCriticalSection(&cs);
}

/* 파일 전송 정보 삭제 */
void ReMoveFileTransInfo(_FileTransInfo* _ptr)
{
	EnterCriticalSection(&cs);

	for (int i = 0; i < TransCount; i++)
	{
		if (TransInfo[i] == _ptr) // 전송 정보 배열에서 해당 정보를 찾아서
		{
			delete _ptr; // 메모리 해제
			for (int j = i; j < TransCount - 1; j++)
			{
				TransInfo[j] = TransInfo[j + 1]; // 삭제된 기준으로 인덱스 앞당겨 저장
			}
		}
	}
	TransCount--;
	LeaveCriticalSection(&cs);
}

/* 파일이 있는지 없는지 확인하는 함수 */
bool SearchFile(const char *filename)
{
	EnterCriticalSection(&cs);
	WIN32_FIND_DATA FindFileData; // 파일 정보를 담을 구조체
	HANDLE hFindFile = FindFirstFile(filename, &FindFileData);
	if (hFindFile == INVALID_HANDLE_VALUE) // 파일이 없는 경우
	{
		LeaveCriticalSection(&cs);
		return false;
	}
	else // 파일이 있는 경우 
	{
		FindClose(hFindFile);
		LeaveCriticalSection(&cs);
		return true;
	}
}
// 사용자 정의 데이터 수신 함수
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

int PackPacket(char* _buf, PROTOCOL  _protocol, const char* _str) 
{
	char* ptr = _buf;
	int strsize = strlen(_str);
	int size = 0;

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

int PackPacket(char* _buf, PROTOCOL  _protocol, int _data, const char* _str)
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


int PackPacket(char* _buf, PROTOCOL _protocol, int _data)
{
	char* ptr = _buf;
	int size = 0;
	
	ptr = ptr + sizeof(size);
	
	memcpy(ptr, &_protocol, sizeof(_protocol));
	ptr = ptr + sizeof(_protocol);
	size = size + sizeof(_protocol);

	memcpy(ptr, &_data, sizeof(_data));
	ptr = ptr + sizeof(_data);
	size = size + sizeof(_data);

	ptr = _buf;
	memcpy(ptr, &size, sizeof(size));

	size = size + sizeof(size);
	return size;
}

PROTOCOL GetProtocol(const char* _buf)
{
	PROTOCOL protocol;
	memcpy(&protocol, _buf, sizeof(PROTOCOL));
	return protocol;
}

void UnPackPacket(const char* _buf, char* _str1, int& _data1)
{
	const char* ptr = _buf + sizeof(PROTOCOL);
	int strsize;

	memcpy(&strsize, ptr, sizeof(strsize));
	ptr = ptr + sizeof(strsize);

	memcpy(_str1, ptr, strsize);
	ptr = ptr + strsize;

	memcpy(&_data1, ptr, sizeof(_data1));
	ptr = ptr + sizeof(_data1);	
}

void UnPackPacket(const char* _buf, int& _size, char* _targetbuf)
{
	const char* ptr = _buf + sizeof(PROTOCOL);

	memcpy(&_size, ptr, sizeof(_size));
	ptr = ptr + sizeof(_size);

	memcpy(_targetbuf, ptr, _size);
}

/* 인트로 메시지 보내는 프로세스 */
void InitProcess(_ClientInfo* _ptr)
{	
	int size = PackPacket(_ptr->send_buf, INTRO, INTRO_MSG);

	int retval = send(_ptr->sock, _ptr->send_buf, size, 0); // 인트로 메시지 보내기
	if (retval == SOCKET_ERROR)
	{
		err_display("send()");
		_ptr->state = DISCONNECTED_STATE;
		return;
	}
	_ptr->state = FILE_TRANS_READY_STATE;
}

/* 파일 전송 준비 프로세스 */
void ReadyProcess(_ClientInfo* _ptr)
{
	char filename[FILENAMESIZE];
	int filesize;
	int retval;	

	if (!PacketRecv(_ptr->sock, _ptr->recv_buf))
	{
		_ptr->state = DISCONNECTED_STATE;
		return;
	}

	_FileTransInfo* trans_info = AddFileTransInfo(_ptr);
	
	PROTOCOL protocol = GetProtocol(_ptr->recv_buf);

	switch (protocol)
	{
	case FILE_INFO:
		memset(filename, 0, sizeof(filename));

		UnPackPacket(_ptr->recv_buf, filename, filesize); // 파일 이름, 파일 크기 언팩

		printf("-> 받을 파일 이름: %s\n", filename);
		printf("-> 받을 파일 크기: %d\n", filesize);

		/* 이름이 같은 파일이 있는 경우 */
		if (SearchFile(filename))
		{
			FILE* fp = fopen(filename, "rb"); // 읽기 모드로 파일 오픈
			fseek(fp, 0, SEEK_END);
			int fsize = ftell(fp); // 파일 총 크기
			fclose(fp); // 크기 구하고 종료

			if (filesize == fsize) // 이름과 용량이 모두 같은 경우(이미 있는 파일)
			{
				printf("존재하는 파일 전송 요구\n");

				int size = PackPacket(_ptr->send_buf, FILE_TRANS_DENY, FILEEXIST, FILE_DUPLICATE_MSG);

				retval = send(_ptr->sock, _ptr->send_buf, size, 0); // 거부 패킷 보내기
				if (retval == SOCKET_ERROR)
				{
					err_display("send()");
					_ptr->state = DISCONNECTED_STATE;
					return;
				}

				if (!PacketRecv(_ptr->sock, _ptr->recv_buf)) // 클라 종료 대기
				{
					_ptr->state = DISCONNECTED_STATE;
					return;
				}
			}
			else // 이미 있는 파일 이지만 원본 파일과 용량이 다른 경우
			{
				strcpy(_ptr->file_info.filename, filename);
				_ptr->file_info.filesize = filesize;
				_ptr->file_info.nowsize = fsize;
			
				/* 가장 먼저 전송을 시작한 클라이언트 */
				if (_ptr == trans_info->cur_trans_client)
				{
					_ptr->state = FILE_TRANS_STATE;
				}
				/* 전송을 나중에 시작한 클라이언트 인데 파일 이름, 전체크기가 같으면 대기 상태 */
				else
				{
					_ptr->state = FILE_TRANS_WAIT_STATE;
					break;
				}
			}

		}

		/* 이름이 같은 파일이 없는 경우 */
		else
		{
			strcpy(_ptr->file_info.filename, filename);
			_ptr->file_info.filesize = filesize;
			_ptr->file_info.nowsize = 0;
			_ptr->state = FILE_TRANS_STATE;
		}

		int size = PackPacket(_ptr->send_buf, FILE_TRANS_START_POINT, _ptr->file_info.nowsize); // 파일의 현재 크기를 패킹(어디서 부터 보낼지 알수 있도록)

		retval = send(_ptr->sock, _ptr->send_buf, size, 0); // 현재 크기 패킷 전송
		if (retval == SOCKET_ERROR)
		{
			err_display("send()");
			_ptr->state = DISCONNECTED_STATE;
			return;
		}

		break;
	}

}

/* 파일 전송 프로세스 */
void FileTransProcess(_ClientInfo* _ptr)
{
	static FILE* fp = nullptr;
	char buf[BUFSIZE];	

	if (!PacketRecv(_ptr->sock, _ptr->recv_buf))
	{
		/* 데이터 받는 도중 에러 */
		_ptr->state = FILE_TRANS_END_STATE;	// 파일 전송 완료 상태로 변경	
		fclose(fp);
		fp = nullptr;
		return;
	}

	PROTOCOL protocol = GetProtocol(_ptr->recv_buf); // 프로토콜 분리

	switch (protocol)
	{
	case FILE_TRANS_DATA:
		if (fp==nullptr)
		{			
			fp = fopen(_ptr->file_info.filename, "ab"); // 이어 쓰기 위해 append모드로 오픈
		}

		int transsize;
		UnPackPacket(_ptr->recv_buf, transsize, buf);
		fwrite(buf, 1, transsize, fp);
		if (ferror(fp)) 
		{
			perror("파일 입출력 오류");
			_ptr->state = FILE_TRANS_END_STATE;
			fclose(fp);
			return;
		}
		_ptr->file_info.nowsize += transsize;
		break;
	}
	
}

/* 다른 클라가 작업을 완료할때 까지 대기하는 프로세스 */
void WaitProcess(_ClientInfo* _ptr)
{
	int size = PackPacket(_ptr->send_buf, FILE_TRANS_WAIT, FILE_TRANS_WAIT_MSG); // 대기 메시지 패킹
	int retval = send(_ptr->sock, _ptr->send_buf, size, 0); // 전송
	if (retval == SOCKET_ERROR)
	{
		err_display("send()");
		_ptr->state = DISCONNECTED_STATE;
		return;
	}

	WaitForSingleObject(_ptr->wait_event, INFINITE); // 이벤트 신호 대기

	size = PackPacket(_ptr->send_buf, FILE_RESEND, FILE_RESEND_MSG); // 재전송 알림 메시지 패킹

	retval = send(_ptr->sock, _ptr->send_buf, size, 0); // 메시지 전송
	if (retval == SOCKET_ERROR)
	{
		err_display("send()");
		_ptr->state = DISCONNECTED_STATE;
		return;
	}

	FILE* fp = fopen(_ptr->file_info.filename, "rb"); // 읽기 모드로 파일 오픈
	fseek(fp, 0, SEEK_END);
	int fsize = ftell(fp); // 파일 총 크기
	fclose(fp); // 크기 구하고 종료

	_ptr->file_info.nowsize = fsize; // 이미 보내져 있는 크기 저장

	size = PackPacket(_ptr->send_buf, FILE_TRANS_START_POINT, _ptr->file_info.nowsize); // 파일의 현재 크기를 패킹(어디서 부터 보낼지 알수 있도록)

	retval = send(_ptr->sock, _ptr->send_buf, size, 0); // 현재 크기 패킷 전송
	if (retval == SOCKET_ERROR)
	{
		err_display("send()");
		_ptr->state = DISCONNECTED_STATE;
		return;
	}
	_ptr->state = FILE_TRANS_STATE;
}

/* 파일 전송하던 클라가 종료할때 파일 전송해줄 다른 클라이언트를 찾는 함수 */
_ClientInfo* SearchNextFileTransClient(_ClientInfo* _ptr)
{
	int index;

	EnterCriticalSection(&cs);
	_FileTransInfo* trans_info = _ptr->trans_info; // 클라가 속한 전송정보 객체 생성
	_ClientInfo* ptr = nullptr;

	// 전송 대기중인 클라가 하나이상 있을때만
	if (trans_info->client_count >= 2)
	{
		/* 전송정보에 속한 클라이언트 수만큼 반복 */
		for (int i = 0; i < trans_info->client_count; i++)
		{
			if (_ptr != trans_info->trans_client[i]) // 전송하는 클라가 아닌 클라 중에서
			{
				index = i;
				break;
			}
		}
		ptr = trans_info->trans_client[index];
		trans_info->cur_trans_client = ptr;
	}

	LeaveCriticalSection(&cs);
	return ptr;
}

/* 연결종료 프로세스 */
void DisconnectedProcess(_ClientInfo* _ptr)
{
	/* 클라 종료시 다음으로 전송해줄 클라 찾기 */
	_ClientInfo* next_ptr = SearchNextFileTransClient(_ptr);
	if (next_ptr != nullptr)
	{
		SetEvent(next_ptr->wait_event); // 대기 이벤트 신호 상태로 변경
	}

	ReMoveFileTransClient(_ptr); // 종료한 클라 삭제
	ReMoveClientInfo(_ptr); // 종료한 클라 정보 삭제
}

DWORD WINAPI ProcessClient(LPVOID arg)
{
	_ClientInfo* ptr = (_ClientInfo*)arg;
	bool endflag = false;

	while (1)
	{

		switch (ptr->state)
		{
			/* 인트로 */
		case INIT_STATE:
			InitProcess(ptr);
			break;

			/* 파일 전송 준비 */
		case FILE_TRANS_READY_STATE:
			ReadyProcess(ptr);
			break;

			/* 파일 전송 */
		case FILE_TRANS_STATE:
			FileTransProcess(ptr);
			break;

			/* 파일 전송 대기 상태(다른 클라가 보내고 있을 때) */
		case FILE_TRANS_WAIT_STATE:
			WaitProcess(ptr);
			break;

			/* 전송 완료 (성공 or 실패) */
		case FILE_TRANS_END_STATE:
			FileTransEndProcess(ptr);
			break;

			/* 클라 연결 종료 */
		case DISCONNECTED_STATE:
			DisconnectedProcess(ptr);
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

void FileTransEndProcess(_ClientInfo* _ptr)
{
	if (_ptr->file_info.filesize != 0 && _ptr->file_info.filesize == _ptr->file_info.nowsize)
	{
		printf("전송성공!!\n");
	}
	else
	{
		printf("전송실패!!\n");
	}

	_ptr->state = DISCONNECTED_STATE;
}