#include "global.h"

/* ������ Ŭ���̾�Ʈ �迭�� �߰� */
_ClientInfo* AddClient(SOCKET sock, SOCKADDR_IN clientaddr)
{
	printf("\nClient ����: IP �ּ�=%s, ��Ʈ ��ȣ=%d\n", inet_ntoa(clientaddr.sin_addr),
		ntohs(clientaddr.sin_port));

	EnterCriticalSection(&cs);
	_ClientInfo* ptr = new _ClientInfo; // ���� �Ҵ�
	ZeroMemory(ptr, sizeof(_ClientInfo)); // �޸� �ʱ�ȭ
	ptr->sock = sock; // ���� ����
	memcpy(&(ptr->clientaddr), &clientaddr, sizeof(clientaddr)); // �ּ� ����
	ptr->state = INITE_STATE;
	ptr->ISanswer = false;
	ClientInfo[Client_Count++] = ptr; // �迭�� �߰�

	LeaveCriticalSection(&cs);
	return ptr; // ������ Ŭ�� ����
}

/* ���� ������ Ŭ���̾�Ʈ Ŭ�� �迭���� ���� */
void RemoveClient(_ClientInfo* ptr)
{
	closesocket(ptr->sock); // ���� ����

	printf("\nClient ����: IP �ּ�=%s, ��Ʈ ��ȣ=%d\n",
		inet_ntoa(ptr->clientaddr.sin_addr),
		ntohs(ptr->clientaddr.sin_port));

	EnterCriticalSection(&cs);

	for (int i = 0; i < Client_Count; i++)
	{
		if (ClientInfo[i] == ptr) // Ŭ�� �迭���� ������ Ŭ�� ã�Ƽ�
		{
			delete ptr; // �޸� ����
			int j;
			for (j = i; j < Client_Count - 1; j++)
			{
				ClientInfo[j] = ClientInfo[j + 1]; // �迭 �ε��� ����
			}
			ClientInfo[j] = nullptr; 
			break;
		}
	}

	Client_Count--; // Ŭ�� ���� -1
	LeaveCriticalSection(&cs);
}

/* Ŭ�� ���� ������ Ŭ�� �迭 �ε��� ���� */
void RemoveGameUser(_ClientInfo* _ptr)
{
	EnterCriticalSection(&cs);
	_GameInfo* game_ptr = _ptr->game_info;

	for (int i = 0; i < game_ptr->user_count; i++)
	{
		// Ŭ�� �迭���� Ŭ���̾�Ʈ�� ã�Ƽ� �ε��� ����(�޸� ������ Ŭ�� ���� �Լ����� ��)
		if (game_ptr->User[i] == _ptr)
		{
			for (int j = i; j < game_ptr->user_count - 1; j++)
			{
				game_ptr->User[j] = game_ptr->User[j + 1];
			}
			game_ptr->user_count--; // ���� �� -1

			// ���� ���� 0�� �Ǹ� ���� ���� ����
			if (game_ptr->user_count == INIT)
			{
				RemoveGameInfo(game_ptr);
			}
		}
	}
	LeaveCriticalSection(&cs);
}

_GameInfo* AddGameInfo(_ClientInfo* _ptr)
{
	EnterCriticalSection(&cs);
	_GameInfo* game_ptr = nullptr;
	int index = NODATA;

	// �����Ǿ� �ִ� ���ӿ� ������ Ŭ���̾�Ʈ ����
	for (int i = 0; i < Game_Count; i++)
	{
		// �����Ǿ� �ִ� ���� ���� ��ŭ �ݺ��ؼ� �ش� ������ �ο��� �������� �ʾ�����
		if (!GameInfo[i]->full)
		{
			// ������ Ŭ���̾�Ʈ ���ӿ� �߰�
			GameInfo[i]->User[GameInfo[i]->user_count++] = _ptr;
			_ptr->game_info = GameInfo[i]; // ���� ���� ���� ����
			
			// ���� �ο��� ��������
			if (GameInfo[i]->user_count == MAX_USER)
			{
				GameInfo[i]->full = true; // �ٸ� Ŭ�� ���̻� ����� ������ true�� ����
				SetEvent(GameInfo[i]->start_event); // �̺�Ʈ�� ��ȣ�༭ ���� ����
				GameInfo[i]->result_event = CreateEvent(nullptr, true /* ���� */, false /* ���ȣ ���� */, nullptr); // ���� ��� �̺�Ʈ ����
			}
			game_ptr = GameInfo[i];
			index = i;
			break;
		}
	}

	// ���ӿ� ù��°�� ���� Ŭ���̾�Ʈ ����, ���� �ʱⰪ ���� �� ����
	if (index == NODATA)
	{
		game_ptr = new _GameInfo; // ���� �Ҵ�
		memset(game_ptr, 0, sizeof(_GameInfo)); // �޸� �ʱ�ȭ
		
		game_ptr->start_event = CreateEvent(nullptr, true/* ���� */, false/* ���ȣ ���� */, nullptr); // ���ӽ��� �̺�Ʈ ����
		game_ptr->full = false; // ���� �ο��� �������� true(5��)
		game_ptr->turn_number = 1; // ���ľ� �ϴ� ���� ����
		game_ptr->last_number = MAX_USER; // ������ ���� ����(���� ������ �ο����� ����)
		game_ptr->finish = false; // ������ ������ �˷��� ����
		game_ptr->User[0] = _ptr; // ù��°�� ���� Ŭ�� ����
		game_ptr->user_count++; // ���ӿ� ������ ������ +1

		GameInfo[Game_Count++] = game_ptr; // ���� ���� �迭�� ������ ���� ���� +1
		_ptr->game_info = game_ptr; // �ڽ��� ���� ���� ���� ����
	}

	LeaveCriticalSection(&cs);
	return game_ptr;
}

/* ���� ���� ����, ���� ���� �迭 �ε��� ���� */
void RemoveGameInfo(_GameInfo* _ptr)
{
	EnterCriticalSection(&cs);
	for (int i = 0; i < Game_Count; i++)
	{
		// ���� ���� �迭���� �ش� ������ ã�Ƽ�
		if (GameInfo[i] == _ptr)
		{
			// �̺�Ʈ �ڵ� ����
			CloseHandle(_ptr->start_event);
			CloseHandle(_ptr->result_event);
			CloseHandle(_ptr->timing_event);
			delete _ptr; // �޸� ����
			int j;
			for (j = i; j < Game_Count - 1; j++)
			{
				GameInfo[j] = GameInfo[j + 1]; // �ε��� ����
			}
			GameInfo[j] = nullptr;
			break;
		}
	}
	Game_Count--; // ���� ���� -1
	LeaveCriticalSection(&cs);
}

/* Ŭ�� ���� ���ӿ� ���� �г����� �ִ��� üũ�ϴ� �Լ� */
bool NicknameCheck(_ClientInfo* _ptr)
{
	EnterCriticalSection(&cs);
	_GameInfo* game_ptr = _ptr->game_info; // Ŭ�� ���� ���� ����
	for (int i = 0; i < game_ptr->Nick_Count; i++)
	{
		// �г��� ����Ʈ�� Ŭ���� �г����� �ִ��� Ȯ��
		if (!strcmp(game_ptr->NickNameList[i], _ptr->nickname))
		{
			LeaveCriticalSection(&cs);
			return false;
		}
	}
	LeaveCriticalSection(&cs);

	return true;
}

/* Ŭ�� ���� ������ �г��Ӹ���Ʈ �迭�� �г����� �߰��ϴ� �Լ� */
void AddNickName(_ClientInfo* _ptr)
{
	EnterCriticalSection(&cs);
	_GameInfo* game_ptr = _ptr->game_info;
	char* ptr = new char[strlen(_ptr->nickname) + 1]; // ���� �Ҵ�
	strcpy(ptr, _ptr->nickname); // �г��� ����
	game_ptr->NickNameList[game_ptr->Nick_Count++] = ptr; // �г��� ����Ʈ�� ������ �г��� �߰�
	LeaveCriticalSection(&cs);
}

/* Ŭ�� ���� ������ �г��Ӹ���Ʈ �迭���� �г����� �����ϴ� �Լ� */
void RemoveNickName(_ClientInfo* _ptr)
{
	EnterCriticalSection(&cs);
	_GameInfo* game_ptr = _ptr->game_info;
	for (int i = 0; i < game_ptr->Nick_Count; i++)
	{
		if (!strcmp(game_ptr->NickNameList[i], _ptr->nickname)) // �г��� ����Ʈ ���� �г����� ã�Ƽ� ����
		{
			int j;
			for (j = i; j < game_ptr->Nick_Count - 1; j++)
			{
				game_ptr->NickNameList[j] = game_ptr->NickNameList[j + 1]; // �ε��� ����
			}
			game_ptr->NickNameList[j] = nullptr;
			break;
		}
	}
	game_ptr->Nick_Count--; // �г��� ���� -1
	LeaveCriticalSection(&cs);
}

/* �޽��� ����� �Լ� */
void MaKeGameMessage(const char* _nick, int _num, char* _chattmsg)
{
	sprintf(_chattmsg, "[ %s ] %d", _nick, _num);
}

void MakeEnterMessage(const char* _nick, char* _msg)
{
	sprintf(_msg, "%s���� �濡 �����ϼ̽��ϴ�.", _nick);
}
void MakeExitMessage(const char* _nick, char* _msg)
{
	sprintf(_msg, "%s���� �濡�� �����ϼ̽��ϴ�.", _nick);
}
//////////////////////

/* ��Ʈ�� */
void IntroProcess(_ClientInfo* _ptr)
{
	int size = PackPacket(_ptr->sendbuf, INTRO, INTRO_MSG); // ��Ʈ�� �޽��� ��ŷ
	int retval = send(_ptr->sock, _ptr->sendbuf, size, 0); // Ŭ��� ����
	if (retval == SOCKET_ERROR)
	{
		err_display("intro Send()");
		_ptr->state = DISCONNECT_STATE;
		return;
	}
	_ptr->state = WAIT_STATE; // WAIT���·� ����
}

/* Ŭ������ �г��� �ް� ��� �޽��� �����ִ� �Լ� */
void WaitProcess(_ClientInfo* _ptr)
{
	if (!PacketRecv(_ptr->sock, _ptr->recvbuf)) // Ŭ��κ��� �г��� �ޱ�
	{
		_ptr->state = DISCONNECT_STATE;
		return;
	}

	UnPackPacket(_ptr->recvbuf, _ptr->nickname); // �г��� ����ŷ
	_GameInfo* game_info = AddGameInfo(_ptr); // ���� ����, ���ӿ� Ŭ�� �߰�

	// Ŭ�� ���� ���ӿ� �̹� �ִ� �г��� �̸� �����޽��� ����
	if (!NicknameCheck(_ptr))
	{
		game_info->user_count--; // ���� �������� ���� ���� �ö� �����Ƿ� -1
		int size = PackPacket(_ptr->sendbuf, DATA_ERROR, NICKNAME_EROR, NICKNAME_ERROR_MSG); // ���� �޽��� ��ŷ
		int retval = send(_ptr->sock, _ptr->sendbuf, size, 0); // Ŭ��� �޽��� ����
		if (retval == SOCKET_ERROR)
		{
			err_display("send()");
			_ptr->state = DISCONNECT_STATE;
		}
		return;
	}

	AddNickName(_ptr); // �г��� ����Ʈ �迭�� ������ Ŭ�� �г��� �߰�

	/* Ŭ���̾�Ʈ�� ���������� ������ �޽�����(�г��� ����Ʈ, ���� �޽���, ��� �޽���) */
	int size = PackPacket(_ptr->sendbuf, NICKNAME_LIST, game_info->NickNameList, game_info->Nick_Count); // Ŭ�� ���� ������ �г��� ����Ʈ ��ŷ
	int retval = send(_ptr->sock, _ptr->sendbuf, size, 0); // Ŭ��� ����
	if (retval == SOCKET_ERROR)
	{
		err_display("send()");
		_ptr->state = DISCONNECT_STATE;
		return;
	}

	char msg[BUFSIZE]; // �޽��� ������ ����
	memset(msg, 0, sizeof(msg)); // �ʱ�ȭ
	MakeEnterMessage(_ptr->nickname, msg); // ���� �޽��� ����

	// �ٸ� ������ ������ ������ ���ӿ� ���� �ٸ� �����鿡�Ե� ��������
	for (int i = 0; i < game_info->user_count; i++)
	{
		size = PackPacket(game_info->User[i]->sendbuf, USER_ENTER, msg); // ���� �޽��� ��ŷ
		retval = send(game_info->User[i]->sock, game_info->User[i]->sendbuf, size, 0); // ���� �޽��� ����
		// ������ ���� �ش� Ŭ���̾�Ʈ ���� ����
		if (game_info->User[i]->sock == SOCKET_ERROR)
		{
			err_display("send()");
			game_info->User[i]->state = DISCONNECT_STATE;
			return;
		}
	}
	
	// ���� �ο��� �������� �ʾ����� ��� �޽��� ��������
	if (!game_info->full)
	{
		size = PackPacket(_ptr->sendbuf, WAIT, WAIT_MSG); // ��� �޽��� ��ŷ
		retval = send(_ptr->sock, _ptr->sendbuf, size, 0); // Ŭ��� �޽��� ����
		if (retval == SOCKET_ERROR)
		{
			err_display("send()");
			_ptr->state = DISCONNECT_STATE;
			return;
		}
	}
	///////////////////////////////////////////////////////////////////////////////
	
	WaitForSingleObject(game_info->start_event, INFINITE); // ���� �ο��� ���� �������� ���

	_ptr->state = GAME_PLAY_STATE; // ���� ä������ ���� ���� ���·� ����
}

/* ���� ���� ���� */
void GamePlayProcess(_ClientInfo* _ptr)
{
	// �̹� ���ڸ� �������� ����
	if (_ptr->ISanswer)
	{
		return;
	}

	_GameInfo* game_info = _ptr->game_info; // Ŭ�� ���� ���� ���� �Ҵ�

	// ���ӿ� ���� Ŭ��鿡�� ���� ���� �ȳ� �޽����� ����
	int size = PackPacket(_ptr->sendbuf, GAME_START, GAME_START_MSG); // ���� �޽��� ��ŷ
	int retval = send(_ptr->sock, _ptr->sendbuf, size, 0); // ����
	// ������ ���� �ش� Ŭ���̾�Ʈ ���� ����
	if (_ptr->sock == SOCKET_ERROR)
	{
		err_display("send()");
		_ptr->state = DISCONNECT_STATE;
		return;
	}

	while (1)
	{
		// Ŭ�� �Է��� ������ �ޱ�
		if (!PacketRecv(_ptr->sock, _ptr->recvbuf))
		{
			_ptr->state = DISCONNECT_STATE;
			return;
		}

		PROTOCOL protocol = GetProtocol(_ptr->recvbuf); // �������� �и�(���� �޽���, ���� ����)

		switch (protocol)
		{
			// Ŭ�� ���� �޽���
		case GAME_MSG:
			char buf[BUFSIZE];
			memset(buf, 0, sizeof(buf));

			UnPackPacket(_ptr->recvbuf, buf); // Ŭ�� ���� ������ ����ŷ
			_ptr->number = atoi(buf); // ���� ���ڿ� ������ ��ȯ�ؼ� �Ҵ�

			if (_ptr->number < 1 || _ptr->number > 5) // ���� ����
			{
				int size = PackPacket(_ptr->sendbuf, DATA_ERROR, RANGE_ERROR, RANGE_ERROR_MSG); // ���� ���� �޽��� ��ŷ
				int retval = send(_ptr->sock, _ptr->sendbuf, size, 0); // Ŭ��� �޽��� ����
				if (retval == SOCKET_ERROR)
				{
					err_display("send()");
					_ptr->state = DISCONNECT_STATE;
					return;
				}
				continue; // �ٽ� ���ư��� ������ �ޱ�
			}
			break;

			// Ŭ�� ���� ����
		case GAME_OUT:
			_ptr->state = DISCONNECT_STATE;
			return;
		}
		break; // �����͸� �ٽ� �޾ƾ��� �ʿ䰡 �����Ƿ� �ݺ��� ����
	}

	game_info->same_time_count++; // ���ÿ� �Է��� Ŭ��� +1
	game_info->timing_event = CreateEvent(nullptr, true, false, nullptr);
	_ptr->ISanswer = true; // 2�� ������ ���ϵ��� ����ó���� ����
	
	char msg[BUFSIZE];
	memset(msg, 0, sizeof(msg));

	MaKeGameMessage(_ptr->nickname, _ptr->number, msg); // �޽��� ����
	// ���ӿ� ���� Ŭ��鿡�� Ŭ����� �Է��� �� ����
	for (int i = 0; i < game_info->user_count; i++)
	{
		size = PackPacket(game_info->User[i]->sendbuf, GAME_MSG, msg); // Ŭ�� �Է��� �� ��ŷ
		retval = send(game_info->User[i]->sock, game_info->User[i]->sendbuf, size, 0); // Ŭ��鿡�� �޽��� ����
		if (game_info->User[i]->sock == SOCKET_ERROR)
		{
			err_display("send()");
			game_info->User[i]->state = DISCONNECT_STATE;
			return;
		}
	}

	_ptr->state = GAME_RESULT_STATE; // �Է��� ���� ���� ���� �� ���

}

/* ���� ��� ���� */
void GameResultProcess(_ClientInfo* _ptr)
{
	_GameInfo* game_info = _ptr->game_info; // Ŭ�� ���� ���� ���� �Ҵ�

	// ���� ���� �÷��װ� false�϶���
	if (!game_info->finish)
	{
		game_info = GameWinLoseProcess(_ptr); // ���� ���� ó��
	}

	WaitForSingleObject(game_info->result_event, INFINITE); // ���� ����� ���ö����� ���

	/* ������ ������ �����鿡�� �¸�, �й� �޽��� ���� */
	for (int i = 0; i < game_info->WinUser_Count; i++)
	{
		/* ���ӿ��� �̱� �������Ը� �޽��� ���� */
		int size = PackPacket(game_info->WinUser[i]->sendbuf, GAME_RESULT, WIN, GAME_WIN_MSG); // �¸� �޽��� ��ŷ
		int retval = send(game_info->WinUser[i]->sock, game_info->WinUser[i]->sendbuf, size, 0); // �¸��� Ŭ��鿡�� �޽��� ����
		if (game_info->WinUser[i]->sock == SOCKET_ERROR)
		{
			err_display("send()");
			game_info->WinUser[i]->state = DISCONNECT_STATE;
			return;
		}

	}
	for (int i = 0; i < game_info->LoseUser_Count; i++)
	{
		/* ���ӿ��� �й��� �����鿡�Ը� �޽��� ���� */
		int size = PackPacket(game_info->LoseUser[i]->sendbuf, GAME_RESULT, LOSE, GAME_LOSE_MSG); // �й� �޽��� ��ŷ
		int retval = send(game_info->LoseUser[i]->sock, game_info->LoseUser[i]->sendbuf, size, 0); // �й��� Ŭ��鿡�� �޽��� ����
		if (game_info->LoseUser[i]->sock == SOCKET_ERROR)
		{
			err_display("send()");
			game_info->LoseUser[i]->state = DISCONNECT_STATE;
			return;
		}
	}

	for (int i = 0; i < game_info->user_count; i++)
	{
		game_info->User[i]->state = DISCONNECT_STATE; // ���� ���� �Ǿ����Ƿ� ��� �÷��̾� ���� ����
	}
}

/* ���� ���� ó�� �Լ� */
_GameInfo* GameWinLoseProcess(_ClientInfo* _ptr)
{
	EnterCriticalSection(&cs);
	_GameInfo* game_info = _ptr->game_info; // ���� Ŭ�� ���� ���� ���� �Ҵ�

	/* ������ ���� �ʴ� ���� �Է� �й� ó�� */
	if (_ptr->number != game_info->turn_number)
	{
		game_info->LoseUser[game_info->LoseUser_Count++] = _ptr; // �й��� ����

		/* �й��� ���� �¸��� ���� �и� */
		for (int i = 0; i < game_info->user_count; i++)
		{
			// �й��� ������ �ƴ�����(�¸��� ����)
			if (game_info->User[i] != _ptr)
			{
				game_info->WinUser[game_info->WinUser_Count++] = game_info->User[i]; // �¸��� ����

			}
		}
		game_info->finish = true; // ���� ���� �÷���
		SetEvent(game_info->result_event); // ���� ��� �̺�Ʈ�� ��ȣ�� ��
		LeaveCriticalSection(&cs);
		return game_info; // ���ŵ� ���� ����
	}

	/* ������ �Է��� ������. ���� ���� ���ڰ� ������ ���ڶ�� �й� ó�� */
	if (game_info->turn_number == game_info->last_number)
	{
		game_info->LoseUser[game_info->LoseUser_Count++] = _ptr; // �й��� ����

		/* �й��� ���� �¸��� ���� �и� */
		for (int i = 0; i < game_info->user_count; i++)
		{
			// �й��� ������ �ƴ�����
			if (game_info->User[i] != _ptr)
			{
				game_info->WinUser[game_info->WinUser_Count++] = game_info->User[i]; // �¸��� ����
			}
		}
		game_info->finish = true; // ���� ���� �÷���
		SetEvent(game_info->result_event); // ���� ��� �̺�Ʈ�� ��ȣ�� ��
		LeaveCriticalSection(&cs);
		return game_info; // ���ŵ� ���� ����
	}


	// �����ص� �ð� �ȿ� ������ �̺�Ʈ�� �й� ó��
	DWORD retval = WaitForSingleObject(game_info->timing_event, SAME_TIMEING);
	if (game_info->same_time_count == 1 )
	{
		for (int i = 0; i < game_info->LoseUser_Count; i++)
		{
			game_info->LoseUser[i] = nullptr;
		}
		game_info->same_time_count = 0;
		game_info->turn_number++; // �� ���� +1
		LeaveCriticalSection(&cs);
		return game_info; // ���ŵ� ���� ����
	}
	if (retval == WAIT_TIMEOUT)
	{
		// �ϴ� ������ Ŭ��� ��� �й� ������ �ְ� ī��Ʈ�� 1�̸� �ʱ�ȭ
		if (game_info->same_time_count > 1)
		{
			game_info->LoseUser[game_info->LoseUser_Count++] = _ptr;
		}

		/* �й��� ���� �¸��� ���� �и� */
		// ���ÿ� ���� �������� �й��� ���� �迭�� �� ������
		if (game_info->same_time_count > 1)
		{
			if (game_info->same_time_count == game_info->LoseUser_Count)
			{
				int count = 0;
				while (true)
				{
					for (int i = 0; i < game_info->LoseUser_Count; i++)
					{
						// �迭 �ε��� ���ߴ� �κ�
						if (game_info->User[count] == game_info->LoseUser[i])
						{
							game_info->TempCliArr[count] = game_info->LoseUser[i];
							break;
						}
					}
					// temp�迭�� �й��� ���� �ε��� �����ؼ� ������ ī��Ʈ +1
					count++;
					if (count == MAX_USER)
						break;
				}

				for (int i = 0; i < game_info->user_count; i++)
				{
					// �¸��� ����(������ �ε����� ���缭 ���� �ٸ����� �¸��� ������ �ȴ�)
					if (game_info->User[i] != game_info->TempCliArr[i])
					{
						game_info->WinUser[game_info->WinUser_Count++] = game_info->User[i];
					}
				}
			}
		}

		if (game_info->WinUser_Count + game_info->LoseUser_Count == game_info->user_count)
		{
			game_info->finish = true; // ���� ���� �÷���
			SetEvent(game_info->result_event); // ���� ��� �̺�Ʈ�� ��ȣ�� ��
			LeaveCriticalSection(&cs);
			return game_info; // ���ŵ� ���� ����
		}
	}
	LeaveCriticalSection(&cs);
	return game_info; // ���ŵ� ���� ����
}

/* Ŭ�� ���� ���� ���μ��� */
void DisConnectProcess(_ClientInfo* _ptr)
{
	_GameInfo* game_info = _ptr->game_info;

	char msg[BUFSIZE];
	memset(msg, 0, sizeof(msg));

	MakeExitMessage(_ptr->nickname, msg); // ���� �޽��� ����

	// �Ѹ� �̻� �������� ���� �޽��� ������
	if (game_info->user_count > INIT)
	{
		for (int i = 0; i < game_info->user_count; i++)
		{
			// �������� ���� Ŭ��鿡�� �޽��� ����
			if (game_info->User[i] != _ptr)
			{
				int size = PackPacket(game_info->User[i]->sendbuf, GAME_MSG, msg);  // ���� �޽��� ��ŷ
				int retval = send(game_info->User[i]->sock, game_info->User[i]->sendbuf, size, 0); // ���� Ŭ��鿡�� ����
				if (game_info->User[i]->sock == SOCKET_ERROR)
				{
					err_display("send()");
					game_info->User[i]->state = DISCONNECT_STATE;
					return;
				}
			}
		}
	}

	game_info->last_number--; // Ŭ�� �������Ƿ� ������ ���ڵ� -1
	RemoveNickName(_ptr); // Ŭ�� ���� ���ӿ��� Ŭ���� �г��� ����
	RemoveGameUser(_ptr); // Ŭ�� ���� ���ӿ��� Ŭ�� �迭 ����
	RemoveClient(_ptr); // Ŭ���̾�Ʈ ����
}
