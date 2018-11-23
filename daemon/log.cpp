/*
 * log.cpp
 *
 *  Created on: 30.05.2016
 *      Author: sunny
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <locale.h>
#include <glib.h>

#ifdef WIN32
static const char *LogfFileName = "D:\\sunny\\My_project\\_gpstracker\\_tracker\\gps_server\\gps_server.log";
#else
//static const char *LogfFileName = "/var/log/gps_server.log";
static const char *LogfFileName = "/home/sunny/gps_server.log";
#endif

static GMutex mutex_log;

// иннициализация
void log_init(void) {
	g_mutex_init(&mutex_log);
	char *loc = setlocale(LC_TIME, "en");
	char *loc1 = setlocale(LC_TIME, "rus");
	loc = 0;
}

// функция записи лога
void WriteLog(const char* msg, ...) {
	gchar str_time[128];
	time_t cur_time;
	struct tm *m_time;

	va_list args;
	va_start(args, msg);
	gchar *str = g_strdup_vprintf(msg, args);
	va_end(args);
	g_mutex_lock(&mutex_log); // блокировка мьютекса
	// Считываем текущее время
	//GTimeVal result;
	//g_get_current_time (&result);
	cur_time = time(NULL);
	// преобразуем считанное время в локальное
	m_time = localtime(&cur_time);
	//Преобразуем локальное время в текстовую строку
	strftime(str_time, 128, "[%d.%m.%y %a %H:%M:%S %Z] ", m_time);

	//gchar *str_time = g_strdup_printf("[%s] ",ctime (&cur_time));
	// пишем данные в файл
	FILE *file = fopen(LogfFileName, "a");//The file is created if it does not exist.
	if (file != NULL) {
		fwrite(str_time, 1, strlen(str_time), file);
		fwrite(str, 1, strlen(str), file);
		fclose(file);
	}
	g_mutex_unlock(&mutex_log);	// разблокировка мьютекса
	g_free(str);
}

void my_printf(const char* msg, ...) {
	va_list args;
	va_start(args, msg);
	gchar *str = g_strdup_vprintf(msg, args);
	va_end(args);

	printf(str);
	g_free(str);
}
