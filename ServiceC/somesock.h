#ifndef _SOMESOCK_H_
#define _SOMESOCK_H_

// somesock.h
// utility for basic socket procedures
// works for Windows and Linux targets
// part of Microservices demo: MSBasics

#include <cstdio>

#ifdef TARGET_WIN
#include <ws2tcpip.h>
#include <winsock2.h>
#endif

#ifdef TARGET_UX
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

// wrap winsock2
typedef uint8_t BYTE;
typedef uint32_t DWORD;
typedef uintptr_t DWORD_PTR;
typedef uint16_t WORD;
struct WSADATA
{
    int dummy;
};
void WSACleanup() {}
int WSAStartup(int p1, WSADATA *p2)
{
    return 0;
}
#define MAKEWORD(a, b) ((WORD)(((BYTE)(((DWORD_PTR)(a)) & 0xff)) | ((WORD)((BYTE)(((DWORD_PTR)(b)) & 0xff))) << 8))
typedef int SOCKET;
#define SOCKET_ERROR -1
#define INVALID_SOCKET -1
#define SOCKADDR struct sockaddr
#define closesocket(a) close(a)
#define SD_SEND SHUT_WR
#endif

int sock_connect(const char *servicename, int serviceport, SOCKET &sock)
{
    WSADATA wsadata;
    int iresult = WSAStartup(MAKEWORD(2, 2), &wsadata);
    if (iresult != 0)
    {
#ifdef _DEBUG
        printf("Socket Interface: error WSAStartup\n");
#endif
        return 0;
    }
    sock = INVALID_SOCKET;
    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET)
    {
#ifdef _DEBUG
        printf("Socket Interface: error socket create\n");
#endif
        WSACleanup();
        return 0;
    }

    struct sockaddr_in client;
    struct hostent *he = gethostbyname(servicename);
    if (he == NULL)
    {
#ifdef _DEBUG
        printf("Socket Interface: error gethostbyname\n");
#endif
        WSACleanup();
        return 0;
    }
    memset((char *)&client, 0, sizeof(client));
    client.sin_family = AF_INET;
    memcpy((char *)&client.sin_addr.s_addr,
           (char *)he->h_addr_list[0],
           he->h_length);
    client.sin_port = htons(serviceport);

    iresult = connect(sock, (SOCKADDR *)&client, sizeof(client));
    if (iresult == SOCKET_ERROR)
    {
        closesocket(sock);
        sock = INVALID_SOCKET;
    }
    if (sock == INVALID_SOCKET)
    {
#ifdef _DEBUG
        printf("Socket Interface: error connect\n");
#endif
        WSACleanup();
        return 0;
    }
    return 1;
}

int sock_send(SOCKET sock, const char *data, int count)
{
    int iresult = send(sock, data, count, 0);
    if (iresult == SOCKET_ERROR)
    {
#ifdef _DEBUG
        printf("Socket Interface: error send\n");
#endif
        closesocket(sock);
        WSACleanup();
        return 0;
    }
    return 1;
}

int sock_shutdown_send(SOCKET sock)
{
    int iresult = shutdown(sock, SD_SEND);
    if (iresult == SOCKET_ERROR)
    {
#ifdef _DEBUG
        printf("Socket Interface: error shutdown send\n");
#endif
        closesocket(sock);
        WSACleanup();
        return 0;
    }
    return 1;
}

int sock_recv(SOCKET sock, char *buf, int len)
{
    return recv(sock, buf, len, 0);
}

void sock_close(SOCKET sock)
{
    closesocket(sock);
    WSACleanup();
}

#endif // _SOMESOCK_H_
