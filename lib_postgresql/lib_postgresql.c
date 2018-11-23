// lib_postgresql.c - Функции для работы с базой данных PostregSQL

#define _CRT_SECURE_NO_WARNINGS
//#define USE_SQLCIPHER	// использовать шифрование базы (добавить: -DSQLITE_HAS_CODEC -Dcrypto)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <glib.h>
#include "lib_postgresql.h"

//from #include "server/catalog/pg_type.h"
// Типы данных, определяется через PQftype(res, col)
#define BPCHAROID		1042 // CHARACTER(n), CHAR(n)
#define VARCHAROID		1043 // VARCHAR
#define TEXTOID			25	 // TEXT
#define INT8OID			20	 // BIGINT
#define INT2OID			21	 // SMALLINT, SMALLSERIAL
#define INT2VECTOROID	22	 // int2vector
#define INT4OID			23	 // INTEGER, SERIAL
#define NUMERICOID		1700 // NUMERIC
#define FLOAT4OID		700	 // REAL
#define FLOAT8OID		701	 // DOUBLE PRECISION
#define BOOLOID			16	 // BOOL
#define BYTEAOID		17	 // BYTEA
#define CHAROID			18	 // CHAR
#define DATEOID			1082 //	date
#define TIMEOID			1083 //	time
#define TIMESTAMPOID	1114 //	timestamp
#define TIMESTAMPTZOID	1184 //	timestamptz
#define INTERVALOID		1186 //	interval
#define TIDOID			27	 // tid
#define XIDOID			28	 // xid
#define CIDOID			29	 // cid
#define ABSTIMEOID		702	 // abstime
#define RELTIMEOID		703	 // reltime
#define TINTERVALOID	704	 // tinterval
#define UNKNOWNOID		705	 // unknown

/*
 * SQL-строка с запрсом к бд

 Типы данных:
 SMALLINT					2 bytes	small-range integer					-32768 to +32767
 INTEGER						4 bytes	typical choice for integer			-2147483648 to +2147483647
 BIGINT						8 bytes	large-range integer					-9223372036854775808 to +9223372036854775807
 DECIMAL						variable	user-specified precision, exact		up to 131072 digits before the decimal point; up to 16383 digits after the decimal point
 NUMERIC(precision, scale)	variable	user-specified precision, exact		up to 131072 digits before the decimal point; up to 16383 digits after the decimal point
 REAL						4 bytes	variable-precision, inexact			6 decimal digits precision
 DOUBLE PRECISION			8 bytes	variable-precision, inexact			15 decimal digits precision
 SMALLSERIAL					2 bytes	small autoincrementing integer		1 to 32767
 SERIAL						4 bytes	autoincrementing integer			1 to 2147483647
 BIGSERIAL					8 bytes	large autoincrementing integer		1 to 9223372036854775807

 VARCHAR(n)					variable-length with limit
 CHARACTER(n), CHAR(n)		fixed-length, blank padded
 TEXT						variable unlimited length

 BYTEA			1 or 4 bytes plus the actual binary string		variable-length binary string
 BOOLEAN			1 byte		state of true or false


 * SELECT E'\\xDEADBEEF';
 * SELECT * FROM data where
 * Выборка только двух столбцов с ограничением числа полученых строк не более 300
 * SELECT id, sd FROM data LIMIT 300
 * сортировка по номеру записи,(ASC - по возрастанию, DESC - по убыванию)
 * SELECT id, sd FROM data ORDER BY id DESC
 * Выборка данных по диапазону времени:
 * SELECT * FROM data WHERE time_gmt>449464766900000 and time_gmt<449464766910000"
 * Выборка по подобию строку
 * SELECT * FROM data WHERE hex(sd) LIKE '%370%' [% любое кол-во любых символов включая 0, _ один символ]
 * Преобразование двоичных данных (BLOB) в шестнадцатеричную строку:
 * hex(x'0806') -> '08f6' или quote(sd) -> X'08f6'
 * Получаем подстроку, начиная с 9 байта и длиной 2 байта
 * substr(x'031407301210361320690000',3,2) -> x'0730'
 * преобразование типов:
 * CAST(substr(sd,5,2) as TEXT)


 Установка и запуск сервера БД
 /usr/local/pgsql/bin/initdb -D /usr/local/pgsql/data
 /usr/local/pgsql/bin/postgres -D /usr/local/pgsql/data >logfile 2>&1 &
 /usr/local/pgsql/bin/createdb test
 /usr/local/pgsql/bin/psql test

 You can now start the database server using:
 "C:\msys32\usr\local\pgsql\bin\pg_ctl" -D "C:/msys32/usr/local/pgsql/data" -l logfile start

 C:\msys32\usr\local\pgsql
 Создать базу
 initdb -D ../data -E UTF8 -U postgres --locale en_US.UTF-8
 Запустить базу
 "pg_ctl" -D "../data" -l logfile start
 Создать новую базу данных (по умолчанию есть база postgres)
 createdb test

 // создать пользователя и созать БД
 createuser -a -d my_user -E -P
 createdb -O my_user my_database
 */

#ifdef _WIN32
#pragma comment(lib,"libpq.lib")
#endif

// Make sure we can call this stuff from C++.
#if __cplusplus
extern "C" {
#endif

// открытие базы данных PostgreSQL
// pghost -  имя или IP-адрес хоста, на котором находится сервер баз данных (NULL - локальное соединение)
// pgport - номер порта (NULL - локальное соединение, по умолчанию -  5432)
// dbName – имя базы данных;
// login, pwd – имя пользователя и пароль доступа к базе данных.
// error_message - сюда будет записано текстовое сообщение об ошибке (память требует удаления через pgsql_free())
// return: идентификатор базы данных при успешном завершении, NULL при возникновении ошибки. 
PGconn* pgsql_open(const char *pghost, const char *pgport, const char *login,
		const char *passwd, const char *dbname, char **error_message) {
	PGconn *conn = NULL;
	// port 
	//conn = PQconnectdb("dbname=ljdata host=localhost user=dataman password=supersecret");
	// pgoptions – дополнительные опции, посылаемые серверу для трассировки/отладки соединения (м.б. NULL)
	// pgtty – терминал или файл для вывода отладочной информации (м.б. NULL)
	conn = PQsetdbLogin(pghost, pgport, NULL, NULL, dbname, login, passwd);
	if (PQstatus(conn) == CONNECTION_BAD) {
		if (error_message)
			*error_message = PQerrorMessage(conn);
		//*error_message = g_strdup_printf("Can't open database: %s\n", PQerrorMessage(conn));
		return NULL;
	}
	return conn;
}

// закрытие базы данных PostgreSQL
// conn - идентификатор открытой базы данных
void pgsql_close(PGconn *conn) {
	if (conn)
		PQfinish(conn);
}

// чистить память выделенную
void pgsql_free(char *memory) {
	if (memory)
		PQfreemem(memory);
}

// выполняет SQL запрос query к базе данных, который не возвращает данные
// conn - идентификатор открытой базы данных
// query - строка запроса, пример:
// CREATE TABLE IF NOT EXISTS auth_log(id SERIAL PRIMARY KEY, imei VARCHAR(20) NOT NULL, time BIGINT,ip INTEGER, port INTEGER, status SMALLINT,ver SMALLINT,raw_data VARCHAR(200))
// CREATE INDEX imei_auth_log_index ON auth_log(imei)
// INSERT INTO auth_log VALUES(DEFAULT, '1', 'привет')
// INSERT INTO auth_log(imei, time) VALUES('%s', '%lld')
// [out] error_message - сюда будет записано текстовое сообщение об ошибке (память не требует удаления)
// return: возвращает код ошибки SQL (если не равно PGRES_COMMAND_OK(1) то ошибка)
ExecStatusType pgsql_exec(PGconn *conn, const char *query,
		char **error_message) {
	ExecStatusType status;	// enum
	PGresult *res;
	if (!conn || !query)
		return PGRES_EMPTY_QUERY;
	res = PQexec((PGconn *) conn, query);
	//char *query_str = PQescapeLiteral(conn, query, strlen(query));
	//PGresult *res = PQexec((PGconn *)conn, query_str);
	//PQfreemem(query_str);

	status = PQresultStatus(res);
	if (status != PGRES_COMMAND_OK) {
		if (error_message)
			*error_message = PQerrorMessage(conn);
		// PQresultErrorMessage(res); -сохраняет код ошибки до удаления соединения PGresult - PQclear(res)
		// PQerrorMessage(conn); -сохраняет код ошибки до удаления соединения PGconn -  PQfinish(conn);
	}
	PQclear(res);
	return status;
}

// Подготовка SQL запроса к базе данных
// conn - идентификатор открытой базы данных
// query - строка запроса
// [out] row_count - число строк с возвращаемыми данными
// [out] row_out - возвращаемые данные, требуют удаления через pgsql_row_value_free()
// [out] error_message - сюда будет записано текстовое сообщение об ошибке (память не требует удаления)
// return: возвращает код ошибки SQL (если не равно PGRES_COMMAND_OK(1) или PGRES_TUPLES_OK(2) то ошибка)
int pgsql_query(PGconn *conn, const char *query, int *rows_count_out,
		SQL_ROW_VALUE **rows_out, char **error_message) {
	ExecStatusType status;		// enum
	PGresult *res;
	if (!conn || !query || !rows_out)
		return PGRES_EMPTY_QUERY;
	//char *query_str = PQescapeLiteral(conn, query, strlen(query));
	//PGresult *res = PQexec((PGconn *)conn, query_str);
	//PQfreemem(query_str);
	res = PQexec((PGconn *) conn, query);
	status = PQresultStatus(res);
	if (status != PGRES_TUPLES_OK) {
		// обнуляем число выходных строк
		if (rows_count_out)
			*rows_count_out = 0;
		if (error_message && status != PGRES_COMMAND_OK)
			*error_message = PQerrorMessage(conn);
		// PQresultErrorMessage(res); -сохраняет код ошибки до удаления соединения PGresult - PQclear(res)
		// PQerrorMessage(conn); -сохраняет код ошибки до удаления соединения PGconn -  PQfinish(conn);
		return status;
	} else {
		// число полученных записей (строк)
		int row_count = PQntuples(res);
		// число полученных колонок
		int col_count = PQnfields(res);
		//int is_bin = PQbinaryTuples(res);
		SQL_ROW_VALUE *rows_val = NULL;
		// если что-то  нашли
		if (row_count > 0) {
			rows_val = (SQL_ROW_VALUE*) calloc(1,
					(row_count + 1) * sizeof(SQL_ROW_VALUE));// последняя строка = NULL
			// массив типов колонок
			int *types = (int*) malloc(col_count * sizeof(int));
			int col, row;
			for (col = 0; col < col_count; col++) {
				//int size_col = PQfsize(res, col);// сколько байт выделено под эту колонку, -1 переменная величина
				//int a1 = PQfmod(res, col);// сколько максимально может быть выделено байт для переменной ячейки
				//int a4 = PQftablecol(res, col);// какая это колонка в таблице по номеру от 1
				types[col] = PQftype(res, col);			// тип данных в колонке
			}
			for (row = 0; row < row_count; row++) {
				rows_val[row].count = col_count;			// // число колонок
				rows_val[row].val = (SQL_VALUE*) calloc(1,
						col_count * sizeof(SQL_VALUE));
				for (col = 0; col < col_count; col++) {
					char *data = PQgetvalue(res, row, col);
					SQL_VALUE *cur_val = rows_val[row].val + col;
					cur_val->len = PQgetlength(res, row, col);// сколько байт надо будет получить
					int is_null = PQgetisnull(res, row, col);// NULL вместо данных
					if (is_null)
						cur_val->type = SQL_NULL;
					else
						switch (types[col]) {
						case BOOLOID:
						case CHAROID:	// CHAR
							cur_val->type = SQL_CHAR;
							cur_val->val.val_char = (int8_t) atoi(data);
							break;
						case INT2OID:	// INT2, SMALLINT, SMALLSERIAL
							cur_val->type = SQL_INT16;
							cur_val->val.val_int16 = (int16_t) atoi(data);
							break;
						case INT4OID:	// INT4, INTEGER, SERIAL
							cur_val->type = SQL_INT32;
							cur_val->val.val_int32 = (int32_t) atoi(data);
							break;
						case INT8OID:	// INT8, INTEGER, SERIAL
							cur_val->type = SQL_INT64;
							cur_val->val.val_int64 = (int64_t) atoll(data);
							break;
							// локаленезависимое преобразование в отличии от strtod и atof
						case FLOAT4OID:	// FLOAT4, REAL
							cur_val->type = SQL_FLOAT;
							cur_val->val.val_float = (float) g_ascii_strtod(
									data, NULL);
							break;
						case FLOAT8OID:	// FLOAT4, REAL
							cur_val->type = SQL_DOUBLE;
							cur_val->val.val_double = g_ascii_strtod(data,
									NULL);
							break;
						case BPCHAROID:	// BPCHAR, CHARACTER(n), CHAR(n)
						case VARCHAROID:	// VARCHAR
							cur_val->type = SQL_TEXT;
							cur_val->val.val_str = (char*) calloc(1,
									cur_val->len + 1);
							g_strlcpy(cur_val->val.val_str, data,
									cur_val->len + 1);// (dest_size - 1) characters will be copied
											/*#ifdef _WIN32
											 cur_val->val.val_str = _strdup(data);
											 #else
											 cur_val->val.val_str = strdup(data);
											 #endif*/
							break;
						case BYTEAOID:	// BYTEA
						default:	// все остальные типы
							cur_val->type = SQL_BLOB;
							cur_val->val.val_blob = calloc(1, cur_val->len + 1);
							memcpy(cur_val->val.val_blob, data, cur_val->len);
							cur_val->val.val_blob[cur_val->len] = 0;
							break;
						}
				}
			}
			free(types);
			*rows_out = rows_val;
			// число выходных строк
			if (rows_count_out)
				*rows_count_out = row_count;
		}
	}
	PQclear(res);
	return status;
}

// чистим память после выборки одной строки
// row - Указатель на строку с данными
static boolean pgsql_row_value_clear(SQL_ROW_VALUE *row_val) {
	if (!row_val)
		return FALSE;
	int iCol;	// порядковый номер колонки в строке
	for (iCol = 0; iCol < row_val->count; iCol++) {
		SQL_VALUE *cur_val = row_val->val + iCol;
		// удаляем значения в стобцах
		if (cur_val->val.val_str
				&& (cur_val->type == SQL_BLOB || cur_val->type == SQL_TEXT))
			free(cur_val->val.val_str);
	}
	// удаляем всю строку
	free((char*) row_val->val);

	return TRUE;
}

// чистим память после выборки строк, последняя строка = NULL
// row - Указатель на массив строк с данными
void pgsql_rows_value_free(SQL_ROW_VALUE *rows_val) {
	SQL_ROW_VALUE *row_val = rows_val;
	int i = 1;
	if (!rows_val)
		return;
	while (row_val->val) {
		pgsql_row_value_clear(row_val);
		row_val = rows_val + i;
		i++;
	}
	// удаляем структуру
	free((char*) rows_val);
}

/*
 // Выбирает следующую запись из результата запроса и возвращает массив
 // res - идентификатор результата полученный через sqlite_query()
 // row_out: Указатель на колонку или NULL при ошибке или отсутсвии данных(кончились строки), память требует удаления через sqlite_free()
 // return: возвращает код ошибки SQL: SQLITE_ROW(100) ещё есть данные, SQLITE_DONE(101) конец, SQLITE_CONSTRAINT(19) - данные не уникальны и добавлены не будут
 int sqlite_fetch_array(sqlite3_stmt *res, SQLITE_ROW_VALUE **row_out)
 {
 SQLITE_ROW_VALUE *row = NULL;
 // переход на след. строку
 int rc = sqlite3_step(res);
 if (rc == SQLITE_ROW)// SQLITE_ROW(100) или SQLITE_DONE(101) или SQLITE_BUSY(5)
 {
 int iCol;// порядковый номер колонки в строке
 // выделяем память для строки с данными
 row = (SQLITE_ROW_VALUE*)sqlite3_malloc(sizeof(SQLITE_ROW_VALUE));
 row->count = sqlite3_column_count(res);// узнаём число столбцов
 //row->count  = sqlite3_data_count(res);// узнаём число столбцов
 // выделяем память для всех стобцов
 row->val = (SQLITE_VALUE*)sqlite3_malloc(row->count*sizeof(SQLITE_VALUE));
 for (iCol = 0; iCol<row->count; iCol++)
 {
 SQLITE_VALUE *cur_val = row->val + iCol;
 cur_val->len = sqlite3_column_bytes(res, iCol);// сколько байт надо будет получить
 cur_val->type = sqlite3_column_type(res, iCol);// тип поля
 if (cur_val->type == SQLITE_INTEGER)
 {
 cur_val->val.val_int64 = sqlite3_column_int64(res, iCol);
 cur_val->val.val_int = sqlite3_column_int(res, iCol);
 }
 else if (cur_val->type == SQLITE_FLOAT)
 cur_val->val.val_float = sqlite3_column_double(res, iCol);
 else if (cur_val->type == SQLITE_BLOB)
 cur_val->val.val_blob = (unsigned char*)sqlite3_column_blob(res, iCol);
 else if (cur_val->type == SQLITE_TEXT)
 cur_val->val.val_str = (char*)sqlite3_column_text(res, iCol);//sqlite3_mprintf("%s",sqlite3_column_text(res,iCol));
 else
 cur_val->val.val_str = NULL;
 }
 }
 *row_out = row;
 return rc;
 }
 */

// чистим память после завершения обработки запроса
// res - идентификатор результата полученный через pgsql_query()
void pgsql_query_free(void *res) {
	PQclear((PGresult *) res);
}

// Начало одной транзакции
// error_message - сюда будет записано текстовое сообщение об ошибке (память не требует удаления)
boolean pgsql_start_transaction(PGconn *conn, char **error_message) {
	if (conn) {
		PGresult *res = PQexec(conn, "BEGIN");
		if (PQresultStatus(res) == PGRES_COMMAND_OK)
			return TRUE;
		if (error_message)
			*error_message = PQerrorMessage(conn);
	}
	return FALSE;
}

// Конец транзакции
// error_message - сюда будет записано текстовое сообщение об ошибке (память не требует удаления)
boolean pgsql_end_transaction(PGconn *conn, char **error_message) {
	if (conn) {
		PGresult *res = PQexec(conn, "END");
		if (PQresultStatus(res) == PGRES_COMMAND_OK)
			return TRUE;
		if (error_message)
			*error_message = PQerrorMessage(conn);
	}
	return FALSE;
}

// чистка базы данных от удалённых данных
// db - идентификатор открытой базы данных
boolean pgsql_vacuum(PGconn *conn, char **error_message) {
	if (conn) {
		PGresult *res = PQexec(conn, "VACUUM");
		if (PQresultStatus(res) == PGRES_COMMAND_OK)
			return TRUE;
		if (error_message)
			*error_message = PQerrorMessage(conn);
	}
	return FALSE;
}

// экранирование строки, строку удалять через g_free()
// db - идентификатор открытой базы данных
char* pgsql_escape(PGconn *conn, const char *str) {
	size_t len;
	if (!str)
		return NULL;
	len = strlen(str);
	if (!len)
		return g_strdup(str);
	char *to = g_malloc0(len * 2);
	//return PQescapeLiteral(conn, str, strlen(str));// одиночные ковычки добавляет по краям строки, как 'str'
	//size_t count = 
	PQescapeStringConn(conn, to, str, len, NULL);
	return to;
}

/*
 // перевод массива в строку для сохранения в blob
 // строка требуют удаления через sqlite_free()
 char* get_string_from_blob(uint8_t *blob, int len)
 {
 if (!blob)
 return NULL;
 int i;// , len = strlen((const char*)blob);
 char *str = (char*)sqlite3_malloc(len * 2 + 1);
 for (i = 0; i<len; i++)
 {
 sprintf(str + i * 2, "%02x", (uint8_t)*(blob + i));
 }
 return str;
 }

 // перевод строки вида "A0EF1C..." в массив чисел
 uint8_t* get_blob_from_string(unsigned char *str, int len)
 {
 if (!str)
 return NULL;
 int i;//, len = strlen((const char*)str);
 uint8_t *blob = (uint8_t*)sqlite3_malloc(len * 2 + 1);
 //return g_strdup(str);
 
 for (i = 0; i<len; i++)
 {
 sprintf((char*)(blob + i * 2), "%02x", *(str + i));
 }
 return blob;
 }*/

/*

 // дополнительная функция для sqlite по переводу байта в число
 static void byte_to_bin(sqlite3_context *context, int argc, sqlite3_value **argv)
 {
 unsigned char *text;
 //void *user_data = sqlite3_user_data(context);
 if (argc != 1) sqlite3_result_null(context);
 text = (unsigned char *)sqlite3_value_blob(argv[0]);
 if (text && text[0])
 {
 int result = (int)text[0];
 sqlite3_result_int(context, result);
 return;
 }
 sqlite3_result_null(context);
 return;
 }

 // открытие базы данных SQLite
 // filename_utf8 - имя файла с базой данных
 // flags - флаги доступа к базе (SQLITE_OPEN_READONLY, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE)
 // error_message - сюда будет записано текстовое сообщение об ошибке (память требует удаления через sqlite_free())
 // return: идентификатор базы данных при успешном завершении, NULL при возникновении ошибки. 
 sqlite3* sqlite_open(char *filename_utf8, int flags, char **error_message)
 {
 sqlite3 *db = NULL;
 int rc = sqlite3_open(filename_utf8, &db);
 //int rc = sqlite3_open_v2(filename_utf8,&db,flags,NULL);// Serialized (надо указать флаг SQLITE_OPEN_FULLMUTEX при открытии соединения). В этом режиме потоки могут как угодно дергать вызовы SQLite, никаких ограничений. Но все вызовы блокируют друг друга и обрабатываются строго последовательно.
 //int rc = sqlite3_open_v2(filename_utf8,&db,flags| SQLITE_OPEN_FULLMUTEX,NULL);// Serialized (надо указать флаг SQLITE_OPEN_FULLMUTEX при открытии соединения). В этом режиме потоки могут как угодно дергать вызовы SQLite, никаких ограничений. Но все вызовы блокируют друг друга и обрабатываются строго последовательно.
 //int rc = sqlite3_open_v2(filename_utf8,&db,SQLITE_OPEN_FULLMUTEX | SQLITE_OPEN_CREATE,NULL);
 // Serialized (надо указать флаг SQLITE_OPEN_FULLMUTEX при открытии соединения).
 // В этом режиме потоки могут как угодно дергать вызовы SQLite, никаких ограничений.
 // Но все вызовы блокируют друг друга и обрабатываются строго последовательно.
 if(rc!=SQLITE_OK)
 {
 if(error_message)
 *error_message = sqlite3_mprintf("Can't open database: %s\n", sqlite3_errmsg(db));
 sqlite3_close(db);
 return NULL;
 }
 // добавляем пользовательские функции
 sqlite3_create_function(db, "byte_to_bin", 1, SQLITE_UTF8, NULL, &byte_to_bin, NULL, NULL);
 #ifdef USE_SQLCIPHER
 // пароль к базе данных
 sqlite3_key(db,CIPHER_PASS,strlen(CIPHER_PASS));
 #endif
 return db;
 }

 // закрытие базы данных
 // db - идентификатор открытой базы данных
 void sqlite_close(sqlite3 *db)
 {
 if(db)
 sqlite3_close(db);
 }


 // чистка базы данных от удалённых данных
 // db - идентификатор открытой базы данных
 boolean sqlite_vacuum(sqlite3 *db)
 {
 int rc = sqlite_exec(db,"VACUUM",NULL);
 if(rc==SQLITE_OK)
 return TRUE;
 return FALSE;
 }
 */
// установить параметр настройки
/* подробнее - www.sqlite.org/pragma.html
 PRAGMA page_size = bytes; // размер страницы БД; страница БД - это единица обмена между диском и кэшом, разумно сделать равным размеру кластера диска (у меня 4096)
 PRAGMA cache_size = -kibibytes; // задать размер кэша соединения в килобайтах, по умолчанию он равен 2000 страниц БД
 PRAGMA encoding = "UTF-8";  // тип данных БД, всегда используйте UTF-8
 PRAGMA foreign_keys = 1; // включить поддержку foreign keys, по умолчанию - ОТКЛЮЧЕНА
 PRAGMA journal_mode = DELETE | TRUNCATE | PERSIST | MEMORY | WAL | OFF;  // задать тип журнала, см. sqlite_set_journal_mode()
 PRAGMA synchronous = 0 | OFF | 1 | NORMAL | 2 | FULL; // тип синхронизации транзакции, см. sqlite_set_synchronous_mode()
 */
/*boolean sqlite_set_pragma(sqlite3 *db, char *param, char *mode)
 {
 char *str_query = sqlite3_mprintf("PRAGMA %s = %s",param,mode);
 int rc = sqlite_exec(db,str_query,NULL);// default synchronous=FULL
 sqlite3_free(str_query);
 if(rc==SQLITE_OK)
 return TRUE;
 return FALSE;
 }

 // узнать параметр настройки
 // возвращённая строка требует удаления через sqlite_free() или sqlite3_free()
 char* sqlite_get_pragma(sqlite3 *db, char *param)
 {
 char *mode = NULL;
 sqlite3_stmt *res;
 char *str_query = sqlite3_mprintf("PRAGMA %s",param,mode);
 int rc = sqlite_query(db,str_query,&res,NULL);// default synchronous=FULL
 sqlite3_free(str_query);
 if(!rc)
 {
 SQLITE_ROW_VALUE *row;
 rc = sqlite_fetch_array(res,&row);
 if(rc==SQLITE_ROW && row)
 {
 if(row->val->type==SQLITE_INTEGER)
 mode = sqlite3_mprintf("%d",row->val->val.val_int);
 else if(row->val->type==SQLITE_FLOAT)
 mode = sqlite3_mprintf("%lf",row->val->val.val_float);
 else// if(row->val->type!=SQLITE_NULL)
 mode = sqlite3_mprintf("%s",row->val->val.val_str);
 sqlite_row_free(row);
 
 }
 sqlite_query_free(res);
 }
 return mode;
 }
 */
// узнать/установить тип журналирования
// PRAGMA journal_mode = DELETE | TRUNCATE | PERSIST | MEMORY | WAL | OFF;  // задать тип журнала, см. далее
/*
 По умолчанию журнал ведется в режиме DELETE .
 PRAGMA journal_mode = DELETE
 Это означает, что файл журнала удаляется после завершения транзакции. Сам факт наличия файла с журналом в этом режиме означает для SQLite, что транзакция не была завершена, база нуждается в восстановлении. Файл журнала имеет имя файла БД, к которому добавлено "-journal".
 В режиме TRUNCATE файл журнала обрезается до нуля (на некоторых системах это работает быстрее, чем удаление файла).
 В режиме PERSIST начало файла журнала забивается нулями (при этом его размер не меняется и он может занимать кучу места).
 В режиме MEMORY файл журнала ведется в памяти и это работает быстро, но не гарантирует восстановление базы при сбоях (копии данных-то нету на диске).
 В режиме WAL (Write-Ahead Logging) «читатели» БД и «писатели» в БД уже не мешают друг другу, то есть допускается модификация данных при одновременном чтении.
 А можно и совсем отключить журнал (PRAGMA journal_mode = OFF). В этой ситуации перестает работать откат транзакций (команда ROLLBACK) и база, скорее всего, испортится, если программа будет завершена аварийно.
 */
/*boolean sqlite_set_journal_mode(sqlite3 *db, char *mode)
 {
 return sqlite_set_pragma(db,"journal_mode",mode);
 }

 char* sqlite_get_journal_mode(sqlite3 *db)
 {
 return sqlite_get_pragma(db,"journal_mode");
 }
 */
// узнать/установить тип синхронизации
// PRAGMA synchronous = 0 | OFF | 1 | NORMAL | 2 | FULL; // тип синхронизации транзакции, см. далее
/*
 Режим OFF (или 0) означает: SQLite считает, что данные фиксированы на диске сразу после того как он передал их ОС (то есть сразу после вызова соот-го API ОС).
 Это означает, что целостность гарантирована при аварии приложения (поскольку ОС продолжает работать), но не при аварии ОС или отключении питания.
 Режим синхронизации NORMAL (или 1) гарантирует целостность при авариях ОС и почти при всех отключениях питания. Существует ненулевой шанс, что при потере питания в самый неподходящий момент база испортится. Это некий средний, компромисный режим по производительности и надежности.
 Режим FULL гарантирует целостность всегда и везде и при любых авариях. Но работает, разумеется, медленнее, поскольку в определенных местах делаются паузы ожидания. И это режим по умолчанию.
 */
/*boolean sqlite_set_synchronous_mode(sqlite3 *db, char *mode)
 {
 return sqlite_set_pragma(db,"synchronous",mode);
 }

 char* sqlite_get_synchronous_mode(sqlite3 *db)
 {
 return sqlite_get_pragma(db,"synchronous");
 }

 // Начало одной транзакции
 // error_message - сюда будет записано текстовое сообщение об ошибке (память требует удаления через sqlite_free())
 boolean sqlite_start_transaction(sqlite3 *db,char **zErrMsg)
 {
 if (db)
 {
 if (SQLITE_OK == sqlite_exec(db, "BEGIN", zErrMsg))
 return TRUE;
 else
 return FALSE;
 }
 else
 return FALSE;
 }

 // Конец транзакции
 // error_message - сюда будет записано текстовое сообщение об ошибке (память требует удаления через sqlite_free())
 boolean sqlite_end_transaction(sqlite3 *db,char **zErrMsg)
 {
 if (db)
 {
 if (SQLITE_OK == sqlite_exec(db, "COMMIT", zErrMsg))
 return TRUE;
 else
 return FALSE;
 }
 else
 return FALSE;
 }

 // выполняет SQL запрос query к базе данных, который не возвращает данные
 // db - идентификатор открытой базы данных
 // query - строка запроса, пример:
 // req = "create table if not exists table1(x INTEGER PRIMARY KEY ASC, y, z)";
 // CREATE TABLE t1(a INTEGER, b INTEGER, c VARCHAR(100), lon REAL KEY, nd BLOB);
 // INSERT INTO t1 VALUES(1,13153,'thirteen thousand one hundred fifty three');
 // UPDATE t2 SET b=468026 WHERE a=1;
 // UPDATE t2 SET c='one hundred forty eight thousand three hundred eighty two' WHERE a=1;
 // DELETE FROM t2 WHERE a>10 AND a<20000; 
 // DROP TABLE t1;
 // error_message - сюда будет записано текстовое сообщение об ошибке (память требует удаления через sqlite_free())
 // return: возвращает код ошибки SQL (если не равно SQLITE_OK(0) то ошибка)
 int sqlite_exec(sqlite3 *db, char *query, char **error_message)
 {
 char *zErrMsg = NULL;
 //int rc = sqlite3_exec(db, query, callback, 0, &zErrMsg);
 int rc = sqlite3_exec(db, query, NULL, 0, &zErrMsg);
 if(rc!=SQLITE_OK)
 {
 if(error_message)
 *error_message = sqlite3_mprintf("SQL error: %s", zErrMsg);
 if(zErrMsg)	sqlite3_free(zErrMsg);
 return rc;
 }
 return rc;
 }

 // Подготовка SQL запроса к базе данных
 // [in]  db - идентификатор открытой базы данных
 // [in]  query - строка запроса, пример:
 // [out] res - идентификатор запроса, память нужно будет удалять через sqlite_query_free()
 // "SELECT name FROM sqlite_master WHERE type='table' ORDER BY name"
 // [out] error_message - сюда будет записано текстовое сообщение об ошибке (память требует удаления через sqlite_free())
 // return: возвращает код ошибки SQL (если не равно SQLITE_OK(0) то ошибка)
 int sqlite_query(sqlite3 *db, char *query, sqlite3_stmt **res, char **error_message)
 {
 //sqlite3_stmt *res=NULL;// OUT: Statement handle
 const char *pzTail;// OUT: Pointer to unused portion of zSql
 int rc = sqlite3_prepare_v2(db,query,-1,res,&pzTail);
 if(rc!=SQLITE_OK)
 {
 if(error_message)
 *error_message = sqlite3_mprintf("SQL Query error: %s\n", sqlite3_errmsg(db));
 return rc;
 }
 // выполнение запроса + переход на первую строку
 //sqlite3_step(res);
 // откат обратно на начало
 //rc= sqlite3_reset(res);
 return rc;
 }

 // Выбирает следующую запись из результата запроса и возвращает массив
 // res - идентификатор результата полученный через sqlite_query()
 // row_out: Указатель на колонку или NULL при ошибке или отсутсвии данных(кончились строки), память требует удаления через sqlite_free()
 // return: возвращает код ошибки SQL: SQLITE_ROW(100) ещё есть данные, SQLITE_DONE(101) конец, SQLITE_CONSTRAINT(19) - данные не уникальны и добавлены не будут
 int sqlite_fetch_array(sqlite3_stmt *res,SQLITE_ROW_VALUE **row_out)
 {
 SQLITE_ROW_VALUE *row=NULL;
 // переход на след. строку
 int rc = sqlite3_step(res);
 if(rc==SQLITE_ROW)// SQLITE_ROW(100) или SQLITE_DONE(101) или SQLITE_BUSY(5)
 {
 int iCol;// порядковый номер колонки в строке
 // выделяем память для строки с данными
 row = (SQLITE_ROW_VALUE*)sqlite3_malloc(sizeof(SQLITE_ROW_VALUE));
 row->count  = sqlite3_column_count(res);// узнаём число столбцов
 //row->count  = sqlite3_data_count(res);// узнаём число столбцов
 // выделяем память для всех стобцов
 row->val = (SQLITE_VALUE*)sqlite3_malloc(row->count*sizeof(SQLITE_VALUE));
 for(iCol=0;iCol<row->count;iCol++)
 {
 SQLITE_VALUE *cur_val = row->val+iCol;
 cur_val->len = sqlite3_column_bytes(res,iCol);// сколько байт надо будет получить
 cur_val->type = sqlite3_column_type(res,iCol);// тип поля
 if(cur_val->type==SQLITE_INTEGER)
 {
 cur_val->val.val_int64 = sqlite3_column_int64(res,iCol);
 cur_val->val.val_int = sqlite3_column_int(res,iCol);
 }
 else if(cur_val->type==SQLITE_FLOAT)
 cur_val->val.val_float = sqlite3_column_double(res,iCol);
 else if(cur_val->type==SQLITE_BLOB)
 cur_val->val.val_blob = (unsigned char*)sqlite3_column_blob(res,iCol);
 else if(cur_val->type==SQLITE_TEXT)
 cur_val->val.val_str = (char*)sqlite3_column_text(res,iCol);//sqlite3_mprintf("%s",sqlite3_column_text(res,iCol));
 else
 cur_val->val.val_str = NULL;
 }
 }
 *row_out = row;
 return rc;
 }


 // чистить память выделенную через sqlite3_mprintf()
 void sqlite_free(char *memory)
 {
 if(memory)
 sqlite3_free(memory);
 }

 // чистим память после завершения обработки запроса
 // res - идентификатор результата полученный через sqlite_query()
 boolean sqlite_query_free(sqlite3_stmt *res)
 {
 int rc = sqlite3_finalize(res);
 if(rc!=SQLITE_OK)
 return FALSE;
 return TRUE;
 }

 // чистим память после выборки строки
 // row - Указатель на колонку, полученный через sqlite_fetch...()
 boolean sqlite_row_free(SQLITE_ROW_VALUE *row)
 {
 if(!row) return FALSE;
 /// удаляем всю строку
 sqlite3_free(row->val);
 // удаляем структуру
 sqlite3_free(row);
 return TRUE;
 }*/

#if __cplusplus
} /* End of the 'extern "C"' block */
#endif
