// protocol_parse_osmand.c : Протокол OsmAnd
//

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
//#include <zlib.h>
#include <glib.h>
#include "protocol_parse.h"
#include "protocol_parse_osmand.h"
#include "crc16.h"
#include "../my_time.h"
// локализация  (из каталога src проекта - определить переменную окр. Include, чтобы найти файл)
#include "package_locale.h" // определение для _(), N_(), C_(), NC_()

// Make sure we can call this stuff from C++.
#if __cplusplus
extern "C" {
#endif

// удалить содержимое структуры OSMAND_PACKET - содержимое пакета
static void del_osmand_packet(struct OSMAND_PACKET *packet) {
	if (!packet)
		return;
	g_free(packet->imei);
	g_free(packet->raw_msg);

	struct DOP_PARAM *param = packet->params;
	int16_t n_params = packet->n_params;
	free_all_dop_param(n_params, param);
}

// очистить структуру OSMAND_PACKET - содержимое пакета
void clean_osmand_packet(void *packet_in) {
	struct OSMAND_PACKET *packet = (struct OSMAND_PACKET*) packet_in;
	if (!packet)
		return;
	del_osmand_packet(packet);
	// обнуляем структуру с пакетом
	memset(packet, 0, sizeof(struct OSMAND_PACKET));
}

// удалить структуру OSMAND_PACKET - содержимое пакета
void free_osmand_packet(void *packet_in) {
	struct OSMAND_PACKET *packet = (struct OSMAND_PACKET*) packet_in;
	if (!packet)
		return;
	del_osmand_packet(packet);
	// удаляем структуру с пакетом
	g_free(packet);
}

// получить текущее Юлианское время в секундах
/*uint64_t get_cur_jdate_time(void)
 {
 int64_t jdate = 0;
 struct tm gmt_tm;
 memset(&gmt_tm, 0, sizeof(struct tm));
 // получаем текущее GMT(UTC) время
 my_time_get_cur_gmt_time(&gmt_tm);
 // переводим время в число милисекунд от 2000 г.
 jdate = my_time_time_to_msec2000(&gmt_tm, 0);
 return jdate;
 }*/

/*
 Запрос:
 GET /wiki/UNIX-%D0%B2%D1%80%D0%B5%D0%BC%D1%8F HTTP/1.1
 Host: ru.wikipedia.org
 User-Agent: Mozilla/5.0 (Windows NT 10.0; WOW64; rv:49.0) Gecko/20100101 Firefox/49.0
 Accept: text/html,application/xhtml+xml,application/xml;q=0.9,* /*;q=0.8
 Accept-Language: ru-RU,ru;q=0.8,en-US;q=0.5,en;q=0.3
 Accept-Encoding: gzip, deflate, br
 Referer: https://www.google.ru/
 Cookie: WMF-Last-Access=12-Oct-2016; CP=H2; GeoIP=RU:CHE:Miass:55.05:60.11:v4; ruwikimwuser-sessionId=502cef350863cdee
 DNT: 1
 X-Compress: 1
 Proxy-Authorization: a35ca66aced08977a2be6c53771eab4f118d38180f6e309e51b89bd543403498988eff0be2769884
 Connection: keep-alive
 Upgrade-Insecure-Requests: 1
 Cache-Control: max-age=0

 Ответ:
 HTTP/2.0 200 OK
 Date: Wed, 12 Oct 2016 18:34:49 GMT
 Content-Type: text/html; charset=UTF-8
 Content-Length: 18365
 Server: mw1270.eqiad.wmnet
 X-Powered-By: HHVM/3.12.7
 Vary: Accept-Encoding,Cookie,Authorization
 x-ua-compatible: IE=Edge
 Content-Language: ru
 Content-Encoding: gzip
 P3P: CP="This is not a P3P policy! See https://ru.wikipedia.org/wiki/%D0%A1%D0%BB%D1%83%D0%B6%D0%B5%D0%B1%D0%BD%D0%B0%D1%8F:CentralAutoLogin/P3P for more info."
 X-Content-Type-Options: nosniff
 Last-Modified: Tue, 27 Sep 2016 17:38:29 GMT
 backend-timing: D=213522 t=1476207509927652
 x-varnish: 3500844018 3485144207, 3064981338 2944186950, 3612456017 3203476202
 Via: 1.1 varnish, 1.1 varnish, 1.1 varnish
 Accept-Ranges: bytes
 Age: 89779
 X-Cache: cp1053 hit/1, cp3032 hit/6, cp3043 hit/16
 x-cache-status: hit
 */

// получить содержимое пакета из строки (без символов \r\n на конце)
// sample со стороны клиента: "httр://demo.traccar.org:5055/?id=123456&lat={0}&lon={1}&timestamp={2}&hdop={3}&altitude={4}&speed={5}"
// sample со стороны сервера: "GET /?id=e450&timestamp=1474673371&lat=55.173645&lon=61.402765&speed=0.13607339644369482&bearing=0.0&altitude=171.7&batt=87.0 HTTP/1.1"
// torque протокол
// return FALSE в случае нераспознанной строки
gboolean osmand_parse_packet(char *str, struct PACKET_INFO *packet_info) {
	if (!str || !packet_info)
		return FALSE;
	// минимально требуемые параметры
	gboolean is_imei = FALSE;
	gboolean is_lon = FALSE;
	gboolean is_lat = FALSE;
	gboolean is_time = FALSE;
	struct OSMAND_PACKET *packet = (struct OSMAND_PACKET*) g_malloc0(
			sizeof(struct OSMAND_PACKET));
	packet_info->packet = packet;
	packet->raw_msg = g_strdup(str);
	gchar **str_step0 = g_strsplit(str, "\r\n", 2); // чтобы отделить первую строку от остальных
	guint count = (str_step0) ? g_strv_length(str_step0) : 0;
	// нераспознанная строка
	if (count != 2) {
		g_strfreev(str_step0);
		packet_info->is_parse = FALSE; // не сохранённый пакет, рвём соединение с клиентом
		return FALSE;
	}
	// убираем  HTTP/1.1 в конце строки
	{
		int i, length = strlen(str_step0[0]);
		for (i = length - 6; i > (length - 10); i--) {
			if (!g_ascii_strncasecmp(str_step0[0] + i, " HTTP/", 6)) // Compare two strings, ignoring the case of ASCII characters
					{
				*(str_step0[0] + i) = 0; // конец строки пораньше
				break;
			}
		}
	}
	gchar **str_step1 = g_strsplit(str_step0[0], "?", 2); // не более 2 полей, в последнем поле, сообщении, могут быть такие символы тоже
	// определяем число подстрок
	count = (str_step1) ? g_strv_length(str_step1) : 0;
	// нераспознанная строка
	if (count != 2) {
		g_strfreev(str_step1);
		packet_info->is_parse = FALSE;// не сохранённый пакет, рвём соединение с клиентом
		return FALSE;
	}
	// определяем число параметров
	gchar **str_step2 = g_strsplit(str_step1[1], "&", -1);
	guint i, count2 = g_strv_length(str_step2);
	for (i = 0; i < count2; i++) {
		gchar **str_step3 = g_strsplit(str_step2[i], "=", -1);
		guint count3 = (str_step3) ? g_strv_length(str_step3) : 0;
		// нераспознанный параметр
		if (count3 != 2) {
			g_strfreev(str_step3);
			continue;
		}
		char *param_name = str_step3[0];
		char *param_val = str_step3[1];

		if (!g_strcmp0(param_name, "id")) {
			// запоминаем imei
			packet_info->imei = g_strdup(param_val);
			is_imei = TRUE;
		} else if (!g_strcmp0(param_name, "lat")
				|| !g_strcmp0(param_name, "kff1006")) {
			packet->lat = g_ascii_strtod(param_val, NULL);//перевести из строки в число double независимо от локали
			is_lat = TRUE;
		} else if (!g_strcmp0(param_name, "lon")
				|| !g_strcmp0(param_name, "kff1005")) {
			packet->lon = g_ascii_strtod(param_val, NULL);//перевести из строки в число double независимо от локали
			is_lon = TRUE;
		} else if (!g_strcmp0(param_name, "timestamp"))	// timestamp в секундах от 1970 г
				{
			int64_t timestamp = g_ascii_strtoll(param_val, NULL, 10);//перевести из строки в число int64
			packet->jdate = my_time_timestamp_to_msec2000(timestamp);// переводим число секунд от 1970 г в число милисекунд от 2000 г. (GMT)
		}
		// для Torque
		else if (!g_strcmp0(param_name, "time"))// timestamp в миллисекундах от 1970 г
				{
			int64_t timestamp = g_ascii_strtoll(param_val, NULL, 10) / 1000;//перевести из строки в число int64
			packet->jdate = my_time_timestamp_to_msec2000(timestamp);// переводим число секунд от 1970 г в число милисекунд от 2000 г. (GMT)
			// восстанавливаем миллисекунды
			packet->jdate += timestamp % 1000;
			struct tm gmt_tm;
			long long jd = my_time_get_cur_msec1900();
			long long ts = my_time_msec2000_to_timestamp(jd);
			//my_time_time_to_msec2000();
			my_time_msec2000_to_time(packet->jdate, &gmt_tm);
			is_time = TRUE;
		} else if (!g_strcmp0(param_name, "bearing")
				|| !g_strcmp0(param_name, "kff1007")
				|| !g_strcmp0(param_name, "kff123b"))			// азимут
						{
			double course = g_ascii_strtod(param_val, NULL);//перевести из строки в число double независимо от локали
			packet->course = okrugl(course);
		} else if (!g_strcmp0(param_name, "altitude")
				|| !g_strcmp0(param_name, "kff1010")) {
			packet->height = atoi(param_val);//перевести из строки в число double независимо от локали
		} else if (!g_strcmp0(param_name, "speed")
				|| !g_strcmp0(param_name, "kff1001")) {
			double speed = g_ascii_strtod(param_val, NULL);	//перевести из строки в число double независимо от локали
			packet->speed = okrugl(speed);
		} else if (packet->n_params <= 32767)// INT16_MAX может быть не более 32767 доп. параметров
				{
			//else if (!g_strcmp0(param_name, "k46"))		{ param_name_new = _("Ambient air temp"); type = DOP_PARAM_VAL_DOUBLE; }//Температура окружающего воздуха
			//else if (!g_strcmp0(param_name, "k5"))		{ param_name_new = _("Engine Coolant Temperature"); type = DOP_PARAM_VAL_DOUBLE; }//Температура охлаждающей жидкости двигателя
			packet->n_params++;		// число доп.параметров
			packet->params = (struct DOP_PARAM *) g_realloc(packet->params,
					packet->n_params * sizeof(struct DOP_PARAM));//  набор дополнительных параметров
			const char* param_name_new = param_name;
			int type = DOP_PARAM_VAL_STR;
			if (!g_strcmp0(param_name, "kd")) {
				param_name_new = _("Speed(OBD)");
				type = DOP_PARAM_VAL_DOUBLE;
			} else if (!g_strcmp0(param_name, "k4")) {
				param_name_new = _("Engine Load");
				type = DOP_PARAM_VAL_DOUBLE;
			} else if (!g_strcmp0(param_name, "kff1207")) {
				param_name_new = _("Litres Per 100 Kilometer(Instant)");
				type = DOP_PARAM_VAL_DOUBLE;
			} else if (!g_strcmp0(param_name, "kff1208")) {
				param_name_new = _("Trip average Litres/100 km");
				type = DOP_PARAM_VAL_DOUBLE;
			} else if (!g_strcmp0(param_name, "kff120c")) {
				param_name_new = _("Trip distance(stored in vehicle profile)");
				type = DOP_PARAM_VAL_DOUBLE;
			} else if (!g_strcmp0(param_name, "kff1238")) {
				param_name_new = _("Voltage (OBD Adapter)");
				type = DOP_PARAM_VAL_DOUBLE;
			} else if (!g_strcmp0(param_name, "kff1237")) {
				param_name_new = _("GPS vs OBD Speed difference");
				type = DOP_PARAM_VAL_DOUBLE;
			} else if (!g_strcmp0(param_name, "kff1239")) {
				param_name_new = _("GPS Accuracy");
				type = DOP_PARAM_VAL_DOUBLE;
			} else if (!g_strcmp0(param_name, "kff123a")) {
				param_name_new = _("GPS Satellites");
				type = DOP_PARAM_VAL_DOUBLE;
			} else if (!g_strcmp0(param_name, "k46")) {
				param_name_new = _("Ambient air temp");
				type = DOP_PARAM_VAL_DOUBLE;
			} else if (!g_strcmp0(param_name, "k5")) {
				param_name_new = _("Engine Coolant Temperature");
				type = DOP_PARAM_VAL_DOUBLE;
			} else if (!g_strcmp0(param_name, "kc")) {
				param_name_new = _("Engine RPM");
				type = DOP_PARAM_VAL_DOUBLE;
			} else if (!g_strcmp0(param_name, "kff1203")) {
				param_name_new = _("Kilometers Per Litre(Instant)");
				type = DOP_PARAM_VAL_DOUBLE;
			} else if (!g_strcmp0(param_name, "kff1204")) {
				param_name_new = _("Trip Distance");
				type = DOP_PARAM_VAL_DOUBLE;
			} else if (!g_strcmp0(param_name, "kff1225")) {
				param_name_new = _("Torque");
				type = DOP_PARAM_VAL_DOUBLE;
			} else if (!g_strcmp0(param_name, "kff1226")) {
				param_name_new = _("Horsepower(At the wheels)");
				type = DOP_PARAM_VAL_DOUBLE;
			}
			// заполнить новый доп. параметр
			fill_dop_param(packet->params + packet->n_params - 1,
					param_name_new, param_val, type);
		}
		g_strfreev(str_step3);
	}

	g_strfreev(str_step0);
	g_strfreev(str_step1);
	g_strfreev(str_step2);

	// распознанное сообщение или нет
	//if (is_imei  && is_lon && is_lat && is_time)// минимально требуемые параметры
	if (is_imei && is_time)	// минимально требуемые параметры
			{
		packet_info->is_parse = TRUE;
		return TRUE;
	}
	packet_info->is_parse = FALSE;
	return FALSE;
}

// получение из packet_info доп. параметров
gboolean osmand_packet_get_dop_param(struct PACKET_INFO *packet_info,
		int *course, int *n_params, struct DOP_PARAM **params) {
	if (!packet_info || !packet_info->packet)
		return FALSE;
	struct OSMAND_PACKET *packet = (struct OSMAND_PACKET*) packet_info->packet;	// g_malloc0(sizeof(struct OSMAND_PACKET));
	if (course)
		*course = packet->course;	// курс, в градусах
	if (n_params && params) {
		*n_params = packet->n_params;
		struct DOP_PARAM *all_dop_params = (struct DOP_PARAM*) g_malloc0(
				(*n_params) * sizeof(struct DOP_PARAM));
		*params = all_dop_params;
		int n_dp;
		for (n_dp = 0; n_dp < *n_params; n_dp++)
			copy_one_dop_param(all_dop_params + n_dp, packet->params + n_dp);
	}
	return TRUE;
}

#if __cplusplus
}  // End of the 'extern "C"' block
#endif
