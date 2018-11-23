// protocol_gt06.с : Работа с протоколом GT06 внутри сервера
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
#include "protocol_gt06.h"
#include "../base_lib.h"
#include "crc16.h"
#include "../my_time.h"

// обработать запрос
// return: id добавленной в базу записи
int64_t gt06_process(struct PACKET_INFO *packet_info) {
	if (!packet_info)
		return 0;
	// пароль никогда не используется
	packet_info->is_auth = TRUE;
	// записать пакет в БД
	int64_t id = base_save_packet(PROTOCOL_GT06, packet_info);
	if (!id) {
		packet_info->is_parse = FALSE;
		printf("gt06_process: base_save_packet type=%d fail!\n");
	} else
		packet_info->is_parse = TRUE;
	return id;
}

// подготовить ответ на запрос
// len_out - длина возвращаемых данных
gchar* gt06_prepare_answer(struct PACKET_INFO *packet_info, int *len_out) {
	if (!packet_info || !packet_info->packet || !packet_info->is_parse)
		return NULL;
	struct GT06_PACKET *packet = (struct GT06_PACKET*) packet_info->packet;
	// подготавливаем ответ
	uint16_t num16 = GUINT16_TO_BE(packet->num); // перевод в big indian
	uint8_t *num8 = (uint8_t*) &num16;
	char *str_out = g_strdup_printf("\x78\x78\x05%c%c%cCC\x0d\x0a",
			packet->type, num8[0], num8[1]); // CC заменим на crc16
	int str_out_len = 10;
	// рассчёт и вставка в пакет crc
	uint16_t crc = crc16_itu((uint8_t*) str_out + 2, str_out_len - 6); // расчёт crc пакета
	*(uint16_t*) (str_out + str_out_len - 4) = GUINT16_TO_BE(crc);
	if (len_out)
		*len_out = 10;
	return str_out;
}

// сформировать команду для отправки устройству
uint8_t* gt06_prepare_cmd_to_dev(const char *cmd_text, int *len_out) {
	uint8_t len_msg = (cmd_text) ? strlen(cmd_text) : 0;
	if (!len_msg)
		return NULL;
	// \x01\x02\x03\x04 - это Server Flag Bit 
	// \x00\x02 - English language
	// \x00\x01 - порядковый номер пакета
	// "SFBi" Server Flag Bit, "Lg" - language, "Nm" - порядковый номер пакета, "СС" заменим на crc16
	uint8_t *str_out = (uint8_t*) g_strdup_printf(
			"\x78\x78%c\x80%cSFBi%sLgNmCC\x0d\x0a", len_msg + 12, len_msg,
			cmd_text);
	size_t str_out_len = strlen((const char*) str_out);	// д.б.= len_msg + 17
	uint32_t Server_Flag_Bit = 0x01020304;
	uint16_t Lang = 0x0002;
	uint16_t num = 0x0001;
	uint16_t crc;
	if (str_out_len != len_msg + 17) {
		g_free(str_out);
		return NULL;
	}
	// записываем в строку требуемые поля
	*(uint32_t*) (str_out + 5) = GUINT32_TO_BE(Server_Flag_Bit);
	*(uint16_t*) (str_out + 9 + len_msg) = GUINT16_TO_BE(Lang);
	*(uint16_t*) (str_out + 11 + len_msg) = GUINT16_TO_BE(num);
	crc = crc16_itu((uint8_t*) str_out + 2, str_out_len - 6);// расчёт crc пакета
	*(uint16_t*) (str_out + 13 + len_msg) = GUINT16_TO_BE(crc);	// вставка в пакет crc
	if (len_out)
		*len_out = str_out_len;
	return str_out;
}

// записать пакет в БД
int64_t base_save_packet_gt06(void *db, struct PACKET_INFO *packet_info) {
	if (!db)
		return 0;
	int64_t id = 0;
	char *err_msg = NULL;
	GT06_PACKET* packet = (GT06_PACKET*) packet_info->packet;
	if (!packet)
		return 0;
	if (packet->type == PACKET_TYPE_GT06_LOGIN) {
		// перевод массива в строку для сохранения в blob
		char *str = get_string_from_blob(packet->raw_msg, packet->raw_msg_len);
		char *query =
				g_strdup_printf(
						"INSERT INTO auth_log VALUES(DEFAULT, '%lld', '%s', '%s', '%d', '%d', '%s', '%s')",
						my_time_get_cur_msec2000(), packet->packet_l.imei,
						packet_info->client_ip_addr, packet_info->client_port,
						packet_info->is_auth, "gt06", str);
		id = base_exec(db, query, &err_msg);
		g_free(query);
		g_free(str);
	} else {
		char *str = get_string_from_blob(packet->raw_msg, packet->raw_msg_len);
		char *query =
				g_strdup_printf(
						"INSERT INTO data_%s VALUES(DEFAULT, '%lld', '%lld', '%s', '%d', '%lf', '%lf', '%d', '%d', '%d', '%s') RETURNING id",
						packet_info->imei,
						(long long int) my_time_get_cur_msec2000(),
						(long long int) packet->jdate,
						packet_info->client_ip_addr, packet_info->client_port,
						packet->packet_gps.lat, packet->packet_gps.lon, 0,//packet->packet_gps.height,
						packet->packet_gps.speed, packet->type, str);
		id = base_exec_ret_id(db, query, &err_msg);
		g_free(query);
		g_free(str);
	}
	g_free(err_msg);
	return id;
}

// преобразовать полученные данные в строку для передачи
// [in] id - id последней записи, добавленной в бд
// [out] n_rows - число строк на выходе
char* gt06_get_data_str(int64_t id, struct PACKET_INFO *packet_info,
		int *n_rows) {
	char *one_str = NULL;
	if (!packet_info)
		return NULL;
	struct GT06_PACKET *packet = (struct GT06_PACKET*) packet_info->packet;
	if (n_rows)
		*n_rows = 0;	// пока ничего нет
	char *str = get_string_from_blob(packet->raw_msg, packet->raw_msg_len);
	// time_save,time,ip,port,lat,lon,height,speed,type_pkt,raw_data
	one_str =
			g_strdup_printf(
					"%lld\001%lld\001%lld\001%s\001%d\001%lf\001%lf\001%d\001%d\001%d\001%s",
					id, (long long int) my_time_get_cur_msec2000(),
					(long long int) packet->jdate, packet_info->client_ip_addr,
					packet_info->client_port, packet->packet_gps.lat,
					packet->packet_gps.lon, 0, packet->packet_gps.speed,
					packet->type, str);
	if (n_rows)
		*n_rows = 1;	// одна строка на выходе
	g_free(str);
	return one_str;
}

