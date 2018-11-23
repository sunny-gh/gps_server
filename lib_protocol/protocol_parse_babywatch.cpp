// protocol_parse_babywatch.c : Протокол Baby Watch Q90
// протокол from www.mesidatech.com/en/download/download-82-515.html

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
//#include <zlib.h>
#include <glib.h>
#include "protocol_parse.h"
#include "protocol_parse_babywatch.h"
#include "crc16.h"
#include "../my_time.h"
// локализация  (из каталога src проекта - определить переменную окр. Include, чтобы найти файл)
#include "package_locale.h" // определение для _(), N_(), C_(), NC_()

// Make sure we can call this stuff from C++.
#if __cplusplus
extern "C" {
#endif

// удалить содержимое структуры BABYWATCH_PACKET - содержимое пакета
static void del_babywatch_packet(struct BABYWATCH_PACKET *packet) {
	if (!packet)
		return;
	g_free(packet->imei);
	g_free(packet->prefix);
	g_free(packet->type_str);
	g_free(packet->raw_msg);

	struct DOP_PARAM *param = packet->params;
	int16_t n_params = packet->n_params;
	free_all_dop_param(n_params, param);
}

// очистить структуру BABYWATCH_PACKET - содержимое пакета
void clean_babywatch_packet(void *packet_in) {
	struct BABYWATCH_PACKET *packet = (struct BABYWATCH_PACKET*) packet_in;
	if (!packet)
		return;
	//SOCKET sockfd = packet->sockfd;
	//bool is_resend_cmd = packet->is_resend_cmd;
	del_babywatch_packet(packet);
	// обнуляем структуру с пакетом
	memset(packet, 0, sizeof(struct BABYWATCH_PACKET));
	// начальная иннициализация параметров
	//if (!sockfd)
	//{
//		sockfd = INVALID_SOCKET;
//		// TODO - считать это их базы
//		packet->is_resend_cmd = TRUE;
	//}
	//packet->sockfd = INVALID_SOCKET;
	//packet->is_resend_cmd = is_resend_cmd;

}

// удалить структуру BABYWATCH_PACKET - содержимое пакета
void free_babywatch_packet(void *packet_in) {
	struct BABYWATCH_PACKET *packet = (struct BABYWATCH_PACKET*) packet_in;
	if (!packet)
		return;
	del_babywatch_packet(packet);
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

// получить содержимое пакета из строки
// gps baby watch протокол
// str = [SG*8800000015*000D*LK,50,100,100]
// str = [3G*8800000015*008B*WAD,CH,220414,134652,A,22.571707,N,113.8613968,E,0.1,0.0,100,7,60,90,1000,50,0001,4,1,460,0,9360,4082,131,9360,4092,148,9360,4091,143,9360,4153,141]
// return FALSE в случае нераспознанной строки
gboolean babywatch_parse_packet(char *str, int str_len,
		struct PACKET_INFO *packet_info) {
	if (!str || !packet_info)
		return FALSE;
	// есть ли в сообщении время
	gboolean is_time = FALSE;
	struct BABYWATCH_PACKET *packet = (struct BABYWATCH_PACKET*) g_malloc0(
			sizeof(struct BABYWATCH_PACKET));
	packet_info->packet = packet;
	packet->raw_msg = g_strdup(str);
	if (str_len < 20 && str[0] != '[' && str[str_len - 1] != ']') {
		packet_info->is_parse = FALSE;
		return FALSE;
	}
	// убираем завершающую ковычку
	str[str_len - 1] = 0;
	gchar **str_step0 = g_strsplit(str + 1, "*", 5);// чтобы разделить параметры
	guint count = g_strv_length(str_step0);
	// нераспознанная строка
	if (count != 4) {
		g_strfreev(str_step0);
		packet_info->is_parse = FALSE;// не сохранённый пакет, рвём соединение с клиентом
		return FALSE;
	}
	// запоминаем префикс сообщения (нужен для ответа)
	packet->prefix = g_strdup(str_step0[0]);
	// запоминаем imei
	packet->imei = g_strdup(str_step0[1]);
	packet_info->imei = g_strdup(str_step0[1]);
	// длина последнего поля в hex виде
	guint64 len = g_ascii_strtoull(str_step0[2], NULL, 16);
	// разбираем пришедшие параметры
	gchar **str_step1 = g_strsplit(str_step0[3], ",", -1);// может быть 1 и более полей (разделители могут отсутствовать когда всего одно поле)
	// определяем число подстрок
	count = (str_step1) ? g_strv_length(str_step1) : 0;
	// нераспознанная строка
	if (count < 1) {
		g_strfreev(str_step0);
		g_strfreev(str_step1);
		packet_info->is_parse = FALSE;// не сохранённый пакет, рвём соединение с клиентом
		return FALSE;
	}
	// тип пришедшего сообщения - команда
	packet->type_str = g_strdup(str_step1[0]);
	char *cmd_name = str_step1[0];

	if (!g_strcmp0(cmd_name, "LK")) {
		;	// команда - link 
	} else if (!g_strcmp0(cmd_name, "UD")) {
		guint i;
		// определяем число параметров
		for (i = 0; i < count; i++) {
			packet->lat = g_ascii_strtod(str_step1[1], NULL);//перевести из строки в число double независимо от локали
		}
	}

	g_strfreev(str_step0);
	g_strfreev(str_step1);
	//g_strfreev(str_step2);

	// распознанное сообщение
	packet_info->is_parse = TRUE;
	return TRUE;
}

// получение из packet_info доп. параметров
gboolean babywatch_packet_get_dop_param(struct PACKET_INFO *packet_info,
		int *course, int *n_params, struct DOP_PARAM **params) {
	if (!packet_info || !packet_info->packet)
		return FALSE;
	struct BABYWATCH_PACKET *packet =
			(struct BABYWATCH_PACKET*) packet_info->packet;	// g_malloc0(sizeof(struct BABYWATCH_PACKET));
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
