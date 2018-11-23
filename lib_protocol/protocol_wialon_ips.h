// protocol_wialon_ips.h : Работа с протоколом Wialon IPS внутри сервера
//

#ifndef _WIALON_IPS_H_
#define _WIALON_IPS_H_

#include "protocol_parse_wialon_ips.h"

// обработать запрос
// return: id добавленной в базу записи
int64_t wialon_process(struct PACKET_INFO *packet_info);

// подготовить ответ на запрос
// len_out - длина возвращаемых данных
gchar* wialon_prepare_answer(struct PACKET_INFO *packet_info, int *len_out);

// сформировать команду для отправки устройству
uint8_t* wialon_prepare_cmd_to_dev(const char *cmd_text, int *len_out);

// записать пакет в БД
// return: id добавленной в базу записи
int64_t base_save_packet_wialon(void *db, struct PACKET_INFO *packet_info);

// преобразовать полученные данные в строку для передачи
// [in] id - id последней записи, добавленной в бд
// [out] n_rows - число строк на выходе
char* wialon_get_data_str(int64_t id, struct PACKET_INFO *packet_info,
		int *n_rows);

#endif	//_WIALON_IPS_H_
