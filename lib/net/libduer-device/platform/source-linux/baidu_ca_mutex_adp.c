// Copyright (2017) Baidu Inc. All rights reserved.
//
// File: baidu_ca_mutex_adp.c
// Auth: Zhang Leliang (zhangleliang@baidu.com)
// Desc: Adapt the mutex function to linux.


#include "lightduer_mutex.h"
#include "lightduer_log.h"
#include "os/os_compat.h"

duer_mutex_t create_mutex(void)
{
    duer_mutex_t mutex;

    pthread_mutex_init((int *)&mutex, NULL); // use the default attribute to initialize the mutex
    if (!mutex) {
        DUER_LOGW("pthread_mutex_init fail!");
        return 0;
    }

    return mutex;
}

duer_status_t lock_mutex(duer_mutex_t mutex)
{
    if (!mutex) {
        return DUER_ERR_FAILED;
    }

    int ret = pthread_mutex_lock((int *)&mutex);
    if (ret) {
        DUER_LOGI("pthread_mutex_lock fail!, ret:%d", ret);
        return DUER_ERR_FAILED;
    }
    return DUER_OK;
}

duer_status_t unlock_mutex(duer_mutex_t mutex)
{
    if (!mutex) {
        return DUER_ERR_FAILED;
    }

    int ret = pthread_mutex_unlock((int *)&mutex);
    if (ret) {
        DUER_LOGW("pthread_mutex_unlock fail!, ret:%d", ret);
        return DUER_ERR_FAILED;
    }
    return DUER_OK;
}

duer_status_t delete_mutex_lock(duer_mutex_t mutex)
{
    if (!mutex) {
        return DUER_ERR_FAILED;
    }

    pthread_mutex_destroy((int *)&mutex);

    return DUER_OK;
}
