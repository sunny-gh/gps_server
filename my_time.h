// my_time.h
// Функции для работы со временем
#ifndef _MYTIME_H_
#define _MYTIME_H_

// Make sure we can call this stuff from C++.
#if __cplusplus
extern "C" {
#endif

// число миллисекунд в часе
#define MSEC_PER_HOUR	 3600000	//(24*3600*1000)
// число миллисекунд в сутках
#define MSEC_PER_DAY	86400000	//(24*3600*1000)

// узнать часовой пояс - в минутах
long my_time_get_timezone();

// получаем текущее число микросекунд от 1900 года GMT(UTC)
long long my_time_get_cur_usec1900(void);
// получаем текущее число микросекунд от 2000 года GMT(UTC)
long long my_time_get_cur_usec2000(void);

// получаем текущее число миллисекунд от 1900 года GMT(UTC)
long long my_time_get_cur_msec1900(void);
// получаем текущее число миллисекунд от 2000 года GMT(UTC)
long long my_time_get_cur_msec2000(void);

// получаем текущее GMT(UTC) время
// return: текущее число миллисекунд в последней секунде (0...999)
// год = год - 1900
// месяц=0...11
long my_time_get_cur_gmt_time(struct tm *gmt_tm);
// получаем текущее местное время
long my_time_get_cur_local_time(struct tm *gmt_tm);

// перевод числа миллисекунд от 1900 года в дату/время
// год = год - 1900
// месяц=0...11
// msec - число миллисекунд
// gmt_tm - структура с датой/временем
// return: число миллисекунд в последней секунде (0...999)
long my_time_msec1900_to_time(long long msec, struct tm *gmt_tm);
// перевод числа миллисекунд от 2000 года в дату/время
long my_time_msec2000_to_time(long long msec, struct tm *gmt_tm);

// перевод даты/времени в число миллисекунд от 1900 года
// год = год - 1900
// месяц=0...11
// gmt_tm - структура с датой/временем
// dmsec - дополнительные миллисекунды
// return: число миллисек., 0 Err
long long my_time_time_to_msec1900(struct tm *gmt_tm, long msec);
// перевод даты/времени в число миллисекунд от 2000 года
long long my_time_time_to_msec2000(struct tm *gmt_tm, long dmsec);

// перевод числа миллисекунд от 1900 года в число миллисекунд от 2000 года
long long my_time_msec1900_to_msec2000(long long dmsec);
// перевод числа миллисекунд от 2000 года в число миллисекунд от 1900 года
long long my_time_msec2000_to_msec1900(long long dmsec);

// Установка системного времени
// correction - смещение в миллисекундах
// return: 1 OK, 0 Err (нет прав для установки времени)
int set_clock_from_dmsec(long long correction);
// Установка системного времени
// correction - смещение в микросекундах
// return: 1 OK, 0 Err (нет прав для установки времени)
int set_clock_from_dusec(long long correction);

// переводим число секунд от 1970 г в число милисекунд от 2000 г. (GMT)
long long my_time_timestamp_to_msec2000(long long timestamp);
// переводим число милисекунд от 2000 г. в число секунд от 1970 г (GMT)
long long my_time_msec2000_to_timestamp(long long ms2000);

#if __cplusplus
}  // End of the 'extern "C"' block
#endif

#endif // #ifndef _MYTIME_H_
