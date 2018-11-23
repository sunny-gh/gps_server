/*
 * monitor.cpp
 *
 *  Created on: 30.05.2016
 *      Author: sunny
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <execinfo.h>
#include <unistd.h>
#include <errno.h>
#include <wait.h>
#include "monitor.h"
#include "log.h"
#include "work.h"

//#define PID_FILE "/var/run/gps_server.pid"
#define PID_FILE "/home/sunny/gps_server.pid"

// Узнать pid запущенной службы
int getPid() {
	FILE* f;
	int pid = -1;
	f = fopen(PID_FILE, "r");
	if (f) {
		fscanf(f, "%u", &pid);
		fclose(f);
	}
	return pid;
}

void SetPidFile(const char* Filename) {
	FILE* f;

	f = fopen(Filename, "w+");
	if (f) {
		fprintf(f, "%u", getpid());
		fclose(f);
	} else
		WriteLog("can't create pid file %s\n", Filename);
}

// Основная цель мониторинг — отслеживание состояния процесса демона. Нам будут важны только два момента:
//    Уведомление о завершении процесса демона.
//    Получение кода завершения демона.
// Весь мониторинг работы демона будет заключен в функцию MonitorProc.
// Весь смысл мониторинга заключается в том, чтобы запустить дочерний процесс и следить за ним,
// и в зависимости от кода его завершения, перезапускать его или завершать свою работу.
int MonitorProc(void) {
	int pid;
	int status;
	int need_start = 1;
	sigset_t sigset;
	siginfo_t siginfo;

	// настраиваем сигналы которые будем обрабатывать
	sigemptyset(&sigset);

	// сигнал остановки процесса пользователем
	sigaddset(&sigset, SIGQUIT);

	// сигнал для остановки процесса пользователем с терминала
	sigaddset(&sigset, SIGINT);

	// сигнал запроса завершения процесса
	sigaddset(&sigset, SIGTERM);

	// сигнал посылаемый при изменении статуса дочернего процесс
	sigaddset(&sigset, SIGCHLD);

	// сигнал посылаемый при изменении статуса дочернего процесс
	sigaddset(&sigset, SIGCHLD);

	// пользовательский сигнал который мы будем использовать для обновления конфига
	sigaddset(&sigset, SIGUSR1);
	sigprocmask(SIG_BLOCK, &sigset, NULL);

	// данная функция создат файл с нашим PID'ом
	SetPidFile(PID_FILE);

	// бесконечный цикл работы
	for (;;) {
		// если необходимо создать потомка
		if (need_start) {
			// создаём потомка
			pid = fork();
		}

		need_start = 1;

		if (pid == -1) // если произошла ошибка
				{
			// запишем в лог сообщение об этом
			WriteLog("[MONITOR] Fork failed (%s)\n", strerror(errno));
		} else if (!pid) // если мы потомок
		{
			// данный код выполняется в потомке

			// запустим функцию отвечающую за работу демона
			status = WorkProc();

			// завершим процесс
			exit(status);
		} else // если мы родитель
		{
			// данный код выполняется в родителе

			// ожидаем поступление сигнала
			sigwaitinfo(&sigset, &siginfo);

			//printf("[MONITOR] get signal=%d\n",siginfo.si_signo);//xxx
			// если пришел сигнал от потомка
			if (siginfo.si_signo == SIGCHLD) {
				// получаем статус завершение
				wait(&status);

				// преобразуем статус в нормальный вид
				status = WEXITSTATUS(status);

				// если потомок завершил работу с кодом говорящем о том, что нет нужны дальше работать
				if (status == CHILD_NEED_TERMINATE) {
					// запишем в лог сообщени об этом
					WriteLog("[MONITOR] Childer stopped\n");

					// прервем цикл
					break;
				} else if (status == CHILD_NEED_WORK) // если требуется перезапустить потомка
						{
					// запишем в лог данное событие
					WriteLog("[MONITOR] Childer restart\n");
				}
			} else if (siginfo.si_signo == SIGUSR1) // если пришел сигнал что необходимо перезагрузить конфиг
					{
				kill(pid, SIGUSR1); // перешлем его потомку
				need_start = 0; // установим флаг что нам не надо запускать потомка заново
			} else // если пришел какой-либо другой ожидаемый сигнал
			{
				// запишем в лог информацию о пришедшем сигнале
				WriteLog("[MONITOR] Signal %d(%s)\n", siginfo.si_signo,
						strsignal(siginfo.si_signo));
				printf("[MONITOR] Signal %d(%s)\n", siginfo.si_signo,
						strsignal(siginfo.si_signo));

				// убьем потомка
				kill(pid, SIGTERM);
				status = 0;
				break;
			}
		}
	}

	// запишем в лог, что мы остановились
	WriteLog("[MONITOR] Stopped\n");

	// удалим файл с PID'ом
	unlink(PID_FILE);

	return status;
}

