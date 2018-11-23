// server_thread_http.c: Основной поток http сервера
//

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#include <glib.h>
#include "gps_server.h"
#include "base_lib.h"
#include "server_thread_ports.h"
//#include "server_api.h"
#include "my_time.h"
#include "lib_net/net_func.h"
#include "lib_protocol/protocol_parse.h"

#define TIMEOUT_MS		5000	//timeout - в миллисекундах
#define TIMEOUT_MS_CLOSE	2000 // Сколько ждать подтверждения закрытия сокета с другой стороны

// Чтение из сокета до конца http заголовка ("\r\n\r\n")
// newsockfd - сокет, из которого читаем
// [out] buf_out - строка, в которой будет прянятое сообщение, выделяем память
// [out] buf_out_len - размер считанных данных, в байтах
static void read_to_end_http_header(SOCKET newsockfd, char **buf_out,
		int *buf_out_len) {
	if (!buf_out)
		return;
	int str_len = 0;
	gchar *str_in = NULL;
	// Чтение из сокета до конца строки (строка без заключительных символов"\r\n\r\n")
	str_in = s_get_next_str(newsockfd, &str_len, "\r\n\r\n", TRUE, TIMEOUT_MS);
	if (buf_out)
		*buf_out = str_in;
	if (buf_out_len)
		*buf_out_len = (*buf_out) ? strlen(*buf_out) : 0;
}

// получить GTM дату и время
// потом требует удаления g_date_time_unref()
static GDateTime* date_time_get_gmt(struct tm *timeptr) {
	GTimeZone *tz = g_time_zone_new_utc();
	GDateTime *datetime = g_date_time_new(tz, timeptr->tm_year + 1900,
			timeptr->tm_mon + 1, timeptr->tm_mday, timeptr->tm_hour,
			timeptr->tm_min, timeptr->tm_sec);
	g_time_zone_unref(tz);
	return datetime;
}

// получить строку с локальной датой и временем из UTC времени
// timeptr - локальное время
// строка потом требует удаления g_free()
static char* date_time_get_local_from_gmt_str(struct tm *timeptr) {
	//gchar *ansi = (gchar*)g_malloc(256);
	//strftime(ansi, 250, "%F", timeptr);//дата
	//ret_str = g_locale_to_utf8(ansi,-1,0,0,0);
	GDateTime *datetime = (timeptr) ? date_time_get_gmt(timeptr) : NULL;
	GDateTime *datetime_l = (datetime) ? g_date_time_to_local(datetime) : NULL;
	gchar *str = (datetime_l) ? g_date_time_format(datetime_l, "%x %X") : NULL;	//"%F %T"
	//gchar *str = g_date_time_format(datetime, "%X");

	if (datetime)
		g_date_time_unref(datetime);
	if (datetime_l)
		g_date_time_unref(datetime_l);
	return str;
}

/*/ получить местную дату и время
 // потом требует удаления g_date_time_unref()
 static  GDateTime* date_time_get_local(struct tm *timeptr)
 {
 GTimeZone *tz = g_time_zone_new_utc();// g_time_zone_new_local();
 GDateTime *datetime = g_date_time_new(tz,
 timeptr->tm_year + 1900,
 timeptr->tm_mon + 1,
 timeptr->tm_mday,
 timeptr->tm_hour,
 timeptr->tm_min,
 timeptr->tm_sec);
 g_time_zone_unref(tz);
 return datetime;
 }

 // получить строку с локальной датой и временем из местного времени
 // timeptr - локальное время
 // строка потом требует удаления g_free()
 static char* date_time_get_local_from_local_str(struct tm *timeptr)
 {
 //gchar *ansi = (gchar*)g_malloc(256);
 //strftime(ansi, 250, "%F", timeptr);//дата
 //ret_str = g_locale_to_utf8(ansi,-1,0,0,0);
 GDateTime *datetime = date_time_get_local(timeptr);
 GDateTime *datetime_l = g_date_time_to_local(datetime);
 gchar *str = g_date_time_format(datetime_l, "%x %X");//%F %T
 //gchar *str = g_date_time_format(datetime, "%X");

 g_date_time_unref(datetime);
 g_date_time_unref(datetime_l);
 return str;
 }*/

// перевести из градусов в строку
// ns_ew - символ (North South East West)
// hi_tocnost - секунды с точностью до 2 знаков(TRUE) или одного(FALSE)
static char *get_grad_str(double gr, gboolean hi_tocnost, const char *ns_ew) {
	int grad = (int) gr;
	double min = (gr - grad) * 60.;
	double sec = (min - (int) min) * 60.;
	if (hi_tocnost)
		return g_strdup_printf("%3.2d° %02d′ %05.2lf″%s", grad, (int) min, sec,
				ns_ew);	// °(degree) ⁰(superscrypt zero)
	return g_strdup_printf("%3.2d° %02d′ %04.1lf″%s", grad, (int) min, sec,
			ns_ew);
}

// перевести долготу из градусов в строку
// hi_tocnost - секунды с точностью до 2 знаков(TRUE) или одного(FALSE)
static char *get_lon_grad_str(double gr, gboolean hi_tocnost) {
	//int grad = (int)gr;
	const char *ns_ew;
	if (gr >= 0)	// East
		ns_ew = "В";	//_("E");
	else
		// West
		ns_ew = "З";		//_("W");
	return get_grad_str(fabs(gr), hi_tocnost, ns_ew);
}

// перевести широту из градусов в строку
// hi_tocnost - секунды с точностью до 2 знаков(TRUE) или одного(FALSE)
static char *get_lat_grad_str(double gr, gboolean hi_tocnost) {
	//int grad = (int)gr;
	const char *ns_ew;
	if (gr >= 0)		// North
		ns_ew = "С";		//_("N");
	else
		// South
		ns_ew = "Ю";		//_("S");
	return get_grad_str(fabs(gr), hi_tocnost, ns_ew);
}

// распарсить n сообщений с данными
static char* parse_get_data(const char *str_in) {
	if (!str_in)
		return NULL;
	GString *string = g_string_new("");
	char **str_records = g_strsplit((const gchar*) str_in, "\2", -1);
	int n_str = 0, n_val = 0;
	while (*(str_records + n_str)) {
		// перевод числа миллисекунд от 2000 года в дату/время
		struct tm tm;
		char **str_vals = g_strsplit((const gchar*) *(str_records + n_str),
				"\1", -1);
		int n_vals = g_strv_length(str_vals);
		if (n_vals != 11)
			break;
		// номер пакета
		//int64_t id = atoll((const gchar*)*(str_vals + 0));
		g_string_append(string, "id=");
		g_string_append(string, (const gchar*) *(str_vals + 0));
		// тип пакета
		//int type_pkt = atoi((const gchar*)*(str_vals + 9));
		g_string_append(string, "  тип пакета: ");
		g_string_append(string, *(str_vals + 9));
		g_string_append(string, "<br>\n");
		// время формирования пакета в устройстве
		int64_t time = atoll((const gchar*) *(str_vals + 2));
		my_time_msec2000_to_time(time, &tm);
		char* time_str = date_time_get_local_from_gmt_str(&tm);
		g_string_append(string, "время=");
		g_string_append(string, time_str);
		g_free(time_str);
		// время получения пакета от устройства
		int64_t time_save = atoll((const gchar*) *(str_vals + 1));
		my_time_msec2000_to_time(time_save, &tm);
		char* time_save_str = date_time_get_local_from_gmt_str(&tm);
		g_string_append(string, " (время save=");
		g_string_append(string, time_save_str);
		g_string_append(string, ")<br>\n");
		g_free(time_save_str);
		// ip адрес клиента и порт
		//char* ip = (char*)(*(str_vals + 3));
		//int port = atoi((const gchar*)*(str_vals + 4));
		g_string_append(string, "клиент: ip=");
		g_string_append(string, *(str_vals + 3));
		g_string_append(string, " port=");
		g_string_append(string, *(str_vals + 4));
		g_string_append(string, "<br>\n");
		// локаленезависимое преобразование в отличии от strtod и atof
		double lat = g_ascii_strtod((const gchar*) *(str_vals + 5), NULL);// широта
		double lon = g_ascii_strtod((const gchar*) *(str_vals + 6), NULL);// долгота
		char *lat_str = get_lat_grad_str(lat, TRUE);
		char *lon_str = get_lon_grad_str(lon, TRUE);
		g_string_append(string, "широта:");
		g_string_append(string, lat_str);
		g_string_append(string, " долгота:");
		g_string_append(string, lon_str);
		g_free(lat_str);
		g_free(lon_str);
		g_string_append(string, "<br>\n");
		// высота, целое число, в метрах
		//int height = atoi((const gchar*)*(str_vals + 7));
		g_string_append(string, "высота:");
		g_string_append(string, *(str_vals + 7));
		g_string_append(string, " м<br>\n");
		// скорость, в км/ч
		//int speed = atoi((const gchar*)*(str_vals + 8));
		g_string_append(string, "скорость:");
		g_string_append(string, *(str_vals + 8));
		g_string_append(string, " км/ч<br>\n");

		// сырое содержимое пакета
		//char* raw_data = (char*)(*(str_vals + 10)); 
		g_string_append(string, "raw:");
		g_string_append(string, *(str_vals + 10));
		g_string_append(string, "<br>\n");

		g_string_append(string, "<br>\n");
		g_strfreev(str_vals);
		n_str++;
	}
	g_strfreev(str_records);
	return g_string_free(string, FALSE);
}

// сформировать информацию на устройство dev_name
static gchar* get_dev_name_info(const char *imei) {
	char *str_dev = NULL;
	if (!imei)
		return NULL;
	int protocol_num = PROTOCOL_NA;
	// текущее сотояние устройства
	int sock = find_dev_connection(imei, NULL, NULL, NULL);
	const char *cur_state_str = (sock) ? "1" : "0";
	// прочитать поледний пакет из БД (по id, а не по времени)
	// max_count_str - сколько последних пакетов прочитать
	// если id=0, значит эту границу не учитываем
	// return: число прочитанных пакетов, -5 ошибка открытия БД
	char *out_str = NULL;
	int n = base_get_last_packets((char*) imei, 5, &out_str, &protocol_num);
	char *pkt_data_str = parse_get_data(out_str);
	// узнать имя протокола по номеру
	char *proto_name = get_proto_by_num(protocol_num);
	// узнать время последнего сеанса связи устройства по imei
	// return: время или -1 в случае ошибки, 0 в случае, если никогда не было сеанса связи
	int64_t time_from = base_get_dev_last_time((char*) imei);
	char *time_from_str = NULL;
	if (time_from > 0) {
		// перевод числа миллисекунд от 2000 года в дату/время
		struct tm tm;
		my_time_msec2000_to_time(time_from, &tm);
		time_from_str = date_time_get_local_from_gmt_str(&tm);
	}
	str_dev =
			g_strdup_printf(
					"<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\">\n"
							"<html>\n"
							"<head>\n"
							"<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\">\n"
							"</head>\n"
							"<body>\n"
							"Устройство: %s, протокол: %s<br>\n"
							"Состояние: %s %s<br>\n"
							"Последние %d сообщений:<br>\n<br>\n%s<br>\n"
							"</body>\n</html>\n", imei, proto_name,
					(sock) ?
							"В сети, подключено с" :
							"Не в сети, потеря связи в",
					(time_from_str) ? time_from_str : "-", n, pkt_data_str);
	g_free(out_str);
	g_free(pkt_data_str);
	return str_dev;
}

// подготовить ответ на http запрос
static gchar* prepare_http_answer(http_header *header, int *header_len) {
	// формирование ответа
	char *str_data_out =
			(header) ? get_dev_name_info(header->ret_code_bin_str + 1) : NULL;
	char *str_out = NULL;
	// Чтение из сокета до конца строки (строка без заключительных символов"\r\n\r\n")
	if (!str_data_out)
		str_out =
				g_strdup_printf(
						"HTTP/1.1 400 Bad Request\r\nServer: %s\r\nContent-Length: 11\r\n\r\nBad Request",
						get_server_name());
	else
		str_out =
				g_strdup_printf(
						"HTTP/1.1 200 OK\r\nServer: %s\r\nCache-Control: no-cache\r\nPragma: no-cache\r\nContent-Length: %d\r\n\r\n%s",
						get_server_name(), (int) strlen(str_data_out),
						str_data_out);
	if (header_len)
		*header_len = strlen(str_out);
	return str_out;
}

// потоковая функция для одного клиента
void* thread_client_func_http(void *vptr_args) {
	GMutex *mutex_rw = NULL;
	THREAD_ARGS *args = (THREAD_ARGS*) vptr_args;
	if (!args)
		return NULL;
	SOCKET newsockfd = args->newsockfd;
	char *client_ip_addr = args->client_ip_addr;
	uint16_t client_port = args->client_port;
	g_free(args);
	printf("start client http thread=%d\n", newsockfd);
	// читаем http запрос
	while (1) {
		// ждём данных от клиента
		int is_data = s_pool(newsockfd, TIMEOUT_MS);
		// timeout
		if (!is_data)
			break;
		// ошибка (м.б. сокет закрыт)
		if (is_data < 0)
			break;
		// принятая строка
		char *str_in = NULL;
		// число байт в принятой строке
		int str_in_recv = 0;
		// Чтение из сокета до конца http заголовка ("\r\n\r\n")
		read_to_end_http_header(newsockfd, &str_in, &str_in_recv);
		printf("[thread_client_func_http] [in] len=%u\nmsg=%s\n", str_in_recv,
				str_in);
		// Если данных не пришло после того как poll() вернул не нулевой результат — это означает только обрыв соединения
		if (!str_in_recv) {
			g_free(str_in);
			break;
		}
		// id последней записи, добавленной в бд
		int64_t last_id = 0;
		if (str_in && str_in_recv > 0)	// не timeout и приняли что-то
				{
			http_page page;
			page.raw_header = str_in;
			page.header_len = str_in_recv;
			// разбор http заголовка из str_in
			http_header *header = http_parse_header(&page);
			// подготовить ответ на запрос
			int str_ret_len = 0;
			gchar *str_ret = prepare_http_answer(header, &str_ret_len);
			if (str_ret) {
				s_send(newsockfd, (unsigned char *) str_ret, str_ret_len,
						TIMEOUT_MS);
				g_free(str_ret);
			}
			// удаление структуры http_header
			http_header_free(header);
			g_free(str_in);
		}
	}
	// закрыть соединение
	s_close(newsockfd, TIMEOUT_MS_CLOSE);
	// ждём 0,1 сек перед закрытием сокета для завершения отправки сообщений
	//g_usleep(100000);
	g_free(client_ip_addr);
	printf("exit client http thread=%d\n", newsockfd);
	return NULL;
}

