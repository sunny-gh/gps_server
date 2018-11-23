// protocol_parse.cpp : Протоколы устройств
//

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <glib.h>
#include "protocol_parse.h"

// Make sure we can call this stuff from C++.
#if __cplusplus
extern "C" {
#endif

// округлить число
int okrugl(double in) {
	double celoe = floor(in);
	double num = in - celoe;
	if (num >= 0.5)
		return (int) (celoe + 1);
	else
		return (int) celoe;
}

// узнать имя протокола по номеру
char* get_proto_by_num(int protocol_num) {
	char *str = NULL;
	switch (protocol_num) {
	case PROTOCOL_NA:
		str = g_strdup("n/a");
		break;
	case PROTOCOL_WIALON:
		str = g_strdup("wialon");
		break;
	case PROTOCOL_OSMAND:
		str = g_strdup("osmand");
		break;
	case PROTOCOL_TRACCAR:
		str = g_strdup("traccar");
		break;
	case PROTOCOL_GT06:
		str = g_strdup("gt06");
		break;
	case PROTOCOL_BABYWATCH:
		str = g_strdup("babywatch");
		break;
	default:
		str = g_strdup_printf("unknown #%d", protocol_num);
		break;
	}
	return str;
}

// узнать номер протокола по типу сообщения
int get_proto_by_type_pkt(int type_pkt, uint16_t *protocol_ver) {
	int protocol_num = PROTOCOL_NA;
	if (type_pkt >= PACKET_TYPE_WIALON_IPS_START
			&& type_pkt < PACKET_TYPE_WIALON_IPS_STOP) {
		protocol_num = PROTOCOL_WIALON;
		if (protocol_ver)
			*protocol_ver = 11;
	} else if (type_pkt >= PACKET_TYPE_WIALON_IPS2_START
			&& type_pkt < PACKET_TYPE_WIALON_IPS2_STOP) {
		protocol_num = PROTOCOL_WIALON;
		if (protocol_ver)
			*protocol_ver = 20;
	} else if (type_pkt >= PACKET_TYPE_OSMAND_START
			&& type_pkt < PACKET_TYPE_OSMAND_STOP) {
		protocol_num = PROTOCOL_OSMAND;
		if (protocol_ver)
			*protocol_ver = 1;
	} else if (type_pkt >= PACKET_TYPE_TRACCAR_START
			&& type_pkt < PACKET_TYPE_TRACCAR_STOP) {
		protocol_num = PROTOCOL_TRACCAR;
		if (protocol_ver)
			*protocol_ver = 1;
	} else if (type_pkt >= PACKET_TYPE_GT06_START
			&& type_pkt < PACKET_TYPE_GT06_STOP) {
		protocol_num = PROTOCOL_GT06;
		if (protocol_ver)
			*protocol_ver = 1;
	} else if (type_pkt >= PACKET_TYPE_BABYWATCH_START
			&& type_pkt < PACKET_TYPE_BABYWATCH_STOP) {
		protocol_num = PROTOCOL_BABYWATCH;
		if (protocol_ver)
			*protocol_ver = 1;
	}
	return protocol_num;
}

// узнать номер протокола по названию и версию протокола, если необходимо
int get_protocol_num_by_name(const char *protocol_name, uint16_t *ver) {
	int protocol_num = PROTOCOL_NA;
	if (!g_ascii_strncasecmp("wialon", protocol_name, 6)) {
		protocol_num = PROTOCOL_WIALON;
		if (ver) {
			if (!g_strcmp0("wialon 1.1", protocol_name))
				*ver = 11;
			if (!g_strcmp0("wialon 2.0", protocol_name))
				*ver = 20;
		}
	} else if (!g_ascii_strncasecmp("osmand", protocol_name, 6)) {
		protocol_num = PROTOCOL_OSMAND;
		if (ver)
			*ver = 1;
	} else if (!g_ascii_strncasecmp("torque", protocol_name, 6)) {
		protocol_num = PROTOCOL_OSMAND;
		if (ver)
			*ver = 1;
	} else if (!g_ascii_strncasecmp("traccar", protocol_name, 7)) {
		protocol_num = PROTOCOL_TRACCAR;
		if (ver)
			*ver = 1;
	} else if (!g_ascii_strncasecmp("gt06", protocol_name, 4)) {
		protocol_num = PROTOCOL_GT06;
		if (ver)
			*ver = 1;
	} else if (!g_ascii_strncasecmp("babywatch", protocol_name, 9)) {
		protocol_num = PROTOCOL_BABYWATCH;
		if (ver)
			*ver = 1;
	}
	return protocol_num;
}

// скопировать один доп.параметр
void copy_one_dop_param(struct DOP_PARAM *dop_param_to,
		struct DOP_PARAM *dop_param_from) {
	memcpy(dop_param_to, dop_param_from, sizeof(struct DOP_PARAM));
	dop_param_to->name = g_strdup(dop_param_from->name);
	// если строковый параметр, то допонительно копируем строку
	if (dop_param_from->type == DOP_PARAM_VAL_STR)
		dop_param_to->val_str = g_strdup(dop_param_from->val_str);
	/*dop_param_to->type = dop_param_from->type;
	 switch (dop_param_from->type)
	 {
	 case DOP_PARAM_VAL_INT:
	 case DOP_PARAM_VAL_BITS:// битовый параметр
	 dop_param_to->val_int64 = dop_param_from->val_int64;
	 break;
	 case DOP_PARAM_VAL_DOUBLE:
	 dop_param_to->val_double = dop_param_from->val_double;
	 break;
	 case DOP_PARAM_VAL_STR:
	 dop_param_to->val_str = g_strdup(dop_param_from->val_str);
	 break;
	 }*/
}

// заполнить доп. параметр
void fill_dop_param(struct DOP_PARAM *dop_param, const char *param_name,
		const char *param_val, int type) {
	if (!dop_param)
		return;
	//memset(dop_param, 0, sizeof(struct DOP_PARAM));
	dop_param->name = g_strdup(param_name);
	dop_param->type = type;
	if (dop_param->type == DOP_PARAM_VAL_DOUBLE) // если c плавающей точкой
		dop_param->val_double =
				(param_val) ? g_ascii_strtod(param_val, NULL) : 0;
	if (dop_param->type == DOP_PARAM_VAL_INT) // если целое число
		dop_param->val_int64 =
				(param_val) ? g_ascii_strtoll(param_val, NULL, 10) : 0;
	if (dop_param->type == DOP_PARAM_VAL_STR) // если строка
		dop_param->val_str = g_strdup((const char*) param_val);
}

// заполнить текстовый доп. параметр
void fill_dop_param_str(struct DOP_PARAM *dop_param, const char *param_name,
		const char *param_val) {
	fill_dop_param(dop_param, param_name, param_val, DOP_PARAM_VAL_STR);
}

/*/ очистить структуру DOP_PARAM
 void clean_dop_param(struct DOP_PARAM *param)
 {
 g_free(param->name);
 if (param->type == DOP_PARAM_VAL_STR)
 g_free(param->val_str);
 }

 // удалить структуру DOP_PARAM
 void free_dop_param(struct DOP_PARAM *param)
 {
 clean_dop_param(param);
 g_free(param);
 }*/

void free_all_dop_param(int n_params, struct DOP_PARAM *params) {
	int i;
	if (!params)
		return;
	for (i = 0; i < n_params; i++) {
		g_free(params[i].name);
		if (params[i].type == DOP_PARAM_VAL_STR)
			g_free(params[i].val_str);
	}
	//clean_dop_param(params+i);
	g_free(params);
}

// очистить часть PACKET_INFO - содержимое пакета
void clean_packet_info(struct PACKET_INFO *packet_info) {
	switch (packet_info->protocol) //switch (packet_type)
	{
	case PROTOCOL_WIALON:
		free_wialon_packet(packet_info->packet);
		break;
	case PROTOCOL_OSMAND:
		free_osmand_packet(packet_info->packet);
		break;
	case PROTOCOL_GT06:
		free_gt06_packet(packet_info->packet);
		break;
	case PROTOCOL_BABYWATCH:
		free_babywatch_packet(packet_info->packet);
		break;
	}
	packet_info->packet = NULL;
}

// удалить структуру PACKET_INFO
void free_packet_info(struct PACKET_INFO *packet_info) {
	clean_packet_info(packet_info);

	g_free(packet_info->imei);
	g_free(packet_info->client_ip_addr);
	g_free(packet_info->proxy_str);
	g_free(packet_info);
}

// получить содержимое пакета из строки
// return FALSE в случае нераспознанной строки
gboolean packet_parse(char *str, int src_len, struct PACKET_INFO *packet_info) {
	switch (packet_info->protocol) {
	case PROTOCOL_WIALON:
		return wialon_parse_packet(str, packet_info);
		break; // (без символов \r\n на конце)
	case PROTOCOL_OSMAND:
		return osmand_parse_packet(str, packet_info);
		break;
	case PROTOCOL_GT06:
		return gt06_parse_packet(str, src_len, packet_info);
		break;
	case PROTOCOL_BABYWATCH:
		return babywatch_parse_packet(str, src_len, packet_info);
		break;
	}
	return FALSE;
}

// это пакет с авторизацией
gboolean packet_is_login(int type_pkt) {
	if (type_pkt == PACKET_TYPE_WIALON_LOGIN
			|| type_pkt == PACKET_TYPE_WIALON_VLOGIN
			|| type_pkt == PACKET_TYPE_GT06_LOGIN)
		return TRUE;
	return FALSE;
}

// это пакет с текстовым сообщением
gboolean packet_is_message(int type_pkt) {
	if (type_pkt == PACKET_TYPE_WIALON_DRIVER
			|| type_pkt == PACKET_TYPE_WIALON_DRIVER2
			|| type_pkt == PACKET_TYPE_GT06_STRING)
		return TRUE;
	return FALSE;
}

// это пакет с данными
gboolean packet_is_data(int type_pkt) {
	if (type_pkt == PACKET_TYPE_WIALON_SHORT_DATA
			|| type_pkt == PACKET_TYPE_WIALON_DATA
			|| type_pkt == PACKET_TYPE_WIALON_SHORT_DATA_FROM_BLACK
			|| type_pkt == PACKET_TYPE_WIALON_DATA_FROM_BLACK
			|| type_pkt == PACKET_TYPE_WIALON_SHORT_DATA2
			|| type_pkt == PACKET_TYPE_WIALON_DATA2
			|| type_pkt == PACKET_TYPE_WIALON_SHORT_DATA_FROM_BLACK2
			|| type_pkt == PACKET_TYPE_WIALON_DATA_FROM_BLACK2
			|| type_pkt == PACKET_TYPE_OSMAND_DATA
			|| type_pkt == PACKET_TYPE_GT06_GPS
			|| type_pkt == PACKET_TYPE_GT06_LBS
			|| type_pkt == PACKET_TYPE_GT06_GPS_LBS
			|| type_pkt == PACKET_TYPE_GT06_GPS_LBS_STATUS
			|| type_pkt == PACKET_TYPE_GT06_STATUS
			|| type_pkt == PACKET_TYPE_BABYWATCH_DATA)
		return TRUE;
	return FALSE;
}

// получение из packet_info доп. параметров
gboolean packet_get_dop_param(struct PACKET_INFO *packet_info, int *course,
		int *n_params, struct DOP_PARAM **params) {
	if (!packet_info)
		return FALSE;
	switch (packet_info->protocol) {
	case PROTOCOL_WIALON:
		return wialon_packet_get_dop_param(packet_info, course, n_params,
				params);
		break;
	case PROTOCOL_OSMAND:
		return osmand_packet_get_dop_param(packet_info, course, n_params,
				params);
		break;
	case PROTOCOL_GT06:
		return gt06_packet_get_dop_param(packet_info, course, n_params, params);
		break;
	case PROTOCOL_BABYWATCH:
		return babywatch_packet_get_dop_param(packet_info, course, n_params,
				params);
		break;
	}
	return TRUE;
}

// получение текстового сообщения из пакета
char* packet_get_message(int type_pkt, char *raw_data, int raw_data_len) {
	char *ret_str = NULL;
	if (!packet_is_message(type_pkt))
		return NULL;
	PACKET_INFO *packet_info = (PACKET_INFO *) g_malloc0(
			sizeof(struct PACKET_INFO)); //memset(&pkt_info, 0, sizeof(struct PACKET_INFO));
	// узнать номер протокола по типу сообщения
	packet_info->protocol = get_proto_by_type_pkt(type_pkt, &packet_info->ver);
	// разобрать пришедший пакет
	if (packet_parse(raw_data, raw_data_len, packet_info)) {
		switch (packet_info->protocol) {
		case PROTOCOL_WIALON:
			ret_str = wialon_get_text_message(packet_info);
			break;
		case PROTOCOL_GT06:
			ret_str = gt06_get_text_message(packet_info);
			break;
		}
	}
	free_packet_info(packet_info);
	return ret_str;
}

#if __cplusplus
}  // End of the 'extern "C"' block
#endif
