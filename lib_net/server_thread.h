// server_thread.h: Основной поток сервера, ожидающий клиентов
//
#ifndef _SERVER_THREAD_H
#define _SERVER_THREAD_H
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include "sock_func.h"

// Make sure we can call this stuff from C++.
#if __cplusplus
extern "C" {
#endif

// пераметры для передачи в дочерний поток
typedef struct _THREAD_ARGS_ {
	char *client_ip_addr;
	uint16_t client_port;
	uint16_t server_port;
	SOCKET newsockfd;
} THREAD_ARGS;

// удаление структуры THREAD_ARGS
void free_thread_args(THREAD_ARGS *arg);

// запустить поток сервера
// func - потоковая функция одного клиента
// return true - успешная остановка, false - нет
bool start_server_tcp(pthread_t *thread, int port, void *(*func)(void *));

// остановить поток сервера
// return true - успешная остановка, false - нет
bool stop_server_tcp(pthread_t thread_id);

// ждём завершения потока сервера
bool wait_server_tcp(pthread_t thread_id);

#if __cplusplus
}  // End of the 'extern "C"' block
#endif

#endif	//_SERVER_THREAD_H
