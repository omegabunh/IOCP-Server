#include <iostream>
#include <stdlib.h>
#include <process.h>
#include <winsock2.h>
#include <windows.h>
#include <vector>
#include <Ws2tcpip.h> //inet_pton 
#include <mutex>
// vs warning and winsock error 
#pragma comment(lib, "ws2_32.lib")
#pragma warning (disable : 4996)

//IOCPClient.exe 127.0.0.1 3800 dkssud

std::mutex push_lock;
std::mutex erase_lock;
std::mutex sock_lock;

constexpr int PORT_NUM = 3800;
constexpr int BUF_SIZE = 1024;
constexpr int READ = 3;
constexpr int WRITE = 5;
constexpr int CLIENT_SIZE = 3000;
int login_count = 0;
int logout_count = 0;

typedef struct    // socket info
{
    SOCKET hClntSock;
    SOCKADDR_IN clntAdr;
    CHAR name[20];
    CHAR ip[22];
} PER_HANDLE_DATA, * LPPER_HANDLE_DATA;

typedef struct    // buffer info
{
    OVERLAPPED overlapped;
    WSABUF wsaBuf;
    char buffer[BUF_SIZE];
    INT rwMode;    // READ or WRITE
} PER_IO_DATA, * LPPER_IO_DATA;

// Ŭ���̾�Ʈ ����� ���� vector container
std::vector<LPPER_HANDLE_DATA> UserList;

void ErrorHandling(const char* message);
unsigned __stdcall ThreadMain(void* CompletionPortIO);

int main(int argc, char* argv[])
{
    WSADATA    wsaData;
    HANDLE hIOCP;
    SYSTEM_INFO sysInfo;
    LPPER_IO_DATA ioInfo;
    LPPER_HANDLE_DATA handleInfo;

    SOCKET hServSock;
    SOCKADDR_IN servAdr;
    DWORD recvBytes, flags = 0;
    INT i;

    // winsock start
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
        ErrorHandling("WSAStartup Error");

    //IOCP ��ü ����
    hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 0);
    GetSystemInfo(&sysInfo);

    // main thread�� ����� thread ����
    for (i = 0; i < sysInfo.dwNumberOfProcessors; i++)
        _beginthreadex(NULL, 0, ThreadMain, (LPVOID)hIOCP, 0, NULL);

    // socket ����
    hServSock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
    memset(&servAdr, 0, sizeof(servAdr));
    servAdr.sin_family = AF_INET;
    servAdr.sin_addr.s_addr = htonl(INADDR_ANY);
    servAdr.sin_port = htons(PORT_NUM);

    // bind and listen q
    bind(hServSock, (SOCKADDR*)&servAdr, sizeof(servAdr));
    listen(hServSock, CLIENT_SIZE);
   
    while (1)
    {
        sock_lock.lock();

        SOCKET hClntSock;
        SOCKADDR_IN clntAdr;
        int addrLen = sizeof(clntAdr);

        hClntSock = accept(hServSock, (SOCKADDR*)&clntAdr, &addrLen);

        sock_lock.unlock();
        handleInfo = (LPPER_HANDLE_DATA)malloc(sizeof(PER_HANDLE_DATA));        // LPPER_HANDLE_DATA �ʱ�ȭ
        inet_ntop(AF_INET, &clntAdr.sin_addr, handleInfo->ip, INET_ADDRSTRLEN);    // get new client ip
        handleInfo->hClntSock = hClntSock;                                // Ŭ���̾�Ʈ�� ������ ����ü�� ��� ���´�.
        std::cout << ++login_count << '\n';
        printf("New Client Access : %s\n", handleInfo->ip);
        memcpy(&(handleInfo->clntAdr), &clntAdr, addrLen);

        // ���� ����� ��Ʈ�� accept�� ���ؼ� return �� Ŭ���̾�Ʈ ������ ���´�.
        // ��Ĺ�� IOCP�� ����
        CreateIoCompletionPort((HANDLE)hClntSock, hIOCP, (DWORD)handleInfo, 0);

        push_lock.lock();
        // Ŭ���̾�Ʈ user data �ʱ�ȭ
        UserList.push_back(handleInfo);

        push_lock.unlock();

        // Ŭ���̾�Ʈ�� ������ �� data �ʱ�ȭ
        ioInfo = (LPPER_IO_DATA)malloc(sizeof(PER_IO_DATA));
        memset(&(ioInfo->overlapped), 0, sizeof(OVERLAPPED));
        memset(ioInfo->buffer, 0x00, BUF_SIZE);
        ioInfo->wsaBuf.len = BUF_SIZE;
        ioInfo->wsaBuf.buf = ioInfo->buffer;
        ioInfo->rwMode = READ;

        // name �ޱ�
        recv(handleInfo->hClntSock, handleInfo->name, 20, 0);
        // �񵿱� ����� ����
        WSARecv(handleInfo->hClntSock, &(ioInfo->wsaBuf), 1, &recvBytes, &flags, &(ioInfo->overlapped), NULL);
    }
    return 0;
}

unsigned __stdcall ThreadMain(void* pCompletionPort)
{
    HANDLE hCompletionPort = (HANDLE)pCompletionPort;
    SOCKET sock;
    DWORD bytesTrans;
    LPPER_HANDLE_DATA handleInfo;
    LPPER_IO_DATA ioInfo;
    int flags = 0;
    CHAR message[BUF_SIZE];

    while (1)
    {
        // �񵿱� ����� �ϷḦ ��ٸ���. 
        //1param: IOCP��ü, 2param: ���۵� ������ ��, 3param: �̸� ���س��� ID, 4param: overlappedI/O ����ü, 5param: timeout
        GetQueuedCompletionStatus(hCompletionPort, &bytesTrans, (LPDWORD)&handleInfo, (LPOVERLAPPED*)&ioInfo, INFINITE);
        // Ŭ���̾�Ʈ�� socket�� �����´�.
        sock = handleInfo->hClntSock;

        // ù ������ �б� ����
        if (ioInfo->rwMode == READ)
        {
            puts("\nmessage received!");
            if (bytesTrans == 0)
            {
                erase_lock.lock();
                std::vector<LPPER_HANDLE_DATA>::iterator iter;
                for (iter = UserList.begin(); iter != UserList.end(); ++iter) {
                    if ((*iter)->hClntSock == sock) {
                        std::cout << "Logout >> "
                            << "IP :" << (*iter)->ip << ", "
                            << "Sock :" << (*iter)->hClntSock << ", "
                            << "name :" << (*iter)->name << '\n';
                        UserList.erase(iter);
                        std::cout << ++logout_count << '\n';
                        break;
                    }
                }
                closesocket(sock);
                free(handleInfo);
                free(ioInfo);
                erase_lock.unlock();
                continue;
            }
            memcpy(message, ioInfo->wsaBuf.buf, BUF_SIZE);
            message[bytesTrans] = '\0';            // ���ڿ��� ���� \0�� �߰��Ѵ� (������ ���� ����)

            /* name ������
            char *ptr = strtok(message, "]");    // [] => ']'�������� ������.
            strcpy_s(handleInfo->name, ptr + 1);    // ]���� ���� name
            ptr = strtok(NULL, "]");            // ]���� �ٽ� ���� message
            strcpy_s(message, ptr);                // message�� name�� ����������.
            */
            printf("name : %s\n", handleInfo->name);
            printf("Sock[%d] : %s\n", sock, message);

            // Ŭ���̾�Ʈ�� ���� ������ ����ü�� ������ �ٲ۴�.
            // ���� ������ ���⸦ ����
            std::vector<LPPER_HANDLE_DATA>::iterator iter;

            free(ioInfo);

            for (iter = UserList.begin(); iter != UserList.end(); ++iter) {
                ioInfo = (LPPER_IO_DATA)malloc(sizeof(PER_IO_DATA));
                memset(&(ioInfo->overlapped), 0x00, sizeof(OVERLAPPED));
                int len = strlen(message);
                ioInfo->wsaBuf.len = len;
                strcpy_s(ioInfo->buffer, message);
                ioInfo->wsaBuf.buf = ioInfo->buffer;
                ioInfo->rwMode = WRITE;
                if (bytesTrans == 0)
                {
                    closesocket(sock);
                    free(handleInfo);
                    free(ioInfo);
                    continue;
                }

                if (WSASend((*iter)->hClntSock, &(ioInfo->wsaBuf), 1, &bytesTrans, 0, &(ioInfo->overlapped), NULL) == SOCKET_ERROR)
                {
                    if (WSAGetLastError() != WSA_IO_PENDING)
                        ErrorHandling("WSASend() error");
                }
            }
            // ������ ����ü �ʱ�ȭ, ���� -> �б� ���� ����
            ioInfo = (LPPER_IO_DATA)malloc(sizeof(PER_IO_DATA));
            memset(&(ioInfo->overlapped), 0x00, sizeof(OVERLAPPED));
            ioInfo->wsaBuf.len = BUF_SIZE;
            ioInfo->wsaBuf.buf = ioInfo->buffer;
            ioInfo->rwMode = READ;

            // �б� ����
            if (WSARecv(sock, &(ioInfo->wsaBuf), 1, &bytesTrans, (LPDWORD)&flags, &(ioInfo->overlapped), NULL) == SOCKET_ERROR)
            {
                if (WSAGetLastError() != WSA_IO_PENDING)
                    ErrorHandling("WSASend() error");
            }
        }
        // ���� ����
        else {
            printf("Message Sent!\n");
            free(ioInfo);
        }
    }
    return 0;
}

void ErrorHandling(const char* message)
{
    perror(message);
    exit(1);
}
