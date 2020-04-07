#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <winsock2.h>
#include "base64.h"

#define FFHTTPD_SERVER_PORT     554
#define FFHTTPD_MAX_CONNECTION  10

static char* parse_params(const char *str, const char *key, char *val, int len)
{
    char *p = (char*)strstr(str, key);
    int   i;

    *val = '\0';
    if (!p) return NULL;
    p += strlen(key);
    if (*p == '\0') return NULL;

    while (*p) {
        if (*p != ':' && *p != ' ') break;
        else p++;
    }

    for (i=0; i<len; i++) {
        if (*p == '\\') {
            p++;
        } else if (*p == '\r' || *p == '\n') {
            break;
        }
        val[i] = *p++;
    }
    val[i] = val[len-1] = '\0';
    return val;
}

static int gen_sdp_str(char *buf, int size, char *username, int sessionid, char *ipaddr,
                int channels, int samprate, uint8_t aaccfg[2],
                int frate, uint8_t *spsdata, int spslen, uint8_t *ppsdata, int ppslen)
{
    char spsstr[256], ppsstr[256];
    int  ret;

    ret = base64_encode(spsdata, spslen, spsstr, sizeof(spsstr)-1);
    spsstr[ret > 0 ? ret : 0] = '\0';
    ret = base64_encode(ppsdata, ppslen, ppsstr, sizeof(ppsstr)-1);
    ppsstr[ret > 0 ? ret : 0] = '\0';

    return snprintf(buf, size,
        "v=0\r\n"
        "o=%s %d 1 in IP4 %s\r\n"
        "s=Session streamed by \"lxqrtsp\""
        "t=0 0\r\n"
        "m=audio 0 RTP/AVP/TCP 104\r\n"
        "c=IN IP4 0.0.0.0\r\n"
        "a=rtpmap:104 MPEG4-GENERIC/%d/%d\r\n"
        "a=fmtp:104 streamtype=5;profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=%02X%02X\r\n"
        "a=control:track0\r\n"
        "m=video 0 RTP/AVP/TCP 96\r\n"
        "c=IN IP4 0.0.0.0\r\n"
        "a=rtpmap:96 H264/90000\r\n"
        "a=fmtp:96 packetization-mode=1;profile-level-id=%02X%02X%02X;sprop-parameter-sets=%s,%s\r\n"
        "a=framerate:%d\r\n"
        "a=control:track1\r\n",
        username, sessionid, ipaddr, samprate, channels, aaccfg[0], aaccfg[1],
        spsdata[1], spsdata[2], spsdata[3], spsstr, ppsstr, frate);
}

int main(void)
{
    struct  sockaddr_in server_addr, client_addr;
    SOCKET  server_fd, conn_fd;
    int     addrlen, length, datalen, cseq;
    int     sessionid = 1;
    int     samprate  = 48000;
    int     channels  = 1;
    int     framerate = 16;
    uint8_t aaccfg[2] = { 0x11, 0x90 };
    uint8_t spsdata[] = { 103,66,192,62,218,1,104,28,254,120,64,0,0,3,0,64,0,0,7,163,194,1,10,128 };
    uint8_t ppsdata[] = { 104,206,60,128 };
    char    localip[] = "192.168.0.111";
    char    recvbuf[1024], sendbuf[1024], temp[256], *reqeust_end, *request_type, *request_url, *request_ver, *request_body;

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return -1;
        exit(1);
    }

    server_addr.sin_family      = AF_INET;
    server_addr.sin_port        = htons(FFHTTPD_SERVER_PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        printf("failed to open socket !\n");
        exit(1);
    }

    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        printf("failed to bind !\n");
        exit(1);
    }

    if (listen(server_fd, FFHTTPD_MAX_CONNECTION) == -1) {
        printf("failed to listen !\n");
        exit(1);
    }

    while (1) {
        addrlen = sizeof(client_addr);
        conn_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addrlen);
        if (conn_fd == -1) {
            printf("failed to accept !\n");
            exit(1);
        }

        while (1) {
            length = recv(conn_fd, recvbuf, 1023, 0);
            if (length <= 0) goto _next;
            recvbuf[length] = '\0';
            reqeust_end = recvbuf + length;
            printf("%s\n", recvbuf); fflush(stdout);

            request_type = recvbuf;
            request_url  = strstr(recvbuf, " ");
            if (request_url) {
               *request_url++ = '\0';
                if (request_url < reqeust_end) {
                    request_ver = strstr(request_url, " ");
                    if (request_ver) {
                       *request_ver++ = '\0';
                        if (request_ver < reqeust_end) {
                            request_body = strstr(request_ver, "\r\n");
                            if (request_body) {
                               *request_body = '\0';
                                request_body += 2;
                            }
                        }
                    }
                }
            }
            if (request_body) {
                strlwr(request_body);
                parse_params(request_body, "cseq", temp, sizeof(temp));
                cseq = atoi(temp);
            }
            printf("type: %s, url: %s, version: %s, cseq: %d\n", request_type, request_url, request_ver, cseq); fflush(stdout);
            if (strcmp(request_type, "OPTIONS") == 0) {
                length = snprintf(sendbuf, sizeof(sendbuf), "RTSP/1.0 200 OK\r\nCSeq: %d\r\nPublic: OPTIONS, DESCRIBE, SETUP, TEARDOWN, PLAY\r\n\r\n", cseq);
                length = send(conn_fd, sendbuf, length, 0);
            } else if (strcmp(request_type, "DESCRIBE") == 0) {
                datalen = gen_sdp_str(sendbuf, sizeof(sendbuf), "-", sessionid, localip,
                            channels, samprate, aaccfg, framerate, spsdata, sizeof(spsdata), ppsdata, sizeof(ppsdata));
                printf("sdp:\n%s\n", sendbuf); fflush(stdout);
                length = snprintf(sendbuf + datalen, sizeof(sendbuf) - datalen,
                            "RTSP/1.0 200 OK\r\nCSeq: %d\r\nContent-length: %d\r\nContent-type: application/sdp\r\n\r\n", cseq, datalen);
                printf("response:\n%s\n", sendbuf + datalen); fflush(stdout);
                send(conn_fd, sendbuf + datalen, length, 0);
                send(conn_fd, sendbuf, datalen, 0);
            }
        }
_next:
        closesocket(conn_fd);
    }

    closesocket(server_fd);
    WSACleanup();
    return 0;
}
