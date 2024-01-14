#ifndef VOLUME_H
#define VOLUME_H

/* C/C++ libraries */
#include <stdint.h>

/* Prototypes */
void volume_set(const float volume);
void volume_set(const char* const volume);
bool volume_increase(void);
int32_t volume_task(void);

#endif
