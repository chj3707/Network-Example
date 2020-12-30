#pragma comment(lib, "ws2_32")
#include <winsock2.h>
#include <stdlib.h>
#include <stdio.h>

#define SERVERIP   "127.0.0.1"
#define SERVERPORT 9000
#define FILENAMESIZE 256
#define BUFSIZE    4096
#define READSIZE   2048

/* ���� ���ۿ� ����� �������� */
enum PROTOCOL
{
	INTRO = 1, // ���� -> Ŭ�� �޽���
	FILE_INFO,  // Ŭ�� -> ���� ��Ŷ ��������(���� �̸�,�� ũ��)
	FILE_TRANS_DENY, // ���� �޴°� �ź� (�̹� ������ ����) �뷮�� �̸��� ����
	FILE_TRANS_START_POINT, // ��� ���� ������ �˷��� ��������
	FILE_TRANS_WAIT, // ���� ���� ����϶�� �޽��� ������ ����� ��������
	FILE_RESEND, // ���� ������ �޽��� ������ ����� ��������
	FILE_TRANS_DATA // Ŭ�� -> ���� ��Ŷ ��������(���� ������)
};

/* ���� ���� ����ü */
struct _File_info
{
	char filename[FILENAMESIZE]; // ���� �̸�
	int  filesize;	// ���� �� �뷮
	int  nowsize; // ������� ���� �뷮
};

enum DENY_CODE
{
	FILEEXIST = -1
};


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

// ����� ���� ������ ���� �Լ�
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

/* ��ŷ �Լ� (���� �̸�) */
int Packing(char* _buf, PROTOCOL _protocol, char* _filename, int _filesize)
{
	char* ptr = _buf + sizeof(int); // ��ġ ����� ���� (���� ��ġ�� �� ũ��(size)�� ���� �� �ֵ��� int ũ�⸸ŭ ������ ��ġ ����)
	int size = 0; // �� ũ��

	/* �������� */
	memcpy(ptr, &_protocol, sizeof(_protocol));
	size = size + sizeof(_protocol);
	ptr = ptr + sizeof(_protocol);

	/* ���� �̸� ���� */
	int strsize = strlen(_filename); // ���� �̸� ����
	memcpy(ptr, &strsize, sizeof(strsize));
	size = size + sizeof(strsize);
	ptr = ptr + sizeof(strsize);

	/* ���� �̸� */
	memcpy(ptr, _filename, strsize);
	size = size + strsize; // �� ũ�� ���� 
	ptr = ptr + strsize; // ������ ��ġ ����

	/* ���� �� ũ�� */
	memcpy(ptr, &_filesize, sizeof(_filesize));
	size = size + sizeof(_filesize);
	ptr = ptr + sizeof(_filesize);

	ptr = _buf; // ������ ��ġ ���� �������� ����
	memcpy(ptr, &size, sizeof(int));

	return size + sizeof(int); // �� ũ�� + �ڱ� �ڽ��� ũ��
}


/* ��ŷ �Լ� (���� ������) */
int Packing(char* _buf, PROTOCOL _protocol, int _byte, char* _filedata)
{
	char* ptr = _buf + sizeof(int); // ��ġ ����� ���� (���� ��ġ�� �� ũ��(size)�� ���� �� �ֵ��� int ũ�⸸ŭ ������ ��ġ ����)
	int size = 0; // �� ũ��

	/* �������� */
	memcpy(ptr, &_protocol, sizeof(_protocol));
	size = size + sizeof(_protocol); // �� ũ�� ���� 
	ptr = ptr + sizeof(_protocol); // ������ ��ġ ����

	/* 1ȸ�� ������ ����Ʈ */
	memcpy(ptr, &_byte, sizeof(_byte));
	size = size + sizeof(_byte);
	ptr = ptr + sizeof(_byte);

	/* ���� ������ */
	memcpy(ptr, _filedata, _byte); // ������ ����Ʈ ��ŭ
	size = size + _byte;
	ptr = ptr + _byte;

	ptr = _buf; // ������ ��ġ ���� �������� ����
	memcpy(ptr, &size, sizeof(int));

	return size + sizeof(int); // �� ũ�� + �ڱ� �ڽ��� ũ��
}

/* ����ŷ �Լ� (INTRO) */
void UnPacking(char* _buf, char* _msg)
{
	const char* ptr = _buf + sizeof(PROTOCOL);
	int strsize;

	/* ��Ʈ�� �޽��� ���ڿ� ũ�� */
	memcpy(&strsize, ptr, sizeof(strsize));
	ptr = ptr + sizeof(strsize);

	/* ��Ʈ�� �޽��� */
	memcpy(_msg, ptr, strsize);
	ptr = ptr + strsize;
}

/* ����ŷ �Լ�(FILE_TRANS_DENY) */
void UnPacking(char* _buf, int& _data, char* _msg)
{
	const char* ptr = _buf + sizeof(PROTOCOL);
	int strsize;

	/* (DENY_CODE) */
	memcpy(&_data, ptr, sizeof(_data));
	ptr = ptr + sizeof(_data);

	/* �ź� �޽��� ���ڿ� ũ�� */
	memcpy(&strsize, ptr, sizeof(strsize));
	ptr = ptr + sizeof(strsize);

	/* �ź� �޽��� */
	memcpy(_msg, ptr, strsize);
	ptr = ptr + strsize;
}

/* ����ŷ �Լ�(FILE_TRANS_START_POINT) */
void UnPacking(char* _buf, int& _data)
{
	const char* ptr = _buf + sizeof(PROTOCOL);

	/* ���� ���� �������� */
	memcpy(&_data, ptr, sizeof(_data));
	ptr = ptr + sizeof(_data);
}

int main(int argc, char* argv[])
{
	char send_buf[BUFSIZE]; // �۾���(���� ������)
	char recv_buf[BUFSIZE]; // ���� ������
	int size; // ��Ŷ ũ��
	int retval; // ���� ��

	// ���� �ʱ�ȭ
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
	ZeroMemory(&info, sizeof(_File_info)); // �ʱ�ȭ

	char msg[BUFSIZE]; // �޽��� ���� ����
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
			/* ��Ʈ�� �޽��� */
		case INTRO:
			ZeroMemory(msg, sizeof(msg)); // �ʱ�ȭ
			UnPacking(recv_buf, msg); // ���� ��Ʈ�� �޽��� ����ŷ
			printf("%s\n", msg);
			
			scanf("%s", &info.filename);
			// ���� ���ϸ� ������ ����
			//strcpy(info.filename, argv[1]); // argv[1] �Ӽ� ����� ����μ�
			
			// ���� ����
			fp = fopen(info.filename, "rb");
			if (fp == NULL) {
				perror("fopen()");
				break;
			}
			fseek(fp, 0, SEEK_END); // ���� ���ۺ��� ������
			info.filesize = ftell(fp); // ���� ��ü ũ�� �Ҵ�
			fclose(fp); // ũ�� �Ҵ� �ް� ���� ����

			size = Packing(send_buf, FILE_INFO, info.filename, info.filesize); // ���� �̸�, ���� �� ũ�� ��ŷ
			retval = send(sock, send_buf, size, 0); // ���� �̸�, ��ü ũ�� ������
			if (retval == SOCKET_ERROR) err_quit("send()");

			break;

			/* �ź� �޽��� */
		case FILE_TRANS_DENY:
			int result; 
			ZeroMemory(msg, sizeof(msg)); // �ʱ�ȭ
			UnPacking(recv_buf, result, msg); // �ź� �޽��� ����ŷ

			switch (result)
			{
			case FILEEXIST:
				printf("%s\n", msg); // �ź� �޽��� ���
				endflag = true;
				break;
			}
			break;

			/* ������,��� �޽��� */
		case FILE_RESEND:
		case FILE_TRANS_WAIT:
			ZeroMemory(msg, sizeof(msg));
			UnPacking(recv_buf, msg); // �޽��� ����ŷ
			printf("%s\n", msg);

			break;
			/* ���� ���� ������ */
		case FILE_TRANS_START_POINT:
			UnPacking(recv_buf, info.nowsize);
			fp = fopen(info.filename, "rb"); // �б� ���� ���� ����
			if (fp == NULL)
			{
				perror("fopen()");
				break;
			}
			fseek(fp, info.nowsize, SEEK_SET); // ������ ���ۺ��� ���� �������� ���� (�������� ���� ��������)
			while (1)
			{
				char filedata[BUFSIZE]; // ���� ���� ������ �۾���
				int nbytes = fread(filedata, 1, READSIZE, fp); // ���� ���� 2048��ŭ filedata�� ����(BUFSIZE��ŭ �ϸ� send_bufũ�⺸�� Ŀ�� ����)

				if (nbytes == 0) // ���о����� ����(���� �����Ͱ� ������)
				{
					endflag = true;
					break; // �ݺ��� ����
				}
				size = Packing(send_buf, FILE_TRANS_DATA, nbytes, filedata); // ���� ������ ��ŷ
				retval = send(sock, send_buf, size, 0); // ���� ������ ������
				if (retval == SOCKET_ERROR)
				{
					err_quit("send()");
				}
				printf("...");
				Sleep(300); // 0.3�� ����
				info.nowsize += nbytes; // ���� ����Ʈ ��ŭ ������
			}
			fclose(fp); // ���� �ݱ�

			break;
		}

		if (endflag) 
		{
			if (info.nowsize == info.filesize) // ���� ��ü ũ�⸸ŭ ���� �� ���
			{
				printf("���� ���� ����!\n");
				break;
			}
			else
			{
				printf("���� ���� ����\n");
				break;
			}
		}
	}
	// closesocket()
	closesocket(sock);

	// ���� ����
	WSACleanup();

	system("pause");
	return 0;
}