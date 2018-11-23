/*
 * work.cpp
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
#include <sys/resource.h>
#include "monitor.h"
#include "log.h"
#include "work.h"

// лимит для установки максимально кол-во открытых дискрипторов
#define FD_LIMIT			1024*10

// функция установки максимального кол-во дескрипторов которое может быть открыто
int SetFdLimit(int MaxFd) {
	struct rlimit lim;
	int status;

	// зададим текущий лимит на кол-во открытых дискриптеров
	lim.rlim_cur = MaxFd;
	// зададим максимальный лимит на кол-во открытых дискриптеров
	lim.rlim_max = MaxFd;

	// установим указанное кол-во
	status = setrlimit(RLIMIT_NOFILE, &lim);

	WriteLog("[SetFdLimit] fd=%d status=%d\n", MaxFd, status);
	printf("[SetFdLimit] fd=%d status=%d\n", MaxFd, status);
	return status;
}

// функция для остановки потоков и освобождения ресурсов
void DestroyWorkThread() {
	WriteLog("{DestroyWorkThread begin}\n");
	// тут должен быть код который остановит все потоки и
	// корректно освободит ресурсы
	destroy_work();
	WriteLog("{DestroyWorkThread end}\n");
}

// функция которая инициализирует рабочие потоки
int InitWorkThread() {
	WriteLog("{InitWorkThread begin}\n");
	int ret = init_work();
	WriteLog("{InitWorkThread end}\n");
	//int a = 54/0;
	// код функции
	return ret;
}

// функция которая загрузит конфиг заново
// и внесет нужные поправки в работу
int ReloadConfig() {
	WriteLog((char*) "{ReloadConfig begin}\n");
	// код функции
	int ret = restart_work();
	WriteLog((char*) "{ReloadConfig end}\n");
	return ret;
}

// функция обработки сигналов
static void signal_error(int sig, siginfo_t *si, void *ptr) {
	void* ErrorAddr;
	void* Trace[16];
	int x;
	int TraceSize;
	char** Messages;

	// запишем в лог что за сигнал пришел
	WriteLog("[DAEMON] Signal: %s, Addr: 0x%0.16X\n", strsignal(sig),
			si->si_addr);

#if __WORDSIZE == 64 // если дело имеем с 64 битной ОС
	// получим адрес инструкции которая вызвала ошибку
	ErrorAddr = (void*)((ucontext_t*)ptr)->uc_mcontext.gregs[REG_RIP];
#else
	// получим адрес инструкции которая вызвала ошибку
	ErrorAddr = (void*) ((ucontext_t*) ptr)->uc_mcontext.gregs[REG_EIP];
#endif

	// произведем backtrace чтобы получить весь стек вызовов
	TraceSize = backtrace(Trace, 16);
	Trace[1] = ErrorAddr;

	// получим расшифровку трасировки
	Messages = backtrace_symbols(Trace, TraceSize);
	if (Messages) {
		WriteLog("== Backtrace ==\n");

		// запишем в лог
		for (x = 1; x < TraceSize; x++) {
			WriteLog("%s\n", Messages[x]);
		}

		WriteLog("== End Backtrace ==\n");
		free(Messages);
	}

	WriteLog("[DAEMON] Stopped\n");

	// остановим все рабочие потоки и корректно закроем всё что надо
	DestroyWorkThread();

	// завершим процесс с кодом требующим перезапуска
	exit (CHILD_NEED_WORK);
}

int WorkProc(void) {
	struct sigaction sigact;
	sigset_t sigset;
	int signo;
	int status;

	// сигналы об ошибках в программе будут обрататывать более тщательно
	// указываем что хотим получать расширенную информацию об ошибках
	sigact.sa_flags = SA_SIGINFO;
	// задаем функцию обработчик сигналов
	sigact.sa_sigaction = signal_error;

	sigemptyset(&sigact.sa_mask);

	// установим наш обработчик на сигналы

	sigaction(SIGFPE, &sigact, 0); // ошибка FPU
	sigaction(SIGILL, &sigact, 0); // ошибочная инструкция
	sigaction(SIGSEGV, &sigact, 0); // ошибка доступа к памяти
	sigaction(SIGBUS, &sigact, 0); // ошибка шины, при обращении к физической памяти

	sigemptyset(&sigset);

	// блокируем сигналы которые будем ожидать
	// сигнал остановки процесса пользователем
	sigaddset(&sigset, SIGQUIT);

	// сигнал для остановки процесса пользователем с терминала
	sigaddset(&sigset, SIGINT);

	// сигнал запроса завершения процесса
	sigaddset(&sigset, SIGTERM);

	// пользовательский сигнал который мы будем использовать для обновления конфига
	sigaddset(&sigset, SIGUSR1);
	sigprocmask(SIG_BLOCK, &sigset, NULL);

	// Установим максимальное кол-во дискрипторов которое можно открыть
	SetFdLimit(FD_LIMIT);

	// запишем в лог, что наш демон стартовал
	WriteLog("[DAEMON] Started\n");

	// запускаем все рабочие потоки
	status = InitWorkThread();
	if (status) {
		// цикл ожижания сообщений
		for (;;) {
			// ждем указанных сообщений
			sigwait(&sigset, &signo);

			// если то сообщение обновления конфига
			if (signo == SIGUSR1) {
				// обновим конфиг
				status = ReloadConfig();
				if (status == 0) {
					WriteLog("[DAEMON] Reload config failed\n");
				} else {
					WriteLog("[DAEMON] Reload config OK\n");
				}
			} else // если какой-либо другой сигнал, то выйдим из цикла
			{
				break;
			}
		}

		// остановим все рабочеи потоки и корректно закроем всё что надо
		DestroyWorkThread();
	} else {
		WriteLog("[DAEMON] Create work thread failed\n");
	}

	WriteLog("[DAEMON] Stopped\n");

	// вернем код не требующим перезапуска
	return CHILD_NEED_TERMINATE;
}

// Работа программы в режиме отладки
int DebugProc(void) {
	init_work();
	wait_stop_servers();
	return 0;
}

