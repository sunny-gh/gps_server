// my_time.c
// Функции для работы со временем

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#endif
#include <string.h>
#include <time.h>
#include <math.h>
#include "my_time.h"

// Make sure we can call this stuff from C++.
#if __cplusplus
extern "C" {
#endif

static double okrugl(double in) {
	double cel = floor(in);
	double num = in - cel;
	if (num >= 0.5)
		return (double) (cel + 1);
	else
		return (double) cel;
}

static void mjd_cal(long long msec, int *mn, double *dy, int *yr) {
	static double last_mj, last_dy;
	static int last_mn, last_yr;
	double d, f;
	double i, a, b, ce, g;
	double mj = msec / (3600. * 24. * 1000.) + 0.5;

	/* we get called with 0 quite a bit from unused epoch fields.
	 * 0 is noon the last day of 1899.
	 */
	if (mj == 0.0) {
		*mn = 12;
		*dy = 31.5;
		*yr = 1899;
		return;
	}

	if (mj == last_mj) {
		*mn = last_mn;
		*yr = last_yr;
		*dy = last_dy;
		return;
	}

	d = mj + 0.5;
	i = floor(d);
	f = d - i;
	if (f == 1) {
		f = 0;
		i += 1;
	}

	if (i > -115860.0) {
		a = floor((i / 36524.25) + .99835726) + 14;
		i += 1 + a - floor(a / 4.0);
	}

	b = floor((i / 365.25) + .802601);
	ce = i - floor((365.25 * b) + .750001) + 416;
	g = floor(ce / 30.6001);
	*mn = (int) (g - 1);
	*dy = ce - floor(30.6001 * g) + f;
	*yr = (int) (b + 1899);

	if (g > 13.5)
		*mn = (int) (g - 13);
	if (*mn < 2.5)
		*yr = (int) (b + 1900);
	if (*yr < 1)
		*yr -= 1;

	last_mn = *mn;
	last_dy = *dy;
	last_yr = *yr;
	last_mj = mj;
}

// узнать часовой пояс - в минутах
long my_time_get_timezone() {
#ifdef _WIN32
	_tzset(); // получить значение timezone
	return -_timezone/60;
#else // Linux, QNX
	tzset(); // получить значение timezone
	return -timezone / 60;
#endif
}

// получаем текущее число микросекунд от 1900 года GMT(UTC)
long long my_time_get_cur_usec1900(void) {
#ifdef _WIN32
	FILETIME ft;
	GetSystemTimeAsFileTime(&ft); //return the number of 100-nanosecond intervals since January 1, 1601 (UTC).
	return *((LONGLONG *)&ft) / 10 - (LONGLONG)(9435484800000000);
#else // Linux, QNX
	//struct timeval tv;gettimeofday(&tv2,NULL);
	struct timespec cur_time;
	clock_gettime(CLOCK_REALTIME, &cur_time); // the number of seconds since 1970
	return cur_time.tv_sec * 1000000ll + (cur_time.tv_nsec + 500) / 1000
			+ 2208988800000000ll; //ret+ = 25567*24*3600*1e6;
#endif
}

// получаем текущее число микросекунд от 2000 года GMT(UTC)
long long my_time_get_cur_usec2000(void) {
	return my_time_get_cur_usec1900() - 36525ll * 3600 * 24 * 1000000;
}

// получаем текущее число миллисекунд от 1900 года GMT(UTC)
long long my_time_get_cur_msec1900(void) {
#ifdef _WIN32
	FILETIME ft;
	GetSystemTimeAsFileTime(&ft); //return the number of 100-nanosecond intervals since January 1, 1601 (UTC).
	return *((LONGLONG *)&ft) / 10000 - (LONGLONG)(9435484800000);
#else // Linux, QNX
	//struct timeval tv;gettimeofday(&tv2,NULL);
	struct timespec cur_time;
	clock_gettime(CLOCK_REALTIME, &cur_time); // the number of seconds since 1970
	return cur_time.tv_sec * 1000ll + (cur_time.tv_nsec + 500000) / 1000000
			+ 2208988800000ll; //ret+ = 25567*24*3600*1000;
#endif
}

// получаем текущее число миллисекунд от 2000 года GMT(UTC)
long long my_time_get_cur_msec2000(void) {
	return my_time_get_cur_msec1900() - 36525ll * 3600 * 24 * 1000;
}

// получаем текущее GMT(UTC) время
// return: текущее число миллисекунд в последней секунде (0...999)
// год = год - 1900
// месяц=0...11
long my_time_get_cur_gmt_time(struct tm *gmt_tm) {
	long long cur_msec = my_time_get_cur_msec1900();
	return my_time_msec1900_to_time(cur_msec, gmt_tm);
}

// получаем текущее местное время
// return: текущее число миллисекунд в последней секунде (0...999)
// год = год - 1900
// месяц=0...11
long my_time_get_cur_local_time(struct tm *gmt_tm) {
	long long cur_msec = my_time_get_cur_msec1900();
#ifdef _WIN32
	_tzset(); // получить значение timezone
#else
	tzset(); // получить значение timezone
#endif
	cur_msec -= timezone * 1000; // добавляем поясное время в миллисек.
	return my_time_msec1900_to_time(cur_msec, gmt_tm);
}

// перевод числа миллисекунд от 1900 года в дату/время
// год = год - 1900
// месяц=0...11
// msec - число миллисекунд
// gmt_tm - структура с датой/временем
// return: число миллисекунд в последней секунде (0...999)
long my_time_msec1900_to_time(long long msec, struct tm *gmt_tm) {
	long ret_ms;
	double day = 0;
	if (!gmt_tm)
		return (time_t) NULL;
	memset(gmt_tm, 0, sizeof(struct tm));
	mjd_cal(msec, &gmt_tm->tm_mon, &day, &gmt_tm->tm_year);
	gmt_tm->tm_mon--; // месяц=0...11
	gmt_tm->tm_year -= 1900; // год = год - 1900
	gmt_tm->tm_mday = (int) day;
	day -= floor(day);
	gmt_tm->tm_hour = (int) (day *= 24);
	day -= floor(day);
	gmt_tm->tm_min = (int) (day *= 60);
	day -= floor(day);
	gmt_tm->tm_sec = (int) (day *= 60);
	day -= floor(day);
	ret_ms = (long) okrugl(day * 1000);
	// если число sec=x.99999999
	if (ret_ms == 1000) {
		gmt_tm->tm_sec++;
		ret_ms = 0;
		if (gmt_tm->tm_sec == 60) {
			gmt_tm->tm_min++;
			gmt_tm->tm_sec = 0;
			if (gmt_tm->tm_min == 60) {
				gmt_tm->tm_hour++;
				gmt_tm->tm_min = 0;
				if (gmt_tm->tm_hour == 24) {
					gmt_tm->tm_mday++;
					gmt_tm->tm_hour = 0;
				}
			}
		}
	}
	return ret_ms;
}

// перевод числа миллисекунд от 2000 года в дату/время
// год = год - 1900
// месяц=0...11
// msec - число миллисекунд
// gmt_tm - структура с датой/временем
// return: число миллисекунд в последней секунде (0...999)
long my_time_msec2000_to_time(long long msec, struct tm *gmt_tm) {
	return my_time_msec1900_to_time(msec + 36525ll * 3600 * 24 * 1000, gmt_tm);
}
// перевод даты/времени в число миллисекунд от 1900 года
// год = год - 1900
// месяц=0...11
// gmt_tm - структура с датой/временем
// dmsec - дополнительные миллисекунды
// return: число миллисек., 0 Err
long long my_time_time_to_msec1900(struct tm *gmt_tm, long dmsec) {
	double km, vj;
	long long ret;
	if (!gmt_tm)
		return 0;
	km = 12 * (gmt_tm->tm_year + 1900 + 4800) + gmt_tm->tm_mon - 2; // в месяцах
	vj = (2 * (km - 12 * floor(km / 12)) + 7 + 365 * km) / 12;
	vj = floor(vj) + gmt_tm->tm_mday + floor(km / 48) - 32083;
	if (vj > 2299171)
		vj += floor(km / 4800) - floor(km / 1200) + 38;
	vj += -2451545 + 36524; // перевод числа дней в эпоху 1900
	ret = (long long) (vj + 0.5) * (3600 * 24 * 1000ll)
			+ (gmt_tm->tm_hour * 3600 + gmt_tm->tm_min * 60 + gmt_tm->tm_sec)
					* 1000 + dmsec;
	return ret;
}

// перевод даты/времени в число миллисекунд от 2000 года
// gmt_tm - структура с датой/временем
// год = год - 1900
// месяц=0...11
// dmsec - дополнительные миллисекунды
// return: число миллисек., 0 Err
long long my_time_time_to_msec2000(struct tm *gmt_tm, long dmsec) {
	long long ret = my_time_time_to_msec1900(gmt_tm, dmsec);
	ret -= 36525ll * 3600 * 24 * 1000; // переводим в эпоху 2000 г. из эпохи 1900;
	return ret;
}

// перевод числа миллисекунд от 1900 года в число миллисекунд от 2000 года
long long my_time_msec1900_to_msec2000(long long dmsec) {
	return dmsec - 36525ll * 3600 * 24 * 1000; // переводим в эпоху 2000 г. из эпохи 1900;
}

// перевод числа миллисекунд от 2000 года в число миллисекунд от 1900 года
long long my_time_msec2000_to_msec1900(long long dmsec) {
	return dmsec + 36525ll * 3600 * 24 * 1000; // переводим в эпоху 1900 г. из эпохи 2000;
}

// Установка системного времени
// correction - смещение в миллисекундах
// return: 1 OK, 0 Err (нет прав для установки времени)
int set_clock_from_dmsec(long long correction) {
#ifdef _WIN32
	DWORD err = 0;
	FILETIME ft;
	SYSTEMTIME st;
	SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS);
	GetSystemTimeAsFileTime(&ft); //return the number of 100-nanosecond intervals since January 1, 1601 (UTC).
	*((long long *)&ft) += correction;

	FileTimeToSystemTime(&ft, &st);
	if (!SetSystemTime(&st))// Coordinated Universal Time (UTC).
	err = GetLastError();
	SetPriorityClass(GetCurrentProcess(), NORMAL_PRIORITY_CLASS);

	if (err != ERROR_PRIVILEGE_NOT_HELD)
	return 0;//Fatal("Cannot set the system time");
#else // Linux, QNX
	struct timespec cur_time;
	clock_gettime(CLOCK_REALTIME, &cur_time);
	// добавляем коррекцию к текущему времени
	cur_time.tv_sec += correction / 1000;
	correction = correction % 1000;
	cur_time.tv_nsec += correction * (long) (1e6);
	if (clock_settime(CLOCK_REALTIME, &cur_time) == -1)
		return 0;
#endif
	return 1;
}

// Установка системного времени
// correction - смещение в микросекундах
// return: 1 OK, 0 Err (нет прав для установки времени)
int set_clock_from_dusec(long long correction) {
#ifdef _WIN32
	DWORD err=0;
	FILETIME ft;
	SYSTEMTIME st;
	if(!correction) return 1;
	SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS);
	GetSystemTimeAsFileTime(&ft); //return the number of 100-nanosecond intervals since January 1, 1601 (UTC).
	*((long long *)&ft) += correction;
	FileTimeToSystemTime(&ft, &st);
	if (!SetSystemTime(&st))// Coordinated Universal Time (UTC).
	err = GetLastError();
	SetPriorityClass(GetCurrentProcess(), NORMAL_PRIORITY_CLASS);

	if (err != ERROR_PRIVILEGE_NOT_HELD)
	return 0;//Fatal("Cannot set the system time");
#else // Linux, QNX
	if (!correction)
		return 1;
	struct timespec cur_time;
	clock_gettime(CLOCK_REALTIME, &cur_time);
	// добавляем коррекцию к текущему времени
	cur_time.tv_sec += correction / 1000000;
	correction = correction % 1000000;
	cur_time.tv_nsec += correction * (long long) (1e3);
	if (cur_time.tv_nsec > (long long) (1e9)) {
		cur_time.tv_sec++;
		cur_time.tv_nsec -= (long long) (1e9);
	}
	if (clock_settime(CLOCK_REALTIME, &cur_time) == -1)
		return 0; //errno;
#endif
	return 1;
}

// переводим число секунд от 1970 г в число милисекунд от 2000 г. (GMT) (проверено)
long long my_time_timestamp_to_msec2000(long long timestamp) {
	return timestamp * 1000ll + 2208988800000ll - 36525ll * 3600 * 24 * 1000;
}

// переводим число милисекунд от 2000 г. в число секунд от 1970 г (GMT)
// test: 13 февраля 2009 года, 23:31:30 UTC, timestamp = 1234567890 (проверено)
long long my_time_msec2000_to_timestamp(long long ms2000) {
	return (ms2000 - 2208988800000ll + 36525ll * 3600 * 24 * 1000) / 1000;
}

#if __cplusplus
}  // End of the 'extern "C"' block
#endif

/*/ test
 {
 struct tm gmt_tm, gmt_tm2;
 //memset(&gmt_tm, 0, sizeof(struct tm));
 gmt_tm.tm_hour = 23;
 gmt_tm.tm_min = 31;
 gmt_tm.tm_sec = 30;
 gmt_tm.tm_year = 2009 - 1900;
 gmt_tm.tm_mon = 1;
 gmt_tm.tm_mday = 13;
 long long  msec = my_time_time_to_msec2000(&gmt_tm, 0);
 // test: 13 февраля 2009 года, 23:31:30 UTC, timestamp = 1234567890
 long long timestamp = my_time_msec2000_to_timestamp(msec);

 // переводим число секунд от 1970 г в число милисекунд от 2000 г. (GMT)
 long long msec2 = my_time_timestamp_to_msec2000(1234567891LL);
 long ms = my_time_msec2000_to_time(msec2, &gmt_tm2);
 timestamp = 4390327890;//3443556690000
 }*/
