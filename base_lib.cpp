// base_lib.cpp : Работа с базой клиентов, сообщений, треков
//

#include <stdint.h>
#include <string.h>
#include <time.h>
#include <glib.h>
#include <pthread.h>
#include "lib_postgresql/lib_postgresql.h"
#include "lib_protocol/protocol.h"
#include "base_lib.h"
#include "my_time.h"
#include "md5.h"
#include "encode.h"
#include "lib_net/sock_func.h"

// перевод массива в строку для сохранения в blob
// строка требуют удаления через g_free()
char* get_string_from_blob(uint8_t *blob, int len) {
	char *str_out;
	int ret;
	//проверить
	if (!blob)
		return NULL;
	str_out = (char*) g_malloc(len * 2 + 1);
	ret = bin2hex((char*) str_out, blob, len);
	// последний символ - конец строки
	str_out[len * 2] = 0;
	return str_out;
}

// перевод строки вида "A0EF1C..." в массив чисел
// строка требуют удаления через g_free()
uint8_t* get_blob_from_string(unsigned char *msg, int msg_len) {
	uint8_t *str_out;
	int ret;
	//проверить
	if (!msg)
		return NULL;
	str_out = (uint8_t*) g_malloc((msg_len + 0) / 2);
	ret = hex2bin((char*) str_out, msg, msg_len);
	return str_out;
}

// иннициализация
void base_init(void) {
	base_dinamic_init();
}

// открыть БД
static void* base_open(void) {
	const char *pghost = "localhost";
	const char *pgport = NULL; // "5432"
	const char *login = "pguser1"; // postgres
	const char *passwd = "sun75sql";
	//char *login = "postgres";
	//char *passwd = "sun75pgsql_sec";
	const char *dbname = "gps_tracking";
	char *error_message = NULL;
	PGconn *conn = pgsql_open(pghost, pgport, login, passwd, dbname,
			&error_message);
	if (!conn) {
		printf("can't open base:%s\n", error_message);
		if (error_message)
			pgsql_free(error_message);// память очистится при pgsql_close(), но pgsql_close() вызван не будет
		return NULL;
	}
	return (void*) conn;
}

// закрытие базы данных PostgreSQL
static void base_close(void *db) {
	if (db)
		pgsql_close((PGconn*) db);
}

/*/ начало транзакции
 static void base_start_transaction(void *db)
 {
 char *error_message = NULL;
 if (!pgsql_start_transaction((PGconn*)db, &error_message))
 printf("base_start_transaction err: %s\n", error_message);
 g_free(error_message);
 }

 // конец транзакции
 static void base_end_transaction(void *db)
 {
 char *error_message = NULL;
 if (!pgsql_end_transaction((PGconn*)db, &error_message))
 printf("base_start_transaction err: %s\n", error_message);
 g_free(error_message);
 }
 */

static gboolean base_query(void *db, const char *query, int *rows_count,
		SQL_ROW_VALUE **rows_out, char **error_message) {
	char *msg = NULL;
	int status = pgsql_query((PGconn *) db, query, rows_count, rows_out, &msg);	// enum
	if (status != PGRES_TUPLES_OK) {
		printf("base_query err: code=%d, %s\n", status, msg);
		if (error_message)
			*error_message = g_strdup(msg);
		//pgsql_free(msg);
		return FALSE;
	}
	return TRUE;
}

// выполняет SQL запрос query к базе данных, который не возвращает данные
// db - идентификатор открытой базы данных
// query - строка запроса, пример:
// error_message - сюда будет записано текстовое сообщение об ошибке (память требует удаления через g_free())
// return: TRUE, если успешно выполнена операция
gboolean base_exec(void *db, const char *query, char **error_message) {
	char *msg = NULL;
	ExecStatusType status = pgsql_exec((PGconn *) db, query, &msg);	// enum
	if (status != PGRES_COMMAND_OK) {
		printf("base_exec err: code=%d, %s\n", status, msg);
		if (error_message)
			*error_message = g_strdup(msg);
		//pgsql_free(msg);
		return FALSE;
	}
	return TRUE;
}

// выполняет SQL запрос query к базе данных, который возвращает id добавленной строки
// запрос должен быть вида "INSERT INTO ... RETURNED id"
// db - идентификатор открытой базы данных
// query - строка запроса, пример:
// error_message - сюда будет записано текстовое сообщение об ошибке (память требует удаления через g_free())
// return: id добавленной строки, если успешно выполнена операция или 0 в случае ошибки
int64_t base_exec_ret_id(void *db, const char *query, char **error_message) {
	int64_t ret_id = 0;
	int rows_count = 0;
	SQL_ROW_VALUE *rows = NULL;
	gboolean ret = base_query(db, query, &rows_count, &rows, error_message);
	if (ret && rows_count == 1 && rows && rows->count == 1) {
		ret_id = rows->val[0].val.val_int64;
	}
	pgsql_rows_value_free(rows);
	return ret_id;
}

// создание первоначальных таблиц в БД
void base_create_table0(void) {
	void *db = base_open();
	int ret;
	char *msg = NULL;
	if (!db) {
		printf("base_create_table0: can't open DB\n");
		return;
	}
	ret =
			base_exec(db,
					"CREATE TABLE auth(imei VARCHAR(40) UNIQUE NOT NULL, alias VARCHAR(40), passwd VARCHAR(60), protocol VARCHAR(20), time BIGINT, state SMALLINT,proxy VARCHAR(36))",
					&msg);
	if (ret) {
		ret = base_exec(db, "CREATE INDEX imei_auth_index ON auth(imei)", NULL);
		ret = base_exec(db, "CREATE INDEX time_auth_index ON auth(time)", NULL);
		ret = base_exec(db,
				"CREATE INDEX protocol_auth_index ON auth(protocol)", NULL);
		ret = base_exec(db, "CREATE INDEX state_auth_index ON auth(state)",
				NULL);
	} else
		printf("base_create_table0: can't create table auth\n");
	g_free(msg);
	msg = NULL;

	ret =
			base_exec(db,
					"CREATE TABLE auth_log(id SERIAL PRIMARY KEY, time BIGINT, imei VARCHAR(40) NOT NULL, ip CHAR(16), port INTEGER, status SMALLINT, protocol VARCHAR(20), raw_data VARCHAR(200))",
					&msg);
	if (ret) {
		ret = base_exec(db,
				"CREATE INDEX imei_auth_log_index ON auth_log(imei)", NULL);
		ret = base_exec(db,
				"CREATE INDEX time_auth_log_index ON auth_log(time)", NULL);
		ret = base_exec(db,
				"CREATE INDEX status_auth_log_index ON auth_log(status)", NULL);
	} else
		printf("base_create_table0: can't create table auth_log\n");
	g_free(msg);
	msg = NULL;

	// Работа с пользователями
	/*	$columns = array('uid' = >array('type' = >'int', 'length' = >11, 'allow_null' = >0, 'auto_increment' = >1, 'permanent' = >1, 'primary' = >1, 'comment' = >'Порядковый номер пользователя'),
	 'sid' = >array('type' = >'char', 'length' = >64, 'allow_null' = >1, 'default' = >NULL, 'index' = >1, 'comment' = >'Идентификатор сессии'),
	 'sid_expire' = >array('type' = >'int', 'length' = >20, 'allow_null' = >1, 'default' = >0, 'comment' = >'Время истечения срока действия идентификатора сессии, timestamp- в сек. от 1970г.'),
	 'sid_cond' = >array('type' = >'char', 'length' = >100, 'allow_null' = >1, 'default' = >NULL, 'comment' = >'Дополительное условие для сверки идентификатора сессии'),
	 'login' = >array('type' = >'char', 'length' = >200, 'allow_null' = >1, 'default' = >NULL, 'index' = >1, 'comment' = >'Имя пользователя'),
	 'password' = >array('type' = >'char', 'length' = >128, 'allow_null' = >1, 'default' = >NULL, 'comment' = >'Хэш пароля'),
	 'permit' = >array('type' = >'int', 'length' = >11, 'allow_null' = >1, 'default' = >NULL, 'comment' = >'Права пользователя'),
	 'username' = >array('type' = >'char', 'length' = >200, 'allow_null' = >1, 'default' = >NULL, 'comment' = >'Реальное имя пользователя'),
	 'email' = >array('type' = >'char', 'length' = >200, 'allow_null' = >1, 'default' = >NULL, 'index' = >1, 'comment' = >'Почтовый адрес пользователя')
	 devs - список подключённых устройств
	 param - остальные параметры
	 */
	ret =
			base_exec(db,
					"CREATE TABLE users(id SERIAL PRIMARY KEY, sid bytea, sid_expire BIGINT, sid_cond VARCHAR(32), login VARCHAR(20) UNIQUE NOT NULL, password bytea, permit SMALLINT, username  VARCHAR(200), email VARCHAR(200), devs TEXT, param TEXT)",
					&msg);
	if (ret) {
		ret = base_exec(db, "CREATE INDEX sid_users_index ON users(sid)", NULL);
		ret = base_exec(db, "CREATE INDEX login_users_index ON users(login)",
				NULL);
	} else
		printf("base_create_table0: can't create table users\n");
	g_free(msg);
	msg = NULL;
	// закрытие базы данных PostgreSQL
	base_close(db);
}

// создание таблиц в БД для imei
static gboolean base_create_table_imei(void *db, char *imei) {
	if (!imei)
		return FALSE;
	char *msg = NULL;

	//char *query =  g_strdup_printf("CREATE TABLE IF NOT EXISTS data_%s(id BIGSERIAL PRIMARY KEY, time_save BIGINT, time BIGINT,ip CHAR(16), port INTEGER, protocol VARCHAR(20), type_pkt SMALLINT, raw_data TEXT)", imei);
	char *es_imei = pgsql_escape((PGconn *) db, (char*) imei);
	char *query =
			g_strdup_printf(
					"CREATE TABLE IF NOT EXISTS data_%s(id BIGSERIAL PRIMARY KEY, time_save BIGINT, time BIGINT,ip CHAR(16), port INTEGER, \
									lat DOUBLE PRECISION, lon DOUBLE PRECISION, height INTEGER, speed INTEGER, type_pkt SMALLINT, raw_data TEXT)",
					es_imei);
	int ret = base_exec(db, query, &msg);
	g_free(es_imei);
	g_free(query);
	if (ret) {
		query = g_strdup_printf(
				"CREATE INDEX time_data_%s_index ON data_%s(time)", imei, imei);
		ret = base_exec(db, query, NULL);
		g_free(query);
		query = g_strdup_printf(
				"CREATE INDEX lon_data_%s_index ON data_%s(lon)", imei, imei);
		ret = base_exec(db, query, NULL);
		query = g_strdup_printf(
				"CREATE INDEX lat_data_%s_index ON data_%s(lat)", imei, imei);
		ret = base_exec(db, query, NULL);
		query = g_strdup_printf(
				"CREATE INDEX speed_data_%s_index ON data_%s(speed)", imei,
				imei);
		ret = base_exec(db, query, NULL);
		query = g_strdup_printf(
				"CREATE INDEX type_pkt_data_%s_index ON data_%s(type_pkt)",
				imei, imei);
		ret = base_exec(db, query, NULL);
		g_free(query);
	}
	g_free(msg);
	return TRUE;
}

// создать пользователя (оборудование)
gboolean base_create_dev(char *imei, char *alias, char *passwd,
		char *protocol) {
	if (!imei)
		return FALSE;
	// отсутствует пароль
	if (!passwd || !g_strcmp0(passwd, "NA") || !g_strcmp0(passwd, "N/A"))
		passwd = NULL;
	void *db = base_open();
	if (!db)
		return FALSE;
	// создание таблиц в БД для imei
	int ret = base_create_table_imei(db, imei);
	if (ret) {
		// получаем текущее число миллисекунд от 2000 года GMT(UTC)
		int64_t time_ms = 0;	// my_time_get_cur_msec2000();
		//struct tm gmt_t;
		//my_time_msec2000_to_time(time_ms, &gmt_t);
		char *es_imei = pgsql_escape((PGconn *) db, (char*) imei);
		char *es_alias = pgsql_escape((PGconn *) db, (char*) alias);
		char *es_passwd = pgsql_escape((PGconn *) db, (char*) passwd);
		char *es_protocol = pgsql_escape((PGconn *) db, (char*) protocol);
		char *query =
				g_strdup_printf(
						"INSERT INTO auth VALUES('%s', '%s', '%s', '%s', '%lld', '1', '')",
						es_imei, (es_alias) ? es_alias : "",
						(es_passwd) ? es_passwd : "", es_protocol,
						(long long int) time_ms);
		ret = base_exec(db, (const char*) query, NULL);
		g_free(es_imei);
		g_free(es_alias);
		g_free(es_passwd);
		g_free(es_protocol);

		g_free(query);
	}
	// закрытие базы данных PostgreSQL
	base_close(db);
	return ret;
}

// удалить пользователя (оборудование)
gboolean base_del_dev(char *imei) {
	void *db = base_open();
	if (!db)
		return FALSE;
	// удаление таблиц в БД для imei
	int ret = 1;		//base_del_table_imei(db, imei);
	if (ret) {
		// получаем текущее число миллисекунд от 2000 года GMT(UTC)
		int64_t time_ms = 0;		// my_time_get_cur_msec2000();
		//struct tm gmt_t;
		//my_time_msec2000_to_time(time_ms, &gmt_t);
		char *es_imei = pgsql_escape((PGconn *) db, (char*) imei);
		char *query = g_strdup_printf("DELETE FROM auth WHERE imei='%s'",
				es_imei);
		ret = base_exec(db, (const char*) query, NULL);
		g_free(es_imei);
		g_free(query);
	}
	// закрытие базы данных PostgreSQL
	base_close(db);
	return ret;
}

// проверка логина и пароля устройства
gboolean base_check_login(char *imei, char *passwd) {
	//return TRUE;
	if (!imei || !passwd)
		return FALSE;
	// отсутствует пароль
	if (!g_strcmp0(passwd, "NA") || !g_strcmp0(passwd, "N/A"))
		passwd = NULL;
	void *db = base_open();
	if (!db)
		return FALSE;
	//char *query = g_strdup_printf("SELECT imei, passwd, state FROM auth");
	char *es_imei = pgsql_escape((PGconn *) db, (char*) imei);
	char *query = g_strdup_printf(
			"SELECT imei, passwd, state FROM auth WHERE imei='%s'", es_imei);
	char *msg = NULL;
	int rows_count = 0;
	SQL_ROW_VALUE *rows = NULL;
	int ret = base_query(db, query, &rows_count, &rows, &msg);
	g_free(es_imei);
	g_free(query);
	gboolean is_auth = FALSE;
	if (ret && rows_count > 0) {
		char *login = rows->val[0].val.val_str;
		char *pass = rows->val[1].val.val_str;
		int state = rows->val[2].val.val_int16;
		gboolean is_login = !g_strcmp0(imei, login);		// совпал ли логин
		gboolean is_pass =
				(!pass || !strlen(pass)) ? TRUE : !g_strcmp0(passwd, pass);	// совпал ли пароль, если не требуется пароль, то любой пароль подойдёт
		//gboolean is_pass = (!passwd && (!pass || !strlen(pass))) ? TRUE : !g_strcmp0(passwd, pass);// совпал ли пароль
		// пользователь не заблокирован и совпали логин и пароль, если есть
		if (state == 1 && is_login && is_pass)
			is_auth = TRUE;
	}
	g_free(msg);
	pgsql_rows_value_free(rows);
	// закрытие базы данных PostgreSQL
	base_close(db);
	return is_auth;
}

// записать завершение сеанса в базу
gboolean base_save_exit(struct PACKET_INFO *packet_info) {
	//return TRUE;
	char *msg = NULL;
	void *db = base_open();
	if (!db)
		return FALSE;
	//packet_L packet_l = wialon_info->packet.packet_l;
	char *es_imei = pgsql_escape((PGconn *) db, (char*) packet_info->imei);
	char *es_client_ip_addr = pgsql_escape((PGconn *) db,
			(char*) packet_info->client_ip_addr);
	char *query =
			g_strdup_printf(
					"INSERT INTO auth_log(imei, time, ip, port, status) VALUES('%s', '%lld', '%s', '%d', '%d')",
					es_imei, my_time_get_cur_msec2000(), es_client_ip_addr,
					packet_info->client_port, 0x10);// код завершение сеанса связи = 16
	int ret = base_exec(db, (const char*) query, &msg);
	g_free(es_imei);
	g_free(es_client_ip_addr);

	g_free(query);
	g_free(msg);
	// закрытие базы данных PostgreSQL
	base_close(db);
	return ret;
}

// записать пакет в БД
// return: id добавленной в базу записи
int64_t base_save_packet(int packet_type, struct PACKET_INFO *packet_info) {
	int64_t id = 0;
	//return TRUE;
	//char *msg = NULL;
	void *db = base_open();
	if (!db)
		return 0;
	//int ret = FALSE;
	switch (packet_type) {
	case PROTOCOL_WIALON:
		// "wialon 1.1" или "wialon 2.0"
		//if (packet_info->ver == 11 || packet_info->ver == 20)
		id = base_save_packet_wialon(db, packet_info);
		break;
	case PROTOCOL_OSMAND:
		id = base_save_packet_osmand(db, packet_info);
		break;
	case PROTOCOL_GT06:
		id = base_save_packet_gt06(db, packet_info);
		break;
	case PROTOCOL_BABYWATCH:
		id = base_save_packet_babywatch(db, packet_info);
		break;
	}
	// закрытие базы данных PostgreSQL
	base_close(db);
	return id;
}

// Работа с устройствами

// узнать параметры устройства по imei
// return: массив строк, удалять через g_strfreev()
char** base_get_dev_proxy(char *imei) {
	char **str_ret = NULL;
	if (!imei)
		return NULL;
	void *db = base_open();
	if (!db)
		return NULL;
	//packet_L packet_l = wialon_info->packet.packet_l;
	char *es_imei = pgsql_escape((PGconn *) db, (char*) imei);
	char *query =
			g_strdup_printf(
					"SELECT imei,alias,passwd,protocol,time,state,proxy FROM auth WHERE imei='%s'",
					es_imei);
	int rows_count = 0;
	SQL_ROW_VALUE *rows = NULL;
	int ret = base_query(db, query, &rows_count, &rows, NULL);
	g_free(es_imei);
	g_free(query);
	if (ret && rows_count == 1 && rows && rows->count == 7) {
		str_ret = (char**) g_malloc0((rows->count + 1) * sizeof(char*));// последняя строка д.б. = 0
		*(str_ret + 0) = g_strdup(rows->val[0].val.val_str);
		*(str_ret + 1) = g_strdup(rows->val[1].val.val_str);
		*(str_ret + 2) = g_strdup(rows->val[2].val.val_str);
		*(str_ret + 3) = g_strdup(rows->val[3].val.val_str);// protocol_num = get_protocol_num_by_name(rows->val[0].val.val_str);
		*(str_ret + 4) = g_strdup_printf("%lld",
				(long long) rows->val[4].val.val_int64);
		*(str_ret + 5) = g_strdup_printf("%d", rows->val[5].val.val_int16);
		*(str_ret + 6) = g_strdup(rows->val[6].val.val_str);
	}
	pgsql_rows_value_free(rows);
	// закрытие базы данных
	base_close(db);
	return str_ret;
}

// узнать параметры устройства по imei
// return: массив строк, удалять через g_strfreev()
char** base_get_dev_param(char *imei) {
	char **str_ret = NULL;
	if (!imei)
		return NULL;
	void *db = base_open();
	if (!db)
		return NULL;
	//packet_L packet_l = wialon_info->packet.packet_l;
	char *es_imei = pgsql_escape((PGconn *) db, (char*) imei);
	char *query =
			g_strdup_printf(
					"SELECT imei,alias,passwd,protocol,time,state,proxy FROM auth WHERE imei='%s'",
					es_imei);
	int rows_count = 0;
	SQL_ROW_VALUE *rows = NULL;
	int ret = base_query(db, query, &rows_count, &rows, NULL);
	g_free(es_imei);
	g_free(query);
	if (ret && rows_count == 1 && rows && rows->count == 7) {
		str_ret = (char**) g_malloc0((rows->count + 1) * sizeof(char*));// последняя строка д.б. = 0
		*(str_ret + 0) = g_strdup(rows->val[0].val.val_str);
		*(str_ret + 1) = g_strdup(rows->val[1].val.val_str);
		*(str_ret + 2) = g_strdup(rows->val[2].val.val_str);
		*(str_ret + 3) = g_strdup(rows->val[3].val.val_str);// protocol_num = get_protocol_num_by_name(rows->val[0].val.val_str);
		*(str_ret + 4) = g_strdup_printf("%lld",
				(long long) rows->val[4].val.val_int64);
		*(str_ret + 5) = g_strdup_printf("%d", rows->val[5].val.val_int16);
		*(str_ret + 6) = g_strdup(rows->val[6].val.val_str);
	}
	pgsql_rows_value_free(rows);
	// закрытие базы данных
	base_close(db);
	return str_ret;
}

// узнать время последнего сеанса связи устройства по imei
// return: время или -1 в случае ошибки, 0 в случае, если никогда не было сеанса связи
int64_t base_get_dev_last_time(char *imei) {
	int64_t time_ret = -1;
	if (!imei)
		return NULL;
	void *db = base_open();
	if (!db)
		return NULL;
	//packet_L packet_l = wialon_info->packet.packet_l;
	char *es_imei = pgsql_escape((PGconn *) db, (char*) imei);
	char *query = g_strdup_printf("SELECT time FROM auth WHERE imei='%s'",
			es_imei);
	int rows_count = 0;
	SQL_ROW_VALUE *rows = NULL;
	int ret = base_query(db, query, &rows_count, &rows, NULL);
	g_free(es_imei);
	g_free(query);

	if (ret && rows_count == 1 && rows && rows->count == 1) {
		time_ret = (long long) (rows->val[0].val.val_int64);
	}
	pgsql_rows_value_free(rows);
	// закрытие базы данных
	base_close(db);
	return time_ret;
}

// узнать параметры устройства по imei
static int base_get_protocol_num(void *db, const char *imei) {
	int protocol_num = PROTOCOL_NA;
	if (!db)
		return FALSE;
	//packet_L packet_l = wialon_info->packet.packet_l;
	char *es_imei = pgsql_escape((PGconn *) db, imei);
	char *query = g_strdup_printf("SELECT protocol FROM auth WHERE imei='%s'",
			es_imei);
	int rows_count = 0;
	SQL_ROW_VALUE *rows = NULL;
	int ret = base_query(db, query, &rows_count, &rows, NULL);
	g_free(es_imei);
	g_free(query);
	if (ret && rows_count > 0) {
		protocol_num = get_protocol_num_by_name(rows->val[0].val.val_str, NULL);
	}
	pgsql_rows_value_free(rows);
	return protocol_num;
}

// преобразовать полученные данные в строку для передачи
static char* get_one_data_str(SQL_ROW_VALUE *row) {
	//row->val[3].len;
	//row->val[10].len;
	char *str_ip = row->val[3].val.val_str;	// ip
	char *str_raw = row->val[10].val.val_str;
	// time_save,time,ip,port,lat,lon,height,speed,type_pkt,raw_data
	char *one_str =
			g_strdup_printf(
					"%lld\001%lld\001%lld\001%s\001%d\001%lf\001%lf\001%d\001%d\001%d\001%s",
					(long long) row->val[0].val.val_int64,	// id
					(long long) row->val[1].val.val_int64,	// time_save
					(long long) row->val[2].val.val_int64,	// time_pkt
					str_ip, //row->val[3].val.val_str,	// ip
					row->val[4].val.val_int32,	// port
					row->val[5].val.val_double,	// lat DOUBLE PRECISION, широта
					row->val[6].val.val_double,	// lon DOUBLE PRECISION, долгота
					row->val[7].val.val_int32,	// height INTEGER, высота
					row->val[8].val.val_int32,	// speed INTEGER, скорость
					row->val[9].val.val_int16,// type_pkt SMALLINT, тип пакета(PACKET_TYPE_DRIVER = 3; PACKET_TYPE_SHORT_DATA = 4, 8; PACKET_TYPE_DATA = 5, 9)
					str_raw //row->val[10].val.val_str	// raw_data TEXT	сырое содержимое пакета
					);
	return one_str;
}

// прочитать поледний пакет из БД (по id, а не по времени)
// max_count_str - сколько последних пакетов прочитать
// если id=0, значит эту границу не учитываем
// return: число прочитанных пакетов, -5 ошибка открытия БД
int base_get_last_packets(const char *imei, int max_count_str, char **out_str,
		int *protocol_num) {
	void *db = base_open();
	if (!db)
		return -5;
	if (!out_str)
		return 0;
	// максимальное число строк в выборке
	//int64_t total_string = 0;// сколько всего строк, делаем выборку не более из MAX_STRINGS строк
	int ret;
	char *query;
	char *msg = NULL;
	int rows_count = 0;
	SQL_ROW_VALUE *rows = NULL;
	char *es_imei = pgsql_escape((PGconn *) db, (char*) imei);
	// читаем строки
	query =
			g_strdup_printf(
					"SELECT id,time_save,time,ip,port,lat,lon,height,speed,type_pkt,raw_data FROM data_%s ORDER BY id DESC LIMIT %d",
					es_imei, max_count_str);
	ret = base_query(db, query, &rows_count, &rows, &msg);
	g_free(query);
	if (ret) {
		int i;
		GString *out = g_string_new(NULL);
		for (i = 0; i < rows_count; i++) {
			char *str = get_one_data_str(rows + i);
			if (i > 0)	// && i < (rows_count - 1))
				g_string_append(out, "\002");	// разделитель записей
			g_string_append(out, str);
			g_free(str);
		}
		*out_str = g_string_free(out, FALSE);
	}
	g_free(es_imei);
	g_free(msg);
	pgsql_rows_value_free(rows);

	// узнать номер протокола по imei
	if (protocol_num)
		*protocol_num = base_get_protocol_num(db, imei);
	// закрытие базы данных PostgreSQL
	base_close(db);
	return rows_count;
}

// прочитать пакеты из БД
// если id=0, значит эту границу не учитываем
// return: число прочитанных пакетов, -5 ошибка открытия БД
int base_get_data_packets(const char *imei, uint64_t id1, uint64_t id2,
		char **out_str, int *protocol_num, int64_t *total_string) {
	void *db = base_open();
	if (!db)
		return -5;
	if (!out_str)
		return 0;
	//  вправо граница максимальная, если нет ограничений
	if (!id2)
		id2 = 9223372036854775807LL;	// INT64_MAX;
	// максимальное число строк в выборке
	const int MAX_STRINGS = 200;
	//int64_t total_string = 0;// сколько всего строк, делаем выборку не более из MAX_STRINGS строк
	int ret;
	char *query;
	char *msg = NULL;
	int rows_count = 0;
	SQL_ROW_VALUE *rows = NULL;
	char *es_imei = pgsql_escape((PGconn *) db, (char*) imei);
	// определяем число строк
	if (total_string) {
		char *query =
				g_strdup_printf(
						"SELECT count(*) FROM data_%s WHERE id>'%llu' AND id<='%llu' LIMIT 1",
						imei, (long long) id1, (long long) id2);
		//char *query = g_strdup_printf("SELECT count(*) FROM data_%s WHERE id>'%llu' AND id<='%llu' LIMIT 1", imei, date1, date2);
		int ret = base_query(db, query, &rows_count, &rows, &msg);
		g_free(query);
		if (ret && rows) {
			*total_string = rows->val[0].val.val_int64;
		}
		rows_count = 0;
		pgsql_rows_value_free(rows);
		rows = NULL;
		g_free(msg);
		msg = NULL;
	}
	// читаем строки
	//query = g_strdup_printf("SELECT id,time_save,time,ip,port,lat,lon,height,speed,type_pkt,raw_data FROM data_%s WHERE time>'%llu' AND time<='%llu' LIMIT %d", imei, date1, date2, MAX_STRINGS);
	query =
			g_strdup_printf(
					"SELECT id,time_save,time,ip,port,lat,lon,height,speed,type_pkt,raw_data FROM data_%s WHERE id>'%llu' AND id<='%llu' ORDER BY id LIMIT %d",
					es_imei, (long long) id1, (long long) id2, MAX_STRINGS);
	ret = base_query(db, query, &rows_count, &rows, &msg);
	g_free(query);
	if (ret) {
		int i;
		GString *out = g_string_new(NULL);
		for (i = 0; i < rows_count; i++) {
			char *str = get_one_data_str(rows + i);
			if (i > 0)	// && i < (rows_count - 1))
				g_string_append(out, "\002");	// разделитель записей
			g_string_append(out, str);
			g_free(str);
		}
		*out_str = g_string_free(out, FALSE);
	}
	g_free(es_imei);
	g_free(msg);
	pgsql_rows_value_free(rows);

	// узнать номер протокола по imei
	if (protocol_num)
		*protocol_num = base_get_protocol_num(db, imei);
	// закрытие базы данных PostgreSQL
	base_close(db);
	return rows_count;
}

// установить новый псевдоним для устройства
// return: TRUE или FALSE (в этом случае будет установлена строка ошибки:err_msg_out, удалять через g_free(err_msg_out))
gboolean base_set_new_dev_alias(const char *imei, const char *new_alias,
		char **err_msg_out) {
	char *err_msg = NULL;
	void *db = base_open();
	if (!db)
		return -5;
	// экранирование строки
	// db - идентификатор открытой базы данных
	char *es_new_alias =
			(new_alias) ?
					pgsql_escape((PGconn *) db, (char*) new_alias) : (char*) "";
	char *es_imei = pgsql_escape((PGconn *) db, (char*) imei);
	char *query = g_strdup_printf(
			"UPDATE auth SET alias = '%s' WHERE imei='%s'", es_new_alias,
			es_imei);	//decode('DEADBEEF', 'hex')
	//char *query = g_strdup_printf("UPDATE auth SET alias = %s WHERE imei=%s", es_new_alias, es_imei);//decode('DEADBEEF', 'hex')
	gboolean ret = base_exec(db, query, &err_msg);
	g_free(query);
	g_free(es_new_alias);
	g_free(es_imei);
	if (!ret) {
		if (err_msg_out)
			*err_msg_out = g_strdup(err_msg);
		pgsql_free(err_msg);
	}
	// закрытие базы данных PostgreSQL
	base_close(db);
	return ret;
}

// установить новое время последней активности для устройства
// return: TRUE или FALSE (в этом случае будет установлена строка ошибки:err_msg_out, удалять через g_free(err_msg_out))
gboolean base_set_new_dev_time(const char *imei, int64_t time_new,
		char **err_msg_out) {
	char *err_msg = NULL;
	void *db = base_open();
	if (!db)
		return -5;
	// экранирование строки
	// db - идентификатор открытой базы данных
	char *es_imei = pgsql_escape((PGconn *) db, (char*) imei);
	char *query = g_strdup_printf(
			"UPDATE auth SET time = '%lld' WHERE imei='%s'", time_new, es_imei);//decode('DEADBEEF', 'hex')
	//char *query = g_strdup_printf("UPDATE auth SET alias = %s WHERE imei=%s", es_new_alias, es_imei);//decode('DEADBEEF', 'hex')
	gboolean ret = base_exec(db, query, &err_msg);
	g_free(query);
	g_free(es_imei);
	if (!ret) {
		if (err_msg_out)
			*err_msg_out = g_strdup(err_msg);
		pgsql_free(err_msg);
	}
	// закрытие базы данных PostgreSQL
	base_close(db);
	return ret;
}

// Работа с пользователями

// записать новый sid для пользователя user в таблицу users
static int base_set_user_sid(void *db, const char *user, unsigned char *sid,
		size_t sid_len, const char *sid_cond) {
	//sid bytea, sid_expire INTEGER, sid_cond VARCHAR(32)
	char *msg = NULL;
	if (!db)
		return FALSE;
	// получаем текущее число миллисекунд от 2000 года GMT(UTC)
	int64_t sid_expire = my_time_get_cur_msec2000() + 48ll * 3600ll * 1000ll;// + 2 дня
	//int64_t sid_expire = my_time_get_cur_msec2000()+ 2*3600*1000ll;// + 2 часа
	//int64_t sid_expire = my_time_get_cur_msec2000() + 5 * 1000ll;// + 5 секунд

	/* Escape формат bytea="\004\003\002\001\000"
	 //one_byte_str[4] = 0;
	 char one_byte_str[6], *sid_str, *sid_str0 = (char*)g_malloc0(sid_len * 5 + 1);
	 sid_str = sid_str0;
	 for (size_t i = 0; i < sid_len; i++)
	 {
	 g_snprintf(one_byte_str, 6, "\\\\%03o", (int)*(sid + i));
	 g_strlcpy(sid_str, one_byte_str,6);
	 sid_str += 5;
	 }*/
	// Hex формат bytea="\xDEADBEEF"
	//bin2hex(sid_str0, sid, sid_len);
	char *es_sid = pgsql_escape((PGconn *) db, (char*) sid);
	char *es_sid_cond = pgsql_escape((PGconn *) db, (char*) sid_cond);
	char *es_user = pgsql_escape((PGconn *) db, (char*) user);
	char *query =
			g_strdup_printf(
					"UPDATE users SET sid = '\\x%s', sid_expire = '%lld',sid_cond = '%s' WHERE login='%s'",
					es_sid, (long long int) sid_expire, es_sid_cond, es_user);//decode('DEADBEEF', 'hex')
	//char *query = g_strdup_printf("UPDATE users SET sid = E'%s', sid_expire = '%lld',sid_cond = '%s' WHERE login='%s'", sid_str0, sid_expire, sid_cond, user);//RETURNING id
	int ret = base_exec(db, query, &msg);
	g_free(es_sid);
	g_free(es_sid_cond);
	g_free(es_user);

	g_free(query);
	//g_free(sid_str0);
	g_free(msg);
	return ret;
}

// проверить sid
static gboolean base_check_sid(char *sid_in, char *sid_cond_in, char *sid,
		int sid_len, int64_t sid_expire, char *sid_cond) {
	gboolean is_sid =
			(*sid_in && sid) ?
					!g_ascii_strncasecmp(sid_in, (const char*) sid, sid_len) :
					FALSE;	// совпал ли sid
	if (!is_sid)
		return FALSE;
	// проверяем доп. условие сида - ip адрес клиента
	if (!sid_cond || g_strcmp0(sid_cond_in, sid_cond))
		return FALSE;
	// проверяем срок действия сида
	int64_t cur_time = my_time_get_cur_msec2000();
	if (sid_expire < cur_time)
		return FALSE;
	return TRUE;
}

// создать пользователя
gboolean base_create_user(const char *user_name, const char *user_passwd,
		const char *full_user_name, int permit, const char *user_devs,
		const char *email, const char *param) {
	char *msg = NULL;
	if (!user_name || !user_passwd)
		return FALSE;
	void *db = base_open();
	if (!db)
		return FALSE;
	// вычисляем хэш пароля
	char pass_hash_hex[33];
	BYTE hash[17];
	findhash((BYTE*) user_passwd, hash);
	bin2hex((char*) pass_hash_hex, hash, 16);
	pass_hash_hex[32] = 0;
	char *es_user_name = pgsql_escape((PGconn *) db, (char*) user_name);
	char *es_pass_hash_hex = pgsql_escape((PGconn *) db, (char*) pass_hash_hex);
	char *es_full_user_name = pgsql_escape((PGconn *) db,
			(char*) full_user_name);
	char *es_user_devs = pgsql_escape((PGconn *) db, (char*) user_devs);
	char *es_email = pgsql_escape((PGconn *) db, (char*) email);
	char *query =
			g_strdup_printf(
					"INSERT INTO users(login, password, username, permit,devs, email, param) VALUES('%s', '\\x%s', '%s', '%d', '%s', '%s', DEFAULT)",
					es_user_name, es_pass_hash_hex, es_full_user_name, permit,
					(es_user_devs) ? es_user_devs : "", es_email);
	//char *query = g_strdup_printf("INSERT INTO users(login, password, username, permit,devs, email, param) VALUES('%s', '\\x%s', '%s', '%d', '%s', '%s', '%s')", user_name, pass_hash_hex, full_user_name, permit, user_devs, email, (param) ? param : "");
	int ret = base_exec(db, query, &msg);
	g_free(es_user_name);
	g_free(es_pass_hash_hex);
	g_free(es_full_user_name);
	g_free(es_user_devs);
	g_free(es_email);

	g_free(query);
	g_free(msg);
	// закрытие базы данных
	base_close(db);
	return ret;
}

// авторизация пользователя по логину и паролю
// [in] user_name - login пользователя, которому разрешён вход на сервер
// [in] client_ip_addr - ip адрес клиента
// [in] client_port - порт клиента
// [out] user_sid - идентификатор пользователя, которому разрешён вход на сервер
// [out] user_devs - список устройств, которые доступны этому пользователю
// [out] full_user_name - полное имя пользователя, которому разрешён вход на сервер
// return: 0 - успешная авторизация, -5 ошибка открытия БД, -4 ошибка выполнения авторизации, -1 нет такого пользователя, -2 неверный пароль, -3 пользователь заблокирован
int base_check_user(const char *user_name, const char *user_passwd,
		const char *client_ip_addr, int client_port, char **user_sid,
		char **user_devs, char **full_user_name) {
	int is_auth = -4;
	if (!user_name)
		return is_auth;
	void *db = base_open();
	if (!db)
		return -5;
	char *es_user_name = pgsql_escape((PGconn *) db, (char*) user_name);
	char *query = g_strdup_printf("SELECT * FROM users WHERE login='%s'",
			user_name);
	char *msg = NULL;
	int rows_count = 0;
	SQL_ROW_VALUE *rows = NULL;
	int ret = base_query(db, query, &rows_count, &rows, &msg);
	g_free(query);
	g_free(es_user_name);
	if (ret) {
		if (rows_count > 0) {
			uint32_t sid_len =
					(rows->val[1].len >= 2) ? rows->val[1].len - 2 : 0;
			uint8_t *sid =
					(sid_len >= 2) ?
							rows->val[1].val.val_blob + 2 :
							rows->val[1].val.val_blob;
			//int64_t sid_expire = rows->val[2].val.val_int64;
			//char *sid_cond = rows->val[3].val.val_str;

			char *name = rows->val[4].val.val_str;
			uint32_t pass_len =
					(rows->val[5].len > 0) ? rows->val[5].len - 2 : 0;
			uint8_t *pass =
					(pass_len >= 2) ?
							rows->val[5].val.val_blob + 2 :
							rows->val[5].val.val_blob;
			int permit = rows->val[6].val.val_int16;
			char *full_name = rows->val[7].val.val_str;
			uint32_t devs_len = rows->val[9].len;
			char *devs = rows->val[9].val.val_str;
			gboolean is_login = (name) ? !g_strcmp0(user_name, name) : FALSE;// совпал ли логин
			gboolean is_pass = FALSE;
			// проверка по паролю
			if (is_login && pass) {
				char pass_hash_hex[33];
				BYTE hash[17];
				if (user_passwd) {
					//C4CA4238A0B923820DCC509A6F75849B
					findhash((BYTE*) user_passwd, hash);
					bin2hex((char*) pass_hash_hex, hash, 16);
					is_pass = (pass_len == 32)
							&& !g_ascii_strncasecmp(pass_hash_hex,
									(const char*) pass, 32);// совпал ли пароль
				}
			}
			// пользователь не заблокирован и совпали логин и пароль
			if (permit > 0 && is_pass) {
				is_auth = 0;			// успешно
				if (user_devs) {
					// так нельзя, т.к. devs не заканчавается нулём: 
					// *user_devs = g_strdup(devs);
					*user_devs = (char*) g_malloc(devs_len + 1);
					g_strlcpy(*user_devs, devs, devs_len + 1);
				}
				// формируем новый sid, если этот пользователь в первом и единственном экземпляре, если уже есть залогиненые пользователи, возвращаем существующий sid
				if (user_sid) {
					// проверяем наличие API соединений с пользователем user_name
					// return: есть или нет активные соединения с пользователем
					if (!find_api_user_connection(user_name))// нет залогиненых пользователей
							{
						guchar *sid_str0 = (guchar*) get_sid(client_ip_addr,
								client_port);
						*user_sid = (char*) g_malloc0(32 + 1);
						// переводим в текстовый формат из двоичного
						bin2hex(*user_sid, sid_str0, 16);
						// записать новый sid в базу
						base_set_user_sid(db, (const char*) name,
								(guchar*) *user_sid, 16, client_ip_addr);
					} else					// узнать существующий sid
					{
						*user_sid = (char*) g_malloc0(sid_len + 1);
						// переводим в текстовый формат из двоичного
						memcpy(*user_sid, sid, sid_len);
					}

				}
				if (full_user_name)
					*full_user_name = g_strdup(full_name);
			} else {
				if (!is_pass)
					is_auth = -2;					// неверный пароль
				if (!permit)
					is_auth = -3;					// пользователь заблокирован
			}
		} else
			is_auth = -1;					// нет такого пользователя
	}
	g_free(msg);
	pgsql_rows_value_free(rows);
	// закрытие базы данных
	base_close(db);
	return is_auth;
}

// авторизация пользователя по sid-у
// [in] user_sid - идентификатор пользователя, которому разрешён вход на сервер
// [in] client_ip_addr - ip адрес клиента
// [out] user_name - login пользователя, которому разрушён вход на сервер
// [out] user_devs - список устройств, которые доступны этому пользователю
// [out] full_user_name - полное имя пользователя, которому разрешён вход на сервер
// return: 0 - успешная авторизация, -6 sid не актуален, -5 ошибка открытия БД, -4 ошибка выполнения авторизации, -1 нет такого sid-a, -3 пользователь заблокирован
int base_check_user_sid(char *user_sid, char *client_ip_addr, char **user_name,
		char **user_devs, char **full_user_name) {
	int is_auth = -4;
	if (!user_sid)
		return is_auth;
	void *db = base_open();
	if (!db)
		return -5;
	char *es_user_sid = pgsql_escape((PGconn *) db, (char*) user_sid);
	char *query = g_strdup_printf("SELECT * FROM users WHERE sid='\\x%s'",
			user_sid);
	char *msg = NULL;
	int rows_count = 0;
	SQL_ROW_VALUE *rows = NULL;
	int ret = base_query(db, query, &rows_count, &rows, &msg);
	g_free(query);
	g_free(es_user_sid);
	if (ret) {
		if (rows_count > 0) {
			uint32_t sid_len =
					(rows->val[1].len >= 2) ? rows->val[1].len - 2 : 0;
			uint8_t *sid =
					(sid_len >= 2) ?
							rows->val[1].val.val_blob + 2 :
							rows->val[1].val.val_blob;
			int64_t sid_expire = rows->val[2].val.val_int64;
			char *sid_cond = rows->val[3].val.val_str;
			char *name = rows->val[4].val.val_str;
			int permit = rows->val[6].val.val_int16;
			char *full_name = rows->val[7].val.val_str;
			uint32_t devs_len = rows->val[9].len;
			char *devs = rows->val[9].val.val_str;
			// проверяем сид
			gboolean is_sid = base_check_sid(user_sid, client_ip_addr,
					(char*) sid, sid_len, sid_expire, sid_cond);

			// пользователь не заблокирован и совпали логин и пароль
			if (permit > 0 && is_sid) {
				is_auth = 0;					// успешно
				if (user_name)
					*user_name = g_strdup(name);
				if (user_devs) {
					// так нельзя, т.к. devs не заканчавается нулём:
					// *user_devs = g_strdup(devs);
					*user_devs = (char*) g_malloc(devs_len + 1);
					g_strlcpy(*user_devs, devs, devs_len + 1);
				}
				if (full_user_name)
					*full_user_name = g_strdup(full_name);
			} else {
				if (!is_sid)
					is_auth = -6;					// sid неактуален
				if (!permit)
					is_auth = -3;					// пользователь заблокирован
			}
		} else
			is_auth = -1;					// нет такого пользователя
	}
	g_free(msg);
	pgsql_rows_value_free(rows);
	// закрытие базы данных PostgreSQL
	base_close(db);
	return is_auth;
}

// добавить к пользователю imei, если e-mail совпал
// return TRUE если добавлен новый imei в список, FALSE - если уже есть imei в списке или ошибка
gboolean base_user_add_imei_by_email(const char *imei, const char *email) {
	gboolean is_add = FALSE;
	if (!imei || !email)
		return FALSE;
	void *db = base_open();
	if (!db)
		return -5;
	char *es_email = pgsql_escape((PGconn *) db, (char*) email);
	char *query = g_strdup_printf(
			"SELECT login, devs FROM users WHERE email='%s'", es_email);
	char *msg = NULL;
	int rows_count = 0;
	SQL_ROW_VALUE *rows = NULL;
	int ret = base_query(db, query, &rows_count, &rows, &msg);
	g_free(query);
	g_free(es_email);
	if (ret) {
		if (rows_count > 0) {
			char *login = rows->val[0].val.val_str;
			char *devs = rows->val[1].val.val_str;
			guint count_devs = 0;
			gboolean is_imei = FALSE;
			// смотрим, есть ли уже такое устройство в списке
			if (devs) {
				guint i;
				gchar **str_step = g_strsplit(devs, ";", -1);
				count_devs = g_strv_length(str_step);
				for (i = 0; i < count_devs; i++) {
					const char *user_imei = str_step[i];
					if (user_imei && !g_ascii_strcasecmp(imei, user_imei)) {
						is_imei = TRUE;	// imei найден в списке (уже добавлен)
						break;
					}
				}
				g_strfreev(str_step);
			}

			// добавить imei в список устройств
			if (!is_imei) {
				char *msg = NULL;
				char *es_imei = pgsql_escape((PGconn *) db, (char*) imei);
				char *devs_new =
						(count_devs > 0) ?
								g_strdup_printf("%s;%s", devs, es_imei) :
								es_imei;
				char *query = g_strdup_printf(
						"UPDATE users SET devs = '%s' WHERE login='%s'",
						devs_new, login);
				// imei добавляется в бд
				is_add = base_exec(db, query, &msg);
				if (es_imei != devs_new)
					g_free(es_imei);
				g_free(devs_new);
				g_free(query);
				g_free(msg);
			}
		}
	}
	g_free(msg);
	pgsql_rows_value_free(rows);
	// закрытие базы данных
	base_close(db);
	return is_add;
}
