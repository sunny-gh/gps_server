// lib_postgresql.h - Функции для работы с базой данных PostregSQL
#ifndef _LIB_POSTGRESQL_H_
#define _LIB_POSTGRESQL_H_
// Make sure we can call this stuff from C++.
#ifdef __cplusplus
extern "C" {
#endif

#include <libpq-fe.h>

#if !defined(boolean)
typedef unsigned char boolean;
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0;
#endif

// Возможные значения типов записи
#define SQL_CHAR	 1
#define SQL_INT16	 2
#define SQL_INT32	 3
#define SQL_INT64	 4
#define SQL_FLOAT    5
#define SQL_DOUBLE   5
#define SQL_TEXT     6
#define SQL_BLOB     7
#define SQL_NULL     8

// значение одного поля в таблице
typedef struct _SQLITE_VALUE_ {
	int32_t len; // длина записи
	char type; // тип записи
	// значение записи
	union {
		int8_t val_char;
		int16_t val_int16;
		int32_t val_int32;
		int64_t val_int64;
		float val_float;
		double val_double;
		char *val_str;
		uint8_t *val_blob;
	} val;
} SQL_VALUE;

// Содержание одной строки в таблице
typedef struct _SQLITE_ROW_ // _SQLITE_COLUMN_
{
	int count; // число колонок в строке
	SQL_VALUE *val; // массив значений полей
} SQL_ROW_VALUE;

// открытие базы данных PostgreSQL
// pghost -  имя или IP-адрес хоста, на котором находится сервер баз данных (NULL - локальное соединение)
// pgport - номер порта (NULL - локальное соединение, по умолчанию -  5432)
// dbName – имя базы данных;
// login, pwd – имя пользователя и пароль доступа к базе данных.
// error_message - сюда будет записано текстовое сообщение об ошибке (память требует удаления через sqlite_free())
// return: идентификатор базы данных при успешном завершении, NULL при возникновении ошибки. 
PGconn* pgsql_open(const char *pghost, const char *pgport, const char *login,
		const char *passwd, const char *dbname, char **error_message);

// закрытие базы данных PostgreSQL
// conn - идентификатор открытой базы данных
void pgsql_close(PGconn *conn);

// чистить память выделенную через PQerrorMessage()
void pgsql_free(char *memory);

// выполняет SQL запрос query к базе данных, который не возвращает данные
// conn - идентификатор открытой базы данных
// query - строка запроса, пример:
// CREATE TABLE IF NOT EXISTS auth_log(id SERIAL PRIMARY KEY, imei VARCHAR(20) NOT NULL, time BIGINT,ip INTEGER, port INTEGER, status SMALLINT,ver SMALLINT,raw_data VARCHAR(200))
// CREATE INDEX imei_auth_log_index ON auth_log(imei)
// INSERT INTO auth_log VALUES(DEFAULT, '1', 'привет')
// INSERT INTO auth_log(imei, time) VALUES('%s', '%lld')
// error_message - сюда будет записано текстовое сообщение об ошибке (память не требует удаления)
// return: возвращает код ошибки SQL (если не равно PGRES_COMMAND_OK(1) то ошибка)
ExecStatusType pgsql_exec(PGconn *conn, const char *query,
		char **error_message);

// Подготовка SQL запроса к базе данных
// conn - идентификатор открытой базы данных
// query - строка запроса
// [out] row_count - число строк с возвращаемыми данными
// [out] row_out - возвращаемые данные, требуют удаления через pgsql_row_value_free()
// [out] error_message - сюда будет записано текстовое сообщение об ошибке (память не требует удаления)
// return: возвращает код ошибки SQL (если не равно PGRES_COMMAND_OK(1) или PGRES_TUPLES_OK(2) то ошибка)
int pgsql_query(PGconn *conn, const char *query, int *row_count_out,
		SQL_ROW_VALUE **row_out, char **error_message);

// чистим память после выборки строк, последняя строка = NULL
// row - Указатель на массив строк с данными
void pgsql_rows_value_free(SQL_ROW_VALUE *rows_val);

// Начало одной транзакции
// error_message - сюда будет записано текстовое сообщение об ошибке (память не требует удаления)
boolean pgsql_start_transaction(PGconn *conn, char **error_message);
// Конец транзакции
// error_message - сюда будет записано текстовое сообщение об ошибке (память не требует удаления)
boolean pgsql_end_transaction(PGconn *conn, char **error_message);

// чистка базы данных от удалённых данных
// db - идентификатор открытой базы данных
boolean pgsql_vacuum(PGconn *conn, char **error_message);

// экранирование строки
// db - идентификатор открытой базы данных
char* pgsql_escape(PGconn *conn, const char *str);

#ifdef __cplusplus
} /* End of the 'extern "C"' block */
#endif

#endif // #ifndef _LIB_POSTGRESQL_H_
