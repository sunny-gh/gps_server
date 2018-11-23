//============================================================================
// Name        : daemon.cpp
// Author      : Aleksandr Lysikov
// Version     :
// Copyright   :
// Description : Стартовый файл демона (службы)
//============================================================================

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <execinfo.h>
#include <unistd.h>
#include <errno.h>
#include <wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <glib.h>
#include "monitor.h"
#include "work.h"
#include "log.h"

//#include "base_lib.h"
//#include "server_thread.h"

/* Настройки проекта в Eclipse:

 Свойства проекта (меню Project - Properties) и С/С++ Build - Settings - GCC C Compiler (default) и GCC C Linker
 command line для компилятора:
 ${COMMAND} $(shell pkg-config --cflags libpq json-glib-1.0 glib-2.0  zlib libcurl) ${FLAGS} ${OUTPUT_FLAG} ${OUTPUT_PREFIX}${OUTPUT} ${INPUTS}
 command line для линкера:
 ${COMMAND} $(shell pkg-config --libs libpq json-glib-1.0 glib-2.0 zlib libcurl)  -lpthread ${FLAGS} ${OUTPUT_FLAG} ${OUTPUT_PREFIX}${OUTPUT} ${INPUTS}


 Command line pattern (Expert settings) box:
 win: `pkg-config --cflags --libs gtk+-3.0`
 lunux: $(shell pkg-config --cflags --libs gtk+-3.0)
 include: $(shell pkg-config --cflags libplq json-glib-1.0 glib-2.0  zlib)
 libs: $(shell pkg-config --libs libpq json-glib-1.0 glib-2.0 zlib)  -lpthread

 Добавить include:
 Свойства проекта (меню Project - Properties) и переходим в левом столбце в раздел C/C++ General - Path and Symbols
 Добавить libs:
 С/С++ Build - Settings - GCC C Linker - Libraries и добавить необходимые библиотеки

 # pkg-config  --libs glib-2.0
 -lglib-2.0
 # pkg-config  --cflags glib-2.0
 -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include
 # pkg-config --libs gtk+-3.0
 -lgtk-3 -lgdk-3 -latk-1.0 -lgio-2.0 -lpangocairo-1.0 -lgdk_pixbuf-2.0 -lcairo-gobject -lpango-1.0 -lcairo -lgobject-2.0 -lglib-2.0
 # pkg-config --cflags gtk+-3.0
 -pthread -I/usr/include/gtk-3.0 -I/usr/include/pango-1.0 -I/usr/include/gio-unix-2.0/ -I/usr/include/atk-1.0 -I/usr/include/cairo -I/usr/include/gdk-pixbuf-2.0 -I/usr/include/freetype2 -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include -I/usr/include/pixman-1 -I/usr/include/libpng12
 */

int main(int argc, char** argv) {
	int status;
	int pid;

	// если параметров командной строки меньше двух, то покажем как использовать демона
	if (argc != 2) {
		printf("Usage: ./gps_server start | stop | restart\n");
		WriteLog("Usage: ./gps_server start | stop | restart %d\n");
		return -1;
	}
	// выполняемое действие
	char *action = argv[1];

	// иннициализация
	log_init();

	// отладка, потоки не запускаются, работает как обычная программа
	if (!g_strcmp0(action, "debug")) {
		// Работа программы в режиме отладки
		DebugProc();
		return 0;
	}

	// что то другое кроме старта
	if (g_strcmp0(action, "start")) {
		int sid = -1;
		// остановка службы
		if (!g_strcmp0(action, "stop"))
			sid = SIGQUIT;
		// перезапуск службы
		else if (!g_strcmp0(action, "restart"))
			sid = SIGUSR1;
		// определились с сигналом, который нужно отправлять
		if (sid > 0) {
			// Узнать pid запущенной службы
			int pid = getPid();
			// шлём сигнал уже запущенному приложению
			if (pid > 0 && !kill(pid, sid)) {
				//printf("[daemon] %s pid=%d successfully\n",action, pid);
				WriteLog("[daemon] %s pid=%d successfully\n", action, pid);
			} else {
				printf("[daemon] %s pid=%d error\n", action, pid);
				WriteLog("[daemon]%s pid=%d error\n", action, pid);
			}
		} else {
			printf("[daemon] unknown arg=%s\n", action);
			WriteLog("[daemon] unknown arg=%s\n", action);
			return 0;
		}
		exit(0);
	}
	// продолжаем запуск службы...

	// создаем потомка
	pid = fork();

	if (pid == -1) // если не удалось запустить потомка
			{
		// выведем на экран ошибку и её описание
		printf("Error: Start Daemon failed (%s)\n", strerror(errno));

		return -1;
	} else if (!pid) // если это потомок
	{
		WriteLog("start child %d\n", pid);
		// данный код уже выполняется в процессе потомка
		// разрешаем выставлять все биты прав на создаваемые файлы,
		// иначе у нас могут быть проблемы с правами доступа
		umask(0);

		// создаём новый сеанс, чтобы не зависеть от родителя
		setsid();

		// переходим в корень диска, если мы этого не сделаем, то могут быть проблемы.
		// к примеру с размантированием дисков
		chdir("/");

		// закрываем дискрипторы ввода/вывода/ошибок, так как нам они больше не понадобятся
		//close(STDIN_FILENO);
		//close(STDOUT_FILENO);
		//close(STDERR_FILENO);

		// Данная функция будет осуществлять слежение за процессом
		status = MonitorProc();

		return status;
	} else // если это родитель
	{
		// завершим процес, т.к. основную свою задачу (запуск демона) мы выполнили
		return 0;
	}
}
