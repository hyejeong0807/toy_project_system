#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/signal.h>
#include <sys/stat.h>

#define HEADER_FMT "HTTP/1.1 %d %s\nContent-Length: %ld\nContent-Type: %s\n\n"

#define NOT_FOUND_CONTENT    "<h1> 404 Not Found </h1>\n"
#define SERVER_ERROR_CONTENT "<h1> 500 Internal Server Error </h1>\n"

#define PORT 9090
#define BUF_SIZE 1024

/*
    주어진 매개 변수를 기준으로 HTTP 헤더 형식 지정
*/
void fill_header(char *header, int status, long len, char *type)
{
    char status_text[40];
    switch (status)
    {
    case 200:
        strcpy(status_text, "OK");
        break;
    case 404:
        strcpy(status_text, "Not Found");
        break;
    case 500:
    default:
        strcpy(status_text, "Internal Server Error");
    }

    sprintf(header, HEADER_FMT, status, status_text, len, type);
}

/*
    url로 부터 content type을 찾기
*/
void find_mime(char *ct_type, char *url)
{
    char *ext = strrchr(url, '.');
    if (!strcmp(ext, ".html"))
        strcpy(ct_type, "text/html");
    else if (!strcmp(ext, ".jpg") || !strcmp(ext, ".jpeg"))
        strcpy(ct_type, "image/jpeg");
    else if (!strcmp(ext, ".png"))
        strcpy(ct_type, "image/png");
    else if (!strcmp(ext, ".css"))
        strcpy(ct_type, "text/css");
    else if (!strcmp(ext, ".js"))
        strcpy(ct_type, "text/javascript");
    else strcpy(ct_type, "text/plain");
}

/*
    Handler for not found (404)
*/
void handle_404(int sock) {
    char header[BUF_SIZE];
    fill_header(header, 404, sizeof(NOT_FOUND_CONTENT), "text/html");
    write(sock, header, strlen(header));
    write(sock, NOT_FOUND_CONTENT, sizeof(NOT_FOUND_CONTENT));
}

/*
    Handler for internal server error (500)
*/
void handle_500(int sock) {
    char header[BUF_SIZE];
    fill_header(header, 500, sizeof(SERVER_ERROR_CONTENT), "text/html");
    write(sock, header, strlen(header));
    write(sock, SERVER_ERROR_CONTENT, sizeof(SERVER_ERROR_CONTENT));
}

/*
    main http handler
    요청된 리소스를 열고 전송
    failure에 대한 에러 호출
*/
void http_handler(int sock)
{
    char header[BUF_SIZE];
    char buf[BUF_SIZE];
    char safe_uri[BUF_SIZE];
    char *local_uri;
    struct stat st;

    if (read(sock, buf, BUF_SIZE) < 0) {
        perror("[ERR] failed to read request.\n");
        handle_500(sock);
        return ;
    }

    printf("%s", buf);    // 버퍼에 읽어들인 내용 모두 출력

    char *method = strtok(buf, " ");
    char *uri = strtok(NULL, " ");

    strcpy(safe_uri, uri);
    if (!strcmp(safe_uri, "/"))
        strcpy(safe_uri, "/index.html");    // '/'라면 자동으로 index.html을 match

    local_uri = safe_uri + 1;
    if (stat(local_uri, &st) < 0) {
        handle_404(sock);
        return ;
    }

    int fd = open(local_uri, O_RDONLY);
    if (fd < 0) {
        handle_500(sock);
        return ;
    }

    int ct_len = st.st_size;
    char ct_type[40];
    find_mime(ct_type, local_uri);
    fill_header(header, 200, ct_len, ct_type);
    write(sock, header, strlen(header));

    int cnt;
    while ((cnt = read(fd, buf, BUF_SIZE)) > 0)
        write(sock, buf, cnt);

}

int bind_socket(int sock)
{
    struct sockaddr_in sin;

    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    sin.sin_port = htons(PORT);

    return bind(sock, (struct sockaddr *)&sin, sizeof(sin));
}

int main(int argc, char *argv[])
{
    int port, pid;
    int lsock, asock;

    struct sockaddr_in sin;
    socklen_t sin_len;

    lsock = socket(AF_INET, SOCK_STREAM, 0); // Create Socket
    if (lsock < 0) {
        perror("[ERR] failed to create lsock\n");
        exit(1);
    }

    if (bind_socket(lsock)) {
        perror("[ERR] failed to bind socket.\n");
        exit(1);
    }

    if (listen(lsock, 10) < 0) {
        perror("[ERR] failed to listen sock\n");
        exit(1);
    }

    printf("socket() success\n");

    signal(SIGCHLD,SIG_IGN);   // 시그널을 무시한다

    while (1) {
        asock = accept(lsock, (struct sockaddr *)&sin, &sin_len);
        if (asock < 0) {
            perror("[ERR] failed to accept\n");
            continue;
        }

        switch (pid = fork()) { // 멀티 프로세스 생성
            case -1:
                printf("fork failed\n");
            case 0:
                close(lsock);
                http_handler(asock);
                close(asock);
                exit(0);
                break;
            default:
                close(asock);
        }   
    }
}