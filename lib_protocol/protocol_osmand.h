// protocol_osmand.h : Работа с протоколом OsmAnd внутри сервера
//

#ifndef _PROTOCOL_OSMAND_H_
#define _PROTOCOL_OSMAND_H_

// Make sure we can call this stuff from C++.
#if __cplusplus
extern "C" {
#endif

#include "protocol_parse_wialon_ips.h"

// обработать запрос
// return: id добавленной в базу записи
int64_t osmand_process(struct PACKET_INFO *packet_info);

// подготовить ответ на запрос
// len_out - длина возвращаемых данных
gchar* osmand_prepare_answer(struct PACKET_INFO *packet_info, int *len_out);

// записать пакет в БД
// return: id добавленной в базу записи
int64_t base_save_packet_osmand(void *db, struct PACKET_INFO *packet_info);

// преобразовать полученные данные в строку для передачи
// [in] id - id последней записи, добавленной в бд
// [out] n_rows - число строк на выходе
char* osmand_get_data_str(int64_t id, struct PACKET_INFO *packet_info,
		int *n_rows);

#if __cplusplus
}  // End of the 'extern "C"' block
#endif

#endif	//_PROTOCOL_OSMAND_H_
