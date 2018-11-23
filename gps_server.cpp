//============================================================================
// Name        : gps_server.cpp
// Author      : Aleksandr Lysikov
// Version     :
// Copyright   : 
// Description : Основные функции, которые связывают демона и программу, объявлены в daemon/work.h:
//					void destroy_work(void);
//					int init_work(void);
//					int restart_work(void);
//============================================================================

#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <glib-object.h>
#include "server_thread_ports.h"
#include "server_thread_http.h"
#include "server_thread_dev.h"
#include "server_thread_api.h"
#include "base_lib.h"

// получить имя web сервера
const char *get_server_name() {
	static const char *server_name = "GPS Tracking API Server 0.1";
	return server_name;
}

static pthread_t thread_api, thread_wialon, thread_osmand, thread_gt06,
		thread_babywatch; // , thread_traccar;

// запустить все потоки
static int start_connectoins(void) {
	/*
	 int port_api = PORT_API;// 8090;
	 int port_wialon = PORT_WIALON;// 9001;
	 int port_osmand = PORT_OSMAND;//9002;
	 int port_traccar = PORT_TRACCAR;//9003;
	 int port_gt06 = PORT_GT06;//9004;
	 int port_babywatch = PORT_BABYWATCH;//9005;
	 */
	// запустить потоки серверов
	if (!start_server_tcp(&thread_wialon, PORT_HTTP, thread_client_func_http))
		return 0;
	if (!start_server_tcp(&thread_wialon, PORT_WIALON, thread_client_func_dev))
		return 0;
	if (!start_server_tcp(&thread_osmand, PORT_OSMAND, thread_client_func_dev))
		return 0;
	if (!start_server_tcp(&thread_gt06, PORT_GT06, thread_client_func_dev))
		return 0;
	if (!start_server_tcp(&thread_babywatch, PORT_BABYWATCH,
			thread_client_func_dev))
		return 0;
	//if (!start_server_tcp(&thread_traccar, port_traccar, thread_client_func_dev))	return 0;
	// запустить поток - для api
	if (!start_server_tcp(&thread_api, PORT_API, thread_client_func_api))
		return 0;
	return 1;
}

// остановить все потоки
static int stop_connections(void) {
	// остановить все соединения
	stop_all_connections();
	//g_usleep(2000*1000);// задержка в микросекундах, 500*1000 = 0,5 сек.
	// на всякий случай пытаемся убить потоки, они уже не должны работать
	//stop_server_tcp(thread_wialon);
	//stop_server_tcp(thread_api);
	return 1;
}

// ожидание остановки потоков
void wait_stop_servers(void) {
	// ждём завершения потоков
	wait_server_tcp(thread_wialon);
	wait_server_tcp(thread_osmand);
	//wait_server_tcp(thread_traccar);
	wait_server_tcp(thread_gt06);
	wait_server_tcp(thread_babywatch);
	wait_server_tcp(thread_api);// стоит последним, т.к. последним завершится
	//thread_wialon = 0;
	//thread_api = 0;
}

// функция для остановки потоков и освобождения ресурсов
// тут должен быть код который остановит все потоки и корректно освободит ресурсы
void destroy_work(void) {
	stop_connections();
	// ждём завершения потоков
	//wait_stop_connectoins();
	// Окончание работы с сокетами
	closeSocks();
}

// функция которая инициализирует рабочие потоки
// return: 1, если успешно
int init_work(void) {
#if !GLIB_CHECK_VERSION(2,36,0)
	g_type_init();
#endif
	// иннициализация БД
	base_init();
	// Иннициализация работы с сокетами
	initSock();
	int ret = start_connectoins();
	return ret;
}

// функция которая загрузит конфиг заново и внесет нужные поправки в работу
// return: 1, если успешно
int restart_work(void) {
	// остановить все потоки (ожидание внутри)
	stop_connections();
	// ожидании остановки потоков
	//wait_stop_connectoins();
	//g_usleep(1000*1000);// задержка в микросекундах, 2 сек.
	// запустить все потоки
	int ret = start_connectoins();
	return ret;
}
