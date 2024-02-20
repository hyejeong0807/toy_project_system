#include <stdio.h>
#include <sys/wait.h>

#include <system_server.h>
#include <gui.h>
#include <input.h>
#include <web_server.h>

int main()
{
    pid_t spid, gpid, ipid, wpid;
    int status, savedErrno;

    printf("메인 함수입니다.\n");
    printf("시스템 서버를 생성합니다.\n");
    spid = create_system_server();
    printf("웹 서버를 생성합니다.\n");
    wpid = create_web_server();
    printf("입력 프로세스를 생성합니다.\n");
    ipid = create_input();
    printf("GUI를 생성합니다.\n");
    gpid = create_gui();

    waitpid(spid, &status, 0);
    waitpid(wpid, &status, 0);
    waitpid(ipid, &statue, 0);
    waitpid(gpid, &status, 0);

    return 0;
}
