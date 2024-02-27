#include <stdio.h>
#include <sys/prctl.h>
#include <signal.h>
#include <sys/time.h>
#include <time.h>

#include <system_server.h>
#include <gui.h>
#include <input.h>
#include <web_server.h>

static int toy_timer = 0;

static void sigalrm_handler(int sig, siginfo_t *si, void *uc)
{
    printf("toy timer: %d\n", toy_timer);
    toy_timer++;
}

int posix_sleep_ms(unsigned int timeout_ms)
{
    struct timespec sleep_time;
    sleep_time.tv_sec = timeout_ms / MILLISEC_PER_SECOND;
    sleep_time.tv_nsec = (timeout_ms % MILLISEC_PER_SECOND) * (NANOSEC_PER_USEC * USEC_PER_MILLISEC);

    return nanosleep(&sleep_time, NULL);
}

int system_server()
{
    struct itimerval ts;    // 자기가 설정한 시간마다 event가 발생하도록 설정
    struct sigaction sa;
    struct sigevent sev;
    timer_t *tidlist;

    printf("나 system_server 프로세스!\n");

    /* 5초 타이머 만들기 */
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = sigalrm_handler;
    if (sigaction(SIGALRM, &sa, NULL) == -1){
        perror("SIGALRM, error");
        exit(1);
    }

    ts.it_value.tv_sec = 1;
    ts.it_value.tv_usec = 0;
    ts.it_interval.tv_sec = 5;
    ts.it_interval.tv_usec = 0;
    if (setitimer(ITIMER_REAL, &ts, NULL) == -1) {
        perror("SIGALRM, setitimer");
        exit(1);
    }

    while (1) {
        posix_sleep_ms(5000);
    }

    return 0;
}

int create_system_server()
{
    pid_t systemPid;
    const char *name = "system_server";

    printf("여기서 시스템 프로세스를 생성합니다.\n");

    /* fork 를 이용하세요 */
    switch (systemPid = fork()) {
    case -1:
        printf("Error\n");
        break;
    case 0:
        printf("Parent pid = %d | Child pid = %d\n", systemPid, getppid());
        system_server();
        break;
    default:
        printf("Parent Pid = %d\n", systemPid);
        break;
    }

    return 0;
}