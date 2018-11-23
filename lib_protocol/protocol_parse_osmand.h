// protocol_parse_osmand.h : Протокол OsmAnd
//

#ifndef _PROTOCOL_PARSE_OSMAND_H_
#define _PROTOCOL_PARSE_OSMAND_H_

// Make sure we can call this stuff from C++.
#if __cplusplus
extern "C" {
#endif

// данные для одного пакета
struct OSMAND_PACKET {
	char *imei;
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

// очистить структуру OSMAND_PACKET - содержимое пакета
void clean_osmand_packet(void *packet_in);
// удалить структуру OSMAND_PACKET - содержимое пакета
void free_osmand_packet(void *packet_in);

// получить содержимое пакета из строки (без символов \r\n на конце)
// return FALSE в случае нераспознанной строки
gboolean osmand_parse_packet(char *str, struct PACKET_INFO *packet_info);

// получение текстового сообщения из пакета
char* osmand_get_text_message(struct PACKET_INFO *packet_info);

// получение из packet_info доп. параметров
gboolean osmand_packet_get_dop_param(struct PACKET_INFO *packet_info,
		int *course, int *n_params, struct DOP_PARAM **params);

#if __cplusplus
}  // End of the 'extern "C"' block
#endif

#endif	//_PROTOCOL_PARSE_OSMAND_H_
