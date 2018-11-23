// server_thread_api.cpp : Основной поток сервера, ожидающий клиентов
//

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <glib.h>
#include "gps_server.h"
#include "base_lib.h"
#include "server_thread_ports.h"
#include "server_api.h"
#include "zlib_util.h"
#include "lib_net/net_func.h"
#include "lib_net/websocket_func.h"

#define TIMEOUT_MS		5000	//timeout - в миллисекундах
//#define TIMEOUT_POOL_MS	2000	//timeout - в миллисекундах
#define TIMEOUT_MS_CLOSE	2000 // Сколько ждать подтверждения закрытия сокета с другой стороны

// потоковая функция для одного клиента
void* thread_client_func_api(void *vptr_args) {
	// первоначальное создание таблиц в БД
	/*if (0) {
		//gboolean ret0 = base_create_user("UserName", "1", "UserFullName", 2, NULL,UserEmailAddr", NULL); // создать пользователя
		printf("base_create_user = %d", ret0);
	}*/
	GMutex *mutex_rw = NULL;
	THREAD_ARGS *args = (THREAD_ARGS*) vptr_args;
	if (!args)
		return NULL;
	SOCKET newsockfd = args->newsockfd;
	char *client_ip_addr = args->client_ip_addr;
	uint16_t client_port = args->client_port;
	g_free(args);
	printf("start client api thread=%d\n", newsockfd);
	// добавить активное соединение
	add_connection((int) newsockfd, (gpointer) NULL, TYPE_API, &mutex_rw);
	// установка соединения
	http_header *header = NULL;
	gboolean is_connect = ws_server_handshake(newsockfd, get_server_name(),
			&header, TIMEOUT_MS);
	http_header_free(header);
	// читаем пакеты
	while (is_connect) {
		//printf("api: before s_pool\n");
		// ждём данных от клиента
		int is_data = s_pool(newsockfd, TIMEOUT_MS);
		//printf("api: after s_pool is_data=%d\n", is_data);
		// timeout
		if (!is_data)
			continue;
		// ошибка (м.б. сокет закрыт)
		if (is_data < 0)
			break;
		//int nRecv=0, lpszBuffer_len=10;
		// получаем сообщение от клиента
		struct WS_PACKET packet;
		memset(&packet, 0, sizeof(packet));
		//packet.need_read = 2;
		g_mutex_lock(mutex_rw);
		do {
			// читаем очередную порцию из пакета
			if (packet.need_read > 0) {
				int ret = s_recv(newsockfd,
						(unsigned char *) (packet.raw + packet.raw_len),
						(int) packet.need_read, TIMEOUT_MS);
				//printf("[in] %s\n", packet.raw + packet.raw_len);
				//printf("[in] %lld byte\n", packet.raw_len);
				if (ret == -1) {
					is_connect = FALSE;
					break;
				}
				if (ret > 0)
					packet.raw_len += ret;
				else
					break;
			}
			printf("[in] %llu byte\n", packet.raw_len);
			// разбираем очередную порцию из пакета и решаем, сколько ещё нужно считать
			is_data = ws_parse_packet(&packet);
			if (packet.data) {
				//printf("[in2] %s\n", packet.data);
				printf("[in2] %llu byte\n", packet.data_len);
			}
			if (!is_data)
				break;
			// добавляем выделенную память под новые данные
			packet.raw = (uint8_t*) g_realloc(packet.raw,
					(gsize)(packet.raw_len + packet.need_read));
		} while (packet.need_read > 0);
		g_mutex_unlock(mutex_rw);
		//printf("api: is_data=%d  packet.need_read=%lld packet.data_len=%lld\n", is_data, (long long)packet.need_read, (long long)packet.data_len);
		// если пакет с ошибкой
		if (!is_data || packet.need_read > 0) {
			//printf("api: start ws_packet_clean is_data=%d  packet.need_read=%lld\n", is_data, (long long)packet.need_read);
			ws_packet_clean(&packet);
			if (packet.raw_len > 0)
				continue;
			else
				break;
		}
		//ws_send_mesage_text(newsockfd, "привЕ\nткцу", TIMEOUT_MS);
		//printf("[msg]%s\n", packet.data);
		switch (packet.frame_type) {
		case WS_TYPE_PING:
			// послать в ответ сообщение PONG с тем же самым содержимым
			ws_send_mesage(newsockfd, WS_TYPE_PONG, packet.data,
					packet.data_len, mutex_rw, TIMEOUT_MS);
			break;
		case WS_TYPE_PONG:
			break;
		case WS_TYPE_CLOSE:
			is_connect = FALSE;
			// послать в ответ сообщение о закрытии соединения с тем же самым содержимым
			ws_send_mesage(newsockfd, WS_TYPE_CLOSE, packet.data,
					packet.data_len, mutex_rw, TIMEOUT_MS);
			break;
		case WS_TYPE_TEXT:
			// что-то прочитали
			if (packet.data && packet.data_len > 0) {
				//printf("[before api_prepare_answer] data=%x data_len=%d\n", packet.data, packet.data_len);
				// разобрать api запрос и подготовить ответ
				char *reply_str = api_prepare_answer((char*) packet.data,
						client_ip_addr, client_port, (int) newsockfd);
				//printf("[after api_prepare_answer] reply_str=%s data_len=%d\n", reply_str, packet.data_len);
				int rep_len = (reply_str) ? strlen(reply_str) : 0;// длина ответа
				/*if(packet.data)
				 {
				 printf("[out] %s\n", reply_str);
				 }*/
				// включить сжатие ответа, если его длина более 1кб и клиент поддерживает сжатие
				int is_compress = (rep_len > 200);
				// сжатие ответа
				if (is_compress) {
					uint8_t *dst;
					uint32_t dst_len_out;
					int err = zlib_compress((uint8_t *) reply_str, rep_len,
							&dst, &dst_len_out, 6);
					if (err == 0)				//Z_OK)
							{
						g_free(reply_str);
						reply_str = (char*) dst;
						rep_len = dst_len_out;
					} else
						is_compress = FALSE;

				}
				printf("[out] %d bytes\n", rep_len);//printf("[out] %s\n", reply_str);
				// отправить ответ
				if (rep_len && reply_str) {
					// если было сжатие, то отправляем двоичные данные
					if (is_compress)
						ws_send_mesage(newsockfd, WS_TYPE_BIN,
								(uint8_t*) reply_str, rep_len, mutex_rw,
								TIMEOUT_MS);
					// иначе отправляем текстовые данные
					else {
						//ws_send_mesage_text(newsockfd, reply_str, TIMEOUT_MS);
						ws_send_mesage(newsockfd, WS_TYPE_TEXT,
								(uint8_t*) reply_str, rep_len, mutex_rw,
								TIMEOUT_MS);
					}

					//s_send(newsockfd, (unsigned char *)reply_str, rep_len, TIMEOUT_MS);
					g_free(reply_str);
				}
			}
			break;
		default:
			break;
		}
		// разрыв соединения
		//else
		//break;
		//g_free(lpszBuffer);
		//break;
	}
	// послать сообщение о закрытии соединения, если эта сторона его закрывает
	if (is_connect)
		ws_send_mesage(newsockfd, WS_TYPE_CLOSE, NULL, 0, mutex_rw, TIMEOUT_MS);
	// ждём 0,1 сек перед закрытием сокета для завершения отправки сообщений
	g_usleep(100000);
	s_close(newsockfd, TIMEOUT_MS_CLOSE);
	// удалить активное соединение
	del_connection((int) newsockfd);
	g_free(client_ip_addr);
	printf("exit client api thread=%d\n", newsockfd);
	return NULL;
}

