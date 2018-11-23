// protocol_osmand.с : Работа с протоколом OsmAnd внутри сервера
//

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
//#include <zlib.h>
#include <glib.h>
#include "../gps_server.h"
#include "protocol.h"
#include "protocol_osmand.h"
#include "../base_lib.h"
#include "crc16.h"
#include "../my_time.h"

// обработать запрос
// return: id добавленной в базу записи
int64_t osmand_process(struct PACKET_INFO *packet_info) {
	if (!packet_info)
		return 0;
	//struct OSMAND_PACKET *packet = (struct OSMAND_PACKET*)packet_info->packet;
	// пароль не используется
	packet_info->is_auth = TRUE;

	// записать пакет в БД
	int64_t id = base_save_packet(PROTOCOL_OSMAND, packet_info);
	if (!id) {
		packet_info->is_parse = FALSE;
		printf("osmand_process: base_save_packet type=%d fail!\n");
	} else
		packet_info->is_parse = TRUE;
	return id;
}

// подготовить ответ на запрос
// len_out - длина возвращаемых данных
gchar* osmand_prepare_answer(struct PACKET_INFO *packet_info, int *len_out) {
	if (!packet_info)
		return NULL;
	//struct OSMAND_PACKET *packet = (struct OSMAND_PACKET*)packet_info->packet;
	char *str;
	// если пакет не был сохранён в базу, то не отвечаем  на запрос
	if (!packet_info->is_parse)
		str =
				g_strdup_printf(
						"HTTP/1.1 400 Bad Request\r\nServer: %s\r\nContent-Length: 0\r\n\r\n",
						get_server_name());
	else
		str = g_strdup_printf(
				"HTTP/1.1 200 OK\r\nServer: %s\r\nContent-Length: 2\r\n\r\nok",
				get_server_name());

	//packet_info->is_parse = FALSE;// разрываем соединение после отправки ответа
	if (len_out) {
		if (str)
			*len_out = strlen(str);
		else
			*len_out = 0;
	}
	return str;
}

// записать пакет в БД
int64_t base_save_packet_osmand(void *db, struct PACKET_INFO *packet_info) {
	//return TRUE;
	char *err_msg = NULL;
	if (!db)
		return 0;
	int64_t id = 0;
	// проверка поддержки мультипотоковой работы библиотеки libpq
	//int smp = PQisthreadsafe();
	//base_create_table0();
	OSMAND_PACKET* packet = (OSMAND_PACKET*) packet_info->packet;
	if (!packet)
		return 0;
	char *query =
			g_strdup_printf(
					"INSERT INTO data_%s VALUES(DEFAULT, '%lld', '%lld', '%s', '%d', '%lf', '%lf', '%d', '%d', '%d', '%s') RETURNING id",
					packet_info->imei,
					(long long int) my_time_get_cur_msec2000(),
					(long long int) packet->jdate, packet_info->client_ip_addr,
					packet_info->client_port, packet->lat, packet->lon,
					packet->height, packet->speed, PACKET_TYPE_OSMAND_DATA,
					packet->raw_msg);
	id = base_exec_ret_id(db, query, &err_msg);
	// возможно это первое сообщение torque
	if (!id) {
		// смотрим, есть ли в сообщении параметр eml
		const char *eml_str = NULL;
		if (packet->n_params > 0 && packet->params) {
			int16_t i;
			for (i = 0; i < packet->n_params; i++) {
				struct DOP_PARAM *dop_param = packet->params + i;
				if (dop_param->name && dop_param->type == DOP_PARAM_VAL_STR
						&& !g_ascii_strcasecmp(dop_param->name, "Eml")) {
					eml_str = dop_param->val_str;
					break;
				}
			}
		}
		// смотрим, есть ли пользователь с е-майлом eml_str
		if (eml_str) {
			// добавить к пользователю imei, если e-mail совпал
			gboolean ret = base_user_add_imei_by_email(packet_info->imei,
					eml_str);
			// создать устройство (оборудование)
			ret = base_create_dev(packet_info->imei, NULL, (char*) NULL,
					(char*) "torque");
			// попробуем снова записать пакет в БД
			id = base_exec_ret_id(db, query, &err_msg);
		}
	}
	g_free(query);
	g_free(err_msg);
	return id;
}

// преобразовать полученные данные в строку для передачи
// [in] id - id последней записи, добавленной в бд
// [out] n_rows - число строк на выходе
char* osmand_get_data_str(int64_t id, struct PACKET_INFO *packet_info,
		int *n_rows) {
	char *one_str = NULL;
	if (!packet_info)
		return NULL;
	struct OSMAND_PACKET *packet = (struct OSMAND_PACKET*) packet_info->packet;
	if (n_rows)
		*n_rows = 0;	// пока ничего нет
	// time_save,time,ip,port,lat,lon,height,speed,type_pkt,raw_data
	one_str =
			g_strdup_printf(
					"%lld\001%lld\001%lld\001%s\001%d\001%lf\001%lf\001%d\001%d\001%d\001%s",
					id, (long long int) my_time_get_cur_msec2000(),
					(long long int) packet->jdate, packet_info->client_ip_addr,
					packet_info->client_port, packet->lat, packet->lon,
					packet->height, packet->speed, PACKET_TYPE_OSMAND_DATA,
					packet->raw_msg);
	if (n_rows)
		*n_rows = 1;	// одна строка на выходе
	return one_str;
}

