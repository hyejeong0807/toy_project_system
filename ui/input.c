#include <stdio.h>
#include <stdlib.h>
#include <sys/prctl.h>
#include <sys/shm.h>    // shmget
#include <execinfo.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <ucontext.h>
#include <unistd.h>
#include <time.h>

#include <system_server.h>
#include <gui.h>
#include <input.h>
#include <web_server.h>
#include <toy_message.h>
#include <shared_memory.h>

#define TOY_BUFSIZE 1024
#define TOY_TOK_BUFSIZE 64
#define TOY_TOK_DELIM " \t\n\r"

static char global_message[TOY_BUFSIZE];
static pthread_mutex_t global_message_mutex = PTHREAD_MUTEX_INITIALIZER;

static shm_sensor_t *bmp_shm_msg; 
static int bmp_shm_fd;
int shm_id[SHM_KEY_MAX - SHM_KEY_BASE];

typedef struct _sig_ucontext {
    unsigned long uc_flags;
    struct ucontext *uc_link;
    stack_t uc_stack;
    struct sigcontext uc_mcontext;
    sigset_t uc_sigmask;
} sig_ucontext_t;

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

char *builtin_str[] = {
    "send",
    "sh",
    "exit",
    "mu", // mutex
    "mq"  // message queue
};

// function table 정의
int (*builtin_func[]) (char **) = {
    &toy_send,
    &toy_shell,
    &toy_exit,
    &toy_mutex,
    &toy_message_queue
};

int toy_num_builtins()
{
    return sizeof(builtin_str) / sizeof(char *);
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
        mqretcode = mq_send("/camera_queue", (char *)&msg, sizeof(msg), 0);
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

    printf("save message: %d\n", args[1]);

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

char **toy_split_line(char *args)
{
    int buf_size = TOY_TOK_BUFSIZE, position = 0;
    char **tokens = malloc(bufsize * sizeof(char *));
    char *token, **tokens_backup;

    if (!tokens) {
        fprintf(stderr, "toy: allocation error\n");
        exit(EXIT_FAILURE);
    }

    token = strtok(args, TOY_TOK_DELIM);
    while (token != NULL) {
        tokens[position] = token;
        position++;

        if (position >= buf_size) {
            buf_size += TOY_TOK_BUFSIZE;
            tokens_backup = tokens;
            tokens = realloc(tokens, buf_size * sizeof(char *));
            if (!tokens) {
                free(tokens_backup);
                perror("fail to realloc for args buffer");
                exit(1);
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
    site_t len = 0;

    do {
        printf("TOY> ");
        if (getline(&line, &len, stdin) < 0) {
            perror("Fail to read from stdin");
            exit(1);
        }
        if (strcmp(line, "\n") == 0) continue;

        // 받아온 문자열 (명령어)을 token으로 분리해서 가져온다.
        args = toy_split_line(line);
        status = toy_execute(args);

        free(line);
        free(args);
    } while(status);
}

void *command_thread(void *arg)
{
    char *s = arg;
    printf("%s", s);
    toy_loop();
}

/*
    Sensor thread
*/
void *sensor_thread(void *arg)
{
    char saved_message[TOY_BUFSIZE];
    char *s = arg;
    int i = 0;
    toy_msg_t msg;
    shm_sensor_t shmsg;

    printf("%s", s);

    while (1) {
        posix_sleep_ms(5000);

        // 현재 고도/기압/온도 정보를 시스템 V 공유 메모리에 저장 후 
        // 모니터 스레드에 메시지를 전송한다.
        shmsg.temp = 22;
        shmsg.press = 1;
        shmsg.humidity = 62;
        
        memcpy(bmp_shm_msg, &shmsg, sizeof(shm_sensor_t));

        msg.msg_type = 1;  // SENSOR DATA
        msg.param1 = shm_id[0];  // shm id
        msg.param2 = 0;
        mqretcode = mq_send(monitor_queue, (char *)&msg, sizeof(msg), 0);
        assert(mqretcode == 0);

        // 뮤텍스 추가
        // 한 글자씩 출력 후 슬립하는
        // sensor thread에서 변경시킬 수 있기 때문에 이 부분을 critical section으로 
        // i = 0;
        // pthread_mutex_lock(&global_message_mutex);
        // while (global_message[i] == NULL) {
        //     printf("%c", global_message[i]);
        //     fflush(stdout);
        //     posix_sleep_ms(500);
        //     i++;
        // }
        // pthread_mutex_unlock(&gloabl_message_mutex);
    }

    return 0;
}

// lab 9: 토이 생산자,소비자 실습
#define MAX 30
#define NUMTHREAD 3 /* number of threads */

char buffer[TOY_BUFSIZE];
// 버퍼를 포인팅하는 변수
int read_count = 0, write_count = 0;
int buflen;
pthread_mutex_t count_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t empty = PTHREAD_COND_INITIALIZER;
int thread_id[NUMTHREAD] = {0, 1, 2};
int producer_count = 0, consumer_count = 0;

void *toy_consumer(int *id)
{
    pthread_mutex_lock(&count_mutex);
    while (consumer_count < MAX) {
        pthread_cond_wait(&empty, &count_mutex);
        // queue에서 하나 꺼낸다.
        printf("                           소비자 [%d]: %c\n", *id, buffer[read_count]);
        read_count = (read_count + 1) % TOY_BUFSIZE;
        fflush(stdout);
        consumer_count++;
    }
    pthread_mutex_unlock(&count_mutex);
}

void *toy_producer(int *id)
{
    while (producer_count < MAX) {
        pthread_mutex_lock(&count_mutex);
        strcpy(buffer,"");
        // global message를 가져온다.
        buffer[write_count] = global_message[write_count % buffer];
        // 큐에 추가한다
        printf("%d - 생산자[%d]: %c \n", producer_count, *id, buffer[write_count]);
        fflush(stdout);
        write_count = (write_count + 1) 5 TOY_BUFSIZE;
        producer_count++;
        pthread_cond_signal(&empty);
        pthread_mutex_unlock(&count_mutex);
        sleep(rand() % 3);
    }
}

int input()
{
    int retcode;
    int i;
    pthread_t command_thread_tid, sensor_thread_tid;

    pthread_t threadp[NUMTHREAD];

    printf("나 input 프로세스!\n");

    struct sigaction sa;
    
    sa.sa_flags = SA_RESTART | SA_SIGINFO;
    sa.sa_sigaction = segfault_handler;

    sigemptyset(&sa.sa_mask);
    if(sigaction(SIGSEGV, &sa, NULL) == -1) {
        fprintf(stderr, "signal registration error\n");
        exit(EXIT_FAILURE);
    }

    // command, sensor 쓰레드 생성
    // 여기서 command 쓰레드는 toy_loop 함수를 호출한다.
    retcode = pthread_create(&command_thread_tid, NULL, command_thread, "command thread\n");
    assert(retcode == 0);
    retcode = pthread_create(&sensor_thread_tid, NULL, sensor_thread, "sensor thread\n");
    assert(retcode == 0);

    /* producer - consumer 실습 */
    // pthread_mutex_lock(&global_message_mutex);
    // strcpy(global_message, "hello world!");
    // buflen = strlen(global_message);
    // pthread_mutex_unlock(&global_message_mutex);
    // pthread_create(&thread[0], NULL, (void *)toy_consumer, &thread_id[0]);
    // pthread_create(&thread[1], NULL, (void *)toy_producer, &thread_id[1]);
    // pthread_create(&thread[2], NULL, (void *)toy_producer, &thread_id[2]);

    // for (i = 0; i < NUMTHREAD; i++) {
    //     pthread_join(thread[i], NULL);
    // }

    /* 
    *    센서 정보를 공유하기 위한 System V 메모리를 생성
    */
    shm_id[BMP280 - SHM_KEY_BASE] = shmget(BMP280, sizeof(shm_sensor_t), IPC_CREATE | 0666);
    if (shm_id[0] == -1) {
        perror("shmget");
        exit(EXIT_FAILURE);
    }

    bmp_shm_msg = (shm_sensor_t *)shmat(shm_id[0], NULL, 0);
    if (bmp_shm_msg == (void *)-1) {
        perror("shmat");
        exit(EXIT_FAILURE);
    }

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