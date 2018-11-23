// protocol_wialon_ips.с : Работа с протоколом Wialon IPS внутри сервера
//

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
//#include <zlib.h>
#include <glib.h>
#include "protocol.h"
#include "protocol_wialon_ips.h"
#include "../base_lib.h"
#include "crc16.h"
#include "../my_time.h"

// обработать запрос
// return: id добавленной в базу записи
int64_t wialon_process(struct PACKET_INFO *packet_info) {
	int64_t id = 0;
	if (!packet_info)
		return 0;
	struct WIALON_PACKET *packet = (struct WIALON_PACKET*) packet_info->packet;
	// если не было авторизации и это не авторизация, то не отвечаем  на запрос
	if (packet->type != PACKET_TYPE_WIALON_LOGIN
			&& packet->type != PACKET_TYPE_WIALON_VLOGIN
			&& !packet_info->is_auth)
		return 0;
	// проверяем контрольную сумму пакета
	if (packet_info->ver == 20 && packet->crc16 != packet->crc16_calc)
		return 0;
	switch (packet->type) {
	case PACKET_TYPE_WIALON_LOGIN:
	case PACKET_TYPE_WIALON_VLOGIN:
		// проверка логина и пароля
		if (base_check_login(packet->packet_l.imei, packet->packet_l.password))
			// пароль успешно прошёл проверку
			packet_info->is_auth = TRUE;
		break;
	}
	// записать пакет в БД
	id = base_save_packet(PROTOCOL_WIALON, packet_info);
	if (!id)
		printf("wialon_process: base_save_packet type=%d fail!\n",
				packet->type);
	return id;
}

// сформировать команду для отправки устройству
uint8_t* wialon_prepare_cmd_to_dev(const char *cmd_text, int *len_out) {
	// напрямую отправляем команду в устройство
	gchar *str = (cmd_text) ? g_strdup(cmd_text) : NULL;
	if (len_out) {
		if (str)
			*len_out = strlen(str);
		else
			*len_out = 0;
	}
	return (uint8_t*) str;
}

// подготовить ответ на запрос
// len_out - длина возвращаемых данных
gchar* wialon_prepare_answer(struct PACKET_INFO *packet_info, int *len_out) {
	if (!packet_info)
		return NULL;
	struct WIALON_PACKET *packet = (struct WIALON_PACKET*) packet_info->packet;
	// если не было авторизации и это не авторизация, то не отвечаем  на запрос
	if (packet->type != PACKET_TYPE_WIALON_LOGIN
			&& packet->type != PACKET_TYPE_WIALON_VLOGIN
			&& !packet_info->is_auth)
		return FALSE;
	gchar *str = NULL;
	switch (packet->type) {
	case PACKET_TYPE_WIALON_VLOGIN:
		// если был пакет с некорректным содержанием
		if (!packet_info->is_parse) {
			//“0” – если сервер отверг подключение
			// м.б. уже большое кол-во подключений или не работает сервис или просто не понравился клиент
			//str = g_strdup("#AL#0\r\n");
			str = g_strdup("#AVL#0\r\n");
			break;
		}
		if (packet_info->ver == 20 && (packet->crc16 != packet->crc16_calc)) {
			// “10” – если ошибка проверки контрольной суммы
			str = g_strdup("#AVL#10\r\n");
			packet_info->is_parse = FALSE;// разрываем соединение после отправки ответа
			break;
		}
		if (!packet_info->is_auth) {
			// “01” – если ошибка проверки пароля
			str = g_strdup("#AVL#01\r\n");
			packet_info->is_parse = FALSE;// разрываем соединение после отправки ответа
		} else {
			// пароль успешно прошёл проверку
			str = g_strdup("#AVL#1\r\n");
		}
		break;
	case PACKET_TYPE_WIALON_LOGIN:

		// если был пакет с некорректным содержанием
		if (!packet_info->is_parse) {
			//“0” – если сервер отверг подключение
			// м.б. уже большое кол-во подключений или не работает сервис или просто не понравился клиент
			//str = g_strdup("#AL#0\r\n");
			str = g_strdup("#AL#0\r\n");
			break;
		}
		if (packet_info->ver == 20 && (packet->crc16 != packet->crc16_calc)) {
			// “10” – если ошибка проверки контрольной суммы
			str = g_strdup("#AL#10\r\n");
			packet_info->is_parse = FALSE;// разрываем соединение после отправки ответа
			break;
		}
		if (!packet_info->is_auth) {
			// “01” – если ошибка проверки пароля
			str = g_strdup("#AL#01\r\n");
			packet_info->is_parse = FALSE;// разрываем соединение после отправки ответа
		} else {
			// пароль успешно прошёл проверку
			str = g_strdup("#AL#1\r\n");
		}
		break;

	case PACKET_TYPE_WIALON_PING:
		str = g_strdup("#AP#\r\n");
		break;
	case PACKET_TYPE_WIALON_DRIVER:
		if (packet_info->is_parse)
			str = g_strdup("#AM#0\r\n");
		else
			str = g_strdup("#AM#1\r\n");
		break;
	case PACKET_TYPE_WIALON_SHORT_DATA:
		if (!packet_info->is_parse) {
			str = g_strdup("#ASD#-1\r\n");
			break;
		}
		if (packet_info->ver == 20 && (packet->crc16 != packet->crc16_calc)) {
			// “13” – если ошибка проверки контрольной суммы
			str = g_strdup("#ASD#13\r\n");
			packet_info->is_parse = FALSE;// разрываем соединение после отправки ответа
			break;
		}
		// пакет успешно принят
		str = g_strdup("#ASD#1\r\n");
		break;

	case PACKET_TYPE_WIALON_DATA:
		if (!packet_info->is_parse) {
			str = g_strdup("#AD#-1\r\n");
			break;
		}
		if (packet_info->ver == 20 && (packet->crc16 != packet->crc16_calc)) {
			// “16” – если ошибка проверки контрольной суммы
			str = g_strdup("#AD#16\r\n");
			packet_info->is_parse = FALSE;// разрываем соединение после отправки ответа
			break;
		}
		// пакет успешно принят
		str = g_strdup("#AD#1\r\n");
		break;
	case PACKET_TYPE_WIALON_BLACK:
		if (!packet_info->is_parse) {
			str = g_strdup("#AB#0\r\n");
			break;
		}
		if (packet_info->ver == 20 && (packet->crc16 != packet->crc16_calc)) {
			// если ошибка проверки контрольной суммы
			str = g_strdup("#AB#\r\n");
			packet_info->is_parse = FALSE;// разрываем соединение после отправки ответа
			break;
		}
		// пакет успешно принят
		str = g_strdup_printf("#AB#%d\r\n", packet->packet_b.n_count);
		break;
	}
	if (len_out) {
		if (str)
			*len_out = strlen(str);
		else
			*len_out = 0;
	}
	return str;
}

// записать пакет в БД
// return: id добавленной в базу записи
int64_t base_save_packet_wialon(void *db, struct PACKET_INFO *packet_info) {
	//return TRUE;
	char *err_msg = NULL;
	if (!db)
		return FALSE;
	int64_t id = FALSE;
	// проверка поддержки мультипотоковой работы библиотеки libpq
	//int smp = PQisthreadsafe();
	//base_create_table0();
	WIALON_PACKET* packet = (WIALON_PACKET*) packet_info->packet;
	if (!packet)
		return 0;
	switch (packet->type) {
	case PACKET_TYPE_WIALON_LOGIN:
	case PACKET_TYPE_WIALON_VLOGIN:
	case PACKET_TYPE_WIALON_LOGIN2:
	case PACKET_TYPE_WIALON_VLOGIN2: {
		packet_L packet_l = packet->packet_l;
		char *query =
				g_strdup_printf(
						"INSERT INTO auth_log VALUES(DEFAULT, '%lld', '%s', '%s', '%d', '%d', '%s', '%s')",
						my_time_get_cur_msec2000(), packet_l.imei,
						packet_info->client_ip_addr, packet_info->client_port,
						packet_info->is_auth,
						(packet_info->ver == 11) ? "wialon 1.1" :
						(packet_info->ver == 20) ? "wialon 2.0" : NULL,
						packet->raw_msg);
		id = base_exec(db, query, &err_msg);
		g_free(query);
	}
		break;
	case PACKET_TYPE_WIALON_DRIVER:
	case PACKET_TYPE_WIALON_DRIVER2:
		if (packet_info->is_auth) {
			//packet_M packet_m = wialon_info->packet.packet_m;
			char *query =
					g_strdup_printf(
							"INSERT INTO data_%s VALUES(DEFAULT, '%lld', '%lld', '%s', '%d', '%lf', '%lf', '%d', '%d', '%d', '%s') RETURNING id",
							packet_info->imei,
							(long long int) my_time_get_cur_msec2000(),
							(long long int) my_time_get_cur_msec2000(),
							packet_info->client_ip_addr,
							packet_info->client_port, 0.0, 0.0, 0, 0,
							//(wialon_info->ver == 11) ? "wialon 1.1" : (wialon_info->ver == 20) ? "wialon 2.0" : NULL,
							packet->type, packet->raw_msg);
			id = base_exec_ret_id(db, query, &err_msg);
			g_free(query);
		}
		break;
	case PACKET_TYPE_WIALON_SHORT_DATA:
	case PACKET_TYPE_WIALON_DATA:
	case PACKET_TYPE_WIALON_SHORT_DATA2:
	case PACKET_TYPE_WIALON_DATA2:
		if (packet_info->is_auth) {
			packet_SD packet_sd =
					(packet->type == PACKET_TYPE_WIALON_DATA) ?
							packet->packet_d.packet_sd : packet->packet_sd;
			char *query =
					g_strdup_printf(
							"INSERT INTO data_%s VALUES(DEFAULT, '%lld', '%lld', '%s', '%d', '%lf', '%lf', '%d', '%d', '%d', '%s') RETURNING id",
							packet_info->imei,
							(long long int) my_time_get_cur_msec2000(),
							(long long int) packet_sd.jdate,
							packet_info->client_ip_addr,
							packet_info->client_port, packet_sd.lat,
							packet_sd.lon, packet_sd.height, packet_sd.speed,
							//(wialon_info->ver == 11) ? "wialon 1.1" : (wialon_info->ver == 20) ? "wialon 2.0" : NULL,
							packet->type, packet->raw_msg);
			id = base_exec_ret_id(db, query, &err_msg);
			g_free(query);
		}
		break;
	case PACKET_TYPE_WIALON_BLACK:
	case PACKET_TYPE_WIALON_BLACK2:
		if (packet_info->is_auth) {
			packet_B packet_b = packet->packet_b;
			int i;
			for (i = 0; i < packet_b.n_count; i++) {

				packet_D *packet_d = packet_b.packet_d + i;
				packet_SD packet_sd = packet_d->packet_sd;
				uint8_t type = PACKET_TYPE_NA;
				if (packet->type == PACKET_TYPE_WIALON_BLACK)
					type = (packet_d->type == PACKET_TYPE_WIALON_DATA) ?
							PACKET_TYPE_WIALON_DATA_FROM_BLACK :
							PACKET_TYPE_WIALON_SHORT_DATA_FROM_BLACK;
				if (packet->type == PACKET_TYPE_WIALON_BLACK2)
					type = (packet_d->type == PACKET_TYPE_WIALON_DATA2) ?
							PACKET_TYPE_WIALON_DATA_FROM_BLACK2 :
							PACKET_TYPE_WIALON_SHORT_DATA_FROM_BLACK2;
				//type = (packet_sd.type == PACKET_TYPE_WIALON_SHORT_DATA) ? PACKET_TYPE_WIALON_SHORT_DATA_FROM_BLACK : PACKET_TYPE_WIALON_DATA_FROM_BLACK;// то же самое
				char *query =
						g_strdup_printf(
								"INSERT INTO data_%s VALUES(DEFAULT, '%lld', '%lld', '%s', '%d', '%lf', '%lf', '%d', '%d', '%d', '%s') RETURNING id",
								packet_info->imei,
								(long long int) my_time_get_cur_msec2000(),
								(long long int) packet_sd.jdate,
								packet_info->client_ip_addr,
								packet_info->client_port, packet_sd.lat,
								packet_sd.lon, packet_sd.height,
								packet_sd.speed,
								//(wialon_info->ver == 11) ? "wialon 1.1" : (wialon_info->ver == 20) ? "wialon 2.0" : NULL,
								type, *(packet_b.raw_msg + i));
				id = base_exec_ret_id(db, query, &err_msg);
				g_free(query);
			}
		}
		break;	// пока что сохраняем все пакеты
	default:
		// неизвестный пакет, сохраняем на всякий случай посмотреть для отладки
	{
		char *query =
				g_strdup_printf(
						"INSERT INTO auth_log VALUES(DEFAULT, '%lld', '%s', '%s', '%d', '%d', '%d', '%s')",
						my_time_get_cur_msec2000(), packet_info->imei,
						packet_info->client_ip_addr, packet_info->client_port,
						packet_info->is_auth, packet_info->ver,
						packet->raw_msg);
		id = base_exec_ret_id(db, query, &err_msg);
		g_free(query);
	}
	}
	// закрытие базы данных PostgreSQL
	g_free(err_msg);
	return id;
}

// преобразовать полученные данные в строку для передачи
// [in] id - id последней записи, добавленной в бд
// [out] n_rows - число строк на выходе
char* wialon_get_data_str(int64_t id, struct PACKET_INFO *packet_info,
		int *n_rows) {
	char *one_str = NULL;
	if (!packet_info)
		return NULL;
	struct WIALON_PACKET *packet = (struct WIALON_PACKET*) packet_info->packet;
	if (n_rows)
		*n_rows = 0;	// пока ничего нет
	switch (packet->type) {
	case PACKET_TYPE_WIALON_LOGIN:
	case PACKET_TYPE_WIALON_VLOGIN:
	case PACKET_TYPE_WIALON_LOGIN2:
	case PACKET_TYPE_WIALON_VLOGIN2:
		if (packet_info->is_auth) {
			int64_t time_ms = my_time_get_cur_msec2000();// получаем текущее число миллисекунд от 2000 года GMT(UTC)
			//packet_M packet_m = wialon_info->packet.packet_m;
			one_str =
					g_strdup_printf(
							"%lld\001%lld\001%lld\001%s\001%d\001%lf\001%lf\001%d\001%d\001%d\001%s",
							id, (long long int) time_ms,
							(long long int) time_ms,
							packet_info->client_ip_addr,	// ip
							packet_info->client_port,	// port
							0.0, 0.0, 0, 0, packet->type, "");
			if (n_rows)
				*n_rows = 1;	// одна строка на выходе
		}
		break;
		break;
	case PACKET_TYPE_WIALON_DRIVER:
	case PACKET_TYPE_WIALON_DRIVER2:
		if (packet_info->is_auth) {
			int64_t time_ms = my_time_get_cur_msec2000();// получаем текущее число миллисекунд от 2000 года GMT(UTC)
			//packet_M packet_m = wialon_info->packet.packet_m;
			one_str =
					g_strdup_printf(
							"%lld\001%lld\001%lld\001%s\001%d\001%lf\001%lf\001%d\001%d\001%d\001%s",
							id, (long long int) time_ms,
							(long long int) time_ms,
							packet_info->client_ip_addr,	// ip
							packet_info->client_port,	// port
							0.0, 0.0, 0, 0, packet->type, packet->raw_msg);
			if (n_rows)
				*n_rows = 1;	// одна строка на выходе
		}
		break;
	case PACKET_TYPE_WIALON_SHORT_DATA:
	case PACKET_TYPE_WIALON_DATA:
	case PACKET_TYPE_WIALON_SHORT_DATA2:
	case PACKET_TYPE_WIALON_DATA2:
		if (packet_info->is_auth) {
			packet_SD packet_sd =
					(packet->type == PACKET_TYPE_WIALON_DATA) ?
							packet->packet_d.packet_sd : packet->packet_sd;
			// time_save,time,ip,port,lat,lon,height,speed,type_pkt,raw_data
			one_str =
					g_strdup_printf(
							"%lld\001%lld\001%lld\001%s\001%d\001%lf\001%lf\001%d\001%d\001%d\001%s",
							id, (long long int) my_time_get_cur_msec2000(),
							(long long int) packet_sd.jdate,
							packet_info->client_ip_addr,	// ip
							packet_info->client_port,	// port

							packet_sd.lat,	// lat DOUBLE PRECISION, широта
							packet_sd.lon,	// lon DOUBLE PRECISION, долгота
							packet_sd.height,	// height INTEGER, высота
							packet_sd.speed,	// speed INTEGER, скорость
							packet->type,// type_pkt SMALLINT, тип пакета(PACKET_TYPE_WIALON_DRIVER = 3; PACKET_TYPE_WIALON_SHORT_DATA = 4, 8; PACKET_TYPE_WIALON_DATA = 5, 9)
							packet->raw_msg	// raw_data TEXT	сырое содержимое пакета
							);
			if (n_rows)
				*n_rows = 1;	// одна строка на выходе
			return one_str;
		}
		break;
	case PACKET_TYPE_WIALON_BLACK:
	case PACKET_TYPE_WIALON_BLACK2:
		if (packet_info->is_auth) {
			packet_B packet_b = packet->packet_b;
			int i;
			GString *out = g_string_new(NULL);
			//int64_t time_ms = my_time_get_cur_msec2000();// получаем текущее число миллисекунд от 2000 года GMT(UTC)
			for (i = 0; i < packet_b.n_count; i++) {

				packet_D *packet_d = packet_b.packet_d + i;
				packet_SD packet_sd = packet_d->packet_sd;
				uint8_t type = PACKET_TYPE_NA;
				if (packet->type == PACKET_TYPE_WIALON_BLACK)
					type = (packet_d->type == PACKET_TYPE_WIALON_DATA) ?
							PACKET_TYPE_WIALON_DATA_FROM_BLACK :
							PACKET_TYPE_WIALON_SHORT_DATA_FROM_BLACK;
				if (packet->type == PACKET_TYPE_WIALON_BLACK2)
					type = (packet_d->type == PACKET_TYPE_WIALON_DATA2) ?
							PACKET_TYPE_WIALON_DATA_FROM_BLACK2 :
							PACKET_TYPE_WIALON_SHORT_DATA_FROM_BLACK2;
				int64_t time_ms = my_time_get_cur_msec2000();// получаем текущее число миллисекунд от 2000 года GMT(UTC)
				// time_save,time,ip,port,lat,lon,height,speed,type_pkt,raw_data
				char *str =
						g_strdup_printf(
								"%lld\001%lld\001%lld\001%s\001%d\001%lf\001%lf\001%d\001%d\001%d\001%s",
								id, (long long int) time_ms,
								(long long int) packet_sd.jdate,
								packet_info->client_ip_addr,			// ip
								packet_info->client_port,	// port

								packet_sd.lat,	// lat DOUBLE PRECISION, широта
								packet_sd.lon,	// lon DOUBLE PRECISION, долгота
								packet_sd.height,	// height INTEGER, высота
								packet_sd.speed,	// speed INTEGER, скорость
								type,// type_pkt SMALLINT, тип пакета(PACKET_TYPE_WIALON_DRIVER = 3; PACKET_TYPE_WIALON_SHORT_DATA = 4, 8; PACKET_TYPE_WIALON_DATA = 5, 9)
								*(packet_b.raw_msg + i));
				if (i > 0 && i < (packet_b.n_count - 1))
					g_string_append(out, "\002");	// разделитель записей
				g_string_append(out, str);
				g_free(str);
			}
			if (n_rows)
				*n_rows = packet_b.n_count;	// число строк на выходе
			one_str = g_string_free(out, FALSE);
		}
		break;
	}
	return one_str;
}

