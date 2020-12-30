#pragma comment(lib, "ws2_32")
#include <winsock2.h>
#include <stdlib.h>
#include <stdio.h>

#define SERVERIP   "127.0.0.1"
#define SERVERPORT 9000
#define FILENAMESIZE 256
#define BUFSIZE    4096
#define READSIZE   2048

/* 파일 전송에 사용할 프로토콜 */
enum PROTOCOL
{
	INTRO = 1, // 서버 -> 클라 메시지
	FILE_INFO,  // 클라 -> 서버 패킷 프로토콜(파일 이름,총 크기)
	FILE_TRANS_DENY, // 전송 받는걸 거부 (이미 파일이 있음) 용량과 이름이 같음
	FILE_TRANS_START_POINT, // 어디서 부터 보낼지 알려줄 프로토콜
	FILE_TRANS_WAIT, // 파일 전송 대기하라고 메시지 보낼때 사용할 프로토콜
	FILE_RESEND, // 파일 재전송 메시지 보낼떄 사용할 프로토콜
	FILE_TRANS_DATA // 클라 -> 서버 패킷 프로토콜(파일 데이터)
};

/* 파일 정보 구조체 */
struct _File_info
{
	char filename[FILENAMESIZE]; // 파일 이름
	int  filesize;	// 파일 총 용량
	int  nowsize; // 현재까지 받은 용량
};

enum DENY_CODE
{
	FILEEXIST = -1
};


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

// 사용자 정의 데이터 수신 함수
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

PROTOCOL GetProtocol(const char* _buf)
{
	PROTOCOL protocol;
	memcpy(&protocol, _buf, sizeof(PROTOCOL));
	return protocol;
}

/* 패킹 함수 (파일 이름) */
int Packing(char* _buf, PROTOCOL _protocol, char* _filename, int _filesize)
{
	char* ptr = _buf + sizeof(int); // 위치 잡아줄 변수 (시작 위치에 총 크기(size)를 넣을 수 있도록 int 크기만큼 포인터 위치 변경)
	int size = 0; // 총 크기

	/* 프로토콜 */
	memcpy(ptr, &_protocol, sizeof(_protocol));
	size = size + sizeof(_protocol);
	ptr = ptr + sizeof(_protocol);

	/* 파일 이름 길이 */
	int strsize = strlen(_filename); // 파일 이름 길이
	memcpy(ptr, &strsize, sizeof(strsize));
	size = size + sizeof(strsize);
	ptr = ptr + sizeof(strsize);

	/* 파일 이름 */
	memcpy(ptr, _filename, strsize);
	size = size + strsize; // 총 크기 증가 
	ptr = ptr + strsize; // 포인터 위치 변경

	/* 파일 총 크기 */
	memcpy(ptr, &_filesize, sizeof(_filesize));
	size = size + sizeof(_filesize);
	ptr = ptr + sizeof(_filesize);

	ptr = _buf; // 포인터 위치 시작 지점으로 변경
	memcpy(ptr, &size, sizeof(int));

	return size + sizeof(int); // 총 크기 + 자기 자신의 크기
}


/* 패킹 함수 (파일 데이터) */
int Packing(char* _buf, PROTOCOL _protocol, int _byte, char* _filedata)
{
	char* ptr = _buf + sizeof(int); // 위치 잡아줄 변수 (시작 위치에 총 크기(size)를 넣을 수 있도록 int 크기만큼 포인터 위치 변경)
	int size = 0; // 총 크기

	/* 프로토콜 */
	memcpy(ptr, &_protocol, sizeof(_protocol));
	size = size + sizeof(_protocol); // 총 크기 증가 
	ptr = ptr + sizeof(_protocol); // 포인터 위치 변경

	/* 1회당 보내는 바이트 */
	memcpy(ptr, &_byte, sizeof(_byte));
	size = size + sizeof(_byte);
	ptr = ptr + sizeof(_byte);

	/* 파일 데이터 */
	memcpy(ptr, _filedata, _byte); // 보내는 바이트 만큼
	size = size + _byte;
	ptr = ptr + _byte;

	ptr = _buf; // 포인터 위치 시작 지점으로 변경
	memcpy(ptr, &size, sizeof(int));

	return size + sizeof(int); // 총 크기 + 자기 자신의 크기
}

/* 언패킹 함수 (INTRO) */
void UnPacking(char* _buf, char* _msg)
{
	const char* ptr = _buf + sizeof(PROTOCOL);
	int strsize;

	/* 인트로 메시지 문자열 크기 */
	memcpy(&strsize, ptr, sizeof(strsize));
	ptr = ptr + sizeof(strsize);

	/* 인트로 메시지 */
	memcpy(_msg, ptr, strsize);
	ptr = ptr + strsize;
}

/* 언패킹 함수(FILE_TRANS_DENY) */
void UnPacking(char* _buf, int& _data, char* _msg)
{
	const char* ptr = _buf + sizeof(PROTOCOL);
	int strsize;

	/* (DENY_CODE) */
	memcpy(&_data, ptr, sizeof(_data));
	ptr = ptr + sizeof(_data);

	/* 거부 메시지 문자열 크기 */
	memcpy(&strsize, ptr, sizeof(strsize));
	ptr = ptr + sizeof(strsize);

	/* 거부 메시지 */
	memcpy(_msg, ptr, strsize);
	ptr = ptr + strsize;
}

/* 언패킹 함수(FILE_TRANS_START_POINT) */
void UnPacking(char* _buf, int& _data)
{
	const char* ptr = _buf + sizeof(PROTOCOL);

	/* 파일 전송 시작지점 */
	memcpy(&_data, ptr, sizeof(_data));
	ptr = ptr + sizeof(_data);
}

int main(int argc, char* argv[])
{
	char send_buf[BUFSIZE]; // 작업대(보낼 데이터)
	char recv_buf[BUFSIZE]; // 받은 데이터
	int size; // 패킷 크기
	int retval; // 리턴 값

	// 윈속 초기화
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return 1;

	// socket()
	SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == INVALID_SOCKET) err_quit("socket()");

	// connect()
	SOCKADDR_IN serveraddr;
	ZeroMemory(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = inet_addr(SERVERIP);
	serveraddr.sin_port = htons(SERVERPORT);
	retval = connect(sock, (SOCKADDR*)&serveraddr, sizeof(serveraddr));
	if (retval == SOCKET_ERROR) err_quit("connect()");

	_File_info info;
	ZeroMemory(&info, sizeof(_File_info)); // 초기화

	char msg[BUFSIZE]; // 메시지 받을 변수
	FILE* fp = nullptr;
	bool endflag = false;
	while (1)
	{
		if (!PacketRecv(sock, recv_buf))
		{
			return 1;
		}

		PROTOCOL protocol = GetProtocol(recv_buf);

		switch (protocol)
		{
			/* 인트로 메시지 */
		case INTRO:
			ZeroMemory(msg, sizeof(msg)); // 초기화
			UnPacking(recv_buf, msg); // 받은 인트로 메시지 언패킹
			printf("%s\n", msg);
			
			scanf("%s", &info.filename);
			// 보낼 파일명 변수에 복사
			//strcpy(info.filename, argv[1]); // argv[1] 속성 디버깅 명령인수
			
			// 파일 열기
			fp = fopen(info.filename, "rb");
			if (fp == NULL) {
				perror("fopen()");
				break;
			}
			fseek(fp, 0, SEEK_END); // 파일 시작부터 끝까지
			info.filesize = ftell(fp); // 파일 전체 크기 할당
			fclose(fp); // 크기 할당 받고 파일 종료

			size = Packing(send_buf, FILE_INFO, info.filename, info.filesize); // 파일 이름, 파일 총 크기 패킹
			retval = send(sock, send_buf, size, 0); // 파일 이름, 전체 크기 보내기
			if (retval == SOCKET_ERROR) err_quit("send()");

			break;

			/* 거부 메시지 */
		case FILE_TRANS_DENY:
			int result; 
			ZeroMemory(msg, sizeof(msg)); // 초기화
			UnPacking(recv_buf, result, msg); // 거부 메시지 언패킹

			switch (result)
			{
			case FILEEXIST:
				printf("%s\n", msg); // 거부 메시지 출력
				endflag = true;
				break;
			}
			break;

			/* 재전송,대기 메시지 */
		case FILE_RESEND:
		case FILE_TRANS_WAIT:
			ZeroMemory(msg, sizeof(msg));
			UnPacking(recv_buf, msg); // 메시지 언패킹
			printf("%s\n", msg);

			break;
			/* 파일 전송 시작점 */
		case FILE_TRANS_START_POINT:
			UnPacking(recv_buf, info.nowsize);
			fp = fopen(info.filename, "rb"); // 읽기 모드로 파일 오픈
			if (fp == NULL)
			{
				perror("fopen()");
				break;
			}
			fseek(fp, info.nowsize, SEEK_SET); // 파일의 시작부터 현재 보내진곳 까지 (보내진곳 부터 보내도록)
			while (1)
			{
				char filedata[BUFSIZE]; // 파일 내용 저장할 작업대
				int nbytes = fread(filedata, 1, READSIZE, fp); // 파일 내용 2048만큼 filedata에 저장(BUFSIZE만큼 하면 send_buf크기보다 커져 에러)

				if (nbytes == 0) // 다읽었으면 종료(읽을 데이터가 없으면)
				{
					endflag = true;
					break; // 반복문 종료
				}
				size = Packing(send_buf, FILE_TRANS_DATA, nbytes, filedata); // 파일 데이터 패킹
				retval = send(sock, send_buf, size, 0); // 파일 데이터 보내기
				if (retval == SOCKET_ERROR)
				{
					err_quit("send()");
				}
				printf("...");
				Sleep(300); // 0.3초 정지
				info.nowsize += nbytes; // 읽은 바이트 만큼 더해줌
			}
			fclose(fp); // 파일 닫기

			break;
		}

		if (endflag) 
		{
			if (info.nowsize == info.filesize) // 파일 전체 크기만큼 전송 한 경우
			{
				printf("파일 전송 성공!\n");
				break;
			}
			else
			{
				printf("파일 전송 실패\n");
				break;
			}
		}
	}
	// closesocket()
	closesocket(sock);

	// 윈속 종료
	WSACleanup();

	system("pause");
	return 0;
}