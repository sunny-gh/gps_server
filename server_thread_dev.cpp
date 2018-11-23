// server_thread_dev.cpp : Основной поток сервера, ожидающий клиентов устройств
//

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
//#include <zlib.h>
#include <glib.h>
#include "lib_protocol/protocol.h"
#include "base_lib.h"
#include "server_thread_ports.h"
#include "daemon/log.h"
#include "my_time.h"
#include "lib_net/net_func.h"
#include "lib_net/websocket_func.h"

#define TIMELINE_MS	(2*60*1000)	// время удержания соединения при отсутствии активности клиента - в миллисекундах (минуты*60*1000)
#define TIMEOUT_MS	5000		//timeout - в миллисекундах
#define TIMEOUT_MS_CLOSE	2000 // Сколько ждать подтверждения закрытия сокета с другой стороны

// Удаляем символы конца строки в конце сообщения
// return: новую длину строки
static int del_endstr(char *str) {
	if (!str)
		return 0;
	int len = strlen(str);
	while (len > 0) {
		if (str[len - 1] == '\r' || str[len - 1] == '\n') {
			str[len - 1] = 0;
			len--;
		} else
			break;
	}
	return len;
}

// Чтение из сокета согласно протоколу GT06
// newsockfd - сокет, из которого читаем
// [out] buf_out - строка, в которой будет прянятое сообщение, выделяем память
// [out] buf_out_len - размер считанных данных, в байтах
static void read_gt06(SOCKET newsockfd, char **buf_out, int *buf_out_len) {
	if (!buf_out)
		return;
//#if (G_BYTE_ORDER == G_LITTLE_ENDIAN)// работает только для архитектуры little endian (x86)
	uint16_t start_bits, stop_bits = GINT16_TO_LE(0x0a0d); // переводим из любой архитектуры в архитектуру little endian (G_BYTE_ORDER = G_LITTLE_ENDIAN)
	uint8_t msg_len = 0;
	char *buf_in = (char*) g_malloc0(3);
	// читаем первые 3 байта
	int ret = s_recv(newsockfd, (unsigned char *) buf_in, 3, TIMEOUT_MS); //(unsigned char *)
	start_bits = *(uint16_t*) buf_in; // стартовые биты
	msg_len = *(uint8_t*) (buf_in + 2) + 3 + 2; // длина сообщения + первые уже полученные 3 байта + дополнительные 2 байта отведены на стоп-биты
	// проверяем наличие старт-битов
	if (start_bits != 0x7878) {
		g_free(buf_in);
		return;
	}
	// принятый буфер
	buf_in = (char*) g_realloc(buf_in, 3 + msg_len);
	if (!buf_in)
		return;
	// число принятых байт
	int buf_in_recv = 3;
	do {
		// читаем 1 байт
		ret = s_recv(newsockfd, (unsigned char *) (buf_in + buf_in_recv),
				msg_len - buf_in_recv, TIMEOUT_MS);
		if (ret <= 0) {
			break;
		}
		buf_in_recv += ret;
	} while (buf_in_recv < msg_len);
	// удалось ли до конца прочитать сообщение
	if (buf_in_recv != msg_len
			|| *((uint16_t*) (buf_in + msg_len - 2)) != stop_bits) {
		g_free(buf_in);
		return;
	}
	*buf_out = buf_in;
	if (buf_out_len)
		*buf_out_len = buf_in_recv;
}

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
	msg_len = (guint) g_ascii_strtoull(buf_in + 15, NULL, 16) + 20 + 1;	// длина сообщения  + первые уже полученные 20 байт + заключительный байт со скобкой
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

// Чтение из сокета до конца сообщения
// newsockfd - сокет, из которого читаем
// [out] buf_out - строка, в которой будет прянятое сообщение, выделяем память
// [out] buf_out_len - размер считанных данных, в байтах
static void read_to_end(SOCKET newsockfd, char **buf_out, int *buf_out_len) {
	gboolean is_data;
	if (!buf_out)
		return;
	// число принятых байт
	int buf_in_recv = 0;
	// число выделенных байт в принятой строке
	int buf_in_len = 256;
	// принятый буфер
	char *buf_in = (char*) g_malloc0(buf_in_len);
	if (!buf_in)
		return;
	do {
		// читаем 1 байт
		int ret = s_recv(newsockfd, (unsigned char *) (buf_in + buf_in_recv), 1,
				TIMEOUT_MS);
		if (ret <= 0) {
			break;
		}
		//printf("%02x",*(lpszBuffer + nRecv));
		while ((buf_in_recv + 1) >= buf_in_len) {
			buf_in_len *= 2;
			buf_in = (char*) g_realloc(buf_in, buf_in_len);
		}
		buf_in_recv++;
		// проверяем, есть ли ещё символы для чтения
		is_data = s_pool(newsockfd, 50);// на последнем символе ждём 50 мс, чтобы пакет был целым, а не разбивался на несколько сообщений
	} while (is_data > 0);

	buf_in = (char*) g_realloc(buf_in, buf_in_recv + 1);
	// ставим метку о конце строки
	if (buf_in_recv > 0)
		buf_in[buf_in_recv] = 0;
	*buf_out = buf_in;
	if (buf_out_len)
		*buf_out_len = buf_in_recv;
}

// Чтение из сокета до конца строки ("\r\n"=0x"0d0a" или "\n"=0x"0a") 
// newsockfd - сокет, из которого читаем
// [out] buf_out - строка, в которой будет прянятое сообщение, выделяем память
// [out] buf_out_len - размер считанных данных, в байтах
static void read_to_new_str(SOCKET newsockfd, char **buf_out,
		int *buf_out_len) {
	int str_len = 0;
	if (!buf_out)
		return;
	// Чтение из сокета до конца строки (строка без с "\r\n")
	gchar *str_in = s_get_next_line(newsockfd, &str_len, TIMEOUT_MS);
	*buf_out = str_in;
	if (buf_out_len)
		*buf_out_len = str_len;
}

// Чтение из сокета до конца http заголовка ("\r\n\r\n")
// newsockfd - сокет, из которого читаем
// [out] buf_out - строка, в которой будет прянятое сообщение, выделяем память
// [out] buf_out_len - размер считанных данных, в байтах
static void read_to_end_http_header(SOCKET newsockfd, char **buf_out,
		int *buf_out_len) {
	if (!buf_out)
		return;
	//GString *string = g_string_new("");
	int str_len = 0;
	gchar *str_in = NULL;
	// Чтение из сокета до конца строки (строка без заключительных символов"\r\n\r\n")
	str_in = s_get_next_str(newsockfd, &str_len, "\r\n\r\n", TRUE, TIMEOUT_MS);
	/*/ цикл чтения всех строк заголовка	
	 do
	 {
	 int str_len = 0;
	 g_free(str_in);
	 // Чтение из сокета до конца строки (строка вместе с "\r\n")
	 //str_in = s_get_next_str(newsockfd, &str_len, "\r\n", FALSE, TIMEOUT_MS);
	 str_in = s_get_next_line_rn(newsockfd, &str_len, TIMEOUT_MS);
	 if (str_in)
	 g_string_append(string, str_in);
	 } 
	 while (str_in && strlen(str_in) > 0);
	 if (buf_out) *buf_out = g_string_free(string, FALSE);
	 else					g_string_free(string, TRUE);*/
	if (buf_out)
		*buf_out = str_in;
	if (buf_out_len)
		*buf_out_len = (*buf_out) ? strlen(*buf_out) : 0;
}

// сообщить клиентам по API, что их устройство подключилось/отключилось
// packet_info - содержимое принятого пакета
// state - TRUE-подключилось/FALSE-отключилось
static gboolean send_online_to_api_conn(gboolean state,
		PACKET_INFO *packet_info) {
	if (packet_info->is_auth && packet_info->is_parse && packet_info->imei) {
		// установить новое время последнего сеанса связи для устройства
		int64_t time_new = my_time_get_cur_msec2000();
		{
			char *err_msg_out = NULL;
			// return: TRUE или FALSE (в этом случае будет установлена строка ошибки:err_msg_out, удалять через g_free(err_msg_out))
			if (!base_set_new_dev_time(packet_info->imei, time_new,
					&err_msg_out)) {
				printf("can't set new time for %s err=%s\n", packet_info->imei,
						err_msg_out);
			}
		}

		GMutex *mutex_rw = NULL;
		// поиск активных API соединений, в которых у пользователя есть устройство imei
		int sockfd, sockfd_prev = 0;
		do {
			char *user_name = NULL;	// пользователей может быть несколько!!!
			// поиск активного API соединения, в котором у пользователя есть устройство imei
			sockfd = find_api_connection(&sockfd_prev, packet_info->imei,
					&user_name, &mutex_rw);
			sockfd_prev = sockfd;
			// шлём сообщение клиенту API
			if (sockfd && mutex_rw) {
				// подготовить ответ пользователю с выбранными точками в формате json:
				char *str_send = online_prepare_data(state, time_new,
						packet_info);// {"action":"updatestate", "imei":xx, "time":xx, "state":TRUE/FALSE=>in/out }
				ws_send_mesage_text(sockfd, str_send, mutex_rw, TIMEOUT_MS);
				g_free(str_send);
			}
			g_free(user_name);
		} while (sockfd);
		return TRUE;
	}
	return FALSE;
}

// сообщить клиентам по API, что их устройство прислало данные
// packet_info - содержимое принятого пакета
// last_id - id последней записи, добавленной в бд
static gboolean send_data_to_api_conn(int64_t last_id,
		PACKET_INFO *packet_info) {

	if (packet_info->is_auth && packet_info->is_parse) {
		GMutex *mutex_rw = NULL;
		// поиск активных API соединений, в которых у пользователя есть устройство imei
		int sockfd, sockfd_prev = 0;
		do {
			char *user_name = NULL;		// пользователей может быть несколько!!!
			// поиск активного API соединения, в котором у пользователя есть устройство imei
			sockfd = find_api_connection(&sockfd_prev, packet_info->imei,
					&user_name, &mutex_rw);
			sockfd_prev = sockfd;
			// шлём сообщение клиенту API
			if (sockfd && mutex_rw) {
				// подготовить ответ пользователю с выбранными точками в формате json:
				char *str_send = packet_prepare_data(last_id, packet_info);
				//char *str_send = g_strdup_printf("{\"action\":\"update_need\", \"imei\":\"%s\"}", packet_info->imei);
				ws_send_mesage_text(sockfd, str_send, mutex_rw, TIMEOUT_MS);
				g_free(str_send);
			}
			g_free(user_name);
		} while (sockfd);
		return TRUE;
	}
	return FALSE;
}

// потоковая функция сервера для одного клиента
void* thread_client_func_dev(void *vptr_args) {
	THREAD_ARGS *args = (THREAD_ARGS*) vptr_args;
	if (!args)
		return NULL;
	SOCKET newsockfd = args->newsockfd;
	char *client_ip_addr = args->client_ip_addr;
	uint16_t client_port = args->client_port;
	uint16_t server_port = args->server_port;
	// пришёл ли первый пакет от клиента
	gboolean is_connect = FALSE;
	// протокол клиентов в этом потоке
	int packet_protocol = get_protocol_num_by_port(server_port);
	int cur_timeout_count = 0, max_timeout_count = TIMELINE_MS / TIMEOUT_MS;
	//g_free(args->client_ip_addr); - перенесётся в packet_info->client_ip_addr
	g_free(args);
	printf(
			"[thread_client_func_dev] start client thread=%d serv_port=%d proto=%d\n",
			newsockfd, server_port, packet_protocol);
	WriteLog(
			"[thread_client_func_dev] start client thread=%d serv_port=%d proto=%d\n",
			newsockfd, server_port, packet_protocol);
	// Не плодить соединения от одного и того же ip-адреса (устройства)
	{
		// Найти соединения с этого же ip
		SOCKET prev_sockfd = find_dev_connection_by_ip(client_ip_addr);
		// завершаем предыдущее соединение
		if (prev_sockfd > 0) {
			// закрыть предыдущее соединение
			int cs = s_close(prev_sockfd, TIMEOUT_MS_CLOSE);
			printf("[thread_client_func_dev] prev_sockfd=%d shutdown=%d\n",
					prev_sockfd, cs);
			WriteLog("[thread_client_func_dev] prev_sockfd=%d shutdown=%d\n",
					prev_sockfd, cs);
		}
	}
	// создаём выходную структуру
	PACKET_INFO *packet_info = (PACKET_INFO *) g_malloc0(
			sizeof(struct PACKET_INFO));
	GMutex *mutex_w = NULL;
	packet_info->client_ip_addr = client_ip_addr;
	packet_info->client_port = client_port;
	packet_info->protocol = packet_protocol;
	packet_info->resend_sockfd = INVALID_SOCKET;
	if (packet_info->protocol == PROTOCOL_BABYWATCH) {
		// TODO - считать это их базы
		packet_info->resend_is_cmd = TRUE;
	}

	// удалить все предыдущие соединения с этого IP
	// добавить активное соединение
	add_connection((int) newsockfd, (gpointer) packet_info, TYPE_DEV, &mutex_w);
	while (1)	//(is_valid_socket(newsockfd))
	{
		//int str_len;
		// ждём данных от клиента
		//printf("before s_pool\n");//xxx
		int is_data = s_pool(newsockfd, TIMEOUT_MS);
		//printf("after s_pool is_data=%d\n", is_data);//xxx
		// timeout
		if (!is_data) {
			cur_timeout_count++;
			// прерывание сеанса связи в связи с отсутсвием активности клиента
			if (cur_timeout_count >= max_timeout_count) {
				printf(
						"[thread_client_func_dev] [timeout dev] %d*%d ms -> break\n",
						cur_timeout_count, TIMEOUT_MS);
				WriteLog(
						"[thread_client_func_dev] [timeout dev] %d*%d ms -> break\n",
						cur_timeout_count, TIMEOUT_MS);
				break;
			}
			continue;
		}
		//printf("dev s_pool ret=%d\n", is_data);
		// сброс числа таймаутов
		cur_timeout_count = 0;
		// ошибка (м.б. сокет закрыт)
		if (is_data < 0)
			break;
		// принятая строка
		char *str_in = NULL;
		// число байт в принятой строке
		int str_in_recv = 0;
		// Чтение из сокета
		if (packet_info->protocol == PROTOCOL_WIALON) {
			// Чтение из сокета до конца строки ("\r\n"=0x"0d0a" или "\n"=0x"0a") 
			read_to_new_str(newsockfd, &str_in, &str_in_recv);
			// Чтение из сокета до конца сообщения
			//read_to_end(newsockfd, &str_in, &str_in_recv);
			// Удаление конца строки
			//str_in_recv = del_endstr(str_in);

		} else if (packet_info->protocol == PROTOCOL_OSMAND)
			// Чтение из сокета до конца http заголовка ("\r\n\r\n")
			read_to_end_http_header(newsockfd, &str_in, &str_in_recv);
		else if (packet_info->protocol == PROTOCOL_GT06) {
			read_gt06(newsockfd, &str_in, &str_in_recv);
		} else if (packet_info->protocol == PROTOCOL_BABYWATCH) {
			read_babywatch(newsockfd, &str_in, &str_in_recv, TIMEOUT_MS);
		} else
			// Чтение из сокета до конца сообщения
			read_to_end(newsockfd, &str_in, &str_in_recv);

		//printf("before s_get_next_line\n");//xxx
		//++gchar *str_in = s_get_next_line(newsockfd, &str_len, TIMEOUT_MS);
		//printf("after s_get_next_line\n");//xxx
		//str_in = "#L#2.0;imei;N/A;BB2B\r\n";
		//str_in = "#L#355674053449792;NA";
		//str_in = g_strdup("#B#060516;175144;5355.09260;N;02732.40990;E;0;0;300;7;NA;0;0;;;uipp:2:NA,oouio:3:NAklkkl|060516;175211;5355.09260;N;02732.40990;E;0;0;300;7;NA;0;0;;;uipp:2:NA,oouio:3:NAklkkl|060516;175138;5355.09260;N;02732.40990;E;0;0;300;7");
		//str_in = g_strdup("#D#060516;213545;5508.24382600000007;N;06008.89144800000011;E;0;0;0;0;28.813;NA;NA;;NA;SOS:1:1");
		// 4500000033353536373430353334343937393200572f6d38000000010bffffffbb000000270102706f73696e666f00ffffffc6ffffffe1ffffffccffffffafffffffe6124e40ffffff8b321b64ffffff92ffffff914b403333333333137440000000ffffffd90a
		// http ://localhost:9002/?id=355674053449792&timestamp=1474673371&lat=55.173645&lon=61.402765&speed=0.13607339644369482&bearing=0.0&altitude=171.7&batt=87.0

		WriteLog("[thread_client_func_dev] [in] len=%u\nmsg='%s'\n",
				str_in_recv,
				(packet_info->protocol != PROTOCOL_GT06) ?
						str_in : "{bin data}");
		// вывод в hex виде
		//if (0)
		{
			printf("[in hex]={");
			int i;
			for (i = 0; i < str_in_recv; i++) {
				if (i && !(i % 50))
					printf("\n");
				printf("%02x", (guchar) str_in[i]);
			}
			printf("}\n");
		}
		//WriteLog("[thread_client_func_dev] [in] len=%d\nmsg=%s\n", str_in_recv, str_in);
		// Если данных не пришло после того как poll() вернул не нулевой результат — это означает только обрыв соединения
		if (!str_in_recv) {
			g_free(str_in);
			break;
		}
		/*/ добавить torque данные в базу
		 int n = 0;
		 if (0)
		 {
		 while (1)
		 {
		 str_in = (char*)msga[n];
		 if (!str_in)
		 break;
		 str_in_recv = strlen(str_in);
		 // разбор пакета из str_in
		 if (packet_parse(str_in, str_in_recv, packet_info))
		 {
		 // обработать запрос и получить id последней записи, добавленной в бд
		 int last_id = packet_process(packet_info);
		 if (!last_id)
		 packet_info->is_parse = FALSE;// не сохранённый пакет, рвём соединение
		 }

		 // очистить часть PACKET_INFO - содержимое пакета
		 clean_packet_info(packet_info);
		 n++;
		 }
		 }*/

		// id последней записи, добавленной в бд
		int64_t last_id = 0;
		if (str_in && str_in_recv > 0)		// не timeout и приняли что-то
				{
			//wialon_info->is_auth = TRUE;
			// разбор пакета из str_in
			if (packet_parse(str_in, str_in_recv, packet_info)) {
				// обработать запрос и получить id последней записи, добавленной в бд
				last_id = packet_process(packet_info);
				if (!last_id)
					packet_info->is_parse = FALSE;// не сохранённый пакет, рвём соединение
			}
			//!!! временно считаем пакет нормальным
			//if (packet_info->protocol == PROTOCOL_OSMAND)
			//	packet_info->is_parse = TRUE;// не сохранённый пакет, рвём соединение

			// подготовить ответ на запрос устройству
			int str_ret_len = 0;
			gchar *str_ret = packet_prepare_answer(packet_info, &str_ret_len);
			WriteLog("[thread_client_func_dev] [out %d byte]\n%s\n",
					str_ret_len,
					(packet_info->protocol != PROTOCOL_GT06) ?
							str_ret : "{bin data}");
			//WriteLog("[thread_client_func_dev] [out]\n%s\n", str_ret);
			if (str_ret) {
				//printf("[thread_client_func_dev] mutex_w=%x\n", mutex_w);
				if (mutex_w)
					g_mutex_lock(mutex_w);
				s_send(newsockfd, (unsigned char *) str_ret, str_ret_len,
						TIMEOUT_MS);
				if (mutex_w)
					g_mutex_unlock(mutex_w);
				g_free(str_ret);
			}
			g_free(str_in);
			// сообщить клиентам по API, что их устройство подключилось
			{
				if (!is_connect)			// пришёл первый пакет от клиента
					is_connect = send_online_to_api_conn(TRUE, packet_info);
			}
			// сообщить клиентам по API, что их устройство прислало данные
			if (packet_info->is_parse)
				send_data_to_api_conn(last_id, packet_info);

			// очистить часть PACKET_INFO - удалить содержимое пакета
			clean_packet_info(packet_info);

			// пакет не распознался в packet_process() или просто решили разорвать соединение в packet_prepare_answer()
			if (!packet_info->is_parse)
				break;			// closesocket(newsockfd);
		}
	}
	// закрыть соединение
	int cs = s_close(newsockfd, TIMEOUT_MS_CLOSE);
	// закрыть соединение для пересылки данных
	if (packet_info->resend_sockfd != INVALID_SOCKET)
		cs = s_close(packet_info->resend_sockfd, TIMEOUT_MS_CLOSE);
	// сообщить клиентам по API, что их устройство отключилось
	send_online_to_api_conn(FALSE, packet_info);
	//printf("before exit client thread=%d cs=%d\n", newsockfd,cs);
	base_save_exit(packet_info);
	// удалить структуру PACKET_INFO
	free_packet_info(packet_info);
	// удалить активное соединение, если сокет был закрыт нормально
	del_connection((int) newsockfd);
	printf("[thread_client_func_dev] exit client thread socket=%d cs=%d\n",
			newsockfd, cs);
	WriteLog("[thread_client_func_dev] exit client thread socket=%d cs=%d\n",
			newsockfd, cs);
	return NULL;
}

