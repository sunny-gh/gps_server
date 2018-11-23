// base_lib.h : Работа с базой клиентов, сообщений, треков
//

// перевод массива в строку для сохранения в blob
// строка требуют удаления через g_free()
char* get_string_from_blob(uint8_t *blob, int len);
// перевод строки вида "A0EF1C..." в массив чисел
// строка требуют удаления через g_free()
uint8_t* get_blob_from_string(unsigned char *msg, int msg_len);

// типы соединений
enum {
	TYPE_NA,		// нет соединения
	TYPE_SERVER,// первичный сервер (висит на accept), доп. информация - номер порта сервера для первичного сокета
	TYPE_DEV,// вторичное соединение с устройством, доп. информация - PACKET_INFO*
	TYPE_API,// вторичное соединение с пользователем по API, доп. информация - NULL, а потом USER_INFO*

	TYPE_COUNT
};

struct USER_INFO {
	int *sockfd;		// сокет активного соединения с клиентом
	char *name;
	char **devs_list;
	int devs_n;
};

struct DEV_POINT {
	int64_t jdate_reg;// юлианская дата регистрации пакета на сервере, в секундах от 2000г
	int64_t jdate;		// юлианская дата, в секундах от 2000г
	double lat;		// широта
	double lon;		// долгота
	double speed;		// скорость, в км/ч
	int course;		// курс (азимут), в градусах
	double height;		// высота, в метрах
	int sats;		// количество спутников, целое число
	// доп. параметры
	int n_params;	// число доп. параметров
	char **name;	// список названий доп. параметров
	uint8_t *type;// список типов доп. параметров // 1 – int, 2 - long long, 3 – double, 4 – string, 5 - битовый параметр (число каналов(используемых бит), значение)
	char *params;	// список доп. параметров,
	char *raw;	// сырое сообщение
};

// удалить структуру USER_INFO
void g_free_userinfo(struct USER_INFO *uinfo);

// иннициализация
void base_dinamic_init(void);
// добавить активное соединение
// id - уникальный идентификатор соединения (номер сокета)
void add_connection(int id, gpointer info, int type_conn, GMutex **mutex);
// добавить/обновить к активному соединению параметр
void add_connection_param(int id, gpointer info);
// поиск активного API соединения, в котором у пользователя есть устройство imei
// return: номер соединения(сокета), 0 - не нашли
int find_api_connection(int *sock_start, const char *imei, char **user_name,
		GMutex **mutex_rw);
// проверяем наличие API соединений с пользователем user_name
// return: есть или нет активные соединения с пользователем
gboolean find_api_user_connection(const char *user_name);
// поиск активного DEV соединения для устройства imei
// [out] protocol -  тип протокола (PROTOCOL_NA, PROTOCOL_WIALON и т.д.)
// [out] ver - версия протокола (0-никакая, Wialon IPS (1.1=11 или 2.0=20))
// return: номер соединения(сокета), 0 - не нашли
int find_dev_connection(const char *imei, int *protocol, uint16_t *ver,
		GMutex **mutex_rw);
// поиск активного DEV соединения по ip адресу
// return: номер соединения(сокета), 0 - не нашли
int find_dev_connection_by_ip(const char *ip);
// удалить активное соединение
void del_connection(int id);
// остановить все соединения
void stop_all_connections(void);
// авторизовать соединение
void auth_connection(int id);
// деавторизовать соединение (для API)
void deauth_connection(int id);
gboolean is_auth_connection(int id);

/*
 Общие таблицы:
 Регистрация на сервере:
 CREATE TABLE auth(		id SERIAL PRIMARY KEY,			порядковый номер записи в таблице
 imei VARCHAR(20) NOT NULL KEY,	логин оборудования
 passwd VARCHAR(20),				пароль
 time BIGINT KEY,				время регистрации, в секундах от 2000г.
 state SMALLINT					1 - пользователь активен, 0 - заблокирован
 )
 Протокол регистрация на сервере (пакеты авторизации):
 CREATE TABLE auth_log(		id SERIAL PRIMARY KEY,			порядковый номер записи в таблице
 imei VARCHAR(20) NOT NULL KEY,	логин оборудования
 time BIGINT KEY,				время запроса, в секундах от 2000г.
 ip	INTEGER,					IP адрес оборудования
 port INTEGER,					порт оборудования
 status SMALLINT KEY,			1 - успешная авторизация, 0 - не успешная, 0x10(16) - завершение сеанса связи
 ver	SMALLINT,					версия протокола (1.1, 2.0)
 raw_data VARCHAR(200)			сырое содержимое пакета авторизации
 )

 Для каждого imei создаётся таблица с данными:
 CREATE TABLE data_[imei](	id BIGSERIAL PRIMARY KEY,		порядковый номер записи в таблице
 time_save BIGINT,			время записи пакета в БД, в секундах от 2000г.
 time BIGINT KEY,			время регистрации пакета в оборудовании, в секундах от 2000г.
 ip	INTEGER,				IP адрес оборудования, с которого пришёл пакет
 port INTEGER,				порт оборудования
 lat DOUBLE PRECISION,		широта
 lon DOUBLE PRECISION,		долгота
 height INTEGER,				высота
 speed INTEGER,				скорость
 type_pkt SMALLINT,			тип пакета (PACKET_TYPE_DRIVER=3; PACKET_TYPE_SHORT_DATA=4,8 ; PACKET_TYPE_DATA=5,9)
 raw_data TEXT				сырое содержимое пакета
 )

 id BIGSERIAL PRIMARY KEY, time_save BIGINT, time BIGINT,ip CHAR(16), port INTEGER, \
							
 */

// выполняет SQL запрос query к базе данных, который не возвращает данные
// db - идентификатор открытой базы данных
// query - строка запроса, пример:
// error_message - сюда будет записано текстовое сообщение об ошибке (память требует удаления через g_free())
// return: TRUE, если успешно выполнена операция
gboolean base_exec(void *db, const char *query, char **error_message);
// выполняет SQL запрос query к базе данных, который возвращает id добавленной строки
// запрос должен быть вида "INSERT INTO ... RETURNED id"
// db - идентификатор открытой базы данных
// query - строка запроса, пример:
// error_message - сюда будет записано текстовое сообщение об ошибке (память требует удаления через g_free())
// return: id добавленной строки, если успешно выполнена операция или 0 в случае ошибки
int64_t base_exec_ret_id(void *db, const char *query, char **error_message);

// иннициализация
void base_init(void);
// создание первоначальных таблиц в БД
void base_create_table0(void);
// создать пользователя (оборудование)
gboolean base_create_dev(char *imei, char *alias, char *passwd, char *protocol);
// удалить пользователя (оборудование)
gboolean base_del_dev(char *imei);
// проверка логина и пароля
gboolean base_check_login(char *imei, char *passwd);

// записать завершение сеанса в базу
gboolean base_save_exit(struct PACKET_INFO *packet_info);
// записать пакет в БД
// return: id добавленной в базу записи
int64_t base_save_packet(int packet_type, struct PACKET_INFO *packet_info);

// Работа с устройствами

// узнать параметры устройства по imei
char** base_get_dev_param(char *imei);

// узнать время последнего сеанса связи устройства по imei
// return: время или -1 в случае ошибки, 0 в случае, если никогда не было сеанса связи
int64_t base_get_dev_last_time(char *imei);

// прочитать поледний пакет из БД (по id, а не по времени)
// max_count_str - сколько последних пакетов прочитать
// если id=0, значит эту границу не учитываем
// return: число прочитанных пакетов, -5 ошибка открытия БД
int base_get_last_packets(const char *imei, int max_count_str, char **out_str,
		int *protocol_num);

// прочитать пакеты из БД
// если id=0, значит эту границу не учитываем
// return: число прочитанных пакетов, -5 ошибка открытия БД
int base_get_data_packets(const char *imei, uint64_t id1, uint64_t id2,
		char **out_str, int *protocol_num, int64_t *total_string);

// установить новый псевдоним для устройства
// return: TRUE или FALSE (в этом случае будет установлена строка ошибки:err_msg_out, удалять через g_free(err_msg_out))
gboolean base_set_new_dev_alias(const char *imei, const char *new_alias,
		char **err_msg_out);

// установить новое время последней активности для устройства
// return: TRUE или FALSE (в этом случае будет установлена строка ошибки:err_msg_out, удалять через g_free(err_msg_out))
gboolean base_set_new_dev_time(const char *imei, int64_t time_new,
		char **err_msg_out);

// Работа с пользователями

// создать пользователя
gboolean base_create_user(const char *user_name, const char *user_passwd,
		const char *full_user_name, int permit, const char *user_devs,
		const char *email, const char *param);
// авторизация пользователя по логину и паролю
// [in] user_name - login пользователя, которому разрешён вход на сервер
// [in] client_ip_addr - ip адрес клиента
// [in] client_port - порт клиента
// [out] user_sid - идентификатор пользователя, которому разрешён вход на сервер
// [out] user_devs - список устройств, которые доступны этому пользователю
// [out] full_user_name - полное имя пользователя, которому разрешён вход на сервер
// return: 0 - успешная авторизация, -4 ошибка выполнения авторизации, -1 нет такого пользователя, -2 неверный пароль, -3 пользователь заблокирован
int base_check_user(const char *user_name, const char *user_passwd,
		const char *client_ip_addr, int client_port, char **user_sid,
		char **user_devs, char **full_user_name);

// авторизация пользователя по sid-у
// [in] user_sid - идентификатор пользователя, которому разрешён вход на сервер
// [in] client_ip_addr - ip адрес клиента
// [out] user_name - login пользователя, которому разрушён вход на сервер
// [out] user_devs - список устройств, которые доступны этому пользователю
// [out] full_user_name - полное имя пользователя, которому разрешён вход на сервер
// return: 0 - успешная авторизация, -4 ошибка выполнения авторизации, -1 нет такого sid-a, -3 пользователь заблокирован
int base_check_user_sid(char *user_sid, char *client_ip_addr, char **user_name,
		char **user_devs, char **full_user_name);

// добавить к пользователю imei, если e-mail совпал
gboolean base_user_add_imei_by_email(const char *imei, const char *email);

