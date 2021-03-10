#pragma once
#include <winsock2.h>
#include <cstdlib>
#include <cstdio>
#include <windows.h>
#include <cassert>

#define SERVERIP   "127.0.0.1"
#define SERVERPORT 9000
#define BUFSIZE    108

class Network
{
public:
	static Network* GetInstance();
	void BindAndListen(SOCKET sock);
	SOCKET Accept(SOCKET sock);
	void Connect(SOCKET sock, const char* address);
	void Release(SOCKET sock);
	void Send(SOCKET sock, char* buf, int dataSize);
	void Recv(SOCKET sock, char* buf, int dataSize);
	void ErrQuit(const wchar_t* msg);
	void ErrDisplay(const wchar_t* msg);
	void ErrDisplay(int errcode);

private:
	Network();
	~Network();
	int Recvn(SOCKET s, char* buf, int len, int flags);

public:
	int retval;
	bool isServer;

private:


};

