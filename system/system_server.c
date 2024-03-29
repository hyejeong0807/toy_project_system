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
#include <toy_message.h>
#include <sensor_info.h>

#define BUFSIZE 1024

pthread_mutex_t system_loop_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t system_loop_cond = PTHREAD_COND_INITIALIZER;
bool system_loop_exit = false;    // true if main loop should exit

static int toy_timer = 0;
static shm_sensor_t *bmp_shm_msg;

mqd_t watchdog_queue;
mqd_t monitor_queue;
mqd_t disk_queue;
mqd_t camera_queue;

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
    ssize_t numRead;
    toy_msg_t msg;
    unsigned int prio;
    
    printf("%s", s);

    while (1) {
        numRead = mq_receive(watchdog_queue, (char *)&msg, sizeof(msg), &prio);
        assert(numRead > 0);
        printf("watchdog_thread: 메시지 수신 받음\n");
        printf("Read %ld bytes: priority = %u\n", (long)numRead, prio);
        printf("msg.type: %d\n", msg.msg_type);
        printf("msg.param1: %d\n", msg.param1);
        printf("msg.param2: %d\n", msg.param2);
    }

    return 0;
}

/*
    수신받은 센서 데이터를 보여주는 모니터링 쓰레드
    shared memory를 사용해서 
*/

#define SENSOR_DATA 1

void *monitor_thread(void *arg)
{
    char *s = arg;
    ssize_t numRead;
    mqd_t mqd;
    toy_msg_t msg;
    shm_sensor_t shmsg;
    unsigned int prio;
    int shmid; // 공유 메모리 식별자

    printf("%s", s);
    while (1) {
        numRead = mq_receive(monitor_queue, (char *)&msg, sizeof(msg), &prio);
        assert(numRead > 0);
        printf("monitor_thread: 메시지 수신 받음\n");
        printf("Read %ld bytes: prioity = %u\n", (long)numRead, prio);
        printf("msg.type: %d\n", msg.msg_type);
        printf("msg.param1: %d\n", msg.param1);
        printf("msg.param2: %d\n", msg.param2);
        if (msg.msg_type == SENSOR_DATA) {
            shmid = msg.param1;
            if (bmp_shm_msg == NULL) 
                bmp_shm_msg = (shm_sensor_t *)shmat(shmid, NULL, 0); // 이미 존재하는 메모리를 참조하기 때문에 0
            if (bmp_shm_msg == (void *)-1) {
                perror("shmat");
            }
            
            shmsg = *bmp_shm_msg;
            
            printf("sensor temp: %d\n", bmp_shm_msg->temp);
            printf("sensor press: %d\n", bmp_shm_msg->press);
            printf("sensor humidity: %d\n", bmp_shm_msg->humidity);
        }
    }

    return 0;
}

void *disk_thread(void *arg)
{
    char *s = arg;
    ssize_t numRead;
    unsigned int prio;
    toy_msg_t msg;
    printf("%s", s);

    FILE *fd;
    char buf[BUFSIZE];

    while (1) {
        numRead = mq_receive(disk_queue, (char *)&msg, sizeof(toy_msg_t), &prio);
        assert(numRead > 0);
        printf("disk_thread: 메시지 수신 받음\n");
        printf("Read %ld bytes: prioity = %u\n", (long)numRead, prio);
        printf("msg.type: %d\n", msg.msg_type);
        printf("msg.param1: %d\n", msg.param1);
        printf("msg.param2: %d\n", msg.param2);
        /*
            popen 사용하여 10초마다 disk 잔여량 출력
            popen으로 받은 fd를 읽어서 출력
        */
        if ((fd = popen("df -h ./", "r")) == NULL) {
            perror("faild to open disk free");
            exit(-1);
        }

        while (fgets(buf, BUFSIZE, fd)) {
            printf("%s", buf);
        }

        posix_sleep_ms(10000);
    }
}


#define CAMERA_TAKE_PICTURE 1

// Camera HAL API를 호출하는 쓰레드
void *camera_thread(void *arg)
{
    char *s = arg;
    toy_msg_t msg;
    ssize_t numRead;
    unsigned int prio;

    printf("%s", s);

    toy_camera_open();

    while (1) {
        numRead = mq_receive(camera_queue, (char *)&msg, sizeof(toy_msg_t), &prio);
        assert(numRead > 0);
        printf("camera_thread: 메시지 수신 받음\n");
        printf("Read %ld bytes: prioity = %u\n", (long)numRead, prio);
        printf("msg.type: %d\n", msg.msg_type);
        printf("msg.param1: %d\n", msg.param1);
        printf("msg.param2: %d\n", msg.param2);

        if (msg.msg_type == CAMERA_TAKE_PICTURE) {
            toy_camera_take_picture();
        }
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

    // 주기적으로 system_loop_exit 값을 검사하는 것이 아닌 cond wait으로 대기한다.
    // 그리고 signal_exit에서 signal을 보내면 즉, 5초 후에 알림이 울리면 "<=== system"을 출력한다.
    pthread_mutex_lock(&system_loop_mutex);
    while (system_loop_exit == false ){
        pthread_cond_wait(&system_loop_cond, &system_loop_mutex);
    }
    pthread_mutex_unlock(&system_loop_mutex);
    printf("<=== system\n");

    // 메시지 큐 open
    watchdog_queue = mq_open("/watchdog_queue", O_RDWR);
    if (watchdog_queue == (mqd_t)-1){
        perror("watchdog_queue open failure");
        exit(-1);
    }

    monitor_queue = mq_open("/monitor_queue", O_RDWR);
    if (monitor_queue == (mqd_t)-1){
        perror("monitor_queue open failure");
        exit(-1);
    }

    disk_queue = mq_open("/disk_queue", O_RDWR);
    if (mqd == (mqd_t)-1){
        perror("disk_queue open failure");
        exit(-1);
    }

    camera_queue = mq_open("/camera_queue", O_RDWR);
    if (camera_queue == (mqd_t)-1){
        perror("camera_queue open failure");
        exit(-1);
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