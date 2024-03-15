#ifndef _SENSOR_INFO_H_
#define _SENSOR_INFO_H_

typedef struct shm_sensor {
    int temp;
    int press;
    int humidity;
} shm_sensor_t;

#endif