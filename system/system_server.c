#include <stdio.h>
#include <sys/prctl.h>
#include <signal.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <assert.h>
#include <pthread.h>
#include <mqueue.h>
#include <assert.h>
#include <semaphore.h>
#include <error.h>
#include <sys/inotify.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>

#include <system_server.h>
#include <gui.h>
#include <input.h>
#include <web_server.h>
#include <camera_HAL.h>
#include <toy_message.h>
#include <sensor_info.h>
#include <shared_memory.h>

#define BUFSIZE 1024

pthread_mutex_t system_loop_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t system_loop_cond = PTHREAD_COND_INITIALIZER;
bool system_loop_exit = false;    // true if main loop should exit

static int toy_timer = 0;
pthread_mutex_t toy_timer_mutex = PTHREAD_MUTEX_INITIALIZER;
static sem_t global_timer_sem;
static bool global_timer_stopped;

static mqd_t watchdog_queue;
static mqd_t monitor_queue;
static mqd_t disk_queue;
static mqd_t camera_queue;

static shm_sensor_t *bmp_shm_msg = NULL;

static void timer_expire_signal_hander()
{
    sem_post(&global_timer_sem);
}

static void system_timeout_handler() 
{
    // signal handler가 아니기 때문에 뮤텍스 사용 가능
    pthread_mutex_lock(&toy_timer_mutex);
    toy_timer++;
    pthread_mutex_unlock(&toy_timer_mutex);
}

int posix_sleep_ms(unsigned int timeout_ms)
{
    struct timespec sleep_time;
    sleep_time.tv_sec = timeout_ms / MILLISEC_PER_SECOND;
    sleep_time.tv_nsec = (timeout_ms % MILLISEC_PER_SECOND) * (NANOSEC_PER_USEC * USEC_PER_MILLISEC);

    return nanosleep(&sleep_time, NULL);
}

void set_periodic_timer(long sec_delay, long usec_delay)
{
    struct itimerval itimer_val = {
        .it_interval = { .tv_sec = sec_delay, .tv_usec = usec_delay },
        .it_value = { .tv_sec = sec_delay, .tv_usec = usec_delay }
    };

    setitimer(ITIMER_REAL, &itimer_val, (struct itimerval *)0);
}

static void *timer_thread(void *not_used) 
{
    // alarm clock 시그널이 울리면 해당 핸들러 함수 실행
    // sem_post는 semapore 값을 +1
    signal(SIGALRM, timer_expire_signal_hander);
    set_periodic_timer(1, 1);

    while (!global_timer_stopped) {
        int rc = sem_wait(&global_timer_sem);
        if (rc == -1 && errno == EINTR) {
            continue;
        }

        if (rc == -1) {
            perror("sem_wait");
            exit(-1);
        }

        system_timeout_handler();
    }
    return 0;
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
    int mqretcode;
    toy_msg_t msg;
    int shmid;

    printf("%s", s);

    while (1) {
        mqretcode = (int)mq_receive(monitor_queue, (void *)&msg, sizeof(toy_msg_t), 0);
        assert(mqretcode >= 0);
        printf("monitor_thread: 메시지가 도착했습니다.\n");
        printf("msg.type: %d\n", msg.msg_type);
        printf("msg.param1: %d\n", msg.param1);
        printf("msg.param2: %d\n", msg.param2);
        if (msg.msg_type == SENSOR_DATA) {
            printf("Read Sensor data\n");
            shmid = msg.param1;
            bmp_shm_msg = toy_shm_attach(shmid);   // 생성된 공유 메모리 연결
            printf("sensor temp: %d\n", bmp_shm_msg->temp);
            printf("sensor info: %d\n", bmp_shm_msg->press);
            printf("sensor humidity: %d\n", bmp_shm_msg->humidity);
            toy_shm_detach(bmp_shm_msg);           // 연결된 메모리 해제
        }
    }

    return 0;
}

static long get_directory_size(char *dirname) 
{
    DIR *dir = opendir(dirname);
    if (dir == 0)
        return 0;

    struct dirent *dit;
    struct stat st;
    long size = 0;
    long total_size = 0;
    char filePath[NAME_MAX];

    while ((dit = readdir(dir)) != NULL)
    {
        if ( (strcmp(dit->d_name, ".") == 0) || (strcmp(dit->d_name, "..") == 0) )
            continue;

        sprintf(filePath, "%s/%s", dirname, dit->d_name);
        if (lstat(filePath, &st) != 0)
            continue;
        size = st.st_size;

        if (S_ISDIR(st.st_mode))
        {
            long dir_size = get_directory_size(filePath) + size;
            printf("DIR\t");
            printf("MODE: %lo\t", (unsigned long) st.st_mode);
            printf("SIZE: %ld\t", dir_size);
            printf("%s\n", filePath);
            total_size += dir_size;
        }
        else
        {
            total_size += size;
            printf("FILES\t");
            printf("MODE: %lo\t", (unsigned long) st.st_mode);
            printf("SIZE: %ld\t", size);
            printf("%s\n", filePath);
        }
    }
    return total_size;
}

#define WATCH_DIR "./fs"

void *disk_thread(void *arg)
{
    char *s = arg;
    ssize_t numRead;
    unsigned int prio;
    toy_msg_t msg;
    
    // 파일 변경 감지
    int inotifyFd;
    struct inotify_event *event;
    int inoretcode;
    int total = 0;
    char *p;

    printf("%s", s);

    FILE *fd;
    char buf[BUFSIZE];

    inotifyFd = inotify_init();        // Create inotify instance
    if (inotifyFd == -1) {
        perror("inotify_init error");
        exit(-1);
    }

    inoretcode = inotify_add_watch(inotifyFd, WATCH_DIR, IN_CREATE);
    if (inoretcode == -1) {
        perror("inotify_add_watch error");
        exit(-1);
    }

    while (1) {
        numRead = read(inotifyFd, buf, BUFSIZE);
        printf("num_read: %ld bytes\n", numRead);

        if (numRead == 0) {
            printf("read() from inotify fd returned 0!\n");
            return 0;
        }

        if (numRead == -1) {
            perror("read error");
            exit(-1);
        }

        for (p = buf; p < buf + numRead; ) {
            event = (struct inotify_event *) p;
            if (event->mask & IN_CREATE) {
                printf("The file %s was created.\n", event->name);
                break;
            }
            p += sizeof(struct inotify_event) + event->len;
        }
        total = get_directory_size(WATCH_DIR);
        printf("directory size: %d\n", total);
    }
    exit(EXIT_SUCCESS);
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
    pthread_t timer_thread_tid;

    printf("나 system_server 프로세스!\n");

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
    if (disk_queue == (mqd_t)-1){
        perror("disk_queue open failure");
        exit(-1);
    }

    camera_queue = mq_open("/camera_queue", O_RDWR);
    if (camera_queue == (mqd_t)-1){
        perror("camera_queue open failure");
        exit(-1);
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
    retcode = pthread_create(&timer_thread_tid, NULL, timer_thread, "timer thread\n");
    assert(retcode == 0);

    // 주기적으로 system_loop_exit 값을 검사하는 것이 아닌 cond wait으로 대기한다.
    // 그리고 signal_exit에서 signal을 보내면 즉, 5초 후에 알림이 울리면 "<=== system"을 출력한다.
    pthread_mutex_lock(&system_loop_mutex);
    while (system_loop_exit == false ){
        pthread_cond_wait(&system_loop_cond, &system_loop_mutex);
    }
    pthread_mutex_unlock(&system_loop_mutex);
    printf("<=== system\n");

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