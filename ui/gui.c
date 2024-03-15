#include <stdio.h>

#include <system_server.h>
#include <gui.h>
#include <input.h>
#include <web_server.h>

int create_gui()
{
    pid_t systemPid;

    printf("여기서 GUI 프로세스를 생성합니다.\n");

    sleep(3);

    switch(systemPid = fork()) {
    case -1:
        printf("Error\n");
        break;
    case 0:
        if (execl("/usr/bin/google-chrome-stable", "google-chrome-stable", "http://localhost:8001", NULL)) {
            printf("Error execl\n");
        }
        break;
    default:
        break;
    }
    
    return 0;
}