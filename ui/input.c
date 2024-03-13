#include <stdio.h>
#include <stdlib.h>
#include <sys/prctl.h>
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

#define TOY_TOK_BUFSIZE 64
#define TOY_TOK_DELIM " \t\n\r"

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

char *builtin_str[] = {
    "send",
    "sh",
    "exit"
};

// function table 정의
int (*builtin_func[]) (char **) = {
    &toy_send,
    &toy_shell,
    &toy_exit,
};


int toy_num_builtins()
{
    return sizeof(builtin_str) / sizeof(char *);
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
    while(1) {
        posix_sleep_ms(5000);
    }
}

void *sensor_thread(void *arg)
{
    char *s = arg;
    printf("%s", s);
    while (1) {
        posix_sleep_ms(5000);
    }
}

int input()
{
    int retcode;
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