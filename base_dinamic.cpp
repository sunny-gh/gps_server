// base_dinamic.cpp : Работа со списком соединений
//

#include <stdint.h>
#include <libintl.h>
#include <stdio.h> // for printf
#include <string.h>
#include <time.h>
#include <glib.h>
#include <pthread.h>
//#include "wialon_ips.h"
#include "base_lib.h"
#include "lib_protocol/protocol.h"
#include "my_time.h"
#include "md5.h"
#include "encode.h"
#include "lib_net/sock_func.h"

static GMutex mutex_conn;

// текущие соединения
static GList *list_conn;
struct CONN {
	int id;		  // уникальный номер - номер сокета
	gpointer info;// доп. информация - номер порта сервера для первичного сокета(у которого is_server=TRUE), для вторичного - NULL(api), PACKET_INFO *packet_info (dev)
	int type_conn;
	//TYPE_SERVER  первичный сервер(висит на accept), доп. информация - номер порта сервера для первичного сокета
	//TYPE_DEV,	   вторичное соединение с устройством, доп. информация - PACKET_INFO*
	//TYPE_API,	   вторичное соединение с пользователем по API, доп. информация - NULL, а потом USER_INFO*

	pthread_t thread;// идентификатор потока, из которого добавлено соединение

	gboolean is_login;		// авторизовано ли это соединение
	GMutex *mutex;		// для блокировки передачи данных
};

// удалить структуру USER_INFO
void g_free_userinfo(struct USER_INFO *uinfo) {
	if (!uinfo)
		return;
	g_free(uinfo->name);
	g_strfreev(uinfo->devs_list);
	g_free(uinfo);
}

// иннициализация
void base_dinamic_init(void) {
	g_mutex_init(&mutex_conn);
}

// добавить активное соединение
// id - уникальный идентификатор соединения (номер сокета)
void add_connection(int id, gpointer info, int type_conn, GMutex **mutex) {
	g_mutex_lock(&mutex_conn);		// блокировка мьютекса
	struct CONN *conn = (struct CONN*) g_malloc0(sizeof(struct CONN));
	conn->id = id;
	conn->info = info;
	conn->type_conn = type_conn;
	conn->thread = pthread_self();
	if (type_conn == TYPE_API || type_conn == TYPE_DEV) {
		conn->mutex = (GMutex*) g_malloc0(sizeof(GMutex));// для блокировки передачи данных
		//conn->mutex = g_slice_new(GMutex);
		g_mutex_init(conn->mutex);
		if (mutex)
			*mutex = conn->mutex;
	}
	list_conn = g_list_prepend(list_conn, conn);// быстрее добавлять в начало списка
	g_mutex_unlock(&mutex_conn);		// разблокировка мьютекса
	printf("start connection %d (type_conn=%d)\n", id, type_conn);		// xxx
}

// функция поиска в списке (return 0, если p1=p2)
static gint func_find(gconstpointer p1, gconstpointer p2) {
	uint64_t a, b;
	struct CONN *conn1 = (struct CONN*) p1;
	struct CONN *conn2 = (struct CONN*) p2;
	a = conn1->id;
	b = conn2->id;
	// сортировка по возрастанию
	return (a > b ? +1 : a == b ? 0 : -1);
}

// добавить/обновить к активному соединению параметр
void add_connection_param(int id, gpointer info) {
	struct CONN conn_tmp;
	conn_tmp.id = id;
	g_mutex_lock(&mutex_conn);		// блокировка мьютекса
	// ищем в списке существующее соединение
	GList *list = g_list_find_custom(list_conn, &conn_tmp, func_find);
	// если нашли
	if (list) {
		struct CONN *conn = (struct CONN*) list->data;
		// удаляем предыдущую структуру, если она есть
		if (conn->info && conn->type_conn == TYPE_API)
			g_free_userinfo((struct USER_INFO*) conn->info);
		conn->info = info;
	}
	g_mutex_unlock(&mutex_conn);	// разблокировка мьютекса
	//printf("auth connection %d\n", id);// xxx
}

// поиск следующего за sock_start активного API соединения, в котором у пользователя есть устройство imei
// [in/out] sock_start - номер сокета, с которого начинаем искать, если = 0, то ищем первое соединение
// return: номер соединения(сокета), 0 - не нашли
int find_api_connection(int *sock_start, const char *imei, char **user_name,
		GMutex **mutex_rw) {
	int ret_id = 0;
	if (!imei)
		return 0;
	gboolean is_find_first_element = FALSE;
	if (!sock_start || !*sock_start)
		is_find_first_element = TRUE;
	g_mutex_lock(&mutex_conn);	// блокировка мьютекса
	// ищем в списке существующее соединение
	GList *list = g_list_first(list_conn);
	while (list) {
		struct CONN *conn = (struct CONN*) list->data;
		// проверяем соединение на авторизацию и тип соединения
		if (conn && conn->info && conn->is_login
				&& conn->type_conn == TYPE_API) {
			int i;
			struct USER_INFO *uinfo = (struct USER_INFO*) conn->info;
			// проходим по всем устройствам пользователя
			for (i = 0; i < uinfo->devs_n; i++) {
				// imei в списке у пользователя совпал с искомым
				if (!g_strcmp0((const char*) *(uinfo->devs_list + i), imei)) {
					// ищем новое соединение
					if (is_find_first_element) {
						ret_id = conn->id;
						if (user_name)
							*user_name = g_strdup(uinfo->name);
						if (mutex_rw)
							*mutex_rw = conn->mutex;
						if (sock_start)
							*sock_start = conn->id;
						list = NULL;
						break;
					}
					// ищем предыдущее соединение
					else {
						if (conn->id == *sock_start)
							is_find_first_element = TRUE;
					}
				}
			}
		}
		list = g_list_next(list);
	}
	g_mutex_unlock(&mutex_conn);	// разблокировка мьютекса
	return ret_id;
}

// проверяем наличие API соединений с пользователем user_name
// return: есть или нет активные соединения с пользователем
gboolean find_api_user_connection(const char *user_name) {
	gboolean ret = FALSE;
	if (!user_name)
		return FALSE;
	g_mutex_lock(&mutex_conn);	// блокировка мьютекса
	// ищем в списке существующее соединение
	GList *list = g_list_first(list_conn);
	while (list) {
		struct CONN *conn = (struct CONN*) list->data;
		// проверяем соединение на авторизацию и тип соединения
		if (conn && conn->info && conn->type_conn == TYPE_API) {
			struct USER_INFO *uinfo = (struct USER_INFO*) conn->info;
			if (!g_strcmp0((const char*) (uinfo->name), user_name)) {
				ret = TRUE;
				break;
			}
		}
		list = g_list_next(list);
	}
	g_mutex_unlock(&mutex_conn);	// разблокировка мьютекса
	return ret;
}

// поиск активного DEV соединения для устройства imei
// [out] protocol -  тип протокола (PROTOCOL_NA, PROTOCOL_WIALON и т.д.)
// [out] ver - версия протокола (0-никакая, Wialon IPS (1.1=11 или 2.0=20))
// return: номер соединения(сокета), 0 - не нашли
int find_dev_connection(const char *imei, int *protocol, uint16_t *ver,
		GMutex **mutex_rw) {
	int ret_id = 0;
	if (!imei)
		return 0;
	g_mutex_lock(&mutex_conn);	// блокировка мьютекса
	// ищем в списке существующее соединение
	GList *list = g_list_first(list_conn);
	while (list) {
		struct CONN *conn = (struct CONN*) list->data;
		//printf("[find_dev_connection] conn->info=%x is_login=%d type_conn=%d mutex_r=%x\n", conn->info, conn->is_login, conn->type_conn, conn->mutex);
		// проверяем соединение на авторизацию и тип соединения
		if (conn && conn->info && conn->type_conn == TYPE_DEV)//&& conn->is_login
				{
			struct PACKET_INFO *packet = (struct PACKET_INFO*) conn->info;
			//printf("[find_dev_connection] imei1=%s imei2=%s\n", packet->imei, imei);
			//printf("[find_dev_connection] imei1=%s\n", packet->imei);
			// imei в списке у пользователя совпал с искомым
			if (!g_strcmp0((const char*) packet->imei, imei)) {
				ret_id = conn->id;
				if (protocol)
					*protocol = packet->protocol;
				if (ver)
					*ver = packet->ver;
				if (mutex_rw)
					*mutex_rw = conn->mutex;
				break;
			}
		}
		list = g_list_next(list);
	}
	g_mutex_unlock(&mutex_conn);			// разблокировка мьютекса
	return ret_id;
}

// поиск активного DEV соединения по ip адресу
// return: номер соединения(сокета), 0 - не нашли
int find_dev_connection_by_ip(const char *ip) {
	int ret_id = 0;
	if (!ip)
		return 0;
	g_mutex_lock(&mutex_conn);			// блокировка мьютекса
	// ищем в списке существующее соединение
	GList *list = g_list_first(list_conn);
	while (list) {
		struct CONN *conn = (struct CONN*) list->data;
		//printf("[find_dev_connection] conn->info=%x is_login=%d type_conn=%d mutex_r=%x\n", conn->info, conn->is_login, conn->type_conn, conn->mutex);
		// проверяем соединение на авторизацию и тип соединения
		if (conn && conn->info && conn->type_conn == TYPE_DEV)//&& conn->is_login
				{
			struct PACKET_INFO *packet = (struct PACKET_INFO*) conn->info;
			//printf("[find_dev_connection] imei1=%s imei2=%s\n", packet->imei, imei);
			// imei в списке у пользователя совпал с искомым
			if (!g_strcmp0((const char*) packet->client_ip_addr, ip)) {
				ret_id = conn->id;
				break;
			}
		}
		list = g_list_next(list);
	}
	g_mutex_unlock(&mutex_conn);			// разблокировка мьютекса
	return ret_id;
}

// удалить активное соединение
void del_connection(int id) {
	struct CONN conn_tmp;
	conn_tmp.id = id;
	g_mutex_lock(&mutex_conn);			// блокировка мьютекса
	// ищем в списке существующее соединение
	GList *list = g_list_find_custom(list_conn, &conn_tmp, func_find);
	//struct CONN *conn_del = (struct CONN *)list->data;
	// если нашли, то удаляем
	if (list) {
		struct CONN *conn = (struct CONN*) list->data;
		// удаляем содержимое структуры conn
		if (conn->mutex)
			g_free(conn->mutex);
		if (conn->info && conn->type_conn == TYPE_API)
			g_free_userinfo((struct USER_INFO*) conn->info);
		// удаляем структуру conn
		g_free(conn);
		list_conn = g_list_remove(list_conn, list->data);
	}
	g_mutex_unlock(&mutex_conn);	// разблокировка мьютекса
	printf("del connection %d (conn=%x)\n", id, list);	// xxx
}

// авторизовать соединение (для API или DEV)
void auth_connection(int id) {
	struct CONN conn_tmp;
	conn_tmp.id = id;
	g_mutex_lock(&mutex_conn);	// блокировка мьютекса
	// ищем в списке существующее соединение
	GList *list = g_list_find_custom(list_conn, &conn_tmp, func_find);
	//struct CONN *conn_del = (struct CONN *)list->data;
	// если нашли, то удаляем
	if (list) {
		struct CONN *conn = (struct CONN*) list->data;
		conn->is_login = TRUE;
	}
	g_mutex_unlock(&mutex_conn);	// разблокировка мьютекса
	printf("auth connection %d\n", id);	// xxx
}

// деавторизовать соединение (для API)
void deauth_connection(int id) {
	struct CONN conn_tmp;
	conn_tmp.id = id;
	g_mutex_lock(&mutex_conn);	// блокировка мьютекса
	// ищем в списке существующее соединение
	GList *list = g_list_find_custom(list_conn, &conn_tmp, func_find);
	//struct CONN *conn_del = (struct CONN *)list->data;
	// если нашли, то удаляем
	if (list) {
		struct CONN *conn = (struct CONN*) list->data;
		conn->is_login = FALSE;
	}
	g_mutex_unlock(&mutex_conn);	// разблокировка мьютекса
	printf("auth connection %d\n", id);	// xxx
}

// проверка авторизации соединения (для API или DEV)
gboolean is_auth_connection(int id) {
	gboolean ret = FALSE;
	struct CONN conn_tmp;
	conn_tmp.id = id;
	g_mutex_lock(&mutex_conn);	// блокировка мьютекса
	// ищем в списке существующее соединение
	GList *list = g_list_find_custom(list_conn, &conn_tmp, func_find);
	//struct CONN *conn_del = (struct CONN *)list->data;
	// если нашли
	if (list) {
		struct CONN *conn = (struct CONN*) list->data;
		ret = conn->is_login;
	}
	g_mutex_unlock(&mutex_conn);	// разблокировка мьютекса
	return ret;
}

/*
 #define TCP_RETR2       5
 #define TCP_SYN_RETRIES  3
 #define TCP_SYNACK_RETRIES 3
 #define TCP_ORPHAN_RETRIES 5
 #define TCP_TIMEWAIT_LEN (5*HZ)

 #define TCP_PAWS_MSL    5
 #define TCP_KEEPALIVE_PROBES    5
 */

// остановить все соединения
void stop_all_connections(void) {
	g_mutex_lock(&mutex_conn);	// блокировка мьютекса
	// останавливаем основные серверные сокеты на приём новых соединений
	GList *list = list_conn;
	while (list) {
		struct CONN *conn = (struct CONN *) list->data;
		if (conn->type_conn == TYPE_SERVER) {
			int retsopt = 2;//setsockopt(conn->id, SOL_SOCKET, SO_REUSEADDR, &a, sizeof(int));
			int retsh = s_close(conn->id, 100);	// shutdown(conn->id, SHUT_RDWR);

			printf("stop1 %d (type_conn=%d) retsopt=%d retclose=%d\n", conn->id,
					conn->type_conn, retsopt, retsh);	// xxx
			//SOCKET sockfd;
			//int port = GPOINTER_TO_INT(conn->info);
			//g_usleep(10*1000);// задержка в микросекундах, 10 мс.
			// пытаемся подключиться к закрытому сокету чтобы сработал accept
			//s_connect("localhost",port,&sockfd);
			// закрываем этот соект
			//retcs = closesocket(sockfd);
			//printf("closesocket2 id=%d ret=%d err=%d\n",conn->id, retcs, errno);// xxx
		}
		list = g_list_next(list);

	}
	// закрываем (на чтение) открытые клиентские сокеты
	list = list_conn;
	while (list) {
		struct CONN *conn = (struct CONN *) list->data;
		if (conn->type_conn != TYPE_SERVER) {
			//linger l = { 1, 0 };
			//int retsopt = //setsockopt(conn->id, SOL_SOCKET, SO_LINGER, &l, sizeof(linger));
			int retsopt = 2;//setsockopt(conn->id, SOL_SOCKET, SO_REUSEADDR, &a, sizeof(int));
			//int retsh = closesocket(conn->id);//shutdown(conn->id,SHUT_RDWR);//closesocket(conn->id);//
			int retsh = s_close(conn->id, 100);	//shutdown(conn->id,SHUT_RD);//closesocket(conn->id);//
			printf("stop2 %d (type_conn=%d) retsopt=%d retclose=%d\n", conn->id,
					conn->type_conn, retsopt, retsh);			// xxx
			// Делаем сокет неблокирующим
			/*int arg=fcntl(conn->id, F_GETFL);
			 printf("id=%d arg=%d(%x) err=%d\n",conn->id, arg, arg, errno);// xxx
			 arg |= O_NONBLOCK;
			 //arg&= (~O_NONBLOCK);
			 arg = fcntl(conn->id, F_SETFL, arg);
			 // EBADF== 9 (Bad file number)
			 //const char *test = "1";
			 //send(conn->id,test,1,0);
			 */
		}
		list = g_list_next(list);
	}
	g_mutex_unlock(&mutex_conn);			// разблокировка мьютекса

	printf("pause 0,1 sec\n");
	g_usleep(100 * 1000);

	// Ожидание завершения потоков
	while (1) {
		static int id_prev = -1;
		int id = -1;
		pthread_t thread = 0;// идентификатор потока, из которого добавлено соединение
		g_mutex_lock(&mutex_conn);			// блокировка мьютекса
		list = list_conn;
		if (list) {
			struct CONN *conn = (struct CONN *) list->data;
			if (conn) {
				thread = conn->thread;
				id = conn->id;

			}
		}
		g_mutex_unlock(&mutex_conn);			// разблокировка мьютекса
		// кончились потоки для ожидания
		if (!thread)
			break;
		// повторно зашли в этот поток, почему-то он не удалил свою запись, сделаем это сейчас
		if (id == id_prev) {
			// удалить активное соединение
			del_connection(id);
		}

		printf("start wait thread %ld (id=%d)\n", thread, id);		// xxx
		// ждём окончания потока сервера
		pthread_join(thread, NULL);
		printf("end wait thread %ld (id=%d)\n", thread, id);		// xxx
		id_prev = id;
	}
	printf("\nend wait threads!!! \n");		// xxx
}
