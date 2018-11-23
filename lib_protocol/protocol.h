// protocol.h : Протоколы устройств
//

#ifndef _PROTOCOL_H_
#define _PROTOCOL_H_

#include "protocol_parse.h"
#include "protocol_wialon_ips.h"
#include "protocol_osmand.h"
#include "protocol_gt06.h"
#include "protocol_babywatch.h"

// // узнать номер протокола по номеру порта
int get_protocol_num_by_port(int server_port);

// обработать запрос
// return: id добавленной в базу записи
int64_t packet_process(struct PACKET_INFO *wialon_info);

// подготовить ответ для устройства на запрос
// len_out - длина возвращаемых данных
gchar* packet_prepare_answer(struct PACKET_INFO *packet_info, int *len_out);

// подготовить ответ пользователю с точками из пакета в формате json: {"action":"monitor", "rows":xx, "data":xx }
// last_id - id последней записи, добавленной в бд
char* packet_prepare_data(int64_t last_id, struct PACKET_INFO *packet_info);

// сформировать команду для отправки устройству
uint8_t* packet_prepare_cmd_to_dev(int protocol, const char *cmd_text,
		int *len_out);

// подготовить ответ пользователю с точками из пакета в формате json: // {"action":"updatestate", "imei":xx, "time":xx, "state":TRUE/FALSE=>in/out }
// state - состояние устройства (TRUE-подключилось к серверу/FALSE-отключилось от сервера)
// time_new - новое время последнего сеанса связи для устройства
char* online_prepare_data(gboolean state, int64_t time_new,
		struct PACKET_INFO *packet_info);

#endif	//_PROTOCOL_H_
