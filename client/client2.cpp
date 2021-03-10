#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <Windows.h>
#include <process.h>

// vs warning and winsock error 
#pragma comment(lib, "ws2_32.lib")
#pragma warning (disable : 4996)

#define BUF_SIZE 1024
#define NAME_SIZE 20

unsigned WINAPI SendMsg(void* arg);//쓰레드 전송함수
unsigned WINAPI RecvMsg(void* arg);//쓰레드 수신함수
void ErrorHandling(const char* msg);

char name[NAME_SIZE] = "[DEFAULT]";
char msg[BUF_SIZE];

int main(int argc, char* argv[]) {
    WSADATA wsaData;
    SOCKET sock;
    SOCKADDR_IN serverAddr;
    HANDLE sendThread, recvThread;
    if (argc != 4) {
        printf("Usage : %s <IP> <port> <name>\n", argv[0]);
        exit(1);
    }
    std::cout << "connect........\n";
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)// 윈도우 소켓을 사용한다고 운영체제에 알림
        ErrorHandling("WSAStartup() error!");

    sprintf(name, "%s", argv[3]);
    sock = socket(PF_INET, SOCK_STREAM, 0);//소켓을 하나 생성한다.

    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr(argv[1]);
    serverAddr.sin_port = htons(atoi(argv[2]));

    if (connect(sock, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)//서버에 접속한다.
        ErrorHandling("connect() error");

    // 접속에 성공하면 이 줄 아래가 실행된다.
    std::cout << "Connect Success!\n";
    std::cout << "Sending client's name\n";
    send(sock, name, sizeof(name), 0);
    std::cout << "Success!\n";

    sendThread = (HANDLE)_beginthreadex(NULL, 0, SendMsg, (void*)&sock, 0, NULL);//메시지 전송용 쓰레드가 실행된다.
    recvThread = (HANDLE)_beginthreadex(NULL, 0, RecvMsg, (void*)&sock, 0, NULL);//메시지 수신용 쓰레드가 실행된다.

    WaitForSingleObject(sendThread, INFINITE);//전송용 쓰레드가 중지될때까지 기다린다./
    WaitForSingleObject(recvThread, INFINITE);//수신용 쓰레드가 중지될때까지 기다린다.
    //클라이언트가 종료를 시도한다면 이줄 아래가 실행된다.
    closesocket(sock);//소켓을 종료한다.
    WSACleanup();//윈도우 소켓 사용중지를 운영체제에 알린다.
    std::cout << "Client exit\n";
    return 0;
}

unsigned WINAPI SendMsg(void* arg) {//전송용 쓰레드함수
    SOCKET sock = *((SOCKET*)arg);//서버용 소켓을 전달한다.
    char msg[BUF_SIZE];
    while (1) {//반복
        fgets(msg, BUF_SIZE, stdin);//입력을 받는다.
        if (!strcmp(msg, "q\n") || !strcmp(msg, "Q\n")) {//q를 입력하면 종료한다.
            closesocket(sock);
            send(sock, "", 0, 0);
            exit(0);
        }
        send(sock, msg, strlen(msg), 0);//nameMsg를 서버에게 전송한다.
    }
    return 0;
}

unsigned WINAPI RecvMsg(void* arg) {
    SOCKET sock = *((SOCKET*)arg);//서버용 소켓을 전달한다.
    char msg[NAME_SIZE + BUF_SIZE];
    int strLen;
    while (1) {//반복
        strLen = recv(sock, msg, NAME_SIZE + BUF_SIZE - 1, 0);//서버로부터 메시지를 수신한다.
        if (strLen == -1)
            return -1;
        msg[strLen] = 0;//문자열의 끝을 알리기 위해 설정
        std::cout << ">>" << msg << '\n';
    }
    return 0;
}

void ErrorHandling(const char* msg) {
    fputs(msg, stderr);
    fputc('\n', stderr);
    exit(1);
}
