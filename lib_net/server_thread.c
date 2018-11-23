// server_thread.cpp : Основной поток сервера, ожидающий клиентов
//

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <glib.h>
#include <signal.h>
#include "server_thread.h"

#define TIMEOUT_MS_CLOSE	2000 // Сколько ждать подтверждения закрытия сокета с другой стороны

// Make sure we can call this stuff from C++.
#if __cplusplus
extern "C" {
#endif

// пераметры для передачи в основной поток
typedef struct _MAIN_THREAD_ARGS_ {
	uint16_t server_port;
	void *(*func)(void *);
} MAIN_THREAD_ARGS;

// функция дозавершение работы потока thread_main_func() после его завершения
static void thread_main_func_unlocker(void * arg) {
	int port = GPOINTER_TO_INT(arg);
	// дозавершить все дочерние потоки потока
	// ...
}
// основная потоковая функция сервера
static void* thread_main_func(void *vptr_args) {
	// запрещаем завершать поток
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

	MAIN_THREAD_ARGS *args = (MAIN_THREAD_ARGS*) vptr_args;
	// номер порта для прослушивания
	int port = args->server_port;
	//printf("thread_main_func start, port=%d\n",port);/
	// клиентская функция
	void *(*func)(void*) = args->func;
	g_free(args);
	struct sockaddr_in server;	// = {AF_INET,37,INADDR_ANY};
	server.sin_family = AF_INET;
	server.sin_port = htons((u_short) port);
	server.sin_addr.s_addr = INADDR_ANY;	//inet_addr("169.254.0.10");

	// выход их потока при его завершении откладывается до ближайшей точки завершения (pthread_testcancel, pthread_cond_wait, sleep, printf и т.д.)
	pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);// это и так сделано по умолчанию
	// регистрация функции при завершении потока через pthread_cancel()
	pthread_cleanup_push(&thread_main_func_unlocker, GINT_TO_POINTER(port));
	// снова разрещаем завершать поток
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

	SOCKET sockfd;
	SOCKET newsockfd;
	// создаем сокет;
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
		printf("[thread_main_func] exit(port=%d), sockfd==INVALID_SOCKET \n",
				port);
		pthread_exit(0);
	}
#ifndef WIN32
	int a = 1;
	// Чтобы не мешали TIME_WAIT от предыдущих соединений при пересоздании сервера
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &a, sizeof(int));
	// время, которое даётся на закрытие сокета - не работает
	//linger l = { 1, 0 };
	//setsockopt(sockfd, SOL_SOCKET, SO_LINGER, &l, sizeof(linger));
#endif
	// связываем адрес с сокетом;
	if (bind(sockfd, (const struct sockaddr*) &server,
			sizeof(struct sockaddr_in)) == SOCKET_ERROR) {
		printf("server can not start. may be port=%d busy.\n", port);
		closesocket(sockfd);
		pthread_exit(0);
	} else
		printf("server on port=%d start.\n", port);
	// включаем прием соединений;
	if (listen(sockfd, 5) == SOCKET_ERROR) {
		printf("[thread_main_func] exit(port=%d), sockfd==INVALID_SOCKET \n",
				port);
		pthread_exit(0);
	}
	//add_connection((int)sockfd, GINT_TO_POINTER(port), TYPE_SERVER, NULL);

	// Делаем сокет неблокирующим
	//gulong arg = 1; ioctlsocket(sockfd, FIONBIO, &arg);
	//rc = setsockopt(sock,SOL_SOCKET,SO_ATTACH_FILTER,&opcode,sizeof(int));
	//fcntl(sockfd, F_SETFL, O_NONBLOCK);

	// ждём клиентов
	while (1) {
		pthread_t threadId;
		struct sockaddr_in peer;
		socklen_t size = sizeof(peer);
		//printf("before accept=%d\n",sockfd);
		// Поступил новый запрос на соединение, используем accept
		if ((newsockfd = accept(sockfd, (struct sockaddr*) &peer, &size))
				== (SOCKET) - 1) {
			printf("accept=%d break (sockfd=%d)\n", newsockfd, sockfd);
			break;
		} else
			printf("accept=%d OK (sockfd=%d)\n", newsockfd, sockfd);
		//printf("accept=%d\n",newsockfd);

		if (!is_valid_socket(sockfd)) {
			printf("socket sockfd=%d invalid break\n", sockfd);
			break;
		}

		//else
		//printf("socket sockfd valid ok\n");

		// заполняем структуру с параметрами для передачи в дочерний поток
		{
			THREAD_ARGS *args = (THREAD_ARGS*) g_malloc(sizeof(THREAD_ARGS));
			args->client_ip_addr = g_strdup(inet_ntoa(peer.sin_addr)); //peer.sin_addr.S_un.S_addr;
			args->client_port = ntohs(peer.sin_port);
			args->server_port = (uint16_t) port;
			args->newsockfd = newsockfd;
			// Дочерний поток
			pthread_create(&threadId, NULL, func, (void*) args);
			// чтобы поток не оставался в состоянии dead после завершения
			pthread_detach(threadId);
		}
	};
	// закрыть соединение
	int cs = s_close(sockfd, TIMEOUT_MS_CLOSE); //int cs = closesocket(sockfd);
	printf("before exit server thread socket=%d cs=%d\n", sockfd, cs);
	//del_connection((int)sockfd);
	//printf("thread_main_func exit, port=%d\n",port);
	printf("exit server thread socket=%d closesocket=%d\n", sockfd, cs);
	// вызов ранее зарегистрированной функции при завершении потока через pthread_cancel() для очистки памяти
	pthread_cleanup_pop(1);
	// выход из потока, pthread_cleanup_pop() выполнится внутри pthread_exit() если ещё не был выполнен
	pthread_exit(0);
	//return NULL;
}
// удаление структуры THREAD_ARGS
void free_thread_args(THREAD_ARGS *arg) {
	if (!arg)
		return;
	g_free(arg->client_ip_addr);
	g_free(arg);
}

// запустить поток сервера
// func - потоковая функция одного клиента
// return true - успешная остановка, false - нет
bool start_server_tcp(pthread_t *thread, int port, void *(*func)(void *)) {
	MAIN_THREAD_ARGS *args = (MAIN_THREAD_ARGS*) g_malloc(
			sizeof(MAIN_THREAD_ARGS));
	if (!args)
		return false;
	args->server_port = port;
	args->func = func;
	if (args->server_port <= 0 || !args->func
			|| pthread_create(thread, NULL, thread_main_func, (void*) args)
					!= 0) {
		// ошибка создания потока
		return false;
	}
	return true;
}

// остановить поток сервера
// return true - успешная остановка, false - нет
bool stop_server_tcp(pthread_t thread_id) {
	// остановить потоковую функцию сервера
	pthread_cancel(thread_id);
	// ждём завершение потока сервера
	pthread_join(thread_id, NULL);
	/*
	 // TODO: попробовать вместо pthread_kill: pthread_cancel(thread); pthread_join(thread, NULL);
	 if (pthread_kill(thread, 9) != 0)//SIGQUIT=3 SIGINT=2 SIGKILL=9 SIGTERM=15
	 {//EINTR
	 // ошибка удаления потока
	 printf("stop pthread_kill err, thread=%x\n",thread);
	 return 0;
	 }
	 //printf("stop pthread_kill ok, thread=%ldn",thread);
	 */
	return true;
}

// ждём завершения потока сервера
bool wait_server_tcp(pthread_t thread_id) {
	if (!thread_id)
		return false;
	// ждём завершения потока
	if (pthread_join(thread_id, NULL) == EOK)
		return true;
	return false;
}

#if __cplusplus
}  // End of the 'extern "C"' block
#endif
