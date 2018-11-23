/*
 * log.h
 *
 *  Created on: 30.05.2016
 *      Author: sunny
 */

#ifndef LOG_H_
#define LOG_H_

// иннициализация
void log_init(void);

// функция записи лога
void WriteLog(const char* msg, ...);

void my_printf(const char* msg, ...);

#endif /* LOG_H_ */
