/*
 * concurrent_import.h
 *
 *  Created on: Mar 9, 2016
 *      Author: aaron
 */

#ifndef USER_APPLICATIONS_TACRONOME_LIBRARIES_FATFS_PARTICLE_CONCURRENT_IMPORT_H_
#define USER_APPLICATIONS_TACRONOME_LIBRARIES_FATFS_PARTICLE_CONCURRENT_IMPORT_H_

#ifdef	__cplusplus
#include "concurrent_hal.h"
#else

#include <stddef.h>

// This code is used by HAL-clients which don't have access to the FreeRTOS sources
// so we cannot directly define __gthread_t as TaskHandle_t, however, we define it
// here and statically assert that it is the same size.

typedef void* __gthread_t;

typedef void* os_thread_t;
typedef int32_t os_result_t;
typedef uint8_t os_thread_prio_t;
/* Default priority is the same as the application thread */
extern const os_thread_prio_t OS_THREAD_PRIORITY_DEFAULT;
extern const os_thread_prio_t OS_THREAD_PRIORITY_CRITICAL;
extern const size_t OS_THREAD_STACK_SIZE_DEFAULT;
#define bool uint8_t
typedef uint32_t system_tick_t;
extern const system_tick_t CONCURRENT_WAIT_FOREVER;

typedef void* os_mutex_t;
typedef void* os_mutex_recursive_t;
typedef void* condition_variable_t;
typedef void* os_timer_t;

typedef os_mutex_t __gthread_mutex_t;
typedef os_mutex_recursive_t __gthread_recursive_mutex_t;


/**
 * Alias for a queue handle in FreeRTOS - all handles are pointers.
 */
typedef void* os_queue_t;
typedef void* os_semaphore_t;

typedef struct timespec __gthread_time_t;

bool __gthread_equal(__gthread_t t1, __gthread_t t2);
__gthread_t __gthread_self();

typedef condition_variable_t __gthread_cond_t;

int __gthread_cond_timedwait (__gthread_cond_t *cond,
                                   __gthread_mutex_t *mutex,
                                   const __gthread_time_t *abs_timeout);


int __gthread_mutex_timedlock (__gthread_mutex_t* mutex, const __gthread_time_t* timeout);
#endif



#endif /* USER_APPLICATIONS_TACRONOME_LIBRARIES_FATFS_PARTICLE_CONCURRENT_IMPORT_H_ */
