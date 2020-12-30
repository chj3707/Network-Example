#pragma comment(lib, "ws2_32")
#include <winsock2.h>
#include <stdlib.h>
#include <stdio.h>
#include "resource.h"

#define SERVERIP   "127.0.0.1"
#define SERVERPORT 9000
#define BUFSIZE 4096
#define NICKNAMESIZE 255
#define THREAD_COUNT 2

enum PROTOCOL
{
	INTRO,
	NICKNAME_LIST, // �г��� ����Ʈ ������ ����� ��������
	CHATT_NICKNAME, // �г��� �Է��Ҷ� ���� ��������
	JOIN_CHATTROOM, // Ŭ�� ������ ä�ù��� ������ ����� ��������
	CONNECT_ERROR, // ���� ���� �϶� ���� �ڵ�� �Բ� ���� ��������
	INIT_SETTING, // �г���, ������ ä�ù� ������ ����� ��������
	USER_ENTER, // ������ ���忡 ���������� ����� ��������
	USER_STATE_CHANGE, // ���ӿ� ������ ������ ���¸� �����Ҷ� ����� ��������
	CHATT_MSG, // ä�� ���� ������ ����� ��������
	CHATT_OUT
};


enum ERRORCODE
{
	NICKNAME_EROR, // �̹� �ִ� �г���
	CHATTROOM_FULL,  // ���� �� ä�ù�
	CHATTROOM_CODE_EROR, // �� ��ȣ �Է� ����
	CHATTROOM_RANGE_EROR // �� ��ȣ ���� ����
};

enum CLOSECHATT
{
	LEAVE,
};

enum STATE
{
	INITE_STATE,
	CHATT_INITE_STATE,
	CHATT_ROOM_STATE,
	CHATTING_STATE,
	CHATT_OUT_STATE
};

struct _MyInfo
{
	SOCKET sock;
	STATE state;
	char sendbuf[BUFSIZE];
	char recvbuf[BUFSIZE];
}*MyInfo;

bool PacketRecv(SOCKET, char*);

// ��ȭ���� ���ν���
BOOL CALLBACK DlgProc(HWND, UINT, WPARAM, LPARAM);
// ���� ��Ʈ�� ��� �Լ�
void DisplayText(char *fmt, ...);
// ���� ��� �Լ�
void err_quit(const char*);
void err_display(const char*);
// ����� ���� ������ ���� �Լ�
int recvn(SOCKET , char*, int, int);
// ���� ��� ������ �Լ�
DWORD WINAPI ClientMain(LPVOID);
DWORD CALLBACK RecvThread(LPVOID);

PROTOCOL GetProtocol(const char*);
int PackPacket(char*, PROTOCOL, const char*);
int PackPacket(char* _buf, PROTOCOL _protocol);
void UnPackPacket(const char*, char*, int&);
void UnPackPacket(const char*, char*);
void UnPackPacket(const char* _buf, int& _num, char* _str);

char buf[BUFSIZE+1]; // ������ �ۼ��� ����
HANDLE hReadEvent, hWriteEvent; // �̺�Ʈ
HWND hSendButton; // ������ ��ư
HWND hEdit1, hEdit2; // ���� ��Ʈ��
//HANDLE hClientMain, hRecvThread;
HANDLE hThread[THREAD_COUNT];

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow)
{
	// �̺�Ʈ ����
	hReadEvent = CreateEvent(NULL, FALSE, TRUE, NULL);
	if(hReadEvent == NULL) return 1;
	hWriteEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	if(hWriteEvent == NULL) return 1;
		
	// ���� ��� ������ ����	
	// ��ȭ���� ����

	MyInfo = new _MyInfo;
	memset(MyInfo, 0, sizeof(_MyInfo));

	hThread[0]= CreateThread(NULL, 0, ClientMain, NULL, 0, NULL);

	DialogBox(hInstance, MAKEINTRESOURCE(IDD_DIALOG1), NULL, DlgProc);


	WaitForMultipleObjects(THREAD_COUNT, hThread, TRUE, INFINITE);

	// �̺�Ʈ ����
	CloseHandle(hWriteEvent);
	CloseHandle(hReadEvent);
	for (int i = 0; i < THREAD_COUNT; i++)
	{
		CloseHandle(hThread[i]);
	}


	// closesocket()
	closesocket(MyInfo->sock);

	// ���� ����
	WSACleanup();
	return 0;
}

// ��ȭ���� ���ν���
BOOL CALLBACK DlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg){
	case WM_INITDIALOG:
		hEdit1 = GetDlgItem(hDlg, IDC_EDIT1);
		hEdit2 = GetDlgItem(hDlg, IDC_EDIT2);
		hSendButton = GetDlgItem(hDlg, IDOK);		
		SendMessage(hEdit1, EM_SETLIMITTEXT, BUFSIZE, 0);	
		hThread[1] = CreateThread(NULL, 0, RecvThread, NULL, 0, NULL);
		MyInfo->state = INITE_STATE;
		return TRUE;

	case WM_COMMAND:
		switch(LOWORD(wParam)){
		case IDOK:
			EnableWindow(hSendButton, FALSE); // ������ ��ư ��Ȱ��ȭ
			WaitForSingleObject(hReadEvent, INFINITE); // �б� �Ϸ� ��ٸ���
			GetDlgItemText(hDlg, IDC_EDIT1, buf, BUFSIZE + 1);
			SetEvent(hWriteEvent); // ���� �Ϸ� �˸���
			SetWindowText(hEdit1, "");
			SetFocus(hEdit1);			
			return TRUE;

		case IDCANCEL:
			WaitForSingleObject(hReadEvent, INFINITE); // �б� �Ϸ� ��ٸ���
			MyInfo->state = CHATT_OUT_STATE;
			SetEvent(hWriteEvent); // ���� �Ϸ� �˸���
			EndDialog(hDlg, IDCANCEL);
			return TRUE;
		}
		return FALSE;
	}
	return FALSE;
}

// ���� ��Ʈ�� ��� �Լ�
void DisplayText(char *fmt, ...)
{
	va_list arg;
	va_start(arg, fmt);

	char cbuf[BUFSIZE+256];
	vsprintf(cbuf, fmt, arg);

	int nLength = GetWindowTextLength(hEdit2);
	SendMessage(hEdit2, EM_SETSEL, nLength, nLength);
	SendMessage(hEdit2, EM_REPLACESEL, FALSE, (LPARAM)cbuf);

	va_end(arg);
}

// ���� �Լ� ���� ��� �� ����
void err_quit(const char *msg)
{
	LPVOID lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	MessageBox(NULL, (LPCTSTR)lpMsgBuf, msg, MB_ICONERROR);
	LocalFree(lpMsgBuf);
	exit(1);
}

// ���� �Լ� ���� ���
void err_display(const char *msg)
{
	LPVOID lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	DisplayText("[%s] %s", msg, (char *)lpMsgBuf);
	LocalFree(lpMsgBuf);
}

// ����� ���� ������ ���� �Լ�
int recvn(SOCKET s, char *buf, int len, int flags)
{
	int received;
	char *ptr = buf;
	int left = len;

	while(left > 0){
		received = recv(s, ptr, left, flags);
		if(received == SOCKET_ERROR)
			return SOCKET_ERROR;
		else if(received == 0)
			break;
		left -= received;
		ptr += received;
	}

	return (len - left);
}

// TCP Ŭ���̾�Ʈ ���� �κ�
DWORD WINAPI ClientMain(LPVOID arg)
{
	int retval;
	int size;

	// ���� �ʱ�ȭ
	WSADATA wsa;
	if(WSAStartup(MAKEWORD(2,2), &wsa) != 0)
		return 1;

	// socket()
	MyInfo->sock = socket(AF_INET, SOCK_STREAM, 0);
	if(MyInfo->sock == INVALID_SOCKET) err_quit("socket()");

	
	// connect()
	SOCKADDR_IN serveraddr;
	ZeroMemory(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = inet_addr(SERVERIP);
	serveraddr.sin_port = htons(SERVERPORT);
	retval = connect(MyInfo->sock, (SOCKADDR *)&serveraddr, sizeof(serveraddr));
	if(retval == SOCKET_ERROR) err_quit("connect()");

	
	char msg[BUFSIZE];
	bool endflag = false;

	while(1)
	{		
		WaitForSingleObject(hWriteEvent, INFINITE); // ���� �Ϸ� ��ٸ���

		// ���ڿ� ���̰� 0�̸� ������ ����
		if(MyInfo->state!= CHATT_OUT_STATE && strlen(buf) == 0)
		{
			EnableWindow(hSendButton, TRUE); // ������ ��ư Ȱ��ȭ
			SetEvent(hReadEvent); // �б� �Ϸ� �˸���
			continue;
		}

		switch (MyInfo->state)
		{		
			// �г��� ����
		case CHATT_INITE_STATE:		
			size = PackPacket(MyInfo->sendbuf, CHATT_NICKNAME, buf);
			retval = send(MyInfo->sock, MyInfo->sendbuf, size, 0);
			if (retval == SOCKET_ERROR)
			{
				err_display("send()");
				MyInfo->state = CHATT_OUT_STATE;
				break;
			}
			break;

			// ä�ù� ����
		case CHATT_ROOM_STATE:
			size = PackPacket(MyInfo->sendbuf, JOIN_CHATTROOM, buf);
			retval = send(MyInfo->sock, MyInfo->sendbuf, size, 0);
			if (retval == SOCKET_ERROR)
			{
				err_display("send()");
				MyInfo->state = CHATT_OUT_STATE;
				break;
			}
			break;

			// ä�� ���ڿ� ����
		case CHATTING_STATE:
			size = PackPacket(MyInfo->sendbuf, CHATT_MSG, buf);
			retval = send(MyInfo->sock, MyInfo->sendbuf, size, 0);
			if (retval == SOCKET_ERROR)
			{
				err_display("send()");
				MyInfo->state = CHATT_OUT_STATE;
				break;
			}
			break;

			// ���� ���� �������� ����
		case CHATT_OUT_STATE:
			size = PackPacket(MyInfo->sendbuf, CHATT_OUT);
			retval = send(MyInfo->sock, MyInfo->sendbuf, size, 0);
			if (retval == SOCKET_ERROR)
			{
				err_display("send()");
				MyInfo->state = CHATT_OUT_STATE;
				break;
			}

			endflag = true;
			break;

		}
		
		EnableWindow(hSendButton, TRUE); // ������ ��ư Ȱ��ȭ
		SetEvent(hReadEvent); // �б� �Ϸ� �˸���

		if (endflag)
		{
			break;
		}
	}

	return 0;
}

bool PacketRecv(SOCKET _sock, char* _buf)
{
	int size;

	int retval = recvn(_sock, (char*)&size, sizeof(size), 0);
	if (retval == SOCKET_ERROR)
	{
		err_display("recv error()");
		return false;
	}
	else if (retval == 0)
	{
		return false;
	}

	retval = recvn(_sock, _buf, size, 0);
	if (retval == SOCKET_ERROR)
	{
		err_display("recv error()");
		return false;

	}
	else if (retval == 0)
	{
		return false;
	}

	return true;
}

DWORD CALLBACK RecvThread(LPVOID _ptr)
{
	PROTOCOL protocol;
	int size;	
	char nickname[NICKNAMESIZE];
	char msg[BUFSIZE];
	int count;	

	bool endflag = false;
	while (1)
	{
		if (!PacketRecv(MyInfo->sock, MyInfo->recvbuf))
		{
			err_display("recv error()");
			return -1;
		}

		protocol=GetProtocol(MyInfo->recvbuf);

		switch (protocol)
		{
			/* ��Ʈ�� */
		case INTRO:
			memset(msg, 0, sizeof(msg));
			UnPackPacket(MyInfo->recvbuf, msg);
			DisplayText("%s\r\n", msg);
			MyInfo->state = CHATT_INITE_STATE;
			break;

			/* ���� �޽��� */
		case CONNECT_ERROR:
			int code;
			memset(msg, 0, sizeof(msg));
			UnPackPacket(MyInfo->recvbuf, code, msg);

			switch (code)
			{
			case NICKNAME_EROR: // �г��� ����
				DisplayText("%s\r\n", msg);
				MyInfo->state = CHATT_INITE_STATE;
				break;

			case CHATTROOM_FULL: // ���� ���� á����
			case CHATTROOM_CODE_EROR: // �� ��ȣ �Է� ����
			case CHATTROOM_RANGE_EROR: // �� ��ȣ ���� ����
				DisplayText("%s\r\n", msg);
				MyInfo->state = CHATT_ROOM_STATE;
				break;
			}
			break;

			/* ä�� �޽��� */
		case CHATT_MSG:
			memset(msg, 0, sizeof(msg));
			UnPackPacket(MyInfo->recvbuf, msg);
			DisplayText("%s\r\n", msg);
			break;

			/* ���� ����Ʈ ��� */
		case NICKNAME_LIST:
			memset(nickname, 0, sizeof(nickname));
			UnPackPacket(MyInfo->recvbuf, nickname, count);
			DisplayText("%s\r\n", nickname); // �г��� ����Ʈ ���
			break;

			/* ���� �޽��� ��� */
		case USER_ENTER:
			memset(msg, 0, sizeof(msg));
			UnPackPacket(MyInfo->recvbuf, msg);
			DisplayText("%s\r\n", msg);
			break;

			/* ���ӿ� ������ ���� ���º��� */
		case USER_STATE_CHANGE:
			memset(msg, 0, sizeof(msg));
			UnPackPacket(MyInfo->recvbuf, msg);
			// ���������� �޾����� Ŭ�� ���¿� ���� ����
			if (MyInfo->state == CHATT_INITE_STATE) // �г��� �������� ���� ����
			{
				DisplayText("%s\r\n", msg); // ä�ù� �Է� ��Ʈ�� �޽���
				MyInfo->state = CHATT_ROOM_STATE;
			}

			else if (MyInfo->state == CHATT_ROOM_STATE) // ä�ù� ���� ����
			{
				MyInfo->state = CHATTING_STATE;
			}
			break;

			/* ���� ���� */
		case CHATT_OUT:
			int result;
			memset(msg, 0, sizeof(msg));
			UnPackPacket(MyInfo->recvbuf, msg);
			DisplayText("%s\r\n", msg);
			result = GetProtocol(MyInfo->recvbuf); // �������� �и�

			if (result == LEAVE)
			{
				endflag = true;
				break;
			}

			else
			{
				continue;
			}

		}
		if (endflag)
		{
			break;
		}
	}

	return 0;
}

PROTOCOL GetProtocol(const char* _ptr)
{
	PROTOCOL protocol;
	memcpy(&protocol, _ptr, sizeof(PROTOCOL));

	return protocol;
}

int PackPacket(char* _buf, PROTOCOL _protocol)
{
	int size = 0;
	char* ptr = _buf + sizeof(size);

	memcpy(ptr, &_protocol, sizeof(_protocol));
	ptr = ptr + sizeof(_protocol);
	size = size + sizeof(size);

	ptr = _buf;

	memcpy(ptr, &size, sizeof(size));
	size = size + sizeof(size);
	return size;
}

int PackPacket(char* _buf, PROTOCOL _protocol, const char* _str1)
{
	char* ptr = _buf;
	int size = 0;
	ptr = ptr + sizeof(size);

	memcpy(ptr, &_protocol, sizeof(_protocol));
	ptr = ptr + sizeof(_protocol);
	size = size + sizeof(size);

	int strsize1 = strlen(_str1);
	memcpy(ptr, &strsize1, sizeof(strsize1));
	ptr = ptr + sizeof(strsize1);
	size = size + sizeof(strsize1);

	memcpy(ptr, _str1, strsize1);
	ptr = ptr + strsize1;
	size = size + strsize1;

	ptr = _buf;

	memcpy(ptr, &size, sizeof(size));
	size = size + sizeof(size);
	return size;
}

void UnPackPacket(const char* _buf, int& _num, char* _str)
{
	int strsize;
	const char* ptr = _buf + sizeof(PROTOCOL);

	memcpy(&_num, ptr, sizeof(_num));
	ptr = ptr + sizeof(_num);

	memcpy(&strsize, ptr, sizeof(strsize));
	ptr = ptr + sizeof(strsize);

	memcpy(_str, ptr, strsize);
	ptr = ptr + strsize;
}

void UnPackPacket(const char* _buf, char* _str)
{
	int strsize;
	const char* ptr = _buf + sizeof(PROTOCOL);

	memcpy(&strsize, ptr, sizeof(strsize));
	ptr = ptr + sizeof(strsize);

	memcpy(_str, ptr, strsize);
	ptr = ptr + strsize;
}


void UnPackPacket(const char* _buf, char* _str, int& _count)
{	
	const char* ptr = _buf + sizeof(PROTOCOL);

	memcpy(&_count, ptr, sizeof(_count));
	ptr = ptr + sizeof(_count);

	for (int i = 0; i < _count; i++)
	{
		int strsize;
		memcpy(&strsize, ptr, sizeof(strsize));
		ptr = ptr + sizeof(strsize);

		memcpy(_str, ptr, strsize);
		ptr = ptr + strsize;
		_str = _str + strsize;
		strcat(_str, ",");
		_str++;
	}

}