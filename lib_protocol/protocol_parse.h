// protocol_parse.h : Протоколы устройств
//

#ifndef _PROTOCOL_PARSE_H_
#define _PROTOCOL_PARSE_H_

#include "protocol_parse_wialon_ips.h"
#include "protocol_parse_osmand.h"
#include "protocol_parse_gt06.h"
#include "protocol_parse_babywatch.h"

// Make sure we can call this stuff from C++.
#if __cplusplus
extern "C" {
#endif

// типы пакетов
enum {
	PACKET_TYPE_NA,			// неопределённый пакет 
	// Wialon ips ver=1.1
	PACKET_TYPE_WIALON_IPS_START,
	PACKET_TYPE_WIALON_LOGIN = PACKET_TYPE_WIALON_IPS_START,	// пакет логина
	PACKET_TYPE_WIALON_VLOGIN,		// пакет логина спец. для wialon сервера
	PACKET_TYPE_WIALON_PING,// пинговый пакет (для первой и второй версии одинаков)
	PACKET_TYPE_WIALON_DRIVER,		// сообщение для водителя
	PACKET_TYPE_WIALON_SHORT_DATA,	// сокращённый пакет с данными
	PACKET_TYPE_WIALON_DATA,		// пакет с данными
	PACKET_TYPE_WIALON_BLACK,		// пакет из чёрного ящика
	PACKET_TYPE_WIALON_SHORT_DATA_FROM_BLACK,//[virtual] сокращённый пакет с данными из чёрного ящика
	PACKET_TYPE_WIALON_DATA_FROM_BLACK,	//[virtual] пакет с данными из чёрного ящика
	PACKET_TYPE_WIALON_IMAGE,		// пакет с фотоизображением
	PACKET_TYPE_WIALON_IPS_STOP,
	// Wialon ips ver=2.0
	PACKET_TYPE_WIALON_IPS2_START,
	PACKET_TYPE_WIALON_LOGIN2 = PACKET_TYPE_WIALON_IPS2_START,	// пакет логина
	PACKET_TYPE_WIALON_VLOGIN2,		// пакет логина спец. для wialon сервера
	PACKET_TYPE_WIALON_DRIVER2,		// сообщение для водителя
	PACKET_TYPE_WIALON_SHORT_DATA2,	// сокращённый пакет с данными
	PACKET_TYPE_WIALON_DATA2,		// пакет с данными
	PACKET_TYPE_WIALON_BLACK2,		// пакет из чёрного ящика
	PACKET_TYPE_WIALON_SHORT_DATA_FROM_BLACK2,//[virtual] сокращённый пакет с данными из чёрного ящика
	PACKET_TYPE_WIALON_DATA_FROM_BLACK2,//[virtual] пакет с данными из чёрного ящика
	PACKET_TYPE_WIALON_IMAGE2,		// пакет с фотоизображением
	PACKET_TYPE_WIALON_INFO_DDD2,	// пакет с информацией о ddd-файле
	PACKET_TYPE_WIALON_DATA_DDD2,	// пакет с блоком ddd-файла
	PACKET_TYPE_WIALON_IPS2_STOP,
	// OsmAnd
	PACKET_TYPE_OSMAND_START = 25,
	PACKET_TYPE_OSMAND_DATA = PACKET_TYPE_OSMAND_START,	// пакет с данными
	PACKET_TYPE_OSMAND_STOP = 30,
	// Traccar
	PACKET_TYPE_TRACCAR_START = 31,
	PACKET_TYPE_TRACCAR_DATA = PACKET_TYPE_TRACCAR_START,	// пакет с данными
	PACKET_TYPE_TRACCAR_STOP = 35,

	// GT06
	PACKET_TYPE_GT06_START = 40,
	PACKET_TYPE_GT06_LOGIN = PACKET_TYPE_GT06_START,	// пакет логина
	PACKET_TYPE_GT06_GPS,								// пакет с GPS данными
	PACKET_TYPE_GT06_LBS,								// пакет с LBS данными
	PACKET_TYPE_GT06_STATUS,							// пакет со статусом
	PACKET_TYPE_GT06_GPS_LBS,						// пакет с GPS и LBS данными
	PACKET_TYPE_GT06_GPS_LBS_STATUS,	// пакет с GPS, LBS данными и статусом
	PACKET_TYPE_GT06_STRING,					// пакет с текстом от устройства

	PACKET_TYPE_GT06_UNPARSE = 59,						// нераспознанный пакет
	PACKET_TYPE_GT06_STOP = 60,

	// Torque
	PACKET_TYPE_TORQUE_START = 65,
	PACKET_TYPE_TORQUE_DATA = PACKET_TYPE_TORQUE_START,	// пакет с данными
	PACKET_TYPE_TORQUE_STOP = 69,

	// Baby Watch Q90
	PACKET_TYPE_BABYWATCH_START = 75,
	PACKET_TYPE_BABYWATCH_DATA = PACKET_TYPE_BABYWATCH_START,// пакет с данными
	PACKET_TYPE_BABYWATCH_STOP = 79,

	// число типов пакетов
	PACKET_TYPE_COUNT
};

// типы протоколов
enum {
	PROTOCOL_NA,			// неопределённый протокол
	PROTOCOL_WIALON,		// протокол Wialon
	PROTOCOL_OSMAND,		// протокол OsmAnd (Web протокол для Traccar)
	PROTOCOL_TORQUE,		// протокол Torque (Web протокол для ODB)
	PROTOCOL_TRACCAR,		// протокол Traccar
	PROTOCOL_GT06,			// протокол GT06
	PROTOCOL_BABYWATCH,		// протокол Baby Watch Q90

	// число протоколов
	PROTOCOL_COUNT
};

struct PACKET_INFO {
	int protocol;			// тип протокола
	uint16_t ver;// версия протокола (0-никакая, Wialon IPS (1.1=11 или 2.0=20))
	char *imei;				// логин пользователя
	gboolean is_auth;		// выполнена ли авторизация клиента
	char *client_ip_addr;		// ip адрес клиента
	uint16_t client_port;	// порт клиента

	// для протоколов, которые передают данные дальше, на другой сервер
	char *proxy_str;		// пересылать ли данные дальше 
	// пересылать ли команды дальше
	bool resend_is_cmd;
	// сокет установленного соединения с китайским сервером для транслирования на него команд чтобы работал SeTracer
	SOCKET resend_sockfd;

	gboolean is_parse; // успешное ли распознование пакета
	// данные для одного пакета (=struct WIALON_PACKET)
	void *packet;
};

// типы доп. параметров
enum {
	DOP_PARAM_VAL_RAW,
	DOP_PARAM_VAL_INT,
	DOP_PARAM_VAL_DOUBLE,
	DOP_PARAM_VAL_STR,
	DOP_PARAM_VAL_BITS,	// битовый параметр, хранится как DOP_PARAM_VAL_INT, отображается как DOP_PARAM_VAL_STR

	DOP_PARAM_VAL_COUNT
};

struct DOP_PARAM {
	// Каждый параметр представляет собой конструкцию NAME : TYPE : VALUE
	char *name;	// NAME – произвольная строка, длиной не более 15 байт
	uint8_t type;// TYPE – тип параметра, 1 –int / long long, 2 – double, 3 – string
	//VALUE – значение в зависимости от типа
	union {
		long long val_int64;
		double val_double;
		char *val_str;
	};
	// Wialon ips: Для передачи тревожной кнопки используется параметр первого типа с именем «SOS», значение 1 означает нажатие тревожной кнопки.
};

// округлить число
int okrugl(double in);

// скопировать один доп.параметр
void copy_one_dop_param(struct DOP_PARAM *dop_param_to,
		struct DOP_PARAM *dop_param_from);

// заполнить доп. параметр
void fill_dop_param(struct DOP_PARAM *dop_param, const char *param_name,
		const char *param_val, int type);
// заполнить текстовый доп. параметр
void fill_dop_param_str(struct DOP_PARAM *dop_param, const char *param_name,
		const char *param_val);

// очистить структуру DOP_PARAM
//void clean_dop_param(struct DOP_PARAM *param);
// удалить все структуры DOP_PARAM
//void free_dop_param(struct DOP_PARAM *param);
void free_all_dop_param(int n_params, struct DOP_PARAM *params);

// узнать имя протокола по номеру
char* get_proto_by_num(int protocol_num);

// узнать номер протокола по типу сообщения
int get_proto_by_type_pkt(int type_pkt, uint16_t *protocol_ver);

// узнать номер протокола по названию и версию протокола, если необходимо
int get_protocol_num_by_name(const char *protocol_name, uint16_t *ver);

// удалить часть PACKET_INFO - содержимое пакета
void clean_packet_info(struct PACKET_INFO *packet_info);
// удалить структуру PACKET_INFO
void free_packet_info(struct PACKET_INFO *packet_info);

// получить текущее Юлианское время в секундах
//uint64_t get_cur_jdate_time(void);-> my_time_get_cur_msec2000()

// получить содержимое пакета из строки (без символов \r\n на конце)
// return FALSE в случае нераспознанной строки
gboolean packet_parse(char *str, int src_len, struct PACKET_INFO *packet_info);

// получение из packet_info доп. параметров
gboolean packet_get_dop_param(struct PACKET_INFO *packet_info, int *course,
		int *n_params, struct DOP_PARAM **params);

// это пакет с авторизацией
gboolean packet_is_login(int type_pkt);
// это пакет с текстовым сообщением
gboolean packet_is_message(int type_pkt);
// это пакет с данными
gboolean packet_is_data(int type_pkt);

// получение текстового сообщения из пакета
char* packet_get_message(int type_pkt, char *raw_data, int raw_data_len);

#if __cplusplus
}  // End of the 'extern "C"' block
#endif

#endif	//_PROTOCOL_PARSE_H_
