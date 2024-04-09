#include <stdio.h>
#include <stdlib.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/shm.h>    // shmget
#include <execinfo.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <ucontext.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <assert.h>
#include <fcntl.h>
#include <mqueue.h>

#include <system_server.h>
#include <gui.h>
#include <input.h>
#include <web_server.h>
#include <toy_message.h>
#include <shared_memory.h>
#include <sensor_info.h>

#define TOY_BUFSIZE 1024
#define TOY_TOK_BUFSIZE 64
#define TOY_TOK_DELIM " \t\n\r"

static char global_message[TOY_BUFSIZE];
static pthread_mutex_t global_message_mutex = PTHREAD_MUTEX_INITIALIZER;

static int bmp_shm_fd;

typedef struct _sig_ucontext {
    unsigned long uc_flags;
    struct ucontext *uc_link;
    stack_t uc_stack;
    struct sigcontext uc_mcontext;
    sigset_t uc_sigmask;
} sig_ucontext_t;

static mqd_t watchdog_queue;
static mqd_t monitor_queue;
static mqd_t disk_queue;
static mqd_t camera_queue;
static shm_sensor_t *bmp_shm_msg = NULL; 

static int mqretcode;

void segfault_handler(int sig_num, siginfo_t *info, void *ucontext) {
    void *array[50];
    void *caller_address;
    char **messages;
    int size, i;
    sig_ucontext_t *uc;

    uc = (sig_ucontext_t *) ucontext;

    caller_address = (void *) uc->uc_mcontext.rip;

    fprintf(stderr, "\n");

    if (sig_num == SIGSEGV)
        printf("signal %d (%s), address is %p from %p\n", sig_num, strsignal(sig_num),
               info->si_addr, (void *) caller_address);
    else
        printf("signal %d (%s)\n", sig_num, strsignal(sig_num));
    
    size = backtrace(array, 50);

    free(messages);
    exit(EXIT_FAILURE);
}

/*
    Command Thread
*/
int toy_send(char **args);
int toy_shell(char **args);
int toy_exit(char **args);
int toy_mutex(char **args);
int toy_message_queue(char **args);
int toy_elf(char **args);

char *builtin_str[] = {
    "send",
    "sh",
    "exit",
    "mu",  // mutex
    "mq",  // message queue
    "elf",
    "dump"
};

// function table 정의
int (*builtin_func[]) (char **) = {
    &toy_send,
    &toy_shell,
    &toy_exit,
    &toy_mutex,
    &toy_message_queue,
    &toy_elf,
    &toy_dump
};

int toy_num_builtins()
{
    return sizeof(builtin_str) / sizeof(char *);
}

int toy_elf(char **args)
{
    int mqretcode;
    toy_msg_t msg;
    int fd;
    struct stat st;
    char *contents = NULL;
    size_t contents_sz;
    Elf64Hdr *map;

    fd = open("./sample/sample.elf", O_RDONLY);
    if (fd < 0) {
        printf("fail to open sample.elf\n");
        return -1;
    }

    if (!fstat(fd, &st)) {
        contents_sz = st.st_size;
        if (!contents_sz) {
            printf("Empty ELF file\n");
            close(fd);
            return -1;
        }

        printf("====================================\n");
        printf("File size: %ld\n", contents_sz);
        map = (Elf64Hdr *)mmap(NULL, contents_sz, PROT_READ, MAP_PRIVATE, fd, 0);
        printf("Object file type: %d\n", map->e_type);
        printf("Architecture: %d\n", map->e_machine);
        printf("Object file version: %d\n", map->e_version);
        printf("Entry point virtual address: %ld\n", map->e_entry);
        printf("Program header table file offset: %ld\n", map->e_phoff);
        printf("====================================\n");
        munmap(map, contents_sz);
    }

    return 0;
}

int toy_message_queue(char **args) 
{
    int mqretcode;
    toy_msg_t msg;

    if (args[1] == NULL || args[2] == NULL) {
        return 1;
    }

    if (!strcmp(args[1], "camera")) {
        msg.msg_type = atoi(args[2]);
        msg.param1 = 0;
        msg.param2 = 0;
        mqretcode = mq_send(camera_queue, (char *)&msg, sizeof(msg), 0);
        assert(mqretcode == 0);
    }

    return 1;
}

// commnad에서 mu를 입력했을 때는 
// 센서 스레드에서 글로벌 문자열을 바꾸는 것을 방지하기 위함
int toy_mutex(char **args)
{
    if (args[1] == NULL) {
        return -1;
    }

    printf("save message: %s\n", args[1]);

    if (pthread_mutex_lock(&global_message_mutex) != 0) {
        perror("pthread_mutex_lock Error");
        exit(-1);
    }
    strcpy(global_message, args[1]);
    if (pthread_mutex_unlock(&global_message_mutex)) {
        perror("pthread_mutex_unlock error");
        exit(-1);
    }
    return 1;
}

int toy_send(char **args)
{
    printf("Send message: %s\n", args[1]);
    return 0;
}

// fork하고 exec을 하여 shell이 실행되는 코드이다.
int toy_shell(char **args)
{
    pid_t pid;
    int status;

    switch (pid = fork())
    {
    case -1:
        perror("fail to fork shell");
        break;
    case 0:
        if (execvp(args[0], args) == -1) {
            perror("fail to run shell");
        }
        exit(EXIT_FAILURE);
    default:
        do waitpid(pid, &status, WUNTRACED);
        while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }

    return 1;
}

int toy_exit(char **args) 
{
    printf("Exit\n");
    exit(0);
}

char *toy_read_line(void)
{
    char *line = NULL;
    ssize_t bufsize = 0;

    if (getline(&line, &bufsize, stdin) == -1) {
        if (feof(stdin)) {
            exit(EXIT_SUCCESS);
        } else {
            perror(": getline\n");
            exit(EXIT_FAILURE);
        }
    }
    return line;
}

char **toy_split_line(char *line)
{
    int bufsize = TOY_TOK_BUFSIZE, position = 0;
    char **tokens = malloc(bufsize * sizeof(char *));
    char *token, **tokens_backup;

    if (!tokens) {
        fprintf(stderr, "toy: allocation error\n");
        exit(EXIT_FAILURE);
    }

    token = strtok(line, TOY_TOK_DELIM);
    while (token != NULL) {
        tokens[position] = token;
        position++;

        if (position >= bufsize) {
            bufsize += TOY_TOK_BUFSIZE;
            tokens_backup = tokens;
            tokens = realloc(tokens, bufsize * sizeof(char *));
            if (!tokens) {
                free(tokens_backup);
                fprintf(stderr, "toy: allocation error\n");
                exit(EXIT_FAILURE);
            }
        }

        token = strtok(NULL, TOY_TOK_DELIM);
    }
    tokens[position] = NULL;
    return tokens;
}

int toy_execute(char **args) 
{
    int i;
    if (args[0] == NULL) {
        return 1;
    }
    for(i = 0; i < toy_num_builtins(); i++) {
        if (strcmp(args[0], builtin_str[i]) == 0) {
            return (*builtin_func[i])(args);
        }
    }
    return 1;
}

void toy_loop(void)
{
    char *line;
    char **args;
    int status;

    do {
        printf("TOY>");
        line = toy_read_line();
        args = toy_split_line(line);
        status = toy_execute(args);
        free(line);
        free(args);
    } while (status);
}


void toy_dump(void)
{
    int mqretcode;
    toy_msg_t msg;

    printf("commnad dump\n");

    msg.msg_type = 2;  // DUMP STATE
    msg.param1 = 0;
    msg.param2 = 0;

    mqretcode = mq_send(monitor_queue, (char *)&msg, sizeof(msg), 0);
    assert(mqretcode == 0);

    return 0;
}


void *command_thread(void *arg)
{
    char *s = arg;
    printf("%s", s);
    toy_loop();
    return 0;
}

/*
    Sensor thread
*/
void *sensor_thread(void *arg)
{
    char *s = arg;
    int i = 0;
    toy_msg_t msg;
    int shmid = toy_shm_get_keyid(SHM_KEY_SENSOR);

    printf("%s", s);

    while (1) {
        posix_sleep_ms(5000);

        // 현재 고도/기압/온도 정보를 시스템 V 공유 메모리에 저장 후 
        // 모니터 스레드에 메시지를 전송한다.
        if (bmp_shm_msg != NULL) {
            bmp_shm_msg->temp = 22;
            bmp_shm_msg->press = 1;
            bmp_shm_msg->humidity = 62;
        }

        msg.msg_type = 1;    // SENSOR DATA
        msg.param1 = shmid;  // shm id
        msg.param2 = 0;
        mqretcode = mq_send(monitor_queue, (char *)&msg, sizeof(msg), 0);
        assert(mqretcode == 0);
    }

    return 0;
}

int input()
{
    int retcode;
    int i;
    pthread_t command_thread_tid, sensor_thread_tid;

    printf("나 input 프로세스!\n");

    struct sigaction sa;
    
    sa.sa_flags = SA_RESTART | SA_SIGINFO;
    sa.sa_sigaction = segfault_handler;

    sigemptyset(&sa.sa_mask);
    if(sigaction(SIGSEGV, &sa, NULL) == -1) {
        fprintf(stderr, "signal registration error\n");
        exit(EXIT_FAILURE);
    }

    /* 센서 정보를 공유하기 위한, 시스템 V 공유 메모리를 생성한다 */
    bmp_shm_msg = (shm_sensor_t *)toy_shm_create(SHM_KEY_SENSOR, sizeof(shm_sensor_t));
    if ( bmp_shm_msg == (void *)-1 ) {
        bmp_shm_msg = NULL;
        printf("Error in shm_create SHMID=%d SHM_KEY_SENSOR\n", SHM_KEY_SENSOR);
    }

    // 메시지 큐 open
    watchdog_queue = mq_open("/watchdog_queue", O_RDWR);
    assert(watchdog_queue != -1);
    monitor_queue = mq_open("/monitor_queue", O_RDWR);
    assert(monitor_queue != -1);
    disk_queue = mq_open("/disk_queue", O_RDWR);
    assert(disk_queue != -1);
    camera_queue = mq_open("/camera_queue", O_RDWR);
    assert(camera_queue != -1);

    // command, sensor 쓰레드 생성
    // 여기서 command 쓰레드는 toy_loop 함수를 호출한다.
    retcode = pthread_create(&command_thread_tid, NULL, command_thread, "command thread\n");
    assert(retcode == 0);
    retcode = pthread_create(&sensor_thread_tid, NULL, sensor_thread, "sensor thread\n");
    assert(retcode == 0);

    while (1) {
        sleep(1);
    }

    return 0;
}

int create_input()
{
    pid_t systemPid;
    const char *name = "input";

    printf("여기서 input 프로세스를 생성합니다.\n");

    /* fork 를 이용하세요 */
    switch (systemPid = fork()) {
    case -1:
        printf("Error\n");
        break;
    case 0:
        printf("Parent pid = %d | Child pid = %d\n", systemPid, getppid());
        input();
        break;
    default:
        printf("Parent Pid = %d\n", systemPid);
        break;
    }
    return 0;
}