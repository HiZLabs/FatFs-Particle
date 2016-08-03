/*
 * concurrent_import.h
 *
 *  Created on: Mar 9, 2016
 *      Author: aaron
 */

#ifndef FATFS_PARTICLE_CONCURRENT_IMPORT_H_
#define FATFS_PARTICLE_CONCURRENT_IMPORT_H_

#ifdef	__cplusplus
#include "concurrent_hal.h"
#else

#include <stddef.h>
#include "logging.h"

#define bool uint8_t
typedef uint32_t system_tick_t;

#define CONCURRENT_WAIT_FOREVER ( (system_tick_t)-1 )

typedef void* os_mutex_t;

int os_mutex_create(os_mutex_t* mutex);
int os_mutex_destroy(os_mutex_t mutex);
int os_mutex_lock(os_mutex_t mutex);
int os_mutex_trylock(os_mutex_t mutex);
int os_mutex_unlock(os_mutex_t mutex);

void delay(unsigned long ms);


#endif


#endif /* FATFS_PARTICLE_CONCURRENT_IMPORT_H_ */
