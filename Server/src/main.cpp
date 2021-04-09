#include "../../Network/Network.h"
#include "../../Network/protocol.h"

#include <unordered_map>
#include <map>

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "MSWsock.lib")

using namespace std;

enum OP_TYPE {
	OP_RECV,
	OP_SEND,
	OP_ACCEPT,
};

struct EX_OVER
{
	WSAOVERLAPPED m_over;
	WSABUF m_wsabuf[1];
	unsigned char m_packetbuf[MAX_BUFFER];
	OP_TYPE m_op;
};

struct SESSION
{
	SOCKET socket;
	int id;

	EX_OVER m_recv_over;
	int m_prev_size;
	char m_name[200];
	short x, y;
};

constexpr int SERVER_ID = 0;

unordered_map <int, SESSION> players;

void DisplayError(const char* msg, int err_no)
{
	WCHAR* lpMsgBuf;
	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, err_no, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&lpMsgBuf, 0, NULL);
	cout << msg;
	wcout << lpMsgBuf << endl;
	LocalFree(lpMsgBuf);
}

void send_packet(int p_id, void* p)
{
	Net::Network* network = Net::Network::GetInstance();
	int p_size = reinterpret_cast<unsigned char*>(p)[0];
	int p_type = reinterpret_cast<unsigned char*>(p)[1];
	cout << "To client [" << p_id << "]: ";
	cout << "Packet [" << p_type << "]\n";
	EX_OVER* s_over = new EX_OVER;
	s_over->m_op = OP_SEND;
	memset(&s_over->m_over, 0, sizeof(s_over->m_over));
	memcpy(s_over->m_packetbuf, p, p_size);
	s_over->m_wsabuf[0].buf = reinterpret_cast<CHAR*>(s_over->m_packetbuf);
	s_over->m_wsabuf[0].len = p_size;
	auto ret = WSASend(players[p_id].socket, s_over->m_wsabuf, 1, NULL, 0, &s_over->m_over, 0);
	if (0 != ret) {
		auto err_no = WSAGetLastError();
		if (WSA_IO_PENDING != err_no)
			DisplayError("Error in SendPacket: ", err_no);
	}
}

void do_recv(int key)
{
	players[key].m_recv_over.m_wsabuf[0].buf = reinterpret_cast<CHAR*>(players[key].m_recv_over.m_packetbuf) + players[key].m_prev_size;
	players[key].m_recv_over.m_wsabuf[0].len = MAX_BUFFER - players[key].m_prev_size;
	DWORD recv_flag = 0;
	auto ret = WSARecv(players[key].socket, players[key].m_recv_over.m_wsabuf, 1, NULL, &recv_flag, &players[key].m_recv_over.m_over, NULL);

	if (0 != ret) {
		auto err_no = WSAGetLastError();
		if (WSA_IO_PENDING != err_no)
			DisplayError("Error in SendPacket: ", err_no);
	}
}

void send_add_player(int c_id, int p_id)
{
	s2c_add_player p;
	p.id = p_id;
	p.size = sizeof(p);
	p.type = S2C_ADD_PLAYER;
	p.x = players[p_id].x;
	p.y = players[p_id].y;
	p.race = 0;
	send_packet(c_id, &p);
}

void send_remove_player(int c_id, int p_id)
{
	s2c_remove_player p;
	p.id = p_id;
	p.size = sizeof(p);
	p.type = S2C_REMOVE_PLAYER;
	send_packet(c_id, &p);
}

void send_move_packet(int c_id, int p_id)
{
	s2c_move_player p;
	p.id = p_id;
	p.size = sizeof(p);
	p.type = S2C_MOVE_PLAYER;
	p.x = players[p_id].x;
	p.y = players[p_id].y;
	send_packet(c_id, &p);
}

void do_move(int p_id, DIRECTION dir) 
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

int get_new_player_id() 
{
	for (size_t i = SERVER_ID + 1; i < MAX_USER; i++)
	{
		if (0 == players.count(i)) return i;
	}
}

void send_log_ok_packet(int p_id) 
{
	s2c_login_ok p;
	p.hp = 10;
	p.id = p_id;
	p.level = 2;
	p.race = 1;
	p.size = sizeof(p);
	p.type = S2C_LOGIN_OK;
	p.x = players[p_id].x;
	p.y = players[p_id].y;
	send_packet(p_id, &p);
}

void process_packet(int p_id, unsigned char* p_buf) 
{
	switch (p_buf[1])
	{
	case C2S_LOGIN: {
		c2s_login* packet = reinterpret_cast<c2s_login*>(p_buf);
		strcpy_s(players[p_id].m_name, packet->name);
		send_log_ok_packet(p_id);
	}
				  break;
	case C2S_MOVE: {
		c2s_move* packet = reinterpret_cast<c2s_move*>(p_buf);
		do_move(p_id, packet->dir);
	}
				 break;
	default:
		cout << "unknown packet type from client[" << p_id << "] packet type [" << p_buf[1] << "]" << endl;
		while (true);
	}

}

void disconnect(int p_id)
{
	closesocket(players[p_id].socket);
	players.erase(p_id);
	for (auto& pl : players)
		send_remove_player(pl.second.id, p_id);
}

int main()
{
	Net::Network* network = Net::Network::GetInstance();

	wcout.imbue(locale("korean"));

	HANDLE h_iocp = network->CreatIOCP();

	SOCKET listenSocket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	network->ConnectIOCP(reinterpret_cast<HANDLE>(listenSocket), h_iocp, 0);

	network->BindAndListen(listenSocket);

	EX_OVER accept_over;
	accept_over.m_op = OP_ACCEPT;
	memset(&accept_over.m_over, 0, sizeof(accept_over.m_over));

	SOCKET c_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	network->AcceptEX(listenSocket, c_socket, accept_over.m_packetbuf, &accept_over.m_over);

	while (true) {
		DWORD num_bytes;
		ULONG_PTR ikey;
		WSAOVERLAPPED* over;
		BOOL ret = GetQueuedCompletionStatus(h_iocp, &num_bytes, &ikey, &over, INFINITE);

		int key = static_cast<int>(ikey);

		if (FALSE == ret)
		{
			if (SERVER_ID == key) {
				network->DisplayError("GQCS: ", WSAGetLastError());
				exit(-1);
			}
			else {
				network->DisplayError("GQCS: ", WSAGetLastError());
				disconnect(key);
			}
		}
		/*if (0 == num_bytes) {
			disconnect(key);
			continue;
		}*/
		EX_OVER* ex_over = reinterpret_cast<EX_OVER*>(over);
		switch (ex_over->m_op)
		{
		case OP_RECV: {
			// 패킷 재조립 및 처리
			unsigned char* packet_ptr = ex_over->m_packetbuf;
			int num_data = num_bytes + players[key].m_prev_size;
			int packet_size = packet_ptr[0];

			while (num_data >= packet_size) {
				process_packet(key, packet_ptr);
				num_data -= packet_size;
				packet_ptr += packet_size;
				if (0 >= num_data)	break;
				packet_size = packet_ptr[0];
			}

			players[key].m_prev_size = num_data;

			if (num_data != 0) {
				memcpy(ex_over->m_packetbuf, packet_ptr, num_data);
			}
			do_recv(key);
			}
			break;

		case OP_SEND:
			delete ex_over;
			break;

		case OP_ACCEPT: {
			int c_id = get_new_player_id();
			if (-1 != c_id) {
				players[c_id] = SESSION{};
				players[c_id].id = c_id;
				players[c_id].m_name[0] = 0;
				players[c_id].m_recv_over.m_op = OP_RECV;
				players[c_id].socket = c_socket;
				players[c_id].m_prev_size = 0;
				network->ConnectIOCP(reinterpret_cast<HANDLE>(c_socket), h_iocp, c_id);
				for (auto& pl : players) {
					if (c_id == pl.second.id) continue;
					send_add_player(c_id, pl.second.id);
					send_add_player(pl.second.id, c_id);
				}
				do_recv(c_id);
			}
			else {
				closesocket(c_socket);
			}
			memset(&accept_over.m_over, 0, sizeof(accept_over.m_over));
			c_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
			network->AcceptEX(listenSocket, c_socket, accept_over.m_packetbuf, &accept_over.m_over);
			}
			break;
		}
	}
	network->Release(listenSocket);
}