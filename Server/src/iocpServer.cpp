#define _WINSOCK_DEPRECATED_NO_WARNINGS
#pragma comment(lib, "ws2_32")

#include <winsock2.h>
#include <stdlib.h>
#include <stdio.h>

#define SERVERPORT 9000
#define BUFSIZE    512

// 소켓 정보 저장을 위한 구조체
struct SOCKETINFO 
{
	OVERLAPPED overlapped;	// 각 소켓마다 OVERLAPPED 구조체
	SOCKET sock;
	char buf[BUFSIZE+1];	// 응용 프로그램 버퍼
	int recvbytes;	// 송신 바이트 수
	int sendbytes;	// 수신 바이트 수
	WSABUF wsabuf;	// WSABUF 구조체 
};

DWORD WINAPI WorkerThread(LPVOID arg);

// 소켓 함수 오류 출력 후 종료
void errQuit(const wchar_t* msg)
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

// 소켓 함수 오류 출력
void errDisplay(const wchar_t* msg)
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

// 소켓 함수 오류 출력
void errDisplay(int errcode)
{
	LPVOID lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, errcode,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	printf("[오류] %s", (char*)lpMsgBuf);
	LocalFree(lpMsgBuf);
}

int main(int argc, char* argv[])
{
	int retval;

	// 윈속 초기화
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return 1;

	// 1. 입출력 완료 포트 생성
	HANDLE hcp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (hcp == NULL) return 1;

	// CPU 개수 확인 - CPU개수에 비례하여 작업자 스레드를 생성하기 위함
	SYSTEM_INFO si;
	GetSystemInfo(&si);

	// 2. (CPU 개수 * 2) 개의 작업자 스레드 생성 - 스레드 함수 인자로 입출력 완료 포트 핸들 값을 전달
	HANDLE hThread;
	for (int i = 0; i < (int)si.dwNumberOfProcessors * 2; i++) {
		hThread = CreateThread(NULL, 0, WorkerThread, hcp, 0, NULL);
		if (hThread == NULL)return 1; 
		CloseHandle(hThread);
	}

	// socket()
	SOCKET listenSock = socket(AF_INET, SOCK_STREAM, 0);
	if (listenSock == INVALID_SOCKET) errQuit(L"socket()");

	// bind()
	SOCKADDR_IN serveraddr;
	ZeroMemory(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons(SERVERPORT);
	retval = bind(listenSock, (SOCKADDR*)&serveraddr, sizeof(serveraddr));
	if (retval == SOCKET_ERROR) errQuit(L"bind()");

	// listen()
	retval = listen(listenSock, SOMAXCONN);
	if (retval == SOCKET_ERROR) errQuit(L"listen()");

	// 비동기 입출력을 지원하는 소켓 생성. 데이터 통신에 사용할 변수
	SOCKET clientSock;
	SOCKADDR_IN clientaddr;
	int addrlen;
	DWORD recvbytes, flags;

	while (1) {
		// accept()
		addrlen = sizeof(clientaddr);
		clientSock = accept(listenSock, (SOCKADDR*)&clientaddr, &addrlen);
		if (clientSock == INVALID_SOCKET) {
			errDisplay(L"accept()");
			break;
		}
		printf("\n[TCP 서버] 클라이언트 접속: IP 주소=%s, 포트 번호=%d\n",
			inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port));

		// 소켓과 입출력 완료 포트 연결 - accept() 함수가 리턴한 소켓을 입출력 완료 포트와 연결
		CreateIoCompletionPort((HANDLE)clientSock, hcp, clientSock, 0);

		// 소켓 정보 구조체 할당 - 할당 및 초기화
		SOCKETINFO* ptr = new SOCKETINFO;
		if (ptr == NULL) break;

		ZeroMemory(&ptr->overlapped, sizeof(ptr->overlapped));
		ptr->sock = clientSock;
		ptr->recvbytes = ptr->sendbytes = 0;
		ptr->wsabuf.buf = ptr->buf;
		ptr->wsabuf.len = BUFSIZE;

		// 비동기 입출력 시작 - WSARecv() 함수를 호출해 비동기 입출력을 시작
		flags = 0;
		retval = WSARecv(clientSock, &ptr->wsabuf, 1, &recvbytes, &flags, &ptr->overlapped, NULL);
		if (retval == SOCKET_ERROR) {
			if (WSAGetLastError() != WSA_IO_PENDING) {
				errDisplay(L"WSARecv()");
				return 1;
			}
			continue;
		}

	}

	// 윈속 종료
	WSACleanup();
	return 0;
}

DWORD WINAPI WorkerThread(LPVOID arg)
{
	int retval;
	HANDLE hcp = (HANDLE)arg;	// 스레드 함수 인자로 전달된 입출력 완료 포트 핸들 값을 저장

	while (1) {
		// 비동기 입출력 완료 기다리기
		DWORD cbTransferred;
		SOCKET clientSock;
		SOCKETINFO *ptr;
		retval = GetQueuedCompletionStatus(hcp, &cbTransferred,
			(PULONG_PTR)&clientSock, (LPOVERLAPPED*)&ptr, INFINITE);

		// 클라이언트 정보 얻기
		SOCKADDR_IN clientaddr;
		int addrlen = sizeof(clientaddr);
		getpeername(ptr->sock, (SOCKADDR*)&clientaddr, &addrlen);

		// 비동기 입출력 결과 확인
		if (retval == 0 || cbTransferred == 0) {
			if (retval == 0) {
				DWORD temp1, temp2;
				WSAGetOverlappedResult(ptr->sock, &ptr->overlapped,
					&temp1, FALSE, &temp2);
				errDisplay(L"WSAGetOverlappedResult()");
			}
			closesocket(ptr->sock);
			printf(" [TCP 서버] 클라이언트 종료: IP 주소=%s, 포트 번호=%d\n",
				inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port));
			delete ptr;
			continue;
		}

		// 데이터 전송량 갱신
		if (ptr->recvbytes == 0) {
			ptr->recvbytes = cbTransferred;
			ptr->sendbytes = 0;
			// 받은 데이터 출력
			ptr->buf[ptr->recvbytes] = '\0';
			printf(" [TCP/%s:%d] %s\n", inet_ntoa(clientaddr.sin_addr),
				ntohs(clientaddr.sin_port), ptr->buf);
		}

		else {
			ptr->sendbytes += cbTransferred;
		}

		// 보낸 데이터가 받은 데이터보다 적으면 아직 보내지 못한 데이터를 마저 보낸다.
		if (ptr->recvbytes > ptr->sendbytes) {
			// 데이터 보내기
			ZeroMemory(&ptr->overlapped, sizeof(ptr->overlapped));
			ptr->wsabuf.buf = ptr->buf + ptr->sendbytes;
			ptr->wsabuf.len = ptr->recvbytes - ptr->sendbytes;

			DWORD sendbytes;
			retval = WSASend(ptr->sock, &ptr->wsabuf, 1, &sendbytes, 0, &ptr->overlapped, NULL);
			if (retval == SOCKET_ERROR) {
				if (WSAGetLastError() != WSA_IO_PENDING) {
					errDisplay(L"WSASend()");
				}
				continue;
			}
		}

		else {
			// 소켓 정보 중 받은 데이터 수를 초기화한 후
			ptr->recvbytes = 0;

			// 도착한 데이터를 읽는다.
			// 데이터 받기
			ZeroMemory(&ptr->overlapped, sizeof(ptr->overlapped));
			ptr->wsabuf.buf = ptr->buf;
			ptr->wsabuf.len = BUFSIZE;

			DWORD recvbytes;
			DWORD flags = 0;
			retval = WSARecv(ptr->sock, &ptr->wsabuf, 1, &recvbytes,
				&flags, &ptr->overlapped, NULL);
			if (retval == SOCKET_ERROR) {
				if (WSAGetLastError() != WSA_IO_PENDING) {
					errDisplay(L"WSARecv()");
				}
				continue;
			}
		}
	}

	return 0;
}
