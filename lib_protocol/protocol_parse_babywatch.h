// protocol_parse_babywatch.h : Протокол Baby Watch Q90
// протокол from www.mesidatech.com/en/download/download-82-515.html

#ifndef _PROTOCOL_PARSE_BABYWATCH_H_
#define _PROTOCOL_PARSE_BABYWATCH_H_

// Make sure we can call this stuff from C++.
#if __cplusplus
extern "C" {
#endif
#include "../lib_net/sock_func.h"

// данные для одного пакета
struct BABYWATCH_PACKET {
	char *imei;
	char *prefix; // префикс сообщения (нужен для ответа)
	char *type_str; // тип пакета
	int64_t jdate; // юлианская дата, UTC в секундах от 2000г
	double lat; // широта
	double lon; // долгота 
	int speed; // скорость, в км/ч
	int course; // курс (азимут, bearing), в градусах
	int height; // высота, целое число, в метрах
	double hdop; // количество спутников
	int16_t n_params; // число доп.параметров
	struct DOP_PARAM *params; //  набор дополнительных параметров
	char *raw_msg; // сырое содержимое пакета

};

// очистить структуру BABYWATCH_PACKET - содержимое пакета
void clean_babywatch_packet(void *packet_in);
// удалить структуру BABYWATCH_PACKET - содержимое пакета
void free_babywatch_packet(void *packet_in);

// получить содержимое пакета из строки
// return FALSE в случае нераспознанной строки
gboolean babywatch_parse_packet(char *str, int str_len,
		struct PACKET_INFO *packet_info);

// получение текстового сообщения из пакета
char* babywatch_get_text_message(struct PACKET_INFO *packet_info);

// получение из packet_info доп. параметров
gboolean babywatch_packet_get_dop_param(struct PACKET_INFO *packet_info,
		int *course, int *n_params, struct DOP_PARAM **params);

#if __cplusplus
}  // End of the 'extern "C"' block
#endif

#endif	//_PROTOCOL_PARSE_BABYWATCH_H_
