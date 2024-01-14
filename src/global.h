#ifndef GLOBAL_H
#define GLOBAL_H

/* C/C++ libraries */
#include <stdint.h>

/* List of virtual sensors */
enum {
    SENSOR_0_MANUAL,  // S_INFO (V_TEXT)
    SENSOR_1_VOLUME,  // S_GAS (V_VOLUME)
};

/* */
extern uint32_t g_millis_slept;

#endif
