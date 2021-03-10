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

unsigned WINAPI SendMsg(void* arg);//������ �����Լ�
unsigned WINAPI RecvMsg(void* arg);//������ �����Լ�
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
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)// ������ ������ ����Ѵٰ� �ü���� �˸�
        ErrorHandling("WSAStartup() error!");

    sprintf(name, "%s", argv[3]);
    sock = socket(PF_INET, SOCK_STREAM, 0);//������ �ϳ� �����Ѵ�.

    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr(argv[1]);
    serverAddr.sin_port = htons(atoi(argv[2]));

    if (connect(sock, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)//������ �����Ѵ�.
        ErrorHandling("connect() error");

    // ���ӿ� �����ϸ� �� �� �Ʒ��� ����ȴ�.
    std::cout << "Connect Success!\n";
    std::cout << "Sending client's name\n";
    send(sock, name, sizeof(name), 0);
    std::cout << "Success!\n";

    sendThread = (HANDLE)_beginthreadex(NULL, 0, SendMsg, (void*)&sock, 0, NULL);//�޽��� ���ۿ� �����尡 ����ȴ�.
    recvThread = (HANDLE)_beginthreadex(NULL, 0, RecvMsg, (void*)&sock, 0, NULL);//�޽��� ���ſ� �����尡 ����ȴ�.

    WaitForSingleObject(sendThread, INFINITE);//���ۿ� �����尡 �����ɶ����� ��ٸ���./
    WaitForSingleObject(recvThread, INFINITE);//���ſ� �����尡 �����ɶ����� ��ٸ���.
    //Ŭ���̾�Ʈ�� ���Ḧ �õ��Ѵٸ� ���� �Ʒ��� ����ȴ�.
    closesocket(sock);//������ �����Ѵ�.
    WSACleanup();//������ ���� ��������� �ü���� �˸���.
    std::cout << "Client exit\n";
    return 0;
}

unsigned WINAPI SendMsg(void* arg) {//���ۿ� �������Լ�
    SOCKET sock = *((SOCKET*)arg);//������ ������ �����Ѵ�.
    char msg[BUF_SIZE];
    while (1) {//�ݺ�
        fgets(msg, BUF_SIZE, stdin);//�Է��� �޴´�.
        if (!strcmp(msg, "q\n") || !strcmp(msg, "Q\n")) {//q�� �Է��ϸ� �����Ѵ�.
            closesocket(sock);
            send(sock, "", 0, 0);
            exit(0);
        }
        send(sock, msg, strlen(msg), 0);//nameMsg�� �������� �����Ѵ�.
    }
    return 0;
}

unsigned WINAPI RecvMsg(void* arg) {
    SOCKET sock = *((SOCKET*)arg);//������ ������ �����Ѵ�.
    char msg[NAME_SIZE + BUF_SIZE];
    int strLen;
    while (1) {//�ݺ�
        strLen = recv(sock, msg, NAME_SIZE + BUF_SIZE - 1, 0);//�����κ��� �޽����� �����Ѵ�.
        if (strLen == -1)
            return -1;
        msg[strLen] = 0;//���ڿ��� ���� �˸��� ���� ����
        std::cout << ">>" << msg << '\n';
    }
    return 0;
}

void ErrorHandling(const char* msg) {
    fputs(msg, stderr);
    fputc('\n', stderr);
    exit(1);
}
