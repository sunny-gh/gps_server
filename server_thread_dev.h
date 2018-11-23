// server_thread_dev.h: Основной поток сервера, ожидающий клиентов устройств
//
#ifndef _SERVER_THREAD_DEV_H
#define _SERVER_THREAD_DEV_H
#include <pthread.h>
#include "lib_net/sock_func.h"

// потоковая функция сервера для одного клиента
void* thread_client_func_dev(void *vptr_args);

#endif	//_SERVER_THREAD_DEV_H
