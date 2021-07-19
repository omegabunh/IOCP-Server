#pragma once
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _WINSOCKAPI_ 

#include <iostream>
#include <WS2tcpip.h>
#include <MSWSock.h>

#include <unordered_map>
#include <thread>
#include <vector>
#include <mutex>

#include <cstdlib>
#include <cassert>
#include <unordered_map>

#include "Crystal/Network/Protocol.h" 
#include "Crystal/GamePlay/World/Level.h"

#define SERVERIP   "127.0.0.1"
#define SERVERPORT 9000
#define BUFSIZE    1024
constexpr int SERVER_ID = 0;

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "MSWsock.lib")

enum OP_TYPE {
	OP_RECV,
	OP_SEND,
	OP_ACCEPT,
};

struct EX_OVER
{
	WSAOVERLAPPED m_over;
	WSABUF m_wsabuf[1];
	unsigned char m_packetbuf[BUFSIZE];
	OP_TYPE m_op;
};

struct SESSION
{
	SOCKET m_socket;
	int id;

	EX_OVER m_recv_over;
	int m_prev_size;
	char m_name[200];
	float x, y, z;
	bool isSpawn;
};

namespace Net
{
	class Network
	{
	public:
		static Network* GetInstance();
		void ErrQuit(const wchar_t* msg);
		void BindAndListen(SOCKET sock);
		SOCKET Accept(SOCKET sock);
		void Connect(SOCKET sock, const char* address);
		void Release(SOCKET sock);
		void Send(SOCKET sock, char* buf, int dataSize);
		int Recv(SOCKET sock, char* buf, int dataSize);
		void DisplayError(const char* msg, int err_no);
		SOCKET AcceptEX(SOCKET sock, SOCKET c_sock, PVOID lpBuf, LPOVERLAPPED over);
		HANDLE CreatIOCP();
		HANDLE ConnectIOCP(HANDLE sock, HANDLE iocp, ULONG_PTR num);

		void ConnectToServer();
		void ConnectToClient();

		int get_new_player_id(int SERVER_ID);
		void send_packet(int p_id, void* p);
		void do_recv(int key);
		void send_add_player(int c_id, int p_id);
		void send_remove_player(int c_id, int p_id);
		void send_move_packet(int c_id, int p_id);
		void send_input_packet(int c_id, int p_id);
		void do_move(int p_id, DIRECTION dir);
		void input(int p_id, KeyInput k);
		void send_log_ok_packet(int p_id);
		void process_packet(int p_id, unsigned char* p_buf);
		void disconnect(int p_id);
		void c2s_send_login_packet(SOCKET m_Socket, std::string& name);
		void c2s_send_move_packet(SOCKET m_Socket, DIRECTION dr);
		void c2s_send_input_packet(SOCKET m_Socket, KeyInput k);
		void ProcessPacket(char* ptr);
		void ProcessData(char* netBuf, int ioByte);
	private:
		Network();
		~Network();

	public:
		int retval;
		std::unordered_map <int, SESSION> players;
		KeyInput k;
		int g_myid;
		int id;
		bool g_isSpawn = false;
		bool g_isAdd = false;
		float g_x, g_y, g_z = 0;
		bool inputCount = false;

		HANDLE h_iocp;
		SOCKET listenSocket;
		EX_OVER accept_over;
		SOCKET c_socket;
		SOCKET m_Socket;
	private:


	};

}

