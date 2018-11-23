// protocol_parse_wialon_ips.h : Протокол Wialon IPS
//

#ifndef _PROTOCOL_PARSE_WIALON_IPS_H_
#define _PROTOCOL_PARSE_WIALON_IPS_H_

// Make sure we can call this stuff from C++.
#if __cplusplus
extern "C" {
#endif

#include <stdint.h>

// пакет с логином
struct packet_L {
	uint8_t type; // тип пакета
	int ver;
	char *imei;
	char *password;
};
// пакет c сообщением водителю
struct packet_M {
	uint8_t type; // тип пакета
	char *msg;
};
// сокращённый пакет с данными
struct packet_SD {
	uint8_t type; // тип пакета
	uint16_t is_param; // если есть параметр, то бит=1, иначе =0 (в случае передачи NA)
	int64_t jdate; // юлианская дата, в секундах от 2000г
	double lat; // широта, если отсутствует, то передаётся NA;NA 
	double lon; // долгота, если отсутствует, то передаётся NA;NA 
	int speed; // скорость, в км/ч, если отсутствует, то передаётся NA
	int course; // курс (азимут), в градусах, если отсутствует, то передаётся NA
	int height; // высота, целое число, в метрах, если отсутствует, то передаётся NA
	int sats; // количество спутников, целое число, если отсутствует, то передаётся NA
};
// пакет с данными
struct packet_D {
	uint8_t type; // тип пакета
	struct packet_SD packet_sd; // сокращённый пакет с данными
	// дополнительные параметры
	double hdop;	// снижение точности, если отсутствует, то передаётся NA
	uint32_t inputs;//цифровые входы, каждый бит числа соответствует одному входу, начиная с младшего, если отсутствует, то передаётся NA
	uint32_t outputs;//цифровые выходы, каждый бит числа соответствует одному входу, начиная с младшего, если отсутствует, то передаётся NA
	double *adc;//аналоговые входы, последний элемент массива = NULL. Передается пустая строка, если нету никаких аналоговых входов
	char *ibutton;// код ключа водителя, строка произвольной длины, если отсутствует, то передаётся NA
	int16_t n_params;	// число доп.параметров
	struct DOP_PARAM *params;//  набор дополнительных параметров через запятую.
};

// пакет c чёрным ящиком
struct packet_B {
	uint8_t type;	// тип пакета
	int n_count;	// число пакетов внутри
	// массив пакетов с данными
	struct packet_D *packet_d;
	//uint8_t *types;// типы субпакета
	char **raw_msg;	// сырое содержимое субпакетов с данными
};

// данные для одного пакета
struct WIALON_PACKET {
	uint8_t type;	// тип пакета
	// для версии 2.0
	uint16_t crc16;	// принятая crc16 для пакета
	uint16_t crc16_calc;	// расчитанная crc16 для пакета	
	char *raw_msg;	// сырое содержимое пакета
	// содержимое пакета
	union {
		// пакет с логином
		struct packet_L packet_l;
		// пинговый пакет
		//struct packet_P packet_p;
		// пакет c сообщением водителю
		struct packet_M packet_m;
		// сокращённый пакет с данными
		struct packet_SD packet_sd;
		// пакет с данными
		struct packet_D packet_d;
		// пакет c чёрным ящиком
		struct packet_B packet_b;
	};
};

// очистить структуру WIALON_PACKET - содержимое пакета
void clean_wialon_packet(void *wialon_packet);
// удалить структуру WIALON_PACKET - содержимое пакета
void free_wialon_packet(void *wialon_packet);

// удалить часть структуры WIALON_INFO - содержимое пакета
//void free_wialon_packet(struct WIALON_INFO *wialon_info);
// удалить структуру WIALON_INFO
//void free_wialon_info(struct WIALON_INFO *wialon_info);

// получить текущее Юлианское время в секундах
//uint64_t get_cur_jdate_time(void);-> my_time_get_cur_msec2000()

// получить содержимое пакета из строки (без символов \r\n на конце)
// return FALSE в случае нераспознанной строки
gboolean wialon_parse_packet(char *str, struct PACKET_INFO *packet_info);

// получение из packet_info доп. параметров
gboolean wialon_packet_get_dop_param(struct PACKET_INFO *packet_info,
		int *course, int *n_params, struct DOP_PARAM **params);

// получение текстового сообщения из пакета
char* wialon_get_text_message(struct PACKET_INFO *packet_info);

#if __cplusplus
}  // End of the 'extern "C"' block
#endif

#endif	//_PROTOCOL_PARSE_WIALON_IPS_H_
