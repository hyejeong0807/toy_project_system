#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <errno.h>
#include <iostream>
#include <cstdio>
#include <unistd.h>
#include "camera_HAL.h"
#include "ControlThread.h"

static ControlThread *control_thread;

int toy_camera_open(void)
{
    cout << "toy_camera_open" << endl;

    control_thread = new 

}