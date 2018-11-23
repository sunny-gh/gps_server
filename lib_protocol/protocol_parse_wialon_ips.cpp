// protocol_parse_wialon_ips.c : Протокол Wialon IPS
//

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <libintl.h>
//#include <zlib.h>
#include <glib.h>
#include "protocol_parse.h"
#include "protocol_parse_wialon_ips.h"
#include "crc16.h"
#include "../my_time.h"

// Make sure we can call this stuff from C++.
#if __cplusplus
extern "C" {
#endif

// удалить содержимое пакета c данными
void clear_wialon_packet_d(struct packet_D *packet_d) {
	g_free(packet_d->adc);
	g_free(packet_d->ibutton);
	struct DOP_PARAM *param = packet_d->params;
	int16_t n_params = packet_d->n_params;
	free_all_dop_param(n_params, param);
}

// удалить содержимое структуры WIALON_PACKET - содержимое пакета
static void del_wialon_packet(struct WIALON_PACKET *wialon_packet) {
	if (!wialon_packet)
		return;
	uint8_t type = wialon_packet->type;
	if (type == PACKET_TYPE_WIALON_LOGIN || type == PACKET_TYPE_WIALON_LOGIN2) {
		g_free(wialon_packet->packet_l.imei);
		g_free(wialon_packet->packet_l.password);
	} else if (type == PACKET_TYPE_WIALON_DRIVER
			|| type == PACKET_TYPE_WIALON_DRIVER2) {
		g_free(wialon_packet->packet_m.msg);
	} else if (type == PACKET_TYPE_WIALON_SHORT_DATA
			|| type == PACKET_TYPE_WIALON_SHORT_DATA2
			|| type == PACKET_TYPE_WIALON_SHORT_DATA_FROM_BLACK
			|| type == PACKET_TYPE_WIALON_SHORT_DATA_FROM_BLACK2) {
		// ничего не надо удалять
	} else if (type == PACKET_TYPE_WIALON_DATA
			|| type == PACKET_TYPE_WIALON_DATA2
			|| type == PACKET_TYPE_WIALON_DATA_FROM_BLACK
			|| type == PACKET_TYPE_WIALON_DATA_FROM_BLACK2) {
		clear_wialon_packet_d(&wialon_packet->packet_d);
	} else if (type == PACKET_TYPE_WIALON_BLACK
			|| type == PACKET_TYPE_WIALON_BLACK2) {
		int n_count = wialon_packet->packet_b.n_count;
		struct packet_B pkt = wialon_packet->packet_b;
		struct packet_D *pkt_d = pkt.packet_d;
		for (int i = 0; i < n_count; i++) {
			clear_wialon_packet_d(pkt_d + i);
			g_free(*(pkt.raw_msg + i));
		}
		//g_free(pkt.types);
		g_free(pkt_d);
	} else if (type != 0) // если type=0, значит пустой пакет
			{
		printf("[del_wialon_packet] no del! type=%d\n", type);
	}
	g_free(wialon_packet->raw_msg);
}

// очистить структуру WIALON_PACKET - содержимое пакета
void clean_wialon_packet(void *wialon_packet_in) {
	struct WIALON_PACKET *wialon_packet =
			(struct WIALON_PACKET *) wialon_packet_in;
	if (!wialon_packet)
		return;
	del_wialon_packet(wialon_packet);
	// обнуляем структуру с пакетом
	memset(wialon_packet, 0, sizeof(struct WIALON_PACKET));
}

// удалить структуру WIALON_PACKET - содержимое пакета
void free_wialon_packet(void *wialon_packet_in) {
	struct WIALON_PACKET *wialon_packet =
			(struct WIALON_PACKET *) wialon_packet_in;
	if (!wialon_packet)
		return;
	del_wialon_packet(wialon_packet);
	// удаляем структуру с пакетом
	g_free(wialon_packet);
}

// получить массив дополнительных параметров из строки вида "count1:1:564,fuel:2:45.8,hw:3:V4.5"
// return: массив структур DOP_PARAM или NULL, последний элемент массива = NULL
static struct DOP_PARAM* get_dop_params(char *params_str, int16_t *n_count) {
	if (!params_str || !strlen(params_str))
		return NULL;
	gchar **strv = g_strsplit(params_str, ",", -1);
	if (!strv || !*strv)
		return NULL;
	// определяем число подстрок
	guint count = g_strv_length(strv);
	if (!count)
		return NULL;
	struct DOP_PARAM *params = (struct DOP_PARAM*) g_malloc0(
			count * sizeof(struct DOP_PARAM)); // последний элемент = 0 - конец массива
	// i -счётчик подстрок, j - счётчик валидных подстрок
	guint i = 0, j = 0;
	for (; i < count; i++) {
		gchar **strv2 = g_strsplit(strv[i], ":", -1);
		guint count2 = g_strv_length(strv2);
		if (count2 != 3)
			continue;
		// пропускаем параметры без названий
		if (strlen(strv2[0]) <= 0)
			continue;
		params[j].name = g_strdup(strv2[0]);
		params[j].type = (uint8_t) atoi(strv2[1]);
		switch (params[j].type) {
		case 1:
			params[j].val_int64 = atoll(strv2[2]);
			break;
		case 2:
			params[j].val_double = g_ascii_strtod(strv2[2], NULL);
			break; // локаленезависимое преобразование в отличии от strtod и atof
		case 3:
			// если параметр определён
			if (g_strcmp0(strv2[2], "NA") && g_strcmp0(strv2[2], "N/A"))
				params[j].val_str = g_strdup(strv2[2]);
			break;
		}
		g_strfreev(strv2);
		j++;
	}
	g_strfreev(strv);
	// если не все строки валидны, то уменьшаем размер выделенной памяти
	if (i != j)
		params = (DOP_PARAM*) g_realloc(params, j * sizeof(struct DOP_PARAM));
	if (n_count)
		*n_count = (int16_t) j;
	return params;
}

// Получить массив аналоговых входов из строки вида: "14.77,0.02,3.6"
// return: массив чисел или NULL, последний элемент массива = NULL
static double* get_adc_from_str(char *adc_str) {
	if (!adc_str)
		return NULL;
	gchar **strv = g_strsplit(adc_str, ",", -1);
	if (!strv)
		return NULL;
	// определяем число подстрок
	guint count = g_strv_length(strv);
	double *mass = (double*) g_malloc0((count + 1) * sizeof(double)); // последний элемент = 0 - конец массива
	for (guint i = 0; i < count; i++) {
		// локаленезависимое преобразование в отличии от strtod и atof
		mass[i] = g_ascii_strtod(strv[i], NULL);
	}
	g_strfreev(strv);
	return mass;
}

// получить широту, в градусах из строк вида lat1="5355.09260" lat2="N", если отсутствует, то передаётся NA;NA; 
static double get_lat_from_str(char *lat1, char *lat2) {
	double lat = 0;
	if (!lat1 || !lat2)
		return 0;
	// если присутсвует дата
	if (g_strcmp0(lat1, "NA") && g_strcmp0(lat1, "N/A") && g_strcmp0(lat2, "NA")
			&& g_strcmp0(lat2, "N/A")) {
		// локаленезависимое преобразование в отличии от strtod и atof
		double val = g_ascii_strtod(lat1, NULL);
		// в градусах + минутах/60
		lat = floor(val / 100.) + fmod(val, 100) / 60.;
		// если южная широта
		if (!g_strcmp0(lat2, "S"))
			lat = -lat;
	}
	return lat;
}

// получить долготу, в градусах из строк вида lat1="03739.6834" lat2="E", если отсутствует, то передаётся NA;NA; 
static double get_lon_from_str(char *lon1, char *lon2) {
	double lon = 0;
	if (!lon1 || !lon2)
		return 0;
	// если присутсвует дата
	if (g_strcmp0(lon1, "NA") && g_strcmp0(lon1, "N/A") && g_strcmp0(lon1, "NA")
			&& g_strcmp0(lon1, "N/A")) {
		// локаленезависимое преобразование в отличии от strtod и atof
		double val = g_ascii_strtod(lon1, NULL);
		// в градусах + минутах/60
		lon = floor(val / 100.) + fmod(val, 100) / 60.;
		// если южная широта
		if (!g_strcmp0(lon2, "W"))
			lon = -lon;
	}
	return lon;
}

// получить текущее Юлианское время в секундах
/*uint64_t get_cur_jdate_time(void)
 {
 int64_t jdate = 0;
 struct tm gmt_tm;
 memset(&gmt_tm, 0, sizeof(struct tm));
 // получаем текущее GMT(UTC) время
 my_time_get_cur_gmt_time(&gmt_tm);
 // переводим время в число милисекунд от 2000 г.
 jdate = my_time_time_to_msec2000(&gmt_tm, 0);
 return jdate;
 }*/

// получить Юлианскую дату в секундах и время из текстовых строк
// date дата в формате DDMMYY, в UTC, если отсутствует, то передаётся NA
// time время в формате HHMMSS, в UTC, если отсутствует, то передаётся NA
static uint64_t get_jdate_time_from_str(char *date, char *time) {
	int64_t jdate = 0;
	struct tm gmt_tm;
	if (!date || !time)
		return 0;
	memset(&gmt_tm, 0, sizeof(struct tm));
	// дата по умолчанию
	gmt_tm.tm_mday = 1;
	gmt_tm.tm_year = 100; // years since 1900
	// если присутсвует время
	if (g_strcmp0(time, "NA") && g_strcmp0(time, "N/A") && strlen(time) == 6) {
		int hms = atoi(time);
		gmt_tm.tm_hour = hms / 10000;
		gmt_tm.tm_min = (hms / 100) % 100;
		gmt_tm.tm_sec = hms % 100;
	} else {
		// получаем текущее GMT(UTC) время
		my_time_get_cur_gmt_time(&gmt_tm);
	}
	// если присутсвует дата
	if (g_strcmp0(date, "NA") && g_strcmp0(date, "N/A") && strlen(date) == 6) {
		int dmy = atoi(date);
		gmt_tm.tm_mday = dmy / 10000;
		gmt_tm.tm_mon = (dmy / 100) % 100 - 1;
		gmt_tm.tm_year = dmy % 100 + 100;
	}
	// переводим время в число милисекунд от 2000 г.
	jdate = my_time_time_to_msec2000(&gmt_tm, 0);
	/*/ проверка
	 {
	 struct tm gmt_tm1;
	 my_time_msec2000_to_time(jdate*1000, &gmt_tm1);
	 gmt_tm.tm_sec += 0;
	 }*/
	return jdate;
}

// разобрать пакет с логином и паролем
static gboolean wialon_parse_packet_l(gchar *strv, struct packet_L *packet_l) {
	if (!strv)
		return FALSE;
	gchar **str_step2 = g_strsplit(strv, ";", -1);
	// определяем число подстрок
	int count = g_strv_length(str_step2);
	if (count == 2) {
		packet_l->ver = 11;
		packet_l->type = PACKET_TYPE_WIALON_LOGIN;	// тип пакета
		// логин и пароль
		packet_l->imei = g_strdup(str_step2[0]);
		packet_l->password = g_strdup(str_step2[1]);
	} else if (count == 4) {
		// локаленезависимое преобразование в отличии от strtod и atof
		packet_l->ver = (uint16_t)(g_ascii_strtod(str_step2[0], NULL) * 10);
		packet_l->type = PACKET_TYPE_WIALON_LOGIN2;		// тип пакета
		// логин и пароль
		packet_l->imei = g_strdup(str_step2[1]);
		packet_l->password = g_strdup(str_step2[2]);
	} else // нераспознанное сообщение
	{
		g_strfreev(str_step2);
		return FALSE;
	}
	g_strfreev(str_step2);
	return TRUE;
}

// разобрать пакет с логином и паролем (в GPSTag для сервера wialon)
static gboolean wialon_parse_packet_vl(gchar *strv, struct packet_L *packet_l) {
	// Примеры пакетов, первое число непонятно зачем
	//#VL#1849;imei
	//#VL#25873;imei; pass
	if (!strv)
		return FALSE;
	gchar **str_step2 = g_strsplit(strv, ";", -1);
	// определяем число подстрок
	int count = g_strv_length(str_step2);
	if (count == 2) {
		packet_l->ver = 11;
		packet_l->type = PACKET_TYPE_WIALON_VLOGIN;	// тип пакета
		// логин и пароль
		packet_l->imei = g_strdup(str_step2[1]);
		packet_l->password = NULL;
	}
	if (count == 3) {
		packet_l->ver = 11;
		packet_l->type = PACKET_TYPE_WIALON_VLOGIN2;		// тип пакета
		// логин и пароль
		packet_l->imei = g_strdup(str_step2[1]);
		packet_l->password = g_strdup(str_step2[2]);
	} else // нераспознанное сообщение
	{
		g_strfreev(str_step2);
		return FALSE;
	}
	g_strfreev(str_step2);
	return TRUE;
}
// разобрать пакет c сообщением для водителя
static gboolean wialon_parse_packet_m(gchar *strv, struct packet_M *packet_m) {
	gchar **str_step2 = g_strsplit(strv, ";", -1);
	// определяем число подстрок
	int count = g_strv_length(str_step2);
	if (count == 1 || count == 2) {
		if (count == 1)
			packet_m->type = PACKET_TYPE_WIALON_DRIVER; // тип пакета
		if (count == 2)
			packet_m->type = PACKET_TYPE_WIALON_DRIVER2; // тип пакета
		packet_m->msg = g_strdup(str_step2[0]);
	}
	// нераспознанное сообщение
	else {
		g_strfreev(str_step2);
		return FALSE;
	}
	g_strfreev(str_step2);
	return TRUE;
}

// разобрать сокращённый пакет с данными
static gboolean wialon_parse_packet_sd(gchar *strv,
		struct packet_SD *packet_sd) {
	if (!strv)
		return FALSE;
	gchar **str_step2 = g_strsplit(strv, ";", -1);
	// определяем число подстрок
	int count = g_strv_length(str_step2);
	if (count == 10 || count == 11) {
		char *date = str_step2[0];
		char *time = str_step2[1];
		char *lat1 = str_step2[2];
		char *lat2 = str_step2[3];
		char *lon1 = str_step2[4];
		char *lon2 = str_step2[5];
		char *speed = str_step2[6];
		char *course = str_step2[7];
		char *height = str_step2[8];
		char *sats = str_step2[9];
		if (count == 10)
			packet_sd->type = PACKET_TYPE_WIALON_SHORT_DATA; // тип пакета
		if (count == 11)
			packet_sd->type = PACKET_TYPE_WIALON_SHORT_DATA2; // тип пакета
		packet_sd->jdate = get_jdate_time_from_str(date, time);
		packet_sd->lat = get_lat_from_str(lat1, lat2);
		packet_sd->lon = get_lon_from_str(lon1, lon2);
		packet_sd->speed = (speed) ? atoi(speed) : 0;
		packet_sd->course = (course) ? atoi(course) : 0;
		packet_sd->height = (height) ? atoi(height) : 0;
		packet_sd->sats = (sats) ? atoi(sats) : 0;
	}
	// нераспознанное сообщение
	else {
		g_strfreev(str_step2);
		return FALSE;
	}
	g_strfreev(str_step2);
	return TRUE;
}

// разобрать пакет с данными
static gboolean wialon_parse_packet_d(gchar *strv, struct packet_D *packet_d) {
	if (!strv)
		return FALSE;
	gchar **str_step2 = g_strsplit(strv, ";", -1);
	// определяем число подстрок
	int count = g_strv_length(str_step2);
	if (count == 16 || count == 17) {
		char *date = str_step2[0];
		char *time = str_step2[1];
		char *lat1 = str_step2[2];
		char *lat2 = str_step2[3];
		char *lon1 = str_step2[4];
		char *lon2 = str_step2[5];
		char *speed = str_step2[6];
		char *course = str_step2[7];
		char *height = str_step2[8];
		char *sats = str_step2[9];
		char *hdop = str_step2[10];
		char *inputs = str_step2[11];
		char *outputs = str_step2[12];
		char *adc = str_step2[13];
		char *ibutton = str_step2[14];
		char *params = str_step2[15];
		if (count == 16)
			packet_d->type = PACKET_TYPE_WIALON_DATA; // тип пакета
		if (count == 17)
			packet_d->type = PACKET_TYPE_WIALON_DATA2; // тип пакета
		packet_d->packet_sd.jdate = get_jdate_time_from_str(date, time);
		packet_d->packet_sd.lat = get_lat_from_str(lat1, lat2);
		packet_d->packet_sd.lon = get_lon_from_str(lon1, lon2);
		packet_d->packet_sd.speed = (speed) ? atoi(speed) : 0;
		packet_d->packet_sd.course = (course) ? atoi(course) : 0;
		packet_d->packet_sd.height = (height) ? atoi(height) : 0;
		packet_d->packet_sd.sats = (sats) ? atoi(sats) : 0;
		// локаленезависимое преобразование в отличии от strtod и atof
		packet_d->hdop = (hdop) ? g_ascii_strtod(hdop, NULL) : 0;
		packet_d->inputs = (inputs) ? strtoul(inputs, NULL, 10) : 0;
		packet_d->outputs = (outputs) ? strtoul(outputs, NULL, 10) : 0;
		packet_d->adc = get_adc_from_str(adc);
		packet_d->ibutton =
				(ibutton && g_strcmp0(ibutton, "NA")
						&& g_strcmp0(ibutton, "N/A")) ?
						g_strdup(ibutton) : NULL;
		packet_d->params = get_dop_params(params, &packet_d->n_params);

	}
	// нераспознанное сообщение
	else {
		g_strfreev(str_step2);
		return FALSE;
	}
	g_strfreev(str_step2);
	return TRUE;
}

// разобрать пакет из чёрного ящика (не опрелеляется версия 2.0!)
static gboolean wialon_parse_packet_b(gchar *strv, struct packet_B *packet_b) {
	gchar **str_step2 = g_strsplit(strv, "|", -1);
	// определяем число подстрок
	int n_count = g_strv_length(str_step2);
	int n_count_real = 0;
	// нераспознанное сообщение
	if (n_count < 1) {
		g_strfreev(str_step2);
		return FALSE;
	}
	packet_b->type = PACKET_TYPE_WIALON_BLACK; // тип пакета
	// массив пакетов с данными или сокращённых пакетов с данными
	packet_b->packet_d = (struct packet_D*) g_malloc0(
			n_count * sizeof(struct packet_D));
	packet_b->raw_msg = (char**) g_malloc0(n_count * sizeof(char*));
	//packet_b->types = (uint8_t*)g_malloc0(n_count*sizeof(uint8_t));
	for (int i = 0; i < n_count; i++) {
		// сначала ищем пакет с данными
		if (wialon_parse_packet_d(str_step2[i],
				packet_b->packet_d + n_count_real)) {
			*(packet_b->raw_msg + n_count_real) = g_strdup_printf("#D#%s",
					str_step2[i]);
			//*(packet_b->types + n_count_real) = PACKET_TYPE_WIALON_DATA_FROM_BLACK;
			n_count_real++;
		} else
		// потом ищем сокращённый пакет с данными
		if (wialon_parse_packet_sd(str_step2[i],
				(struct packet_SD*) &(packet_b->packet_d + n_count_real)->packet_sd)) {
			*(packet_b->raw_msg + n_count_real) = g_strdup_printf("#SD#%s",
					str_step2[i]);
			//*(packet_b->types + n_count_real) = PACKET_TYPE_WIALON_SHORT_DATA_FROM_BLACK;
			n_count_real++;
		}

	}
	// сохраняем реальное число пакетов
	packet_b->n_count = n_count_real;
	// перевыделение памяти, если не все пакеты успешно распознаны
	if (n_count_real != n_count) {
		packet_b->packet_d = (struct packet_D*) g_realloc(packet_b->packet_d,
				n_count_real * sizeof(struct packet_D));
		packet_b->raw_msg = (char**) g_realloc(packet_b->raw_msg,
				n_count_real * sizeof(char*));
		//packet_b->types = (uint8_t*)g_realloc(packet_b->types, n_count_real*sizeof(uint8_t));
	}
	g_strfreev(str_step2);
	return TRUE;
}

// получить содержимое пакета из строки (без символов \r\n на конце)
// return FALSE в случае нераспознанной строки
gboolean wialon_parse_packet(char *str, struct PACKET_INFO *packet_info) {
	if (!str || !packet_info)
		return FALSE;
	// распознанное сообщение или нет
	gboolean parse_ok = FALSE;
	struct WIALON_PACKET *packet = (struct WIALON_PACKET*) g_malloc0(
			sizeof(struct WIALON_PACKET));
	packet_info->packet = packet;
	packet->raw_msg = g_strdup(str);
	gchar **str_step1 = g_strsplit(str, "#", 3);// не более 3 полей, в последнем поле, сообщении, могут быть такие символы тоже
	// определяем число подстрок
	guint count = g_strv_length(str_step1);
	// нераспознанная строка
	if (count != 3) {
		g_strfreev(str_step1);
		packet_info->is_parse = FALSE;	// не сохранённый пакет, рвём соединение
		return FALSE;
	}
	gchar *type_str = str_step1[1];
	gchar *msg_str = str_step1[2];
	// определяем тип принятого пакета
	if (!g_strcmp0(type_str, "L"))	// пакет логина
			{
		if (packet_info->ver != 20)
			packet->type = PACKET_TYPE_WIALON_LOGIN;
		else
			packet->type = PACKET_TYPE_WIALON_LOGIN2;
		if (wialon_parse_packet_l(msg_str, &packet->packet_l)) {
			parse_ok = TRUE;
			// запоминаем версию пакета
			packet_info->ver = (uint16_t)(packet->packet_l.ver);
			// запоминаем imei устройства
			packet_info->imei = g_strdup(packet->packet_l.imei);
		}
	}
	// определяем тип принятого пакета
	else if (!g_strcmp0(type_str, "VL"))	// пакет логина
			{
		if (packet_info->ver != 20)
			packet->type = PACKET_TYPE_WIALON_VLOGIN;
		else
			packet->type = PACKET_TYPE_WIALON_VLOGIN2;
		if (wialon_parse_packet_vl(msg_str, &packet->packet_l)) {
			parse_ok = TRUE;
			// запоминаем версию пакета
			packet_info->ver = (uint16_t)(packet->packet_l.ver);
			packet_info->imei = g_strdup(packet->packet_l.imei);
		}
	} else if (!g_strcmp0(type_str, "P"))	// пинговый пакет 
			{
		packet->type = PACKET_TYPE_WIALON_PING;
		parse_ok = TRUE;
	} else if (!g_strcmp0(type_str, "M"))	// сообщение для водителя
			{
		if (packet_info->ver != 20)
			packet->type = PACKET_TYPE_WIALON_DRIVER;
		else
			packet->type = PACKET_TYPE_WIALON_DRIVER2;
		if (wialon_parse_packet_m(msg_str, &packet->packet_m))
			parse_ok = TRUE;
	} else if (!g_strcmp0(type_str, "SD"))	// сокращённый пакет с данными
			{
		if (packet_info->ver != 20)
			packet->type = PACKET_TYPE_WIALON_SHORT_DATA;
		else
			packet->type = PACKET_TYPE_WIALON_SHORT_DATA2;
		if (wialon_parse_packet_sd(msg_str, &packet->packet_sd))
			parse_ok = TRUE;
	} else if (!g_strcmp0(type_str, "D"))	// пакет с данными
			{
		if (packet_info->ver != 20)
			packet->type = PACKET_TYPE_WIALON_DATA;
		else
			packet->type = PACKET_TYPE_WIALON_DATA2;
		if (wialon_parse_packet_d(msg_str, &packet->packet_d))
			parse_ok = TRUE;
	}

	else if (!g_strcmp0(type_str, "B"))	// пакет из чёрного ящика
			{
		if (packet_info->ver != 20)
			packet->type = PACKET_TYPE_WIALON_BLACK;
		else
			packet->type = PACKET_TYPE_WIALON_BLACK2;
		if (wialon_parse_packet_b(msg_str, &packet->packet_b))
			parse_ok = TRUE;
	} else if (!g_strcmp0(type_str, "I"))	// пакет с фотоизображением
			{
		if (packet_info->ver != 20)
			packet->type = PACKET_TYPE_WIALON_IMAGE;
		else
			packet->type = PACKET_TYPE_WIALON_IMAGE2;
	} else if (!g_strcmp0(type_str, "IT"))	// пакет с информацией о ddd-файле
		packet->type = PACKET_TYPE_WIALON_INFO_DDD2;
	else if (!g_strcmp0(type_str, "T"))	// пакет с блоком ddd-файла
		packet->type = PACKET_TYPE_WIALON_DATA_DDD2;
	// нераспознанный тип сообщения
	else {
		parse_ok = TRUE;
	}

	// запоминаем контрольную сумму
	if (parse_ok && packet_info->ver == 20) {
		int msg_str_len = strlen(msg_str) - 4;// вычитаем последние 4 символа, пример сообщения "...BB2B"
		if (msg_str_len > 0) {
			// сохраняем пришедшую контрольную сумму
			packet->crc16 = crc16_from_string((uint8_t*) msg_str + msg_str_len);// перевод строки вида "A0EF" в число
			// вычисляем контрольную сумму
			packet->crc16_calc = crc16((uint8_t*) msg_str, msg_str_len);
		}
	}
	g_strfreev(str_step1);
	// распознанное сообщение или нет
	packet_info->is_parse = parse_ok;
	return parse_ok;
}

// получение из packet_info доп. параметров
gboolean wialon_packet_get_dop_param(struct PACKET_INFO *packet_info,
		int *course, int *n_params, struct DOP_PARAM **params) {
	if (!packet_info || !packet_info->packet)
		return FALSE;
	struct WIALON_PACKET *packet = (struct WIALON_PACKET*) packet_info->packet;
	struct packet_D *packet_d = NULL;
	struct packet_SD *packet_sd = NULL;
	if (packet->type == PACKET_TYPE_WIALON_SHORT_DATA
			|| packet->type == PACKET_TYPE_WIALON_SHORT_DATA_FROM_BLACK
			|| packet->type == PACKET_TYPE_WIALON_SHORT_DATA2
			|| packet->type == PACKET_TYPE_WIALON_SHORT_DATA_FROM_BLACK2)
		packet_sd = &packet->packet_sd;
	if (packet->type == PACKET_TYPE_WIALON_DATA
			|| packet->type == PACKET_TYPE_WIALON_DATA_FROM_BLACK
			|| packet->type == PACKET_TYPE_WIALON_DATA2
			|| packet->type == PACKET_TYPE_WIALON_DATA_FROM_BLACK2) {
		packet_d = &packet->packet_d;
		packet_sd = &packet->packet_d.packet_sd;
	}
	if (packet_sd) {
		int16_t add_param = (packet_d) ? 4 : 0;	// дополнительные доп. параметры
		int16_t n_dop_param = (packet_d) ? packet_d->n_params : 0;
		struct DOP_PARAM *dop_params = (packet_d) ? packet_d->params : NULL;

		//double hdop;// снижение точности, если отсутствует, то передаётся NA
		//uint32_t inputs;//цифровые входы, каждый бит числа соответствует одному входу, начиная с младшего, если отсутствует, то передаётся NA
		//uint32_t outputs;//цифровые выходы, каждый бит числа соответствует одному входу, начиная с младшего, если отсутствует, то передаётся NA
		//double *adc;//аналоговые входы, последний элемент массива = NULL. Передается пустая строка, если нету никаких аналоговых входов
		//char *ibutton;// код ключа водителя, строка произвольной длины, если отсутствует, то передаётся NA

		// вычисляемые параметры из raw_data
		if (course)
			*course = packet_sd->course;		// курс, в градусах
		// доп. параметры, есть только в пакете packet_d
		if (n_dop_param > 0 && dop_params)	//  набор дополнительных параметров
				{
			// доп. параметры
			//wialon: uint8_t type;// TYPE – тип параметра, 1 –int / long long, 2 – double, 3 – string
			//base: uint8_t *type;// список типов доп. параметров // 1 –int / long long, 2 – double, 3 – string, 4 - битовый параметр (число каналов(используемых бит), значение)
			if (n_params)
				*n_params = n_dop_param + add_param;
			struct DOP_PARAM *all_dop_params = (struct DOP_PARAM*) g_malloc0(
					(*n_params) * sizeof(struct DOP_PARAM));
			if (params)
				*params = all_dop_params;
			all_dop_params[1].name = g_strdup("hdop");
			all_dop_params[1].type = DOP_PARAM_VAL_DOUBLE;
			all_dop_params[1].val_double = packet_d->hdop;
			all_dop_params[0].name = g_strdup("Digital_In");
			all_dop_params[0].type = DOP_PARAM_VAL_BITS;
			all_dop_params[0].val_int64 = packet_d->inputs;
			all_dop_params[2].name = g_strdup("Digital_Out");
			all_dop_params[2].type = DOP_PARAM_VAL_BITS;
			all_dop_params[2].val_int64 = packet_d->outputs;
			// количество спутников, целое число
			all_dop_params[3].name = g_strdup("Sats");
			all_dop_params[3].type = DOP_PARAM_VAL_INT;
			all_dop_params[3].val_int64 = packet_sd->sats;

			//data_cur->params[3].name = g_strdup(_("Driver key"));
			//data_cur->params[3].type = 6;
			//data_cur->params[3].val_int64 = packet_d->ibutton;
			int n_dp;
			for (n_dp = 0; n_dp < n_dop_param; n_dp++)
				copy_one_dop_param(all_dop_params + n_dp + add_param,
						dop_params + n_dp);
			/*
			 
			 {
			 all_dop_params[n_dp + add_param].name = g_strdup(dop_params[n_dp].name);
			 switch (dop_params[n_dp].type)
			 {
			 case 1:
			 data_cur->params[n_dp + add_param].type = 1;
			 data_cur->params[n_dp + add_param].val_int64 = dop_params[n_dp].val_int64;
			 break;
			 case 2:
			 data_cur->params[n_dp + add_param].type = 3;
			 data_cur->params[n_dp + add_param].val_double = dop_params[n_dp].val_double;
			 break;
			 case 3:
			 data_cur->params[n_dp + add_param].type = 4;
			 data_cur->params[n_dp + add_param].val_str = g_strdup(dop_params[n_dp].val_str);
			 break;
			 }
			 }*/
		}

	}
	//clean_wialon_packet(packet);
	return TRUE;
}

// получение текстового сообщения из пакета
char *wialon_get_text_message(PACKET_INFO *packet_info) {
	struct WIALON_PACKET *wialon_packet =
			(packet_info) ? (struct WIALON_PACKET *) packet_info->packet : NULL;
	if (wialon_packet
			&& (wialon_packet->type == PACKET_TYPE_WIALON_DRIVER
					|| wialon_packet->type == PACKET_TYPE_WIALON_DRIVER2)) {
		return g_strdup(wialon_packet->packet_m.msg);
	}
	return NULL;
}

#if __cplusplus
}  // End of the 'extern "C"' block
#endif
