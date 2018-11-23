// protocol.cpp : Протоколы устройств
//

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <glib.h>
#include <json-glib/json-glib.h>
#include "../server_thread_ports.h" // для номеров портов
#include "../base_lib.h"
#include "protocol.h"

// узнать номер протокола по номеру порта
int get_protocol_num_by_port(int server_port) {
	int packet_protocol;
	switch (server_port) {
	case PORT_WIALON:
		packet_protocol = PROTOCOL_WIALON;
		break;
	case PORT_OSMAND:
		packet_protocol = PROTOCOL_OSMAND;
		break;
	case PORT_TRACCAR:
		packet_protocol = PROTOCOL_TRACCAR;
		break;
	case PORT_GT06:
		packet_protocol = PROTOCOL_GT06;
		break;
	case PORT_BABYWATCH:
		packet_protocol = PROTOCOL_BABYWATCH;
		break;
	default:
		packet_protocol = PROTOCOL_NA;
	}
	return packet_protocol;
}

// обработать запрос
// return: id добавленной в базу записи
int64_t packet_process(struct PACKET_INFO *packet_info) {
	switch (packet_info->protocol) {
	case PROTOCOL_WIALON:
		return wialon_process(packet_info);
		break;
	case PROTOCOL_OSMAND:
		return osmand_process(packet_info);
		break;
	case PROTOCOL_GT06:
		return gt06_process(packet_info);
		break;
	case PROTOCOL_BABYWATCH:
		return babywatch_process(packet_info);
		break;
	}
	return 0;
}

// подготовить ответ для устройства на запрос
// len_out - длина возвращаемых данных
gchar* packet_prepare_answer(struct PACKET_INFO *packet_info, int *len_out) {
	switch (packet_info->protocol) {
	case PROTOCOL_WIALON:
		return wialon_prepare_answer(packet_info, len_out);
		break;
	case PROTOCOL_OSMAND:
		return osmand_prepare_answer(packet_info, len_out);
		break;
	case PROTOCOL_GT06:
		return gt06_prepare_answer(packet_info, len_out);
		break;
	case PROTOCOL_BABYWATCH:
		return babywatch_prepare_answer(packet_info, len_out);
		break;
	}
	return FALSE;
}

// сформировать команду для отправки устройству
uint8_t* packet_prepare_cmd_to_dev(int protocol, const char *cmd_text,
		int *len_out) {
	switch (protocol) {
	case PROTOCOL_WIALON:
		return wialon_prepare_cmd_to_dev(cmd_text, len_out);
		break;
	case PROTOCOL_OSMAND:
		return NULL;
		break;
	case PROTOCOL_GT06:
		return gt06_prepare_cmd_to_dev(cmd_text, len_out);
		break; // сформировать команду для отправки устройству
	case PROTOCOL_BABYWATCH:
		return babywatch_prepare_cmd_to_dev(cmd_text, len_out);
		break;
	}
	return NULL;
}

// подготовить ответ пользователю с точками из пакета в формате json: {"action":"updatedata", "protocol":xx, "imei":xx, "rows":xx, "data":xx }
// last_id - id последней записи, добавленной в бд
char* packet_prepare_data(int64_t last_id, struct PACKET_INFO *packet_info) {
	// если был пакет с некорректным содержанием или неавторизованный пакет
	if (!packet_info || !packet_info->is_parse || !packet_info->is_auth) {
		return NULL;
	}
	// преобразовать полученные данные в строку для передачи
	char *str = NULL;
	int n_rows = 0;
	switch (packet_info->protocol) {
	case PROTOCOL_WIALON:
		str = wialon_get_data_str(last_id, packet_info, &n_rows);
		break;
	case PROTOCOL_OSMAND:
		str = osmand_get_data_str(last_id, packet_info, &n_rows);
		break;
	case PROTOCOL_GT06:
		str = gt06_get_data_str(last_id, packet_info, &n_rows);
		break;
	case PROTOCOL_BABYWATCH:
		str = babywatch_get_data_str(last_id, packet_info, &n_rows);
		break;
	}
	// нечего передавать на выход
	if (!str || !n_rows)
		return NULL;
	// сформировать выходную json строку
	JsonBuilder *builder = json_builder_new();
	json_builder_begin_object(builder);
	json_builder_set_member_name(builder, "action");
	json_builder_add_string_value(builder, "update");
	json_builder_set_member_name(builder, "protocol");
	json_builder_add_int_value(builder, packet_info->protocol);
	json_builder_set_member_name(builder, "imei");
	json_builder_add_string_value(builder, packet_info->imei);
	json_builder_set_member_name(builder, "rows");
	json_builder_add_int_value(builder, n_rows);
	//json_builder_set_member_name(builder, "data");
	//json_builder_add_string_value(builder, str);
	{
		json_builder_set_member_name(builder, "data");
		int len = strlen(str);
		gchar *str_base64 = g_base64_encode((const guchar*) str, len);
		//int len2 = strlen(str_base64);
		json_builder_add_string_value(builder, (const char*) str_base64);
		g_free(str_base64);
	}
	json_builder_end_object(builder);
	// получаем выходную json строку
	JsonGenerator *gen = json_generator_new();
	JsonNode *root = json_builder_get_root(builder);
	json_generator_set_root(gen, root);
	char *ret_str = json_generator_to_data(gen, NULL);

	json_node_free(root);
	g_object_unref(gen);
	g_object_unref(builder);
	g_free(str);
	return ret_str;
}

// подготовить ответ пользователю с точками из пакета в формате json: // {"action":"updatestate", "imei":xx, "time":xx, "state":TRUE/FALSE=>in/out }
// state - состояние устройства (TRUE-подключилось к серверу/FALSE-отключилось от сервера)
// time_new - новое время последнего сеанса связи для устройства
char* online_prepare_data(gboolean state, int64_t time_new,
		struct PACKET_INFO *packet_info) {
	// если был пакет с некорректным содержанием или неавторизованный пакет
	if (!packet_info || !packet_info->is_parse || !packet_info->is_auth) {
		return NULL;
	}

	// узнать время последнего сеанса связи устройства по imei
	// return: время или -1 в случае ошибки, 0 в случае, если никогда не было сеанса связи
	int64_t cur_time = base_get_dev_last_time(packet_info->imei);

	// сформировать выходную json строку
	JsonBuilder *builder = json_builder_new();
	json_builder_begin_object(builder);
	json_builder_set_member_name(builder, "action");
	json_builder_add_string_value(builder, "updatestate");
	json_builder_set_member_name(builder, "imei");
	json_builder_add_string_value(builder, packet_info->imei);
	json_builder_set_member_name(builder, "time");
	json_builder_add_int_value(builder, cur_time);
	json_builder_set_member_name(builder, "state");
	json_builder_add_boolean_value(builder, state);
	json_builder_end_object(builder);
	// получаем выходную json строку
	JsonGenerator *gen = json_generator_new();
	JsonNode *root = json_builder_get_root(builder);
	json_generator_set_root(gen, root);
	char *ret_str = json_generator_to_data(gen, NULL);

	json_node_free(root);
	g_object_unref(gen);
	g_object_unref(builder);
	return ret_str;
}
