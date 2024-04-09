#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <poll.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#include <dump_state.h>

#define BUF_SIZE 4096
#define DATETIME_FORMAT "%Y-%m-%d %H:%M:%S"

int dump_file(const char *title, const char *path)
{
    char buf[BUF_SIZE], timestamp[80];
    char *line = NULL;
    int err;
    size_t len;
    ssize_t nread;
    FILE *fp;
    struct stat st;
    time_t mtime;

    if ((fp = fopen(path, "r")) == NULL) {
        err = errno;
        printf("------ %s (%s) ------\n", title, path);
        printf("*** %s: %s\n", path, strerror(err));
        printf("\n");
        exit(-1);
    }

    if ((strncmp(path, "/proc/", 6) != 0) && (strncmp(path, "/sys/", 5) != 0)) return -1;
    if (stat(path, &st) != 0) return -1;

    mtime = st.st_mtime;
    strftime(timestamp, sizeof(timestamp), DATETIME_FORMAT, localtime(&mtime));

    printf("------ %s (%s: %s) ------\n", title, path, timestamp);

    while (1) {
        nread = getline(&line, &len, fp);
        if (nread <= 0) break;
        printf(line);
    }

    printf("\n");
    fclose(fp);

    return 0;
}

void dump_state()
{
    char date[80];
    time_t now = time(NULL);

    strftime(date, sizeof(date), DATETIME_FORMAT, localtime(&now));

    printf("========================================================\n");
    printf("== dumpstate: %s ==\n", date);
    printf("========================================================\n");

    dump_file("KERNEL", "/proc/version");
    dump_file("MEMORY INFO", "/proc/meminfo");
    dump_file("VIRTUAL MEMORY STATS", "/proc/vmstat");
    dump_file("SLAB INFO", "/proc/slabinfo");
    dump_file("ZONE INFO", "/proc/zoneinfo");
    dump_file("PAGE TYPE INFO", "/proc/pagetypeinfo");
    dump_file("BUDDY INFO", "/proc/buddyinfo");
    dump_file("NETWORK DEV INFO", "/proc/net/dev");
    dump_file("NETWORK ROUTES", "/proc/net/route");
    dump_file("NETWORK ROUTES", "/proc/net/ipv6_route");
    dump_file("INTERRUPTS", "/proc/interrupts");
    printf("========================================================\n");
}