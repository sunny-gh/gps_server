// protocol_parse_gt06.c : Протокол GT06
//

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
//#include <zlib.h>
#include <glib.h>
#include "protocol_parse.h"
#include "protocol_parse_gt06.h"
#include "crc16.h"
#include "../my_time.h"

// Make sure we can call this stuff from C++.
#if __cplusplus
extern "C" {
#endif

// типы сообщений
enum {
	TYPE_LOGIN = 0x01, TYPE_GPS = 0x10, TYPE_LBS = 0x11, TYPE_GPS_LBS = 0x12,
	//TYPE_GPS_LBS_2 = 0x22,
	TYPE_STATUS = 0x13,
	//TYPE_SATELLITE = 0x14,
	TYPE_STRING = 0x15,
	TYPE_GPS_LBS_STATUS = 0x16,
	//TYPE_GPS_LBS_STATUS_2 = 0x26,
	//TYPE_GPS_LBS_STATUS_3 = 0x27,
//	TYPE_LBS_PHONE = 0x17,
//	TYPE_LBS_EXTEND = 0x18,
	TYPE_LBS_STATUS = 0x19,
	TYPE_GPS_PHONE = 0x1A,
	TYPE_GPS_LBS_EXTEND = 0x1E,
	TYPE_COMMAND = 0x80,
//	TYPE_COMMAND_1 = 0x81,
	//TYPE_COMMAND_2 = 0x82,
	//TYPE_INFO = 0x94,
	TYPE_COUNT
};

// удалить содержимое структуры GT06_PACKET - содержимое пакета
static void del_gt06_packet(struct GT06_PACKET *packet) {
	if (!packet)
		return;
	g_free(packet->packet_l.imei);
	g_free(packet->raw_msg);
}

// очистить структуру GT06_PACKET - содержимое пакета
void clean_gt06_packet(void *packet_in) {
	struct GT06_PACKET *packet = (struct GT06_PACKET *) packet_in;
	if (!packet)
		return;
	del_gt06_packet(packet);
	// обнуляем структуру с пакетом
	memset(packet, 0, sizeof(struct GT06_PACKET));
}

// удалить структуру GT06_PACKET - содержимое пакета
void free_gt06_packet(void *packet_in) {
	struct GT06_PACKET *packet = (struct GT06_PACKET *) packet_in;
	if (!packet)
		return;
	del_gt06_packet(packet);
	// удаляем структуру с пакетом
	g_free(packet);
}

// получить число из строки
static uint16_t str_to_uint16(uint8_t *str) {
	return GUINT16_TO_BE(*(uint16_t*) (str));
	/*#if (G_BYTE_ORDER == G_LITTLE_ENDIAN) 
	 return GUINT16_SWAP_LE_BE_CONSTANT(*(uint16_t*)(str));
	 #else
	 return (*(uint16_t*)(str));
	 #endif
	 */
}

// получить юлианскую дату, в милисекундах от 2000г из строки 6 байт
static int64_t parse_time(uint8_t *str) {
	struct tm gmt_tm;
	gmt_tm.tm_year = *(str + 0) + 100;	// years since 1900
	gmt_tm.tm_mon = *(str + 1) - 1;
	gmt_tm.tm_mday = *(str + 2);
	gmt_tm.tm_hour = *(str + 3);
	gmt_tm.tm_min = *(str + 4);
	gmt_tm.tm_sec = *(str + 5);
	// перевод даты/времени в число миллисекунд от 2000 года
	int64_t time_ms = my_time_time_to_msec2000(&gmt_tm, 0);
	return time_ms - (8 * 60 * 60 * 1000);	// -8 часов (китайское время)
}

// получить широту или долготу в градусы из строки 4 байта
static double parse_latlon(uint8_t *str) {
	//22º32.7658’ = (22X60 + 32.7658)X3000 = 40582974
	uint32_t latlon = GUINT32_TO_BE(*(uint32_t*) str);// переводим из любой архитектуры в архитектуру big endian (G_BYTE_ORDER = G_LITTLE_ENDIAN)
	double d_latlon = latlon / 30000. / 60.;
	return d_latlon;
}

// разобрать пакет с логином (содержит только imei)
static gboolean parse_packet_l(struct GT06_PACKET *packet) {
	uint8_t *s_imei = packet->raw_msg + 4;
	if (packet->raw_msg_len != 18)
		return FALSE;
	// 15 символов imei
	packet->packet_l.imei = g_strdup_printf("%x%02x%02x%02x%02x%02x%02x%02x",
			s_imei[0], s_imei[1], s_imei[2], s_imei[3], s_imei[4], s_imei[5],
			s_imei[6], s_imei[7]);
	return TRUE;
}

// разобрать пакет с gps данными
// заполняется packet_gps
static gboolean parse_packet_gps(struct GT06_PACKET *packet) {
	struct packet_GT06_GPS *packet_gps = &packet->packet_gps;
	uint8_t *s_time = packet->raw_msg + 4;
	packet->jdate = parse_time(packet->raw_msg + 4);
	uint8_t gps_info = *(packet->raw_msg + 10); // байт с длиной GPS данных и числом спутников
	uint8_t gps_len = (gps_info & 0xf0) >> 4; // длина GPS данных
	packet_gps->sats = gps_info & 0xf; // число спутников
	// определяем широту и долготу
	if (gps_len >= 8) {
		packet_gps->lat = parse_latlon(packet->raw_msg + 11);
		packet_gps->lon = parse_latlon(packet->raw_msg + 15);
	} else
		return FALSE;	// хотя бы широта и долгота д.б.
	// определяем скорость
	if (gps_len >= 10) {
		packet_gps->speed = *(packet->raw_msg + 19);// range from 0 to 225 km/h
	}
	// определяем направление широты и долготы и курс (азимут), в градусах
	if (gps_len >= 11) {
		sizeof(packet_GT06_GPS);
		uint16_t word = GUINT16_TO_BE(*(uint16_t*) (packet->raw_msg + 20));
		// курс
		packet_gps->course = word & 0x3ff;	// range from 0 to 360 градусов
		// части света координат
		if (!(word & 0x400))		// южная ли широта
			packet_gps->lat = -packet_gps->lat;
		if (word & 0x800)		// западная ли долгота
			packet_gps->lon = -packet_gps->lon;
		// доп. флаги
		packet_gps->gps_have_pos = (word & 0x1000) ? 1 : 0;	//GPS having been positioning or not 
		packet_gps->gps_is_rt = (word & 0x2000) ? 1 : 0;//GPS real-time/differential positioning 
	}
	return TRUE;
}

// разобрать пакет с LBS данными
static gboolean parse_packet_lbs(struct GT06_PACKET *packet) {
	struct packet_GT06_LBS *packet_lbs = &packet->packet_lbs;
	int shift = 4;
	int lbs_len = 8;
	// есть ли время в пакете
	if (packet->type == PACKET_TYPE_GT06_LBS) {
		uint8_t *s_time = packet->raw_msg + shift;
		packet->jdate = parse_time(packet->raw_msg + shift);
		shift += 6;
	} else if (packet->type == PACKET_TYPE_GT06_GPS_LBS) {
		shift = 22;
		//uint8_t gps_info = *(packet->raw_msg + 10); // байт с длиной GPS данных и числом спутников
		//uint8_t gps_len = (gps_info & 0xf0) >> 4;// длина GPS данных
		//shift = 11 + gps_len;

	} else if (packet->type == PACKET_TYPE_GT06_GPS_LBS_STATUS)	// не проверено!
			{
		shift = 22;
		lbs_len = *(uint8_t*) (packet->raw_msg + shift);		// =9
		shift++;
	} else
		return FALSE;

	packet_lbs->MCC = GUINT16_TO_BE(*(uint16_t*) (packet->raw_msg + shift));
	shift += 2;
	packet_lbs->MNC = *(uint8_t*) (packet->raw_msg + shift);
	shift++;
	packet_lbs->LAC = GUINT16_TO_BE(*(uint16_t*) (packet->raw_msg + shift));
	shift += 2;
	packet_lbs->cell_id = GUINT32_TO_BE(*(uint32_t*) (packet->raw_msg + shift));
	packet_lbs->cell_id >>= 8;
	shift += 3;
	/*
	 real cell ID: 27112 (0x69e8)

	 Beeline
	 MCC: 250 (Russian Federation)
	 MNC: 99 (Beeline)
	 LAC: 37453
	 cell ID: 31111093

	 latitude: 55.138161
	 longitude: 60.147636
	 */
	return TRUE;
}

// разобрать пакет с GPS и LBS данными
static gboolean parse_packet_gps_lbs(struct GT06_PACKET *packet) {
	// разобрать первую часть сообщения - GPS пакет
	gboolean ret = parse_packet_gps(packet);
	// разобрать вторую часть сообщения - LBS пакет
	if (ret)
		ret = parse_packet_lbs(packet);
	return ret;
}

// разобрать пакет со статусом
static gboolean parse_packet_status(struct GT06_PACKET *packet) {
	int shift;
	// одиночный статусный пакет
	if (packet->type == PACKET_TYPE_GT06_STATUS) {
		shift = 4;
		// тек. время, т.к. в пакете нет времени
		packet->jdate = my_time_get_cur_msec2000();
	}
	// статусный пакет в составе GPS+LBS пакета
	else if (packet->type == PACKET_TYPE_GT06_GPS_LBS_STATUS)
		shift = 31;
	else
		return FALSE;
	packet->packet_status.info = *(uint8_t*) (packet->raw_msg + shift);
	packet->packet_status.voltage = *(uint8_t*) (packet->raw_msg + shift + 1);
	packet->packet_status.signal = *(uint8_t*) (packet->raw_msg + shift + 2);
	packet->packet_status.alarm = *(uint8_t*) (packet->raw_msg + shift + 3);
	packet->packet_status.lang = *(uint8_t*) (packet->raw_msg + shift + 4);
	return TRUE;
}

// разобрать пакет с GPS, LBS данными и статусом
static gboolean parse_packet_gps_lbs_status(struct GT06_PACKET *packet) {
	// разобрать первую часть сообщения - GPS пакет
	gboolean ret = parse_packet_gps(packet);
	// разобрать вторую часть сообщения - LBS пакет
	if (ret)
		ret = parse_packet_lbs(packet);
	// разобрать третью часть сообщения - пакет со статусом
	if (ret)
		ret = parse_packet_status(packet);
	return ret;
}

// разобрать пакет с текстом
static gboolean parse_packet_string(struct GT06_PACKET *packet) {
	int shift = 4;
	if (packet->raw_msg_len < 20)
		return FALSE;
	int len_mgs = (int) (*(uint8_t*) (packet->raw_msg + shift)) - 4;// длина сообщения = длина команды - длина res_server_id
	shift++;
	uint32_t res_server_id = GUINT32_TO_BE(
			*(uint32_t*) (packet->raw_msg + shift));//It is reserved to the identification of the server. The binary data received by the terminal is returned without change.
	shift += 4;
	if (len_mgs > 0) {
		packet->packet_str.msg = (char*) g_malloc0(len_mgs + 1);
		memcpy(packet->packet_str.msg, packet->raw_msg + shift, len_mgs);
	} else		// такого не д.б.
	{
		printf("[parse_packet_string] len_mgs=%d", len_mgs);
		packet->packet_str.msg = NULL;
	}
	return TRUE;
}

// получить содержимое пакета из принятой строки
// sample: "78780d010355488020653096000439a70d0a"
// return FALSE в случае нераспознанной строки
gboolean gt06_parse_packet(char *str, int str_len,
		struct PACKET_INFO *packet_info) {
	if (!str || str_len < 11 || !packet_info)
		return FALSE;
	// распознанное сообщение или нет
	gboolean parse_ok = FALSE;
	struct GT06_PACKET *packet;
	uint16_t start_bits = *(uint16_t*) str;		// стартовые байты
	uint8_t len_data = *(uint8_t*) (str + 2);// длина сообщения без 2 первых стартовых байт, этого байта и 2 последних байт отведённых на стоп-биты
	uint16_t crc_msg = str_to_uint16((uint8_t*) str + str_len - 4);	// crc из принятого пакета
	uint16_t crc = crc16_itu((uint8_t*) str + 2, str_len - 6);// расчёт crc пакета
	if (crc != crc_msg)
		return FALSE;
	packet = (struct GT06_PACKET*) g_malloc0(sizeof(struct GT06_PACKET));
	uint8_t type = *(uint8_t*) (str + 3);		// тип сообщения
	packet->raw_msg_len = (uint8_t) str_len;
	packet->raw_msg = (uint8_t*) g_malloc(str_len);
	packet_info->packet = packet;
	memcpy(packet->raw_msg, str, str_len);
	packet->num = str_to_uint16(packet->raw_msg + str_len - 6);	// порядковый номер пакета
	// определяем тип принятого пакета
	switch (type)	// пакет логина
	{
	case TYPE_LOGIN:	// 0x01
		packet->type = PACKET_TYPE_GT06_LOGIN;
		if (parse_packet_l(packet)) {
			// запоминаем imei устройства
			packet_info->imei = g_strdup(packet->packet_l.imei);
			parse_ok = TRUE;
		}
		break;
	case TYPE_GPS:	// 0x10 отдельно не проверен!
		packet->type = PACKET_TYPE_GT06_GPS;
		if (parse_packet_gps(packet))
			parse_ok = TRUE;
		break;
	case TYPE_LBS:	// 0x11 отдельно не проверен!
		packet->type = PACKET_TYPE_GT06_LBS;
		if (parse_packet_lbs(packet))
			parse_ok = TRUE;
		break;
	case TYPE_GPS_LBS:	// 0x12
		packet->type = PACKET_TYPE_GT06_GPS_LBS;
		if (parse_packet_gps_lbs(packet))
			parse_ok = TRUE;
		break;
	case TYPE_STATUS:	// 0x13
		packet->type = PACKET_TYPE_GT06_STATUS;
		if (parse_packet_status(packet))
			parse_ok = TRUE;
		break;
	case TYPE_STRING:	// 0x15 не проверен!
		packet->type = PACKET_TYPE_GT06_STRING;
		if (parse_packet_string(packet))
			parse_ok = TRUE;
		break;
	case TYPE_GPS_LBS_STATUS:	// 0x16
		packet->type = PACKET_TYPE_GT06_GPS_LBS_STATUS;
		if (parse_packet_gps_lbs_status(packet))
			parse_ok = TRUE;
		break;
	default:
		packet->type = PACKET_TYPE_GT06_UNPARSE;
		parse_ok = TRUE;	// пакет уже подходит, ведь crc прошла проверку
		break;
	}
	// распознанное сообщение или нет
	packet_info->is_parse = parse_ok;
	return parse_ok;
}

// получение текстового сообщения из пакета
char* gt06_get_text_message(struct PACKET_INFO *packet_info) {
	struct GT06_PACKET *gt06_packet =
			(packet_info) ? (struct GT06_PACKET *) packet_info->packet : NULL;
	if (gt06_packet && gt06_packet->type == PACKET_TYPE_GT06_STRING) {
		return g_strdup(gt06_packet->packet_str.msg);
	}
	return NULL;
}

// получение из packet_info доп. параметров
gboolean gt06_packet_get_dop_param(struct PACKET_INFO *packet_info, int *course,
		int *n_params, struct DOP_PARAM **params) {
	if (!packet_info || !packet_info->packet)
		return FALSE;
	struct GT06_PACKET *packet = (struct GT06_PACKET*) packet_info->packet;	// g_malloc0(sizeof(struct GT06_PACKET));
	/*if (course) *course = packet->course;// курс, в градусах
	 if (n_params && params)
	 {
	 *n_params = packet->n_params;
	 struct DOP_PARAM *all_dop_params = (struct DOP_PARAM*)g_malloc0((*n_params)*sizeof(struct DOP_PARAM));
	 *params = all_dop_params;
	 int n_dp;
	 for (n_dp = 0; n_dp < *n_params; n_dp++)
	 ;// copy_one_dop_param(all_dop_params + n_dp, packet->params + n_dp);
	 }*/
	return TRUE;
}

#if __cplusplus
}  // End of the 'extern "C"' block
#endif
