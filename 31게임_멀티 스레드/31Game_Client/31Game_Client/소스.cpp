// Winsock2�� ����ϱ� ���� lib �߰�
#pragma comment(lib, "ws2_32.lib") // ������Ʈ �Ӽ� - ��Ŀ - �Է� - �߰� ���Ӽ� (ws2_32.lib)�� �ص� ������ ��ó���� ����
#include <WinSock2.h> // ������ ����ϱ� ���� �������
#include <stdio.h> // ����� �Լ�
#include <stdlib.h> // ǥ�� ���̺귯�� �Լ�

/* ����� ��ȣ��� ���� */
#define SERVERIP "127.0.0.1" // ������ �ּ�
#define SERVERPORT 9000 // ��Ʈ ��ȣ
#define BUFSIZE 512 // ���� ũ��

#define NUMBER_COUNT 3 // �Է� ���� ���� ����
#define GAME_NUMBER_SIZE 31

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

/* �ɰ��� ���� (���� ����)
 * ���̻� ���α׷��� ������ �� ������ ��� */
void err_quit(const char* msg)
{
	LPVOID lpmsgbuf;
	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM,
		NULL,
		WSAGetLastError(), // �ֱ��� ���� �ڵ带 �ѱ�� ����޽����� ���� ���ڿ��� �˷���
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPSTR)&lpmsgbuf,
		0, NULL);
	MessageBox(NULL, (LPCSTR)lpmsgbuf, msg, MB_OK);
	LocalFree(lpmsgbuf);
	exit(-1);
}

// ����� ������ (printf�� ó��)
void err_display(const char* msg)
{
	LPVOID lpmsgbuf;
	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM,
		NULL,
		WSAGetLastError(), // �ֱ��� ���� �ڵ带 �ѱ�� ����޽����� ���� ���ڿ��� �˷��� 
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPSTR)&lpmsgbuf,
		0, NULL);
	printf("[%s] %s\n", msg, (LPSTR)lpmsgbuf);
	LocalFree(lpmsgbuf);
}

/* ����� ���� ������ ���� �Լ�
 * �� ����Ʈ�� ������ ��Ȯ�ϰ� �˶� ����Ѵ�. */
int recvn(SOCKET sock, char* buf, int len, int flags)
{
	char* ptr = buf; // ���� ���� �ּ�
	int left = len; // ���� ���� ���� ������ ũ��
	int recived; // recv() ���ϰ� ������ ����

	while (left > 0) // �����͸� ���� ������ ���� �ݺ�
	{
		recived = recv(sock /* ���� ����� ���� */
			, ptr /* ������ ���� �ּ� */
			, left /* ������ �ִ� ũ�� */
			, flags);

		if (recived == SOCKET_ERROR)
		{
			return SOCKET_ERROR; // ���� �߻��� ����
		}

		// ���� ����
		if (recived == 0)
		{
			break;
		}
		// ������ ����
		left -= recived;
		ptr += recived;
	}

	return (len - left); // ���� ����Ʈ �� (left ���� ����, ���� ���ᰡ �ƴѰ�� 0�̹Ƿ�) ���ϰ��� len
}

/* SELECT_NUM */
int Packing(char* _buf, PROTOCOL _protocol, int _data)
{
	char* ptr = _buf + sizeof(int); // ��ġ ����� ���� (���� ��ġ�� �� ũ��(size)�� ���� �� �ֵ��� int ũ�⸸ŭ ������ ��ġ ����)
	int size = 0; // �� ũ��

	memcpy(ptr, &_protocol, sizeof(PROTOCOL));
	size = size + sizeof(PROTOCOL);
	ptr = ptr + sizeof(PROTOCOL);

	memcpy(ptr, &_data, sizeof(int));
	size = size + sizeof(int); // �� ũ�� ���� 
	ptr = ptr + sizeof(int); // ������ ��ġ ����

	ptr = _buf; // ������ ��ġ ���� �������� ����
	memcpy(ptr, &size, sizeof(int));

	return size + sizeof(int); // �� ũ�� + �ڱ� �ڽ��� ũ��
}

/* ��Ŷ ���� �Լ� (���� �޽���)*/
void UnPacking(const char* _buf, int& _result, char* str1)
{
	const char* ptr = _buf + sizeof(PROTOCOL); // ������ġ�� �������� ũ�⸸ŭ ���ؼ� �ڸ� �������
	int strsize1; // �޽��� ���� ������ ����

	memcpy(&_result, ptr, sizeof(int));
	ptr = ptr + sizeof(int); // ������ ��ġ ����

	memcpy(&strsize1, ptr, sizeof(int));
	ptr = ptr + sizeof(int);

	memcpy(str1, ptr, strsize1);
	ptr = ptr + strsize1;
}

/* ��Ŷ ���� �Լ� (�ȳ� �޽���) */
void UnPacking(const char* _buf, char* str1)
{
	const char* ptr = _buf + sizeof(PROTOCOL); // ������ġ�� int ũ�⸸ŭ ���ؼ� �ڸ� �������
	int strsize1; // �ȳ��޽��� ���� ������ ����

	memcpy(&strsize1, ptr, sizeof(int)); // int ����Ʈ ũ���� ���̸�ŭ ������ ����
	ptr = ptr + sizeof(int); // ������ ��ġ ����

	memcpy(str1, ptr, strsize1);
	ptr = ptr + strsize1;
}

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

/* �ȳ� �޽��� */
void PrintMessage(char* _buf)
{
	char msg[BUFSIZE]; // ���� �޽��� ������ ����
	ZeroMemory(msg, sizeof(msg)); // �޸� �ʱ�ȭ

	UnPacking(_buf, msg); // �޽��� ����ŷ
	printf("%s", msg); // �޽��� ���
}

/* ���� �޽��� */
void PrintErrorMsg(char* _buf)
{
	int result;
	char msg[BUFSIZE]; // ���� �޽��� ������ ����
	ZeroMemory(msg, sizeof(msg)); // �޸� �ʱ�ȭ

	UnPacking(_buf, result, msg); // �����޽��� ����ŷ
	switch (result)
	{
	case DATA_RANGE_ERROR:
		printf("\n%s", msg); // �޽��� ���
		break;
	}
}

/* ���� ��� �޽��� */
void PrintResultMsg(char* _buf)
{
	int result;
	char msg[BUFSIZE]; // ���� �޽��� ������ ����
	ZeroMemory(msg, sizeof(msg)); // �޸� �ʱ�ȭ

	UnPacking(_buf, result, msg); // ���� ��� ����ŷ
	switch (result)
	{
	case WIN: 
	case LOSE:
		printf("\n%s", msg);
		break;
	}
}
/* ���� �Լ� */
int main()
{
	// ���� �ʱ�ȭ
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return 1; // WSAStartup : ���� ���̺귯��(������ �Ҷ� ���̺귯���� ���α׷��� ����) �ʱ�ȭ
	// ���� ���ϰ��� ����

	// socket()
	SOCKET sock = socket(AF_INET/* ���ͳ� �������� ���� */, SOCK_STREAM/* ���� �������� ���� TCP */, 0 /* �׻� 0 */); // IPv4 TCP ���� ����
	if (sock == INVALID_SOCKET)
	{
		err_quit("socket()"); // ������ ������ �ʾ�����
	}

	/* connect() ���� ��û�� ���� ������ �ּ� ���� */
	SOCKADDR_IN serveraddr; // �ּ� ������ ���� ����ü (IPv4)
	ZeroMemory(&serveraddr, sizeof(serveraddr)); // �޸� �ʱ�ȭ
	serveraddr.sin_family = AF_INET; // IPv4 ���ͳ� ��������
	serveraddr.sin_addr.s_addr/* long���� �������� */ = inet_addr(SERVERIP); // ���ڿ��� �̷���� ������ �ּҸ� s_addr�� ����� long������ �ٲ�
	serveraddr.sin_port = htons/* ����Ʈ ���� ���� */(SERVERPORT); // ��Ʈ ��ȣ ����
	int retval = connect(sock, (SOCKADDR*)&serveraddr, sizeof(serveraddr)/* 16 ����Ʈ */);
	if (retval == SOCKET_ERROR)
	{
		err_quit("connect()");
	}

	// ��ſ� ����� ����
	char send_buf[BUFSIZE]; // ������ ����� ����
	char recv_buf[BUFSIZE]; // ������ ����� ����
	bool endflag = false; // �ݺ��� ���� �÷���

	int user_num; // ������ �Է��� ��
	int server_num; // ������ �Է��� ��
	
	int size;

	// ������ ������ ���
	while (1)
	{
		if (!PacketRecv(sock, recv_buf)) // ������ �ޱ�
		{
			break;
		}

		PROTOCOL protocol = GetProtocol(recv_buf); // �޾ƿ� �������� �Ҵ�

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
			PrintMessage(recv_buf); // �ȳ� �޽��� ���
			scanf("%d", &user_num); // ���� �Է�

			size = Packing(send_buf, SELECT_NUM, user_num); // �Է��� �� ��ŷ

			// ������ ������ (�Է��� ����)
			retval = send(sock, send_buf, size, 0); // (����� ����, ���� ������, ���� ũ��, 0)
			if (retval == SOCKET_ERROR) // ���� ����
			{
				err_display("send()");
				break;
			}

			break;

			// ����
		case DATA_ERROR:
			PrintErrorMsg(recv_buf); // ���� �޽��� ���
			break;

			// ���
		case GAME_RESULT:
			PrintResultMsg(recv_buf); // ���Ӱ�� �޽��� ���
			endflag = true; // ���� ���� �÷��� ��
			break;
		}

		if (endflag)
		{
			Sleep(3000);
			break; // �ݺ��� ����
		}
	}

	closesocket(sock); // ���� ����

	WSACleanup(); // ���� ����, ��� ����

	system("pause");
	return 0;
}