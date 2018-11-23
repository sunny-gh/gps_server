// server_thread_api.h: Основной поток сервера, для обмена с клиентским приложением
//
#ifndef _SERVER_THREAD_API_H
#define _SERVER_THREAD_API_H
#include <pthread.h>
#include "lib_net/sock_func.h"

// потоковая функция для одного клиента
void* thread_client_func_api(void *vptr_args);

#endif	//_SERVER_THREAD_API_H
