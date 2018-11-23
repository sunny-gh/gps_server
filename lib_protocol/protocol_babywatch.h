// protocol_babywatch.h : Работа с протоколом Baby Watch Q90 внутри сервера
//

#ifndef _PROTOCOL_BABYWATCH_H_
#define _PROTOCOL_BABYWATCH_H_

// Make sure we can call this stuff from C++.
#if __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "protocol_parse_babywatch.h"

// обработать запрос
// return: id добавленной в базу записи
int64_t babywatch_process(struct PACKET_INFO *packet_info);

// подготовить ответ на запрос
// len_out - длина возвращаемых данных
char* babywatch_prepare_answer(struct PACKET_INFO *packet_info, int *len_out);

// сформировать команду для отправки устройству
uint8_t* babywatch_prepare_cmd_to_dev(const char *cmd_text, int *len_out);

// записать пакет в БД
// return: id добавленной в базу записи
int64_t base_save_packet_babywatch(void *db, struct PACKET_INFO *packet_info);

// преобразовать полученные данные в строку для передачи
// [in] id - id последней записи, добавленной в бд
// [out] n_rows - число строк на выходе
char* babywatch_get_data_str(int64_t id, struct PACKET_INFO *packet_info,
		int *n_rows);

#if __cplusplus
}  // End of the 'extern "C"' block
#endif

#endif	//_PROTOCOL_BABYWATCH_H_
