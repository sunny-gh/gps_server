// protocol_parse_gt06.h : Протокол GT06
//

#ifndef _PROTOCOL_PARSE_GT06_H_
#define _PROTOCOL_PARSE_GT06_H_

// Make sure we can call this stuff from C++.
#if __cplusplus
extern "C" {
#endif

// пакет с логином
struct packet_GT06_L {
	char *imei;
};

// пакет с GPS данными
struct packet_GT06_GPS {
	double lat; // широта, в градусах
	double lon; // долгота, в градусах
	int16_t course; // курс (азимут), в градусах
	int8_t speed; // скорость, в км/ч, 0-255
	//int height;// высота, целое число, в метрах
	int8_t sats;	// количество спутников, целое число

	// битовые позиции
	int8_t gps_is_rt;	//GPS real-time/differential positioning 
	int8_t gps_have_pos;	//GPS having been positioning or not 
};

// пакет с LBS данными
struct packet_GT06_LBS {
	uint16_t MCC;// код страны в системе мобильной связи (MCC) [country code of mobile user is Mobile Country Code (MCC). Value ranges from 0x0000 to 0x03E7 ]
	uint8_t MNC;// код сети в системе мобильной связи (MNC) [Mobile Network Code (MNC)]
	uint16_t LAC;// код местоположения (LAC) [Location Area Code(LAC), ranges from 0x0001－0xFFFE(not include 0x0001 and 0xFFFE).One location area can contain one or more areas.]
	uint32_t cell_id;// уникальный идентификатор конкретной соты в сети [Cell Tower ID(Cell ID) ranges from 0x000000 to 0xFFFFFF]
	/* пример:	Beeline
	 MCC: 250 (Russian Federation)
	 MNC: 99 (Beeline)
	 LAC: 37453
	 cell ID: 31111093
	 */
};

// пакет со статусом
struct packet_GT06_status {
	int8_t info;	// флаги состояния устройства
	/*
	 0 bit		0: Deactivated 1: Activated (или 0：Disarm		1：Arm)
	 1 bit		0：Low ACC		1：High ACC
	 2 bit		0: Charge Off 1: Charge On
	 3-5 bit		000: Normal 001: Shock Alarm 010: Power Cut Alarm 011: Low Battery Alarm 100: SOS
	 6 bit		0：GPS has not located		1：GPS has located
	 7 bit		0: gas oil and electricity connected 1: oil and electricity disconnected
	 */
	int8_t voltage;	// напряжение питания, 0-6, чем больше, тем выше  [Decimal, range from 0-6]
	int8_t signal;// уровень мощности сигнала GSM, 0-4, чем больше, тем выше   [GSM signal strength degree]
	// reserved
	uint8_t alarm;	// сигнальные биты 
	uint8_t lang;	// язык (0x01: Chinese, 0x02: English)
};

// пакет с текстом
struct packet_GT06_str {
	char *msg;
	uint8_t lang;	// язык (0x01: Chinese, 0x02: English)
};

// данные для одного пакета
struct GT06_PACKET {
	uint8_t type;	// тип пакета
	//uint16_t crc16;// принятая crc16 для пакета
	uint8_t *raw_msg;	// сырое содержимое пакета
	uint8_t raw_msg_len;	// длина сырого содержимого пакета
	uint16_t num;	// порядковый номер пакета
	int64_t jdate;	// юлианская дата, в милисекундах от 2000г
	// пакет с логином
	struct packet_GT06_L packet_l;
	// пакет с GPS данными
	struct packet_GT06_GPS packet_gps;
	// пакет с LBS данными
	struct packet_GT06_LBS packet_lbs;
	// пакет со статусом
	struct packet_GT06_status packet_status;
	// пакет с текстом
	struct packet_GT06_str packet_str;
	// пакет с GPS и LBS данными
	//struct packet_GT06_GPS_LBS packet_gps_lbs;
	// пакет с GPS, LBS данными  и статусом
	//struct packet_GT06_GPS_LBS_status packet_gps_lbs_status;
};

// очистить структуру GT06_PACKET - содержимое пакета
void clean_gt06_packet(void *packet);
// удалить структуру GT06_PACKET - содержимое пакета
void free_gt06_packet(void *packet);

// получить содержимое пакета из строки (без символов \r\n на конце)
// return FALSE в случае нераспознанной строки
gboolean gt06_parse_packet(char *str, int str_len,
		struct PACKET_INFO *packet_info);

// получение текстового сообщения из пакета
char* gt06_get_text_message(struct PACKET_INFO *packet_info);

// получение из packet_info доп. параметров
gboolean gt06_packet_get_dop_param(struct PACKET_INFO *packet_info, int *course,
		int *n_params, struct DOP_PARAM **params);

#if __cplusplus
}  // End of the 'extern "C"' block
#endif

#endif	//_PROTOCOL_PARSE_GT06_H_
