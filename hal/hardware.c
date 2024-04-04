#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <errno.h>
#include <limits.h>
#include <dlfcn.h>

#include <hardware.h>

#define HAL_LIBRARY_PATH1 "./libcamera.so"

static int load(const struct hw_module_t **pHmi)
{
    int status;
    void *handle;
    struct hw_module_t *hmi;

    handle = dlopen(HAL_LIBRARY_PATH1, RTLD_NOW);
    if (!handle) {
        printf("fail to dlopen, %s\n", dlerror());
        return 0;
    }

    // dlsym: 열려진 라이브러리의 심볼 값을 찾아준다. 
    hmi = dlsym(handle, HAL_MODULE_INFO_SYM_AS_STR);
    if ( dlerror() != NULL ) {
        printf("fail to dlsym, %s\n", dlerror());
        return 0;
    }

    printf("loaded HAL hmi: %p, handle: %p\n", pHmi, handle);
    *pHmi = pmi;

    return status;
}

int hw_get_camera_module(const struct hw_module_t **module)
{
    return load()
}