#include "cspch.h"
#include "Network.h"


namespace Net {
	Network::Network()
	{
		WSADATA wsa;
		if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
			ErrQuit(L"WSAStartup() Error");
	}

	Network::~Network()
	{
	}

	Network* Network::GetInstance()
	{
		static Network Instance;
		return &Instance;
	}

	void Network::ErrQuit(const wchar_t* msg)
	{
		LPVOID lpMsgBuf;
		FormatMessage(
			FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
			NULL, WSAGetLastError(),
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPTSTR)&lpMsgBuf, 0, NULL);
		MessageBox(NULL, (LPCWSTR)lpMsgBuf, msg, MB_ICONERROR);
		LocalFree(lpMsgBuf);
		exit(1);
	}

	void Network::Connect(SOCKET sock, const char* address)
	{
		SOCKADDR_IN serverAddr;
		ZeroMemory(&serverAddr, sizeof(serverAddr));
		serverAddr.sin_family = AF_INET;
		serverAddr.sin_addr.s_addr = inet_addr(address);
		serverAddr.sin_port = htons(SERVERPORT);

		retval = connect(sock, (SOCKADDR*)&serverAddr, sizeof(serverAddr));
		if (retval == SOCKET_ERROR)	ErrQuit(L"connect error()");
	}

	void Network::BindAndListen(SOCKET sock)
	{
		SOCKADDR_IN serverAddr;
		ZeroMemory(&serverAddr, sizeof(serverAddr));
		serverAddr.sin_family = AF_INET;
		serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
		serverAddr.sin_port = htons(SERVERPORT);

		retval = bind(sock, (SOCKADDR*)&serverAddr, sizeof(serverAddr));
		if (retval == SOCKET_ERROR) ErrQuit(L"bind error()");

		retval = listen(sock, SOMAXCONN);
		if (retval == SOCKET_ERROR) ErrQuit(L"listen error()");
	}

	SOCKET Network::Accept(SOCKET sock)
	{
		SOCKADDR_IN clientAddr = {};
		int clientAddrSize = sizeof(clientAddr);
		return accept(sock, (SOCKADDR*)&clientAddr, &clientAddrSize);
	}

	void Network::Send(SOCKET sock, char* buf, int dataSize)
	{
		retval = send(sock, buf, dataSize, 0);
	}

	int Network::Recv(SOCKET sock, char* buf, int dataSize)
	{
		retval = recv(sock, buf, dataSize, 0);
		return retval;
	}

	void Network::Release(SOCKET sock)
	{
		closesocket(sock);
		WSACleanup();
	}

	//----------------------------------------IOCP server--------------------------------------------------------

	void Network::DisplayError(const char* msg, int err_no)
	{
		WCHAR* lpMsgBuf;
		FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
			FORMAT_MESSAGE_FROM_SYSTEM, NULL,
			err_no,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPTSTR)&lpMsgBuf, 0, NULL);
		std::cout << msg;
		std::wcout << lpMsgBuf << std::endl;
		LocalFree(lpMsgBuf);
	}

	SOCKET Network::AcceptEX(SOCKET sock, SOCKET c_sock, PVOID lpBuf, LPOVERLAPPED over)
	{
		return AcceptEx(sock, c_sock, lpBuf, 0, 32, 32, NULL, over);
	}

	HANDLE Network::CreatIOCP()
	{
		return CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0);
	}

	HANDLE Network::ConnectIOCP(HANDLE sock, HANDLE iocp, ULONG_PTR num)
	{
		return CreateIoCompletionPort(sock, iocp, num, 0);
	}

	void Network::ConnectToServer()
	{
		m_Socket = socket(AF_INET, SOCK_STREAM, 0);
		
		Connect(m_Socket, SERVERIP);

		u_long on = 1;
		ioctlsocket(m_Socket, FIONBIO, &on);

		std::string name{ "CL" };
		name += std::to_string(rand() % 100);

		c2s_send_login_packet(m_Socket, name);
	}

	void Network::ConnectToClient()
	{
		h_iocp = CreatIOCP();

		listenSocket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
		ConnectIOCP(reinterpret_cast<HANDLE> (listenSocket), h_iocp, 0);
		BindAndListen(listenSocket);

		accept_over.m_op = OP_ACCEPT;
		memset(&accept_over.m_over, 0, sizeof(accept_over.m_over));

		c_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
		BOOL ret = AcceptEx(listenSocket, c_socket, accept_over.m_packetbuf, 0, 32, 32, NULL, &accept_over.m_over);

		if (FALSE == ret) {
			int err_num = WSAGetLastError();
			if (err_num != WSA_IO_PENDING)
				DisplayError("AcceptEX Error: ", err_num);
		}
	}

	//---------------------------------------------------------------------------------------------------

	int Network::get_new_player_id(int SERVER_ID)
	{
		for (size_t i = SERVER_ID + 1; i < MAX_USER; i++)
		{
			if (0 == players.count(i)) return i;
		}
		return -1;
	}

	void Network::send_packet(int p_id, void* p)
	{
		int p_size = reinterpret_cast<unsigned char*>(p)[0];
		int p_type = reinterpret_cast<unsigned char*>(p)[1];
		/*std::cout << "To client [" << p_id << "]: ";
		std::cout << "Packet [" << p_type << "]\n";*/
		EX_OVER* s_over = new EX_OVER;
		s_over->m_op = OP_SEND;
		memset(&s_over->m_over, 0, sizeof(s_over->m_over));
		memcpy(s_over->m_packetbuf, p, p_size);
		s_over->m_wsabuf[0].buf = reinterpret_cast<CHAR*>(s_over->m_packetbuf);
		s_over->m_wsabuf[0].len = p_size;
		auto ret = WSASend(players[p_id].m_socket, s_over->m_wsabuf, 1, NULL, 0, &s_over->m_over, 0);
		if (0 != ret) {
			auto err_no = WSAGetLastError();
			if (WSA_IO_PENDING != err_no)
				DisplayError("Error in SendPacket: ", err_no);
		}
	}

	void Network::do_recv(int key)
	{
		players[key].m_recv_over.m_wsabuf[0].buf = reinterpret_cast<CHAR*>(players[key].m_recv_over.m_packetbuf) + players[key].m_prev_size;
		players[key].m_recv_over.m_wsabuf[0].len = MAX_BUFFER - players[key].m_prev_size;
		DWORD recv_flag = 0;
		auto ret = WSARecv(players[key].m_socket, players[key].m_recv_over.m_wsabuf, 1, NULL, &recv_flag, &players[key].m_recv_over.m_over, NULL);

		if (0 != ret) {
			auto err_no = WSAGetLastError();
			if (WSA_IO_PENDING != err_no)
				DisplayError("Error in SendPacket: ", err_no);
		}
	}

	void Network::send_add_player(int c_id, int p_id)
	{
		s2c_add_player p;
		p.id = p_id;
		p.size = sizeof(p);
		p.type = S2C_ADD_PLAYER;
		p.x = players[p_id].x;
		p.y = players[p_id].y;
		p.z = players[p_id].z;
		p.isAdd = true;
		send_packet(c_id, &p);
	}

	void Network::send_remove_player(int c_id, int p_id)
	{
		s2c_remove_player p;
		p.id = p_id;
		p.size = sizeof(p);
		p.type = S2C_REMOVE_PLAYER;
		send_packet(c_id, &p);
	}

	void Network::send_move_packet(int c_id, int p_id)
	{
		s2c_move_player p;
		p.id = p_id;
		p.size = sizeof(p);
		p.type = S2C_MOVE_PLAYER;
		send_packet(c_id, &p);
	}

	void Network::send_input_packet(int c_id, int p_id)
	{
		s2c_input_player p;
		p.id = p_id;
		p.size = sizeof(p);
		p.type = S2C_INPUT_PLAYER;
		send_packet(c_id, &p);
	}

	void Network::do_move(int p_id, DIRECTION dir)
	{
		auto& x = players[p_id].x;
		auto& y = players[p_id].y;
		switch (dir) {
		case D_N:
			if (y > 0) y--;
			break;
		case D_S:
			if (y < WORLD_Y_SIZE - 1) y++;
			break;
		case D_W:
			if (x > 0) x--;
			break;
		case D_E:
			if (x < WORLD_X_SIZE - 1) x++;
			break;
		}

		for (auto& pl : players)
			send_move_packet(pl.second.id, p_id);
	}

	void Network::input(int p_id, KeyInput key)
	{
		k.hWnd = key.hWnd;
		k.uMsg = key.uMsg;
		k.wParam = key.wParam;
		k.lParam = key.lParam;
		inputCount = true;
		for (auto& pl : players)
			send_input_packet(pl.second.id, p_id);
	}

	void Network::send_log_ok_packet(int p_id)
	{
		s2c_login_ok p;
		p.id = p_id;
		p.size = sizeof(p);
		p.type = S2C_LOGIN_OK;
		p.isSpawn = true;
		p.x = players[p_id].x;
		p.y = players[p_id].y;
		p.z = players[p_id].z;
		send_packet(p_id, &p);
	}

	void Network::process_packet(int p_id, unsigned char* p_buf)
	{
		switch (p_buf[1])
		{
		case C2S_LOGIN: {
			c2s_login* packet = reinterpret_cast<c2s_login*>(p_buf);
			strcpy_s(players[p_id].m_name, packet->name);
			players[p_id].x = 1.f* p_id;
			players[p_id].y = 1.f* p_id;
			players[p_id].z = -2000.f* p_id;
			std::cout << players[p_id].x << " " << players[p_id].y << " " << players[p_id].z << std::endl;
			send_log_ok_packet(p_id);
		}
					 break;
		case C2S_MOVE: {
			c2s_move* packet = reinterpret_cast<c2s_move*>(p_buf);
			do_move(p_id, packet->dir);
		}
					 break;
		case C2S_INPUT: {
			c2s_input* packet = reinterpret_cast<c2s_input*>(p_buf);
			input(p_id, packet->key);
		}
					 break;
		default:
			std::cout << "unknown packet type from client[" << p_id << "] packet type [" << p_buf[1] << "]" << std::endl;
			while (true);
		}

	}

	void Network::disconnect(int p_id)
	{
		closesocket(players[p_id].m_socket);
		players.erase(p_id);
		for (auto& pl : players)
			send_remove_player(pl.second.id, p_id);
	}

	//--------------------------------------------------CLIENT TO SERVER FUNC-----------------------------------------

	void Network::c2s_send_login_packet(SOCKET m_Socket, std::string& name)
	{
		c2s_login packet;
		packet.size = sizeof(packet);
		packet.type = C2S_LOGIN;
		strcpy_s(packet.name, name.c_str());
		Send(m_Socket, (char*)&packet, packet.size);
	}

	void Network::c2s_send_move_packet(SOCKET m_Socket, DIRECTION dr)
	{
		c2s_move packet;
		packet.size = sizeof(packet);
		packet.type = C2S_MOVE;
		packet.dir = dr;
		Send(m_Socket, (char*)&packet, packet.size);
	}

	void Network::c2s_send_input_packet(SOCKET m_Socket, KeyInput k)
	{
		c2s_input packet;
		packet.size = sizeof(packet);
		packet.type = C2S_INPUT;
		packet.key = k;
		Send(m_Socket, (char*)&packet, packet.size);
	}

	void Network::ProcessData(char* netBuf, int ioByte)
	{
		char* ptr = netBuf;
		static int inPacketSize = 0;
		static int savedPacketSize = 0;
		static char packetBuffer[BUFSIZE];

		while (0 != ioByte) {
			if (0 == inPacketSize) inPacketSize = ptr[0];
			if (ioByte + savedPacketSize >= inPacketSize) {
				memcpy(packetBuffer + savedPacketSize, ptr, inPacketSize - savedPacketSize);
				ProcessPacket(packetBuffer);
				ptr += inPacketSize - savedPacketSize;
				ioByte -= inPacketSize - savedPacketSize;
				inPacketSize = 0;
				savedPacketSize = 0;
			}
			else {
				memcpy(packetBuffer + savedPacketSize, ptr, ioByte);
				savedPacketSize += ioByte;
				ioByte = 0;
			}
		}
	}

	void Network::ProcessPacket(char* ptr)
	{
		switch (ptr[1])
		{
		case S2C_LOGIN_OK:
		{
			s2c_login_ok* packet = reinterpret_cast<s2c_login_ok*>(ptr);
			g_myid = packet->id;
			char buff[100];
			sprintf(buff, "id: %d\n", g_myid);
			OutputDebugStringA(buff);
			g_isSpawn = packet->isSpawn;
			g_x = players[g_myid].x = packet->x;
			g_y = players[g_myid].y = packet->y;
			g_z = players[g_myid].z = packet->z;
		
		}
		break;
		case S2C_INPUT_PLAYER:
		{
			s2c_input_player* packet = reinterpret_cast<s2c_input_player*>(ptr);
			int other_id = packet->id;
			if (other_id == g_myid)
			{

			}
			else if (other_id < MAX_USER)
			{

			}
			else
			{

			}
			break;
		}
		break;
		case S2C_ADD_PLAYER:
		{
			s2c_add_player* packet = reinterpret_cast<s2c_add_player*>(ptr);
			id = packet->id;
			if (id == g_myid) {
				
			}
			else if (id < MAX_USER) {
				players[id].x = packet->x;
				players[id].y = packet->y;
				players[id].z = packet->z;
				g_isAdd = packet->isAdd;
			}
			else {
				
			}
			break;
		}
		case S2C_REMOVE_PLAYER:
		{
			s2c_remove_player* my_packet = reinterpret_cast<s2c_remove_player*>(ptr);
			int other_id = my_packet->id;
			if (other_id == g_myid) {
			}
			else if (other_id < MAX_USER) {
			}
			else {
				//		npc[other_id - NPC_START].attr &= ~BOB_ATTR_VISIBLE;
			}
			break;
		}
		}
	}

}
