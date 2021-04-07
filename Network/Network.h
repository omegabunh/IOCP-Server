#pragma once
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <iostream>
#include <WS2tcpip.h>
#include <MSWSock.h>

#include <cstdlib>
#include <windows.h>
#include <cassert>


#define SERVERIP   "127.0.0.1"
#define SERVERPORT 3500
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
	void DisplayError(const char* msg, int err_no);
	void AcceptEX(SOCKET sock, SOCKET c_sock, PVOID lpBuf, LPOVERLAPPED over);
	HANDLE CreatIOCP();
	HANDLE ConnectIOCP(HANDLE sock, HANDLE iocp, ULONG_PTR num);

private:
	Network();
	~Network();
	int Recvn(SOCKET s, char* buf, int len, int flags);

public:
	int retval;

private:

	
};


