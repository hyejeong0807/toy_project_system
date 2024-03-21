#ifndef _SHARED_MEMORY_H
#define _SHARED_MEMORY_H

enum def_shm_key {
    SHM_KEY_BASE = 10,
    SHM_KEY_SENSOR = SHM_KEY_BASE,
    SHM_KEY_MAX
};

#define SHM_NUM (SHM_KEY_MAX - SHM_KEY_BASE - 1)

static char *shm_names[] = {"bmp280"};
extern int shm_id[SHM_KEY_MAX - SHM_KEY_BASE];

// POSIX shared memory 방식
int create_shm(const char *shm_name);
int open_shm(const char *shm_name);

void *toy_shm_create(int key, int size);
void *toy_shm_attach(int shmid);
int toy_shm_detach(void *ptr);
int toy_shm_remove(int shmid);
int toy_shm_get_keyid(int key);

#endif