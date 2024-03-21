#include <stdio.h>
#include <stdlib.h>
#include <mqueue.h>    // 메시지 큐
#include <sys/wait.h>
#include <sys/types.h>  
#include <fcntl.h>     // flags 정의
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <assert.h>

#include <system_server.h>
#include <gui.h>
#include <input.h>
#include <web_server.h>
#include <sensor_info.h>
#include <toy_message.h>

#define NUM_MESSAGES 10

static mqd_t watchdog_queue;
static mqd_t monitor_queue;
static mqd_t disk_queue;
static mqd_t camera_queue;

void sigchld_handler(int sig)
{
    int status, saved_errno;
    pid_t child_pid;

    saved_errno = errno;

    while ((child_pid = waitpid(-1, &status, WNOHANG)) > 0) {
        printf("SIGCHLD(%d) caught\n", child_pid);
    }

    if (child_pid < 0 && errno != ECHILD) {
        perror("fail to wait child process");
        exit(1);
    }

    errno = saved_errno;

}

int create_message_queue(mqd_t *msgq_ptr, const char *mq_name, int num_msg, int msg_size)
{
    struct mq_attr mq_attrib;
    int mq_errno;
    mqd_t msgq;

    printf("%s name=%s nummsgs=%d\n", __func__, mq_name, num_msg);

    memset(&mq_attrib, 0, sizeof(mq_attrib));
    mq_attrib.mq_msgsize = msg_size;
    mq_attrib.mq_maxmsg = num_msg;

    mq_unlink(mq_name);
    msgq = mq_open(mq_name, O_RDWR | O_CREAT | O_CLOEXEC, 0777, &mq_attrib);
    if (msgq == -1) {
        printf("%s queue=%s already exists so try to open\n",
                            __func__, mq_name);
        msgq = mq_open(mq_name, O_RDWR);
        assert(msgq != (mqd_t) -1);
        printf("%s queue=%s opened successfully\n",
                            __func__, mq_name);
        return -1;
    }

    *msgq_ptr = msgq;
    return 0;
}

int main()
{
    pid_t spid, gpid, ipid, wpid;
    int status, savedErrno;

    // Message queue 추가
    char *watchdog_name = "/watchdog_queue";
    char *monitor_name = "/monitor_queue";
    char *disk_name = "/disk_queue";
    char *camera_name = "/camera_queue";

    signal(SIGCHLD, sigchld_handler);

    // message queue 생성
    watchdog_queue = create_message_queue(&watchdog_queue, watchdog_name, NUM_MESSAGES, sizeof(toy_msg_t));
    monitor_queue = create_message_queue(&monitor_queue, monitor_name, NUM_MESSAGES, sizeof(toy_msg_t));
    disk_queue = create_message_queue(&disk_queue, disk_name, NUM_MESSAGES, sizeof(toy_msg_t));
    camera_queue = create_message_queue(&camera_queue, camera_name, NUM_MESSAGES, sizeof(toy_msg_t));

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
    waitpid(ipid, &status, 0);
    waitpid(gpid, &status, 0);

    return 0;
}
