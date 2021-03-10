#include "Network.h"

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

void Network::ErrDisplay(const wchar_t* msg)
{
	LPVOID lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	printf("[%s] %s", msg, lpMsgBuf);
	LocalFree(lpMsgBuf);
}

void Network::ErrDisplay(int errcode)
{
	LPVOID lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, errcode,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	printf("[¿À·ù] %s", (char*)lpMsgBuf);
	LocalFree(lpMsgBuf);
}

int Network::Recvn(SOCKET s, char* buf, int len, int flags)
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

void Network::Recv(SOCKET sock, char* buf, int dataSize)
{
	retval = Recvn(sock, buf, dataSize, 0);
}

void Network::Release(SOCKET sock)
{
	closesocket(sock);
	WSACleanup();
}
