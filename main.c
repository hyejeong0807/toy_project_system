#include <stdio.h>
#include <mqueue.h>
#include <sys/wait.h>

#include <system_server.h>
#include <gui.h>
#include <input.h>
#include <web_server.h>
#include <common_shm.h>
#include <sensor_info.h>
#include <toy_message.h>

void sigchld_handler(int sig)
{
    int status, saved_errno;
    pid_t child_pid;

    saved_errno = errno;

    while ((child_pid = waitpid(-1, &status, WNOHANG)) > 0) {
        printf("SIGCHLD(%d) caught\n", child_pid);
    }

}

int create_message_queue(const char *mq_name, int num_msg, int msg_size)
{
    mqd_t mq;
    struct mq_attr attr;
    int flags = O_RDWR | O_CREAT | O_CLOEXEC;

    printf("%s name=%s nummsgs=%d\n", __func__, mq_name, num_msg);

    attr.mq_maxmsg = msg_size;
    attr.mq_msgsize = num_msg;

    mq_unlink(mq_name); // 만약 만들어졌으면 해제
    mq = mq_open(mq_name, flags, 0666, &attr);

    if (mq == (mqt_t) -1) {
        printf("%s mq=%s already exists\n", __func__, mq_name);
        mq = mq_open(mq_name, O_RDWR);
        assert(mq != (mqt_t) -1);
        printf("mq=%s opened\n", mq_name);
        return -1;
    }

    return 0;
}

int main()
{
    pid_t spid, gpid, ipid, wpid;
    int status, savedErrno;

    // Message queue 추가
    char *watchdog_queue = "/watchdog_queue";
    char *monitor_queue = "/monitor_queue";
    char *disk_queue = "/disk_queue";
    char *camera_queue = "/camera_queue";

    signal(SIGCHLD, sigchld_handler);

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

    // message queue 생성
    savedErrno = create_message_queue(watchdog_queue, 10, sizeof(toy_msg_t));
    assert(savedErrno >= 0);
    savedErrno = create_message_queue(monitor_queue, 10, sizeof(toy_msg_t));
    assert(savedErrno >= 0);
    savedErrno = create_message_queue(disk_queue, 10, sizeof(toy_msg_t));
    assert(savedErrno >= 0);
    savedErrno = create_message_queue(camera_queue, 10, sizeof(toy_msg_t));
    assert(savedErrno >= 0);

    return 0;
}
