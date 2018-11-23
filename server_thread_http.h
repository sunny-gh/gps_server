// server_thread_http.h: Основной поток http сервера
//
#ifndef _SERVER_THREAD_HTTP_H
#define _SERVER_THREAD_HTTP_H
#include "lib_net/sock_func.h"
#include <pthread.h>

// потоковая функция сервера для одного клиента
void* thread_client_func_http(void *vptr_args);

#endif	//_SERVER_THREAD_HTTP_H
