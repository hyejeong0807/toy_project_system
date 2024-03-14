#include <stdio.h>
#include <sys/prctl.h>
#include <signal.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <assert.h>

#include <system_server.h>
#include <gui.h>
#include <input.h>
#include <web_server.h>
#include <camera_HAL.h>

pthread_mutex_t system_loop_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t system_loop_cond = PTHREAD_COND_INITIALIZER;
bool system_loop_exit = false;    // true if main loop should exit

static int toy_timer = 0;

static void sigalrm_handler(int sig, siginfo_t *si, void *uc)
{
    printf("toy timer: %d\n", toy_timer);
    toy_timer++;
    signal_exit();
}

int posix_sleep_ms(unsigned int timeout_ms)
{
    struct timespec sleep_time;
    sleep_time.tv_sec = timeout_ms / MILLISEC_PER_SECOND;
    sleep_time.tv_nsec = (timeout_ms % MILLISEC_PER_SECOND) * (NANOSEC_PER_USEC * USEC_PER_MILLISEC);

    return nanosleep(&sleep_time, NULL);
}

void *watchdog_thread(void *arg)
{
    char *s = arg;
    printf("%s", s);
    while (1) {
        posix_sleep_ms(5000);
    }
    return 0;
}

/*
    수신받은 센서 데이터를 보여주는 모니터링 쓰레드
    shared memory를 사용해서 
*/
void *monitor_thread(void *arg)
{
    char *s = arg;
    printf("%s", s);
    while (1) {
        posix_sleep_ms(5000);
    }
    return 0;
}

void *disk_thread(void *arg)
{
    char *s = arg;
    printf("%s", s);
    while (1) {
        posix_sleep_ms(5000);
    }
}

// Camera HAL API를 호출하는 쓰레드
void *camera_thread(void *arg)
{
    char *s = arg;
    printf("%s", s);

    toy_camera_open();
    toy_camera_take_picture();

    while (1) {
        posix_sleep_ms(5000);
    }
}

void signal_exit(void)
{
    pthread_mutex_lock(&system_loop_mutex);
    system_loop_exit = true;
    pthread_cond_signal(&system_loop_cond);
    pthread_mutex_unlock(&system_loop_mutex);
}

int system_server()
{
    struct itimerval ts;    // 자기가 설정한 시간마다 event가 발생하도록 설정
    struct sigaction sa;
    struct sigevent sev;
    timer_t *tidlist;

    int retcode, status;
    pthread_t watchdog_thread_tid, monitor_thread_tid, disk_service_thread_tid, camera_service_thread_tid;

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

    /* Watchdog, Monitor, Disk_service, Camera_service 스레드 생성 */
    retcode = pthread_create(&watchdog_thread_tid, NULL, watchdog_thread, "watchdog thread\n");
    assert(retcode == 0);
    retcode = pthread_create(&monitor_thread_tid, NULL, monitor_thread, "monitor thread\n");
    assert(retcode == 0);
    retcode = pthread_create(&disk_service_thread_tid, NULL, disk_thread, "disk thread\n");
    assert(retcode == 0);
    retcode = pthread_create(&camera_service_thread_tid, NULL, camera_thread, "camera thread\n");
    assert(retcode == 0);

    // cond wait으로 대기한다.
    // 5초 후에 알림이 울리면 <=== system 출력
    while (system_loop_exit == false ){
        pthread_mutex_lock(&system_loop_mutex);
        pthread_cond_wait(&system_loop_cond, &system_loop_mutex);
        printf("<=== system\n");
        pthread_mutex_unlock(&system_loop_mutex);
    }    

    while (1) {
        sleep(1);
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