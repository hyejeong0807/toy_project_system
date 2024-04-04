#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <errno.h>
#include <iostream>
#include <cstdio>
#include <unistd.h>

#include <hardware.h>

#include "camera_HAL.h"
#include "ControlThread.h"

static ControlThread *control_thread;

using std::cout;
using std::endl;

int toy_camera_open(void)
{
    cout << "toy_camera_open" << endl;

    control_thread = new ControlThread();

    if (control_thread == NULL) {
        cout << "Memory allocation error!" << endl;
        return -ENOMEM;
    }

    return 0;
}

int toy_camera_take_picture(void)
{
    return control_thread->takePicture();
}

int toy_camera_dump(void)
{
    return control_thread->dump();
}

hw_module_t HAL_MODULE_INFO_SYM = {
    .tag: HARDWARE_MODULE_TAG,
    .id: CAMERA_HARDWARE_MODULE_ID,
    .name: "Toy Camera Hardware Module",
    .open: toy_camera_open,
    .take_picture: toy_camera_take_picture,
    .dump: toy_camera_dump,
};
