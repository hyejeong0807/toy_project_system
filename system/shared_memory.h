#ifndef _SHARED_MEMORY_H
#define _SHARED_MEMORY_H

enum def_shm_key {
    SENSOR_BASE = -1,
    BMP280,
    SENSOR_MAX    
};

#define SHM_NUM (SENSOR_MAX - SENSOR_BASE - 1)

// POSIX shared memory 방식 사용
int create_shm(const char *shm_name);

#endif