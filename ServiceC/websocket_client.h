#ifndef _WEBSOCKET_CLIENT_H_
#define _WEBSOCKET_CLIENT_H_

// somewebsock.h
// the basic websocket procedures for the client side of a websocket connection
// see: RFC 6455
// part of Microservices demo: MSBasics

#include <cstdio>
#include <cstdint>
#include <cstring>
#include "sometools.h"
#include "somesock.h"
#include "sha1.h"

#define MAX_BUF_SIZE 512
#define MAX_KEY_SIZE 128

#define _LOBYTE(w) ((unsigned char) (((unsigned long long) (w)) & 0xff))
#define _LOWORD(l) ((unsigned short) (((unsigned long long) (l)) & 0xffff))



/*  Data Frame according RFC

    0                   1                   2                   3
      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
     +-+-+-+-+-------+-+-------------+-------------------------------+
     |F|R|R|R| opcode|M| Payload len |    Extended payload length    |
     |I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
     |N|V|V|V|       |S|             |   (if payload len==126/127)   |
     | |1|2|3|       |K|             |                               |
     +-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
     |     Extended payload length continued, if payload len == 127  |
     + - - - - - - - - - - - - - - - +-------------------------------+
     |                               |Masking-key, if MASK set to 1  |
     +-------------------------------+-------------------------------+
     | Masking-key (continued)       |          Payload Data         |
     +-------------------------------- - - - - - - - - - - - - - - - +
     :                     Payload Data continued ...                :
     + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
     |                     Payload Data continued ...                |
     +---------------------------------------------------------------+
*/

#pragma pack(1)
union WS_PAYLOAD
{
    uint16_t status_code;
    uint8_t data[127];
};

// This works only for MASKED SMALL (<126) payloads 
struct WS_FRAME_MASKED
{
    uint8_t b0;
    uint8_t b1;
    uint32_t mask;
    WS_PAYLOAD payload;
};

// This works only for UNMASKED SMALL (<126) payloads 
struct WS_FRAME_UNMASKED
{
    uint8_t b0;
    uint8_t b1;
    WS_PAYLOAD payload;
};

void ws_mask(const char *buf_in, char* buf_out, uint32_t mask)
{
    size_t i;
    size_t buflen = strlen(buf_in);
    unsigned char map[4];
    map[0] = _LOBYTE(_LOWORD(mask >> 24));
    map[1] = _LOBYTE(_LOWORD(mask >> 16));
    map[2] = _LOBYTE(_LOWORD(mask >> 8));
    map[3] = _LOBYTE(_LOWORD(mask));
    for (i = 0; i < buflen; i++)
        buf_out[i] = buf_in[i] ^ map[i % 4];
    buf_out[i] = 0;
}

const char *ws_open_handshake = "GET / HTTP/1.1\r\nHost: %s:%d\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: %s\r\nSec-WebSocket-Version: 13\r\n\r\n";
const char *ws_magic = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

int check_sha1(const char *sec_key, const char *sha)
{
    unsigned char hash[20];
    char tmp[128];
    strcpy(tmp, sec_key);
    strcat(tmp, ws_magic);
    sha1::calc(tmp, strlen(tmp), hash);
    char hashb64[20];
    b64_encode(hash, 20, hashb64);
    if (strcmp(sha, hashb64) != 0)
    {
        return 0;
    }
    return 1;
}


int ws_check_response(const char *response, const char *sec_key)
{
    // required header fields acc. RFC
    // (1): HTTP status code 101
    // (2): Field "Upgrade: websocket"
    // (3): Field "Connection: Upgrade"
    // (4): Field "Sec-WebSocket-Accept: <?>"
    // (5): Field "Sec-WebSocket-Extensions: <?>"
    char result[50];
    int errcode = 0;
    if (((http_header_value(response, "HTTP/1.1", 1, (char *)&result) == NULL) && (errcode = 1)) ||
        ((strcmp(result, "101") != 0) && (errcode = 2)) ||
        ((http_header_value(response, "Upgrade", 1, (char *)&result) == NULL) && (errcode = 3)) ||
        ((strcmp(result, "websocket") != 0) && (errcode = 4)) ||
        ((http_header_value(response, "Connection", 1, (char *)&result) == NULL) && (errcode = 5)) || 
        ((strcmp(result, "Upgrade") != 0) && (errcode = 6)) ||
        ((http_header_value(response, "Sec-WebSocket-Accept", 1, (char *)&result) == NULL)&& (errcode = 7)) ||
        ((!check_sha1(sec_key, result)) && (errcode = 8)) ||
        ((http_header_value(response, "Sec-WebSocket-Extensions", 1, (char *)&result) != NULL) && (errcode = 9))) {
#ifdef _DEBUG
        printf("(MSBasics-Websocket) Error #%d\n", errcode);
#endif
        return 0;
    }
    return 1;
}


void ws_send_close(SOCKET sock, uint16_t code)
{
    int bufsize;
    WS_FRAME_UNMASKED ws_close;
    ws_close.b0 = 0x88;
    ws_close.b1 = 0x02;
    ws_close.payload.status_code = htons(code);
    bufsize = sizeof(ws_close.b0) + sizeof(ws_close.b1) + sizeof(ws_close.payload.status_code);
    if (!sock_send(sock, (char*)&ws_close, bufsize))
    {
#ifdef _DEBUG
        printf("(MSBasics-Websocket) Error sending closing handshake\n");
#endif
        return;
    }
    sock_shutdown_send(sock);
    char* buf = (char*)malloc(MAX_BUF_SIZE);

    bufsize = sock_recv(sock, buf, MAX_BUF_SIZE);
    if (bufsize > MAX_BUF_SIZE)
    {
#ifdef _DEBUG
        printf("(MSBasics-Websocket) Error buffer too small\n");
#endif
    }
    free(buf);
}


int ws_handshake(const char* servicename, int serviceport, SOCKET& sock) {
   if (!sock_connect(servicename, serviceport, sock))
        return 0;

    // WebSocket handshake client side
    char *buf = (char *)malloc(MAX_BUF_SIZE);
    unsigned char in[16];
    srand(time(NULL));
    for (int i = 0; i < 16; i++)
        in[i] = rand() % 256;

    char sec_key[20];
    b64_encode(in, 16, sec_key);
    sprintf(buf, ws_open_handshake, servicename, serviceport, sec_key);
    if (!sock_send(sock, buf, strlen(buf)))
    {
#ifdef _DEBUG
        printf("(MSBasics-Websocket) Error sending open handshake\n");
#endif
        free(buf);
        sock_close(sock);
        return 0;
    }

    int iresult = sock_recv(sock, buf, MAX_BUF_SIZE);
    if (iresult > MAX_BUF_SIZE)
    {
#ifdef _DEBUG
        printf("(MSBasics-Websocket) Error buffer too small\n");
#endif
        free(buf);
        ws_send_close(sock, 1008);
        sock_close(sock);
        return 0;
    }
    buf[iresult] = 0;
    if (!ws_check_response(buf, sec_key))
    {
#ifdef _DEBUG
        printf("(MSBasics-Websocket) Error invalid response from server\n");
#endif
        free(buf);
        ws_send_close(sock, 1008);
        sock_close(sock);
        return 0;
    }
    free(buf);
    return 1;
}

int ws_sendpayload(SOCKET sock, const char* data_in) {
    char *buf = (char *)malloc(MAX_BUF_SIZE);
    WS_FRAME_MASKED ws_send;

    memset((void *)&ws_send, 0, sizeof(ws_send));
    ws_send.b0 = 0x81; // set FIN bit, opcode TEXT
    size_t buflen = strlen(data_in);
    if (buflen > 125)
    {
#ifdef _DEBUG
        printf("(MSBasics-Websocket) Error: data buffer too small\n");
#endif
        free(buf);
        return 0;
    }

    ws_send.b1 = _LOBYTE(_LOWORD(buflen));
    ws_send.b1 = ws_send.b1 | 0x80; // set MASK bit
    ws_send.mask = gen_rnd32();

    ws_mask(data_in, buf, ws_send.mask);
    ws_send.mask = htonl(ws_send.mask);

    for (int i = 0; i < buflen; i++)
        ws_send.payload.data[i] = buf[i];
    int bytes = sizeof(ws_send.b0) + sizeof(ws_send.b1) + sizeof(ws_send.mask) + buflen;

    if (!sock_send(sock, (const char *)&ws_send, bytes))
    {
#ifdef _DEBUG
        printf("(MSBasics-Websocket) Error sending data\n");
#endif
        free(buf);
        return 0;
    }
    return 1;
}

int ws_recv_close(SOCKET sock) {
    char *buf = (char *)malloc(MAX_BUF_SIZE);
    int iresult = sock_recv(sock, buf, MAX_BUF_SIZE);
    if (iresult > MAX_BUF_SIZE)
    {
#ifdef _DEBUG
        printf("(MSBasics-Websocket) Error buffer too small\n");
#endif
        free(buf);
        return 0;
    }
    
    WS_FRAME_UNMASKED *ws_recv = (WS_FRAME_UNMASKED *)buf;

    if (((ws_recv->b0 & 0x0F) != 0x08) || (ntohs(ws_recv->payload.status_code) != 1000))
    {
#ifdef _DEBUG
        printf("(MSBasics-Websocket) protocol error\n");
#endif
        free(buf);
        return 0;
    }

    sock_send(sock, (const char *)ws_recv, iresult);
    free(buf);
    return 1;
}



#endif // _WEBSOCKET_CLIENT_H_