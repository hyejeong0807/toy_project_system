#ifndef _SHARED_MEMORY_H
#define _SHARED_MEMORY_H

enum def_shm_key {
    SHM_KEY_BASE = 10,
    BMP280 = SHM_KEY_BASE,
    SHM_KEY_MAX
};

#define SHM_NUM (SHM_KEY_MAX - SHM_KEY_BASE - 1)

static char *shm_names[] = {"bmp280"};
extern int shm_id[SHM_KEY_MAX - SHM_KEY_BASE];

// POSIX shared memory 방식
int create_shm(const char *shm_name);
int open_shm(const char *shm_name);

// System V 방식
void *toy_create_shm(int key, int size);
void *toy_attach_shm(int shmid);
int toy_detach_shm(void *ptr);
int toy_remove_shm(int shmid);
int toy_shm_get_keyid(int key);

#endif