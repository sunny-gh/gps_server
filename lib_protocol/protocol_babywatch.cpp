// protocol_babywatch.с : Работа с протоколом Baby Watch Q90 внутри сервера
//

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <pthread.h>
//#include <zlib.h>
#include <glib.h>
#include "../gps_server.h"
#include "protocol.h"
#include "protocol_babywatch.h"
#include "../base_lib.h"
#include "crc16.h"
#include "../my_time.h"

#define TIMEOUT_MS	5000		//timeout - в миллисекундах

// Чтение из сокета согласно протоколу BABYWATCH
// newsockfd - сокет, из которого читаем
// [out] buf_out - строка, в которой будет прянятое сообщение, выделяем память
// [out] buf_out_len - размер считанных данных, в байтах
// timeout_ms - в миллисекундах
static void read_babywatch(SOCKET newsockfd, char **buf_out, int *buf_out_len,
		int timeout_ms) {
	// пока что ничего не считали
	if (buf_out_len)
		*buf_out_len = 0;
	if (!buf_out)
		return;
	// пример = [SG*8800000015*000D*LK,50,100,100]
	guint msg_len = 0;
	char *buf_in = (char*) g_malloc0(21);
	// читаем первые 20 байт
	int ret = s_recv(newsockfd, (unsigned char *) buf_in, 20, timeout_ms);
	if (ret != 20) {
		g_free(buf_in);
		return;
	}
	// длина сообщения
	msg_len = (guint) g_ascii_strtoull(buf_in + 15, NULL, 16) + 20 + 1; // длина сообщения  + первые уже полученные 20 байт + заключительный байт со скобкой
	// проверяем наличие открывающей скобки
	if (msg_len > 1000 || buf_in[0] != '[' || buf_in[3] != '*'
			|| buf_in[14] != '*' || buf_in[19] != '*') {
		g_free(buf_in);
		return;
	}
	// принятый буфер
	buf_in = (char*) g_realloc(buf_in, 20 + msg_len + 1);
	if (!buf_in)
		return;
	// число принятых байт
	guint buf_in_recv = 20;
	do {
		// читаем 1 байт
		ret = s_recv(newsockfd, (unsigned char *) (buf_in + buf_in_recv),
				msg_len - buf_in_recv, timeout_ms);
		if (ret <= 0) {
			break;
		}
		buf_in_recv += ret;
	} while (buf_in_recv < msg_len);
	// удалось ли до конца прочитать сообщение
	if (buf_in_recv != msg_len || *(buf_in + msg_len - 1) != ']') {
		g_free(buf_in);
		return;
	}
	// ставим метку о конце строки
	if (buf_in_recv > 0)
		buf_in[buf_in_recv] = 0;
	if (buf_out)
		*buf_out = buf_in;
	if (buf_out_len)
		*buf_out_len = buf_in_recv;
}

// пераметры для передачи в дочерний поток
typedef struct _THREAD_RESEND_ARGS_ {
	char *imei;
	SOCKET sockfd_client;// соединение с клиентом, которому будем отправлять ответы
	SOCKET sockfd_resend;// соединение с сервером, от которого будем ждать данные
} THREAD_RESEND_ARGS;

// потоковая функция ожидания пакетов от китайского сервера
static void* thread_resend_func(void *vptr_args) {
	THREAD_RESEND_ARGS *args = (THREAD_RESEND_ARGS*) vptr_args;
	if (!args)
		return NULL;
	char *imei = args->imei;
	SOCKET sockfd_client = args->sockfd_client;	// соединение с клиентом, которому будем отправлять ответы
	SOCKET sockfd_resend = args->sockfd_resend;	// соединение с сервером, от которого будем ждать данные
	while (1)	//(is_valid_socket(newsockfd))
	{
		//int str_len;
		// ждём данных от клиента
		//printf("before s_pool\n");//xxx
		int is_data = s_pool(sockfd_resend, TIMEOUT_MS);
		//printf("after s_pool is_data=%d\n", is_data);//xxx
		// timeout
		if (!is_data) {
			continue;
		}
		// ошибка (м.б. сокет закрыт)
		if (is_data < 0)
			break;
		// принятая строка
		char *str_in = NULL;
		// число байт в принятой строке
		int str_in_recv = 0;
		// Чтение из сокета
		read_babywatch(sockfd_resend, &str_in, &str_in_recv, TIMEOUT_MS);
		printf("[thread_resend_func] [in] len=%u\nmsg='%s'\n", str_in_recv,
				(str_in) ? str_in : "null");
		// Если данных не пришло после того как poll() вернул не нулевой результат — это означает только обрыв соединения
		if (!str_in_recv) {
			g_free(str_in);
			break;
		}
		// id последней записи, добавленной в бд
		int64_t last_id = 0;
		if (str_in && str_in_recv > 0)		// не timeout и приняли что-то
				{
			// переслать сообщение клиенту
			// поиск активного DEV соединения для устройства imei
			// return: номер соединения(сокета), 0 - не нашли
			GMutex *mutex_w = NULL;
			int protocol = PROTOCOL_NA;
			uint16_t ver = 0;
			int sockfd = find_dev_connection(imei, &protocol, &ver, &mutex_w);
			// перенаправляем команду напрямую устройству
			{
				if (mutex_w)
					g_mutex_lock(mutex_w);
				s_send(sockfd, (unsigned char *) str_in, str_in_recv,
						TIMEOUT_MS);
				if (mutex_w)
					g_mutex_unlock(mutex_w);

			}
		}
		g_free(str_in);
	}
	g_free(imei);
	g_free(args);
	// закрыть соединения
	s_close(sockfd_resend, TIMEOUT_MS);
	s_close(sockfd_client, TIMEOUT_MS);
	return NULL;
}

// переслать команду дальше и получить ответ
// return: полученная команда или NULL в случае ошибки
static char* resend_cmd(struct PACKET_INFO *packet_info)			//
		{
	//для приложения WhereYouGo правильный сервер pw, 123456, ip, 58.96.177.100, 7755
	//для приложения SeTracker:
	// Европа и Африка 52.28.132.157
	// Азия и Океания 54.169.10.136
	// Северная Америка 54.153.6.9
	// Южная Америка 54.207.93.14
	// Гонконг 58.96.181.173 (agpse.3g - elec.com)

	const char *server = "52.28.132.157";
	int port = 8001;
	int timeout = TIMEOUT_MS;	// таймаут 5 секунд
	// пересылать ли команды дальше
	if (!packet_info || !packet_info->resend_is_cmd)
		return NULL;
	SOCKET resend_sockfd = packet_info->resend_sockfd;
	struct BABYWATCH_PACKET *packet =
			(struct BABYWATCH_PACKET *) packet_info->packet;
	// установка соединения с китайским сервером
	if (resend_sockfd == INVALID_SOCKET || !is_valid_socket(resend_sockfd))	// сокет установленного соединения с китайским сервером для транслирования на него команд чтобы работал SeTracer
			{
		// return: eStatusOK=0, а если ошибка - то другое
		int status = s_connect(server, port, &resend_sockfd, timeout);
		if (status != eStatusOK) {
			char *err_str = get_err_text(status);
			printf("s_connect status=%d (%s)\n", status, err_str);
			g_free(err_str);
			return NULL;
		}
		packet_info->resend_sockfd = resend_sockfd;
		// заполняем структуру с параметрами для передачи в дочерний поток
		THREAD_RESEND_ARGS *args = (THREAD_RESEND_ARGS*) g_malloc(
				sizeof(THREAD_RESEND_ARGS));
		args->imei = g_strdup(packet_info->imei);
		args->sockfd_resend = resend_sockfd;
		// Дочерний поток
		pthread_t threadId;
		pthread_create(&threadId, NULL, thread_resend_func, (void*) args);
		// чтобы поток не оставался в состоянии dead после завершения
		pthread_detach(threadId);

	}
	// отсылаем данные на сервер
	unsigned char *msg = (unsigned char *) packet->raw_msg;
	int mgs_len = strlen(packet->raw_msg);
	int ret = s_send(resend_sockfd, msg, mgs_len, timeout);	// timeout - в милисекундах
	// ждём ответ сервера
	if (ret == mgs_len) {
		char *ret_msg = NULL;
		int ret_msg_len = 0;
		read_babywatch(resend_sockfd, &ret_msg, &ret_msg_len, timeout);
		if (ret_msg_len > 0)
			return ret_msg;
	}
	return NULL;
}

// обработать запрос
// return: id добавленной в базу записи
int64_t babywatch_process(struct PACKET_INFO *packet_info) {
	if (!packet_info)
		return 0;
	// пароль не используется
	packet_info->is_auth = TRUE;

	// записать пакет в БД
	int64_t id = 1;	//debug base_save_packet(PROTOCOL_BABYWATCH, packet_info);
	if (!id) {
		packet_info->is_parse = FALSE;
		printf("babywatch_process: base_save_packet type=%d fail!\n");
	} else {
		packet_info->is_parse = TRUE;
	}
	return id;
}

// подготовить ответ на запрос
// len_out - длина возвращаемых данных
char* babywatch_prepare_answer(struct PACKET_INFO *packet_info, int *len_out) {
	if (!packet_info)
		return NULL;
	char *str;
	// если пакет не был сохранён в базу, то не отвечаем  на запрос
	if (!packet_info->is_parse)
		str = NULL;
	else {
		struct BABYWATCH_PACKET *watch_packet =
				(struct BABYWATCH_PACKET*) packet_info->packet;
		// переслать команду дальше, если надо
		if (packet_info->resend_is_cmd)
			str = resend_cmd(packet_info);// &packet_info->resend_sockfd, watch_packet);
	}
	//str = g_strdup_printf("HTTP/1.1 200 OK\r\nServer: %s\r\nContent-Length: 2\r\n\r\nok", get_server_name());

	//packet_info->is_parse = FALSE;// разрываем соединение после отправки ответа
	if (len_out) {
		if (str)
			*len_out = strlen(str);
		else
			*len_out = 0;
	}
	return str;
}

// сформировать команду для отправки устройству
uint8_t* babywatch_prepare_cmd_to_dev(const char *cmd_text, int *len_out) {
	return NULL;
}

// записать пакет в БД
int64_t base_save_packet_babywatch(void *db, struct PACKET_INFO *packet_info) {
	//return TRUE;
	char *err_msg = NULL;
	if (!db)
		return 0;
	int64_t id = 0;
	// проверка поддержки мультипотоковой работы библиотеки libpq
	//int smp = PQisthreadsafe();
	//base_create_table0();
	BABYWATCH_PACKET* packet = (BABYWATCH_PACKET*) packet_info->packet;
	if (!packet)
		return 0;
	char *query =
			g_strdup_printf(
					"INSERT INTO data_%s VALUES(DEFAULT, '%lld', '%lld', '%s', '%d', '%lf', '%lf', '%d', '%d', '%d', '%s') RETURNING id",
					packet_info->imei,
					(long long int) my_time_get_cur_msec2000(),
					(long long int) packet->jdate, packet_info->client_ip_addr,
					packet_info->client_port, packet->lat, packet->lon,
					packet->height, packet->speed, PACKET_TYPE_BABYWATCH_DATA,
					packet->raw_msg);
	id = base_exec_ret_id(db, query, &err_msg);
	g_free(query);
	g_free(err_msg);
	return id;
}

// преобразовать полученные данные в строку для передачи
// [in] id - id последней записи, добавленной в бд
// [out] n_rows - число строк на выходе
char* babywatch_get_data_str(int64_t id, struct PACKET_INFO *packet_info,
		int *n_rows) {
	char *one_str = NULL;
	if (!packet_info)
		return NULL;
	struct BABYWATCH_PACKET *packet =
			(struct BABYWATCH_PACKET*) packet_info->packet;
	if (n_rows)
		*n_rows = 0;	// пока ничего нет
	// time_save,time,ip,port,lat,lon,height,speed,type_pkt,raw_data
	one_str =
			g_strdup_printf(
					"%lld\001%lld\001%lld\001%s\001%d\001%lf\001%lf\001%d\001%d\001%d\001%s",
					id, (long long int) my_time_get_cur_msec2000(),
					(long long int) packet->jdate, packet_info->client_ip_addr,
					packet_info->client_port, packet->lat, packet->lon,
					packet->height, packet->speed, PACKET_TYPE_BABYWATCH_DATA,
					packet->raw_msg);
	if (n_rows)
		*n_rows = 1;	// одна строка на выходе
	return one_str;
}

