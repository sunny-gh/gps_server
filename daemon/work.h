/*
 * work.h
 *
 *  Created on: 30.05.2016
 *      Author: sunny
 */

#ifndef WORK_H_
#define WORK_H_

// константы для кодов завершения процесса
#define CHILD_NEED_WORK			1
#define CHILD_NEED_TERMINATE	2

int WorkProc(void);

// ожидание остановки потоков
void wait_stop_servers(void);

// Внешние функции, которые нужно переопределить для службы
// функция для остановки потоков и освобождения ресурсов
void destroy_work(void);

// функция которая инициализирует рабочие потоки
// return: 1, если успешно
int init_work(void);

// функция которая загрузит конфиг заново и внесет нужные поправки в работу
// return: 1, если успешно
int restart_work(void);

// Работа программы в режиме отладки
int DebugProc(void);

#endif /* WORK_H_ */
