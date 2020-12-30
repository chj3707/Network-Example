#pragma comment(lib, "ws2_32.lib")
#include <winsock2.h>
#include <stdlib.h>
#include <stdio.h>

#define BUFSIZE 4096
#define FILENAMESIZE 256
#define CLIENTMAX 100
#define FILETRANSMAX 10
#define NODATA -1

#define INTRO_MSG "������ ���ϸ��� �Է��ϼ���"
#define FILE_DUPLICATE_MSG "�����ϰ��� �ϴ� ������ �̹� ������ �����ϴ� �����Դϴ�."
#define FILE_TRANS_WAIT_MSG "���� ������ �̹� ������ �ֽ��ϴ�. ����� �ּ���.\n"
#define FILE_RESEND_MSG "������� Ŭ���̾�Ʈ�� �̾ �����մϴ�.\n"
CRITICAL_SECTION cs;

/* ���� ���ۿ� ����� �������� */
enum PROTOCOL 
{ 
	INTRO=1, // ���� -> Ŭ�� �޽���
	FILE_INFO,  // Ŭ�� -> ���� ��Ŷ ��������(���� �̸�,�� ũ��)
	FILE_TRANS_DENY, // ���� �޴°� �ź� (�̹� ������ ����) �뷮�� �̸��� ����
	FILE_TRANS_START_POINT, // ��� ���� ������ �˷��� ��������
	FILE_TRANS_WAIT, // ���� ���� ����϶�� �޽��� ������ ����� ��������
	FILE_RESEND, // ���� ������ �޽��� ������ ����� ��������
	FILE_TRANS_DATA // Ŭ�� -> ���� ��Ŷ ��������(���� ������)
};

/* Ŭ���̾�Ʈ ���� */
enum STATE
{
	INIT_STATE=1, // ��Ʈ��
	FILE_TRANS_READY_STATE, // ���� ���� �غ� �ܰ�
	FILE_TRANS_STATE, // ������ �޾Ƽ� ���� �ܰ�
	FILE_TRANS_WAIT_STATE, // �ٸ� Ŭ���̾�Ʈ�� ������ ������ ��ٸ��� ����
	FILE_TRANS_END_STATE, // ������ �������ų� �Ϸ�� �ܰ�
	DISCONNECTED_STATE
};

enum DENY_CODE
{
	FILEEXIST = -1
};

struct _FileInfo
{
	char filename[FILENAMESIZE]; // ���� �̸�
	int  filesize;	// ���� �� �뷮
	int  nowsize; // ������� ���� �뷮
};

struct _ClientInfo;

/* ���� ���� ���� ����ü */
struct _FileTransInfo
{
	_ClientInfo* trans_client[CLIENTMAX]; // ���� ������ �����ϴ� Ŭ���̾�Ʈ
	_ClientInfo* cur_trans_client; // ���� �������� Ŭ���̾�Ʈ
	int client_count; // ���� ������ �����ϴ� Ŭ���̾�Ʈ ��
};

/* Ŭ���̾�Ʈ ���� ����ü */
struct _ClientInfo
{
	SOCKET sock;
	SOCKADDR_IN addr;
	STATE	state; // Ŭ���̾�Ʈ ����
	_FileInfo file_info; // ���� ����

	_FileTransInfo* trans_info; // Ŭ�� ���� ���� ���� ����
	HANDLE wait_event; // ��� �̺�Ʈ �ڵ�
	int trans_turn_number; // ���� ���� ���ϴ� �뵵

	char recv_buf[BUFSIZE];
	char send_buf[BUFSIZE];
};

_FileTransInfo* TransInfo[FILETRANSMAX];
int TransCount = 0;
_ClientInfo* ClientInfo[CLIENTMAX];
int count;

/* �Լ� ���� */
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
	InitializeCriticalSection(&cs); // �ʱ�ȭ

	int retval;

	// ���� �ʱ�ȭ
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

	// ������ ��ſ� ����� ����

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

		_ClientInfo* ClientPtr = AddClientInfo(client_sock, clientaddr); // Ŭ������ �߰�		
		
		HANDLE hThread = CreateThread(nullptr, 0, ProcessClient, ClientPtr, 0, nullptr); // ������ ����
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

	// ���� ����
	WSACleanup();
	return 0;
}

// ���� �Լ� ���� ��� �� ����
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

// ���� �Լ� ���� ���
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
	ptr->wait_event = CreateEvent(nullptr, false/* �ڵ� */, false/* ���ȣ ���� */, nullptr); // �̺�Ʈ ����

	ClientInfo[count++] = ptr;
	
	LeaveCriticalSection(&cs);

	printf("\nFileSender ����: IP �ּ�=%s, ��Ʈ ��ȣ=%d\n",
		inet_ntoa(ptr->addr.sin_addr), ntohs(ptr->addr.sin_port));

	return ptr;
}

void ReMoveClientInfo(_ClientInfo* ptr)
{
	
	printf("FileSender ����: IP �ּ�=%s, ��Ʈ ��ȣ=%d\n",
		inet_ntoa(ptr->addr.sin_addr), ntohs(ptr->addr.sin_port));

	EnterCriticalSection(&cs);

	for (int i = 0; i<count; i++)
	{
		if (ClientInfo[i] == ptr)
		{
			closesocket(ptr->sock); // ���� �ݱ�
			CloseHandle(ptr->wait_event); // �̺�Ʈ �ڵ� �ݱ�
			delete ptr; // �޸� ����
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

/* ���� ���� ���� �߰� */
_FileTransInfo* AddFileTransInfo(_ClientInfo* _ptr)
{
	EnterCriticalSection(&cs); // ������ ��ȣ

	_FileTransInfo* trans_ptr = nullptr; // ��ü ����
	int index = NODATA;

	for (int i = 0; i < TransCount; i++)
	{
		TransInfo[i]->trans_client[TransInfo[i]->client_count++] = _ptr; // ������ Ŭ�� ����
		_ptr->trans_info = TransInfo[i]; // ���� ���� ���� ���� ����
		_ptr->trans_turn_number = TransInfo[i]->client_count; // ���� ����
		TransInfo[i]->cur_trans_client = TransInfo[i]->trans_client[0];

		trans_ptr = TransInfo[i];
		index = i;
		break;
	}

	/* ���� ���� ���� Ŭ���̾�Ʈ */
	if (index == NODATA)
	{
		trans_ptr = new _FileTransInfo;
		ZeroMemory(trans_ptr, sizeof(_FileTransInfo)); // �ʱ�ȭ

		trans_ptr->trans_client[0] = _ptr; // ���� ���� ���� Ŭ�� ����
		trans_ptr->cur_trans_client = _ptr; // ���� ���� ���� Ŭ����� ����
		trans_ptr->client_count++; // ���� ī��Ʈ +1
		TransInfo[TransCount++] = trans_ptr; // ���� ���� �迭�� ����

		_ptr->trans_info = trans_ptr; // ���� ���� ���� ���� ����
		_ptr->trans_turn_number = trans_ptr->client_count; // ���� ����(1��)
	}

	LeaveCriticalSection(&cs);
	return trans_ptr;
}

/* ���� �����ϴٰ� ���� Ŭ���̾�Ʈ ���� */
void ReMoveFileTransClient(_ClientInfo* _ptr)
{
	EnterCriticalSection(&cs);
	_FileTransInfo* trans_info = _ptr->trans_info; // Ŭ�� ���� �������� ��ü ����

	for (int i = 0; i < trans_info->client_count; i++) // Ŭ�� ���� ��ŭ �ݺ�
	{
		if (_ptr == trans_info->trans_client[i]) // Ŭ�� �迭���� ������ Ŭ�� ã�Ƽ�
		{
			for (int j = i; j < trans_info->client_count - 1; j++)
			{
				trans_info->trans_client[j] = trans_info->trans_client[j + 1]; // �ε��� �մ�� �ֱ�
			}

			trans_info->client_count--; // Ŭ�� ���� -1

			if (trans_info->client_count == 0) // Ŭ�� �ϳ��� ������
			{
				ReMoveFileTransInfo(trans_info); // �������� ����
			}
		}
	}

	LeaveCriticalSection(&cs);
}

/* ���� ���� ���� ���� */
void ReMoveFileTransInfo(_FileTransInfo* _ptr)
{
	EnterCriticalSection(&cs);

	for (int i = 0; i < TransCount; i++)
	{
		if (TransInfo[i] == _ptr) // ���� ���� �迭���� �ش� ������ ã�Ƽ�
		{
			delete _ptr; // �޸� ����
			for (int j = i; j < TransCount - 1; j++)
			{
				TransInfo[j] = TransInfo[j + 1]; // ������ �������� �ε��� �մ�� ����
			}
		}
	}
	TransCount--;
	LeaveCriticalSection(&cs);
}

/* ������ �ִ��� ������ Ȯ���ϴ� �Լ� */
bool SearchFile(const char *filename)
{
	EnterCriticalSection(&cs);
	WIN32_FIND_DATA FindFileData; // ���� ������ ���� ����ü
	HANDLE hFindFile = FindFirstFile(filename, &FindFileData);
	if (hFindFile == INVALID_HANDLE_VALUE) // ������ ���� ���
	{
		LeaveCriticalSection(&cs);
		return false;
	}
	else // ������ �ִ� ��� 
	{
		FindClose(hFindFile);
		LeaveCriticalSection(&cs);
		return true;
	}
}
// ����� ���� ������ ���� �Լ�
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

/* ��Ʈ�� �޽��� ������ ���μ��� */
void InitProcess(_ClientInfo* _ptr)
{	
	int size = PackPacket(_ptr->send_buf, INTRO, INTRO_MSG);

	int retval = send(_ptr->sock, _ptr->send_buf, size, 0); // ��Ʈ�� �޽��� ������
	if (retval == SOCKET_ERROR)
	{
		err_display("send()");
		_ptr->state = DISCONNECTED_STATE;
		return;
	}
	_ptr->state = FILE_TRANS_READY_STATE;
}

/* ���� ���� �غ� ���μ��� */
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

		UnPackPacket(_ptr->recv_buf, filename, filesize); // ���� �̸�, ���� ũ�� ����

		printf("-> ���� ���� �̸�: %s\n", filename);
		printf("-> ���� ���� ũ��: %d\n", filesize);

		/* �̸��� ���� ������ �ִ� ��� */
		if (SearchFile(filename))
		{
			FILE* fp = fopen(filename, "rb"); // �б� ���� ���� ����
			fseek(fp, 0, SEEK_END);
			int fsize = ftell(fp); // ���� �� ũ��
			fclose(fp); // ũ�� ���ϰ� ����

			if (filesize == fsize) // �̸��� �뷮�� ��� ���� ���(�̹� �ִ� ����)
			{
				printf("�����ϴ� ���� ���� �䱸\n");

				int size = PackPacket(_ptr->send_buf, FILE_TRANS_DENY, FILEEXIST, FILE_DUPLICATE_MSG);

				retval = send(_ptr->sock, _ptr->send_buf, size, 0); // �ź� ��Ŷ ������
				if (retval == SOCKET_ERROR)
				{
					err_display("send()");
					_ptr->state = DISCONNECTED_STATE;
					return;
				}

				if (!PacketRecv(_ptr->sock, _ptr->recv_buf)) // Ŭ�� ���� ���
				{
					_ptr->state = DISCONNECTED_STATE;
					return;
				}
			}
			else // �̹� �ִ� ���� ������ ���� ���ϰ� �뷮�� �ٸ� ���
			{
				strcpy(_ptr->file_info.filename, filename);
				_ptr->file_info.filesize = filesize;
				_ptr->file_info.nowsize = fsize;
			
				/* ���� ���� ������ ������ Ŭ���̾�Ʈ */
				if (_ptr == trans_info->cur_trans_client)
				{
					_ptr->state = FILE_TRANS_STATE;
				}
				/* ������ ���߿� ������ Ŭ���̾�Ʈ �ε� ���� �̸�, ��üũ�Ⱑ ������ ��� ���� */
				else
				{
					_ptr->state = FILE_TRANS_WAIT_STATE;
					break;
				}
			}

		}

		/* �̸��� ���� ������ ���� ��� */
		else
		{
			strcpy(_ptr->file_info.filename, filename);
			_ptr->file_info.filesize = filesize;
			_ptr->file_info.nowsize = 0;
			_ptr->state = FILE_TRANS_STATE;
		}

		int size = PackPacket(_ptr->send_buf, FILE_TRANS_START_POINT, _ptr->file_info.nowsize); // ������ ���� ũ�⸦ ��ŷ(��� ���� ������ �˼� �ֵ���)

		retval = send(_ptr->sock, _ptr->send_buf, size, 0); // ���� ũ�� ��Ŷ ����
		if (retval == SOCKET_ERROR)
		{
			err_display("send()");
			_ptr->state = DISCONNECTED_STATE;
			return;
		}

		break;
	}

}

/* ���� ���� ���μ��� */
void FileTransProcess(_ClientInfo* _ptr)
{
	static FILE* fp = nullptr;
	char buf[BUFSIZE];	

	if (!PacketRecv(_ptr->sock, _ptr->recv_buf))
	{
		/* ������ �޴� ���� ���� */
		_ptr->state = FILE_TRANS_END_STATE;	// ���� ���� �Ϸ� ���·� ����	
		fclose(fp);
		fp = nullptr;
		return;
	}

	PROTOCOL protocol = GetProtocol(_ptr->recv_buf); // �������� �и�

	switch (protocol)
	{
	case FILE_TRANS_DATA:
		if (fp==nullptr)
		{			
			fp = fopen(_ptr->file_info.filename, "ab"); // �̾� ���� ���� append���� ����
		}

		int transsize;
		UnPackPacket(_ptr->recv_buf, transsize, buf);
		fwrite(buf, 1, transsize, fp);
		if (ferror(fp)) 
		{
			perror("���� ����� ����");
			_ptr->state = FILE_TRANS_END_STATE;
			fclose(fp);
			return;
		}
		_ptr->file_info.nowsize += transsize;
		break;
	}
	
}

/* �ٸ� Ŭ�� �۾��� �Ϸ��Ҷ� ���� ����ϴ� ���μ��� */
void WaitProcess(_ClientInfo* _ptr)
{
	int size = PackPacket(_ptr->send_buf, FILE_TRANS_WAIT, FILE_TRANS_WAIT_MSG); // ��� �޽��� ��ŷ
	int retval = send(_ptr->sock, _ptr->send_buf, size, 0); // ����
	if (retval == SOCKET_ERROR)
	{
		err_display("send()");
		_ptr->state = DISCONNECTED_STATE;
		return;
	}

	WaitForSingleObject(_ptr->wait_event, INFINITE); // �̺�Ʈ ��ȣ ���

	size = PackPacket(_ptr->send_buf, FILE_RESEND, FILE_RESEND_MSG); // ������ �˸� �޽��� ��ŷ

	retval = send(_ptr->sock, _ptr->send_buf, size, 0); // �޽��� ����
	if (retval == SOCKET_ERROR)
	{
		err_display("send()");
		_ptr->state = DISCONNECTED_STATE;
		return;
	}

	FILE* fp = fopen(_ptr->file_info.filename, "rb"); // �б� ���� ���� ����
	fseek(fp, 0, SEEK_END);
	int fsize = ftell(fp); // ���� �� ũ��
	fclose(fp); // ũ�� ���ϰ� ����

	_ptr->file_info.nowsize = fsize; // �̹� ������ �ִ� ũ�� ����

	size = PackPacket(_ptr->send_buf, FILE_TRANS_START_POINT, _ptr->file_info.nowsize); // ������ ���� ũ�⸦ ��ŷ(��� ���� ������ �˼� �ֵ���)

	retval = send(_ptr->sock, _ptr->send_buf, size, 0); // ���� ũ�� ��Ŷ ����
	if (retval == SOCKET_ERROR)
	{
		err_display("send()");
		_ptr->state = DISCONNECTED_STATE;
		return;
	}
	_ptr->state = FILE_TRANS_STATE;
}

/* ���� �����ϴ� Ŭ�� �����Ҷ� ���� �������� �ٸ� Ŭ���̾�Ʈ�� ã�� �Լ� */
_ClientInfo* SearchNextFileTransClient(_ClientInfo* _ptr)
{
	int index;

	EnterCriticalSection(&cs);
	_FileTransInfo* trans_info = _ptr->trans_info; // Ŭ�� ���� �������� ��ü ����
	_ClientInfo* ptr = nullptr;

	// ���� ������� Ŭ�� �ϳ��̻� ��������
	if (trans_info->client_count >= 2)
	{
		/* ���������� ���� Ŭ���̾�Ʈ ����ŭ �ݺ� */
		for (int i = 0; i < trans_info->client_count; i++)
		{
			if (_ptr != trans_info->trans_client[i]) // �����ϴ� Ŭ�� �ƴ� Ŭ�� �߿���
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

/* �������� ���μ��� */
void DisconnectedProcess(_ClientInfo* _ptr)
{
	/* Ŭ�� ����� �������� �������� Ŭ�� ã�� */
	_ClientInfo* next_ptr = SearchNextFileTransClient(_ptr);
	if (next_ptr != nullptr)
	{
		SetEvent(next_ptr->wait_event); // ��� �̺�Ʈ ��ȣ ���·� ����
	}

	ReMoveFileTransClient(_ptr); // ������ Ŭ�� ����
	ReMoveClientInfo(_ptr); // ������ Ŭ�� ���� ����
}

DWORD WINAPI ProcessClient(LPVOID arg)
{
	_ClientInfo* ptr = (_ClientInfo*)arg;
	bool endflag = false;

	while (1)
	{

		switch (ptr->state)
		{
			/* ��Ʈ�� */
		case INIT_STATE:
			InitProcess(ptr);
			break;

			/* ���� ���� �غ� */
		case FILE_TRANS_READY_STATE:
			ReadyProcess(ptr);
			break;

			/* ���� ���� */
		case FILE_TRANS_STATE:
			FileTransProcess(ptr);
			break;

			/* ���� ���� ��� ����(�ٸ� Ŭ�� ������ ���� ��) */
		case FILE_TRANS_WAIT_STATE:
			WaitProcess(ptr);
			break;

			/* ���� �Ϸ� (���� or ����) */
		case FILE_TRANS_END_STATE:
			FileTransEndProcess(ptr);
			break;

			/* Ŭ�� ���� ���� */
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
		printf("���ۼ���!!\n");
	}
	else
	{
		printf("���۽���!!\n");
	}

	_ptr->state = DISCONNECTED_STATE;
}