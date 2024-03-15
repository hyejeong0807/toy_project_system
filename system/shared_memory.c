#include <sys/types.h>
#include <sys/shm.h>
#include <errno.h>

#include <shared_memory.h>

#define ERROR_CHECK(x, y, z) if ((x) == (-1)) { perror(y); return z; }

int create_shm(const char *shm_name)
{
    int fd;

    fd = shm_open(shm_name, O_RDWR | O_CREAT | O_EXCL, 0644);
    ERROR_CHECK(fd, "shm_open()", -1);

    return fd;
}

int open_shm(const char *shm_name)
{
    int fd;

    
}