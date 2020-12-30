// Winsock2를 사용하기 위한 lib 추가
#pragma comment(lib, "ws2_32.lib") // 프로젝트 속성 - 링커 - 입력 - 추가 종속성 (ws2_32.lib)로 해도 되지만 전처리문 권장
#include <WinSock2.h> // 윈속을 사용하기 위한 헤더파일
#include <stdio.h> // 입출력 함수
#include <stdlib.h> // 표준 라이브러리 함수

/* 사용할 기호상수 정의 */
#define SERVERIP "127.0.0.1" // 루프백 주소
#define SERVERPORT 9000 // 포트 번호
#define BUFSIZE 512 // 버프 크기

#define NUMBER_COUNT 3 // 입력 받을 숫자 개수
#define GAME_NUMBER_SIZE 31

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

/* 심각한 오류 (강제 종료)
 * 더이상 프로그램을 진행할 수 없을때 사용 */
void err_quit(const char* msg)
{
	LPVOID lpmsgbuf;
	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM,
		NULL,
		WSAGetLastError(), // 최근의 에러 코드를 넘기면 포멧메시지가 에러 문자열을 알려줌
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPSTR)&lpmsgbuf,
		0, NULL);
	MessageBox(NULL, (LPCSTR)lpmsgbuf, msg, MB_OK);
	LocalFree(lpmsgbuf);
	exit(-1);
}

// 사소한 오류들 (printf로 처리)
void err_display(const char* msg)
{
	LPVOID lpmsgbuf;
	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM,
		NULL,
		WSAGetLastError(), // 최근의 에러 코드를 넘기면 포멧메시지가 에러 문자열을 알려줌 
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPSTR)&lpmsgbuf,
		0, NULL);
	printf("[%s] %s\n", msg, (LPSTR)lpmsgbuf);
	LocalFree(lpmsgbuf);
}

/* 사용자 정의 데이터 수신 함수
 * 몇 바이트를 받을지 정확하게 알때 사용한다. */
int recvn(SOCKET sock, char* buf, int len, int flags)
{
	char* ptr = buf; // 버퍼 시작 주소
	int left = len; // 아직 읽지 않은 데이터 크기
	int recived; // recv() 리턴값 저장할 변수

	while (left > 0) // 데이터를 전부 읽을때 까지 반복
	{
		recived = recv(sock /* 대상과 연결된 소켓 */
			, ptr /* 저장할 버퍼 주소 */
			, left /* 데이터 최대 크기 */
			, flags);

		if (recived == SOCKET_ERROR)
		{
			return SOCKET_ERROR; // 에러 발생시 리턴
		}

		// 정상 종료
		if (recived == 0)
		{
			break;
		}
		// 변수값 갱신
		left -= recived;
		ptr += recived;
	}

	return (len - left); // 읽은 바이트 수 (left 값은 오류, 접속 종료가 아닌경우 0이므로) 리턴값은 len
}

/* SELECT_NUM */
int Packing(char* _buf, PROTOCOL _protocol, int _data)
{
	char* ptr = _buf + sizeof(int); // 위치 잡아줄 변수 (시작 위치에 총 크기(size)를 넣을 수 있도록 int 크기만큼 포인터 위치 변경)
	int size = 0; // 총 크기

	memcpy(ptr, &_protocol, sizeof(PROTOCOL));
	size = size + sizeof(PROTOCOL);
	ptr = ptr + sizeof(PROTOCOL);

	memcpy(ptr, &_data, sizeof(int));
	size = size + sizeof(int); // 총 크기 증가 
	ptr = ptr + sizeof(int); // 포인터 위치 변경

	ptr = _buf; // 포인터 위치 시작 지점으로 변경
	memcpy(ptr, &size, sizeof(int));

	return size + sizeof(int); // 총 크기 + 자기 자신의 크기
}

/* 패킷 해제 함수 (에러 메시지)*/
void UnPacking(const char* _buf, int& _result, char* str1)
{
	const char* ptr = _buf + sizeof(PROTOCOL); // 시작위치에 프로토콜 크기만큼 더해서 자리 만들어줌
	int strsize1; // 메시지 길이 저장할 변수

	memcpy(&_result, ptr, sizeof(int));
	ptr = ptr + sizeof(int); // 포인터 위치 변경

	memcpy(&strsize1, ptr, sizeof(int));
	ptr = ptr + sizeof(int);

	memcpy(str1, ptr, strsize1);
	ptr = ptr + strsize1;
}

/* 패킷 해제 함수 (안내 메시지) */
void UnPacking(const char* _buf, char* str1)
{
	const char* ptr = _buf + sizeof(PROTOCOL); // 시작위치에 int 크기만큼 더해서 자리 만들어줌
	int strsize1; // 안내메시지 길이 저장할 변수

	memcpy(&strsize1, ptr, sizeof(int)); // int 바이트 크기의 길이만큼 변수에 복사
	ptr = ptr + sizeof(int); // 포인터 위치 변경

	memcpy(str1, ptr, strsize1);
	ptr = ptr + strsize1;
}

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

/* 안내 메시지 */
void PrintMessage(char* _buf)
{
	char msg[BUFSIZE]; // 받은 메시지 저장할 변수
	ZeroMemory(msg, sizeof(msg)); // 메모리 초기화

	UnPacking(_buf, msg); // 메시지 언패킹
	printf("%s", msg); // 메시지 출력
}

/* 에러 메시지 */
void PrintErrorMsg(char* _buf)
{
	int result;
	char msg[BUFSIZE]; // 받은 메시지 저장할 변수
	ZeroMemory(msg, sizeof(msg)); // 메모리 초기화

	UnPacking(_buf, result, msg); // 에러메시지 언패킹
	switch (result)
	{
	case DATA_RANGE_ERROR:
		printf("\n%s", msg); // 메시지 출력
		break;
	}
}

/* 게임 결과 메시지 */
void PrintResultMsg(char* _buf)
{
	int result;
	char msg[BUFSIZE]; // 받은 메시지 저장할 변수
	ZeroMemory(msg, sizeof(msg)); // 메모리 초기화

	UnPacking(_buf, result, msg); // 게임 결과 언패킹
	switch (result)
	{
	case WIN: 
	case LOSE:
		printf("\n%s", msg);
		break;
	}
}
/* 메인 함수 */
int main()
{
	// 윈속 초기화
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return 1; // WSAStartup : 정적 라이브러리(컴파일 할때 라이브러리가 프로그램에 포함) 초기화
	// 실행 파일과는 별도

	// socket()
	SOCKET sock = socket(AF_INET/* 인터넷 프로토콜 설정 */, SOCK_STREAM/* 전송 프로토콜 설정 TCP */, 0 /* 항상 0 */); // IPv4 TCP 소켓 생성
	if (sock == INVALID_SOCKET)
	{
		err_quit("socket()"); // 소켓을 만들지 않았을때
	}

	/* connect() 연결 요청을 보낼 서버의 주소 세팅 */
	SOCKADDR_IN serveraddr; // 주소 정보를 담을 구조체 (IPv4)
	ZeroMemory(&serveraddr, sizeof(serveraddr)); // 메모리 초기화
	serveraddr.sin_family = AF_INET; // IPv4 인터넷 프로토콜
	serveraddr.sin_addr.s_addr/* long형의 정수가됨 */ = inet_addr(SERVERIP); // 문자열로 이루어진 아이피 주소를 s_addr를 사용해 long형으로 바꿈
	serveraddr.sin_port = htons/* 바이트 정렬 변경 */(SERVERPORT); // 포트 번호 설정
	int retval = connect(sock, (SOCKADDR*)&serveraddr, sizeof(serveraddr)/* 16 바이트 */);
	if (retval == SOCKET_ERROR)
	{
		err_quit("connect()");
	}

	// 통신에 사용할 변수
	char send_buf[BUFSIZE]; // 보낼때 사용할 버퍼
	char recv_buf[BUFSIZE]; // 받을때 사용할 버퍼
	bool endflag = false; // 반복문 종료 플래그

	int user_num; // 유저가 입력할 수
	int server_num; // 서버가 입력한 수
	
	int size;

	// 서버와 데이터 통신
	while (1)
	{
		if (!PacketRecv(sock, recv_buf)) // 데이터 받기
		{
			break;
		}

		PROTOCOL protocol = GetProtocol(recv_buf); // 받아온 프로토콜 할당

		switch (protocol)
		{
			
		case WAIT:
		case INTRO: 
		case PLAYER_INFO:
		case COUNT_VALUE:
		case PLAYER_ESCAPE:
		case GAME_CLOSE:
			PrintMessage(recv_buf);
			break;

			/* CLIENT_TURN */
		case CLIENT_TURN:
			PrintMessage(recv_buf); // 안내 메시지 출력
			scanf("%d", &user_num); // 숫자 입력

			size = Packing(send_buf, SELECT_NUM, user_num); // 입력한 수 패킹

			// 데이터 보내기 (입력한 숫자)
			retval = send(sock, send_buf, size, 0); // (연결된 소켓, 보낼 데이터, 보낼 크기, 0)
			if (retval == SOCKET_ERROR) // 강제 종료
			{
				err_display("send()");
				break;
			}

			break;

			// 에러
		case DATA_ERROR:
			PrintErrorMsg(recv_buf); // 에러 메시지 출력
			break;

			// 결과
		case GAME_RESULT:
			PrintResultMsg(recv_buf); // 게임결과 메시지 출력
			endflag = true; // 게임 종료 플래그 온
			break;
		}

		if (endflag)
		{
			Sleep(3000);
			break; // 반복문 종료
		}
	}

	closesocket(sock); // 소켓 종료

	WSACleanup(); // 윈속 종료, 사용 중지

	system("pause");
	return 0;
}