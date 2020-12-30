#include "global.h"

/* 접속한 클라이언트 배열에 추가 */
_ClientInfo* AddClient(SOCKET sock, SOCKADDR_IN clientaddr)
{
	printf("\nClient 접속: IP 주소=%s, 포트 번호=%d\n", inet_ntoa(clientaddr.sin_addr),
		ntohs(clientaddr.sin_port));

	EnterCriticalSection(&cs);
	_ClientInfo* ptr = new _ClientInfo; // 동적 할당
	ZeroMemory(ptr, sizeof(_ClientInfo)); // 메모리 초기화
	ptr->sock = sock; // 소켓 설정
	memcpy(&(ptr->clientaddr), &clientaddr, sizeof(clientaddr)); // 주소 설정
	ptr->state = INITE_STATE;
	ptr->ISanswer = false;
	ClientInfo[Client_Count++] = ptr; // 배열에 추가

	LeaveCriticalSection(&cs);
	return ptr; // 설정한 클라 리턴
}

/* 접속 종료한 클라이언트 클라 배열에서 삭제 */
void RemoveClient(_ClientInfo* ptr)
{
	closesocket(ptr->sock); // 소켓 종료

	printf("\nClient 종료: IP 주소=%s, 포트 번호=%d\n",
		inet_ntoa(ptr->clientaddr.sin_addr),
		ntohs(ptr->clientaddr.sin_port));

	EnterCriticalSection(&cs);

	for (int i = 0; i < Client_Count; i++)
	{
		if (ClientInfo[i] == ptr) // 클라 배열에서 종료한 클라 찾아서
		{
			delete ptr; // 메모리 해제
			int j;
			for (j = i; j < Client_Count - 1; j++)
			{
				ClientInfo[j] = ClientInfo[j + 1]; // 배열 인덱스 정리
			}
			ClientInfo[j] = nullptr; 
			break;
		}
	}

	Client_Count--; // 클라 개수 -1
	LeaveCriticalSection(&cs);
}

/* 클라가 속한 게임의 클라 배열 인덱스 정리 */
void RemoveGameUser(_ClientInfo* _ptr)
{
	EnterCriticalSection(&cs);
	_GameInfo* game_ptr = _ptr->game_info;

	for (int i = 0; i < game_ptr->user_count; i++)
	{
		// 클라 배열에서 클라이언트를 찾아서 인덱스 정리(메모리 해제는 클라 삭제 함수에서 함)
		if (game_ptr->User[i] == _ptr)
		{
			for (int j = i; j < game_ptr->user_count - 1; j++)
			{
				game_ptr->User[j] = game_ptr->User[j + 1];
			}
			game_ptr->user_count--; // 유저 수 -1

			// 유저 수가 0이 되면 게임 정보 삭제
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

	// 생성되어 있는 게임에 들어오는 클라이언트 관리
	for (int i = 0; i < Game_Count; i++)
	{
		// 생성되어 있는 게임 개수 만큼 반복해서 해당 게임의 인원이 가득차지 않았으면
		if (!GameInfo[i]->full)
		{
			// 접속한 클라이언트 게임에 추가
			GameInfo[i]->User[GameInfo[i]->user_count++] = _ptr;
			_ptr->game_info = GameInfo[i]; // 속한 게임 정보 저장
			
			// 게임 인원이 가득차면
			if (GameInfo[i]->user_count == MAX_USER)
			{
				GameInfo[i]->full = true; // 다른 클라가 더이상 못들어 오도록 true로 변경
				SetEvent(GameInfo[i]->start_event); // 이벤트에 신호줘서 게임 시작
				GameInfo[i]->result_event = CreateEvent(nullptr, true /* 수동 */, false /* 비신호 상태 */, nullptr); // 게임 결과 이벤트 생성
			}
			game_ptr = GameInfo[i];
			index = i;
			break;
		}
	}

	// 게임에 첫번째로 들어온 클라이언트 관리, 게임 초기값 설정 및 생성
	if (index == NODATA)
	{
		game_ptr = new _GameInfo; // 동적 할당
		memset(game_ptr, 0, sizeof(_GameInfo)); // 메모리 초기화
		
		game_ptr->start_event = CreateEvent(nullptr, true/* 수동 */, false/* 비신호 상태 */, nullptr); // 게임시작 이벤트 생성
		game_ptr->full = false; // 게임 인원이 가득차면 true(5명)
		game_ptr->turn_number = 1; // 외쳐야 하는 숫자 설정
		game_ptr->last_number = MAX_USER; // 마지막 숫자 설정(접속 가능한 인원수랑 동일)
		game_ptr->finish = false; // 게임이 끝난지 알려줄 변수
		game_ptr->User[0] = _ptr; // 첫번째로 들어온 클라 저장
		game_ptr->user_count++; // 게임에 참여한 유저수 +1

		GameInfo[Game_Count++] = game_ptr; // 게임 정보 배열에 저장후 게임 개수 +1
		_ptr->game_info = game_ptr; // 자신이 속한 게임 정보 저장
	}

	LeaveCriticalSection(&cs);
	return game_ptr;
}

/* 게임 정보 삭제, 게임 정보 배열 인덱스 정리 */
void RemoveGameInfo(_GameInfo* _ptr)
{
	EnterCriticalSection(&cs);
	for (int i = 0; i < Game_Count; i++)
	{
		// 게임 정보 배열에서 해당 게임을 찾아서
		if (GameInfo[i] == _ptr)
		{
			// 이벤트 핸들 종료
			CloseHandle(_ptr->start_event);
			CloseHandle(_ptr->result_event);
			CloseHandle(_ptr->timing_event);
			delete _ptr; // 메모리 해제
			int j;
			for (j = i; j < Game_Count - 1; j++)
			{
				GameInfo[j] = GameInfo[j + 1]; // 인덱스 정리
			}
			GameInfo[j] = nullptr;
			break;
		}
	}
	Game_Count--; // 게임 개수 -1
	LeaveCriticalSection(&cs);
}

/* 클라가 속한 게임에 같은 닉네임이 있는지 체크하는 함수 */
bool NicknameCheck(_ClientInfo* _ptr)
{
	EnterCriticalSection(&cs);
	_GameInfo* game_ptr = _ptr->game_info; // 클라가 속한 게임 정보
	for (int i = 0; i < game_ptr->Nick_Count; i++)
	{
		// 닉네임 리스트에 클라의 닉네임이 있는지 확인
		if (!strcmp(game_ptr->NickNameList[i], _ptr->nickname))
		{
			LeaveCriticalSection(&cs);
			return false;
		}
	}
	LeaveCriticalSection(&cs);

	return true;
}

/* 클라가 속한 게임의 닉네임리스트 배열에 닉네임을 추가하는 함수 */
void AddNickName(_ClientInfo* _ptr)
{
	EnterCriticalSection(&cs);
	_GameInfo* game_ptr = _ptr->game_info;
	char* ptr = new char[strlen(_ptr->nickname) + 1]; // 동적 할당
	strcpy(ptr, _ptr->nickname); // 닉네임 복사
	game_ptr->NickNameList[game_ptr->Nick_Count++] = ptr; // 닉네임 리스트에 복사한 닉네임 추가
	LeaveCriticalSection(&cs);
}

/* 클라가 속한 게임의 닉네임리스트 배열에서 닉네임을 삭제하는 함수 */
void RemoveNickName(_ClientInfo* _ptr)
{
	EnterCriticalSection(&cs);
	_GameInfo* game_ptr = _ptr->game_info;
	for (int i = 0; i < game_ptr->Nick_Count; i++)
	{
		if (!strcmp(game_ptr->NickNameList[i], _ptr->nickname)) // 닉네임 리스트 에서 닉네임을 찾아서 삭제
		{
			int j;
			for (j = i; j < game_ptr->Nick_Count - 1; j++)
			{
				game_ptr->NickNameList[j] = game_ptr->NickNameList[j + 1]; // 인덱스 정리
			}
			game_ptr->NickNameList[j] = nullptr;
			break;
		}
	}
	game_ptr->Nick_Count--; // 닉네임 개수 -1
	LeaveCriticalSection(&cs);
}

/* 메시지 만드는 함수 */
void MaKeGameMessage(const char* _nick, int _num, char* _chattmsg)
{
	sprintf(_chattmsg, "[ %s ] %d", _nick, _num);
}

void MakeEnterMessage(const char* _nick, char* _msg)
{
	sprintf(_msg, "%s님이 방에 입장하셨습니다.", _nick);
}
void MakeExitMessage(const char* _nick, char* _msg)
{
	sprintf(_msg, "%s님이 방에서 퇴장하셨습니다.", _nick);
}
//////////////////////

/* 인트로 */
void IntroProcess(_ClientInfo* _ptr)
{
	int size = PackPacket(_ptr->sendbuf, INTRO, INTRO_MSG); // 인트로 메시지 패킹
	int retval = send(_ptr->sock, _ptr->sendbuf, size, 0); // 클라로 전송
	if (retval == SOCKET_ERROR)
	{
		err_display("intro Send()");
		_ptr->state = DISCONNECT_STATE;
		return;
	}
	_ptr->state = WAIT_STATE; // WAIT상태로 변경
}

/* 클라한테 닉네임 받고 대기 메시지 보내주는 함수 */
void WaitProcess(_ClientInfo* _ptr)
{
	if (!PacketRecv(_ptr->sock, _ptr->recvbuf)) // 클라로부터 닉네임 받기
	{
		_ptr->state = DISCONNECT_STATE;
		return;
	}

	UnPackPacket(_ptr->recvbuf, _ptr->nickname); // 닉네임 언패킹
	_GameInfo* game_info = AddGameInfo(_ptr); // 게임 생성, 게임에 클라 추가

	// 클라가 속한 게임에 이미 있는 닉네임 이면 에러메시지 전송
	if (!NicknameCheck(_ptr))
	{
		game_info->user_count--; // 게임 정보에는 유저 수가 올라 갔으므로 -1
		int size = PackPacket(_ptr->sendbuf, DATA_ERROR, NICKNAME_EROR, NICKNAME_ERROR_MSG); // 에러 메시지 패킹
		int retval = send(_ptr->sock, _ptr->sendbuf, size, 0); // 클라로 메시지 전송
		if (retval == SOCKET_ERROR)
		{
			err_display("send()");
			_ptr->state = DISCONNECT_STATE;
		}
		return;
	}

	AddNickName(_ptr); // 닉네임 리스트 배열에 접속한 클라 닉네임 추가

	/* 클라이언트가 접속했을때 보내줄 메시지들(닉네임 리스트, 접속 메시지, 대기 메시지) */
	int size = PackPacket(_ptr->sendbuf, NICKNAME_LIST, game_info->NickNameList, game_info->Nick_Count); // 클라가 속한 게임의 닉네임 리스트 패킹
	int retval = send(_ptr->sock, _ptr->sendbuf, size, 0); // 클라로 전송
	if (retval == SOCKET_ERROR)
	{
		err_display("send()");
		_ptr->state = DISCONNECT_STATE;
		return;
	}

	char msg[BUFSIZE]; // 메시지 저장할 버퍼
	memset(msg, 0, sizeof(msg)); // 초기화
	MakeEnterMessage(_ptr->nickname, msg); // 접속 메시지 제작

	// 다른 유저가 접속한 정보를 게임에 속한 다른 유저들에게도 전송해줌
	for (int i = 0; i < game_info->user_count; i++)
	{
		size = PackPacket(game_info->User[i]->sendbuf, USER_ENTER, msg); // 접속 메시지 패킹
		retval = send(game_info->User[i]->sock, game_info->User[i]->sendbuf, size, 0); // 접속 메시지 전송
		// 오류가 나면 해당 클라이언트 상태 변경
		if (game_info->User[i]->sock == SOCKET_ERROR)
		{
			err_display("send()");
			game_info->User[i]->state = DISCONNECT_STATE;
			return;
		}
	}
	
	// 게임 인원이 가득차지 않았으면 대기 메시지 전송해줌
	if (!game_info->full)
	{
		size = PackPacket(_ptr->sendbuf, WAIT, WAIT_MSG); // 대기 메시지 패킹
		retval = send(_ptr->sock, _ptr->sendbuf, size, 0); // 클라로 메시지 전송
		if (retval == SOCKET_ERROR)
		{
			err_display("send()");
			_ptr->state = DISCONNECT_STATE;
			return;
		}
	}
	///////////////////////////////////////////////////////////////////////////////
	
	WaitForSingleObject(game_info->start_event, INFINITE); // 게임 인원이 가득 찰때까지 대기

	_ptr->state = GAME_PLAY_STATE; // 가득 채워지면 게임 시작 상태로 변경
}

/* 게임 진행 관리 */
void GamePlayProcess(_ClientInfo* _ptr)
{
	// 이미 숫자를 보냈으면 리턴
	if (_ptr->ISanswer)
	{
		return;
	}

	_GameInfo* game_info = _ptr->game_info; // 클라가 속한 게임 정보 할당

	// 게임에 속한 클라들에게 게임 시작 안내 메시지를 전송
	int size = PackPacket(_ptr->sendbuf, GAME_START, GAME_START_MSG); // 시작 메시지 패킹
	int retval = send(_ptr->sock, _ptr->sendbuf, size, 0); // 전송
	// 오류가 나면 해당 클라이언트 상태 변경
	if (_ptr->sock == SOCKET_ERROR)
	{
		err_display("send()");
		_ptr->state = DISCONNECT_STATE;
		return;
	}

	while (1)
	{
		// 클라가 입력한 데이터 받기
		if (!PacketRecv(_ptr->sock, _ptr->recvbuf))
		{
			_ptr->state = DISCONNECT_STATE;
			return;
		}

		PROTOCOL protocol = GetProtocol(_ptr->recvbuf); // 프로토콜 분리(게임 메시지, 게임 종료)

		switch (protocol)
		{
			// 클라 게임 메시지
		case GAME_MSG:
			char buf[BUFSIZE];
			memset(buf, 0, sizeof(buf));

			UnPackPacket(_ptr->recvbuf, buf); // 클라가 보낸 데이터 언패킹
			_ptr->number = atoi(buf); // 받은 문자열 정수로 변환해서 할당

			if (_ptr->number < 1 || _ptr->number > 5) // 범위 오류
			{
				int size = PackPacket(_ptr->sendbuf, DATA_ERROR, RANGE_ERROR, RANGE_ERROR_MSG); // 범위 에러 메시지 패킹
				int retval = send(_ptr->sock, _ptr->sendbuf, size, 0); // 클라로 메시지 전송
				if (retval == SOCKET_ERROR)
				{
					err_display("send()");
					_ptr->state = DISCONNECT_STATE;
					return;
				}
				continue; // 다시 돌아가서 데이터 받기
			}
			break;

			// 클라 접속 종료
		case GAME_OUT:
			_ptr->state = DISCONNECT_STATE;
			return;
		}
		break; // 데이터를 다시 받아야할 필요가 없으므로 반복문 종료
	}

	game_info->same_time_count++; // 동시에 입력한 클라수 +1
	game_info->timing_event = CreateEvent(nullptr, true, false, nullptr);
	_ptr->ISanswer = true; // 2번 보내지 못하도록 예외처리할 변수
	
	char msg[BUFSIZE];
	memset(msg, 0, sizeof(msg));

	MaKeGameMessage(_ptr->nickname, _ptr->number, msg); // 메시지 제작
	// 게임에 속한 클라들에게 클라들이 입력한 수 전송
	for (int i = 0; i < game_info->user_count; i++)
	{
		size = PackPacket(game_info->User[i]->sendbuf, GAME_MSG, msg); // 클라가 입력한 수 패킹
		retval = send(game_info->User[i]->sock, game_info->User[i]->sendbuf, size, 0); // 클라들에게 메시지 전송
		if (game_info->User[i]->sock == SOCKET_ERROR)
		{
			err_display("send()");
			game_info->User[i]->state = DISCONNECT_STATE;
			return;
		}
	}

	_ptr->state = GAME_RESULT_STATE; // 입력한 유저 상태 변경 후 대기

}

/* 게임 결과 전송 */
void GameResultProcess(_ClientInfo* _ptr)
{
	_GameInfo* game_info = _ptr->game_info; // 클라가 속한 게임 정보 할당

	// 게임 종료 플래그가 false일때만
	if (!game_info->finish)
	{
		game_info = GameWinLoseProcess(_ptr); // 게임 승패 처리
	}

	WaitForSingleObject(game_info->result_event, INFINITE); // 게임 결과가 나올때까지 대기

	/* 게임이 끝나면 유저들에게 승리, 패배 메시지 전송 */
	for (int i = 0; i < game_info->WinUser_Count; i++)
	{
		/* 게임에서 이긴 유저에게만 메시지 전송 */
		int size = PackPacket(game_info->WinUser[i]->sendbuf, GAME_RESULT, WIN, GAME_WIN_MSG); // 승리 메시지 패킹
		int retval = send(game_info->WinUser[i]->sock, game_info->WinUser[i]->sendbuf, size, 0); // 승리한 클라들에게 메시지 전송
		if (game_info->WinUser[i]->sock == SOCKET_ERROR)
		{
			err_display("send()");
			game_info->WinUser[i]->state = DISCONNECT_STATE;
			return;
		}

	}
	for (int i = 0; i < game_info->LoseUser_Count; i++)
	{
		/* 게임에서 패배한 유저들에게만 메시지 전송 */
		int size = PackPacket(game_info->LoseUser[i]->sendbuf, GAME_RESULT, LOSE, GAME_LOSE_MSG); // 패배 메시지 패킹
		int retval = send(game_info->LoseUser[i]->sock, game_info->LoseUser[i]->sendbuf, size, 0); // 패배한 클라들에게 메시지 전송
		if (game_info->LoseUser[i]->sock == SOCKET_ERROR)
		{
			err_display("send()");
			game_info->LoseUser[i]->state = DISCONNECT_STATE;
			return;
		}
	}

	for (int i = 0; i < game_info->user_count; i++)
	{
		game_info->User[i]->state = DISCONNECT_STATE; // 게임 종료 되었으므로 모든 플레이어 연결 종료
	}
}

/* 게임 승패 처리 함수 */
_GameInfo* GameWinLoseProcess(_ClientInfo* _ptr)
{
	EnterCriticalSection(&cs);
	_GameInfo* game_info = _ptr->game_info; // 현재 클라가 속한 게임 정보 할당

	/* 순서에 맞지 않는 숫자 입력 패배 처리 */
	if (_ptr->number != game_info->turn_number)
	{
		game_info->LoseUser[game_info->LoseUser_Count++] = _ptr; // 패배한 유저

		/* 패배한 유저 승리한 유저 분리 */
		for (int i = 0; i < game_info->user_count; i++)
		{
			// 패배한 유저가 아닌유저(승리한 유저)
			if (game_info->User[i] != _ptr)
			{
				game_info->WinUser[game_info->WinUser_Count++] = game_info->User[i]; // 승리한 유저

			}
		}
		game_info->finish = true; // 게임 종료 플래그
		SetEvent(game_info->result_event); // 게임 결과 이벤트에 신호를 줌
		LeaveCriticalSection(&cs);
		return game_info; // 갱신된 정보 리턴
	}

	/* 유저가 입력을 했을때. 현재 턴의 숫자가 마지막 숫자라면 패배 처리 */
	if (game_info->turn_number == game_info->last_number)
	{
		game_info->LoseUser[game_info->LoseUser_Count++] = _ptr; // 패배한 유저

		/* 패배한 유저 승리한 유저 분리 */
		for (int i = 0; i < game_info->user_count; i++)
		{
			// 패배한 유저가 아닌유저
			if (game_info->User[i] != _ptr)
			{
				game_info->WinUser[game_info->WinUser_Count++] = game_info->User[i]; // 승리한 유저
			}
		}
		game_info->finish = true; // 게임 종료 플래그
		SetEvent(game_info->result_event); // 게임 결과 이벤트에 신호를 줌
		LeaveCriticalSection(&cs);
		return game_info; // 갱신된 정보 리턴
	}


	// 지정해둔 시간 안에 들어오는 이벤트들 패배 처리
	DWORD retval = WaitForSingleObject(game_info->timing_event, SAME_TIMEING);
	if (game_info->same_time_count == 1 )
	{
		for (int i = 0; i < game_info->LoseUser_Count; i++)
		{
			game_info->LoseUser[i] = nullptr;
		}
		game_info->same_time_count = 0;
		game_info->turn_number++; // 턴 숫자 +1
		LeaveCriticalSection(&cs);
		return game_info; // 갱신된 정보 리턴
	}
	if (retval == WAIT_TIMEOUT)
	{
		// 일단 들어오는 클라는 모두 패배 유저에 넣고 카운트가 1이면 초기화
		if (game_info->same_time_count > 1)
		{
			game_info->LoseUser[game_info->LoseUser_Count++] = _ptr;
		}

		/* 패배한 유저 승리한 유저 분리 */
		// 동시에 들어온 유저들을 패배한 유저 배열에 다 넣으면
		if (game_info->same_time_count > 1)
		{
			if (game_info->same_time_count == game_info->LoseUser_Count)
			{
				int count = 0;
				while (true)
				{
					for (int i = 0; i < game_info->LoseUser_Count; i++)
					{
						// 배열 인덱스 맞추는 부분
						if (game_info->User[count] == game_info->LoseUser[i])
						{
							game_info->TempCliArr[count] = game_info->LoseUser[i];
							break;
						}
					}
					// temp배열에 패배한 유저 인덱스 조절해서 넣은뒤 카운트 +1
					count++;
					if (count == MAX_USER)
						break;
				}

				for (int i = 0; i < game_info->user_count; i++)
				{
					// 승리한 유저(유저와 인덱스를 맞춰서 값이 다른곳은 승리한 유저가 된다)
					if (game_info->User[i] != game_info->TempCliArr[i])
					{
						game_info->WinUser[game_info->WinUser_Count++] = game_info->User[i];
					}
				}
			}
		}

		if (game_info->WinUser_Count + game_info->LoseUser_Count == game_info->user_count)
		{
			game_info->finish = true; // 게임 종료 플래그
			SetEvent(game_info->result_event); // 게임 결과 이벤트에 신호를 줌
			LeaveCriticalSection(&cs);
			return game_info; // 갱신된 정보 리턴
		}
	}
	LeaveCriticalSection(&cs);
	return game_info; // 갱신된 정보 리턴
}

/* 클라 연결 종료 프로세스 */
void DisConnectProcess(_ClientInfo* _ptr)
{
	_GameInfo* game_info = _ptr->game_info;

	char msg[BUFSIZE];
	memset(msg, 0, sizeof(msg));

	MakeExitMessage(_ptr->nickname, msg); // 종료 메시지 제작

	// 한명 이상 있을때만 종료 메시지 보내줌
	if (game_info->user_count > INIT)
	{
		for (int i = 0; i < game_info->user_count; i++)
		{
			// 종료하지 않은 클라들에게 메시지 전송
			if (game_info->User[i] != _ptr)
			{
				int size = PackPacket(game_info->User[i]->sendbuf, GAME_MSG, msg);  // 종료 메시지 패킹
				int retval = send(game_info->User[i]->sock, game_info->User[i]->sendbuf, size, 0); // 남은 클라들에게 전송
				if (game_info->User[i]->sock == SOCKET_ERROR)
				{
					err_display("send()");
					game_info->User[i]->state = DISCONNECT_STATE;
					return;
				}
			}
		}
	}

	game_info->last_number--; // 클라가 나갔으므로 마지막 숫자도 -1
	RemoveNickName(_ptr); // 클라가 속한 게임에서 클라의 닉네임 삭제
	RemoveGameUser(_ptr); // 클라가 속한 게임에서 클라 배열 정리
	RemoveClient(_ptr); // 클라이언트 삭제
}
