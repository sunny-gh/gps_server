// encode.cpp : шифрование и сжатие
//

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <glib.h>
#include "my_time.h"
#include "md5.h"
#include "encode.h"

/* Convert binary data to binhex encoded data.
 ** The out[] buffer must be twice the number of bytes to encode.
 ** "len" is the size of the data in the in[] buffer to encode.
 ** Return the number of bytes encoded, or -1 on error.
 */
int bin2hex(char *out, const unsigned char *in, int len) {
	int ct = len;
	static char hex[] = "0123456789ABCDEF";
	if (!in || !out || len < 0)
		return -1;
	// hexadecimal lookup table
	while (ct-- > 0) {
		*out++ = hex[*in >> 4];
		*out++ = hex[*in++ & 0x0F];
	}
	return len;
}

// Convert binhex encoded data to binary data.
// len - должно быть чётным числом
int hex2bin(char *out, const unsigned char *in, int len) {
	// '0'-'9' = 0x30-0x39
	// 'a'-'f' = 0x61-0x66
	// 'A'-'F' = 0x41-0x46
	int ct = len;
	if (!in || !out || len < 0 || len & 1)
		return -1;
	while (ct > 0) {
		char ch1 = (
				(*in >= 'a') ?
						(*in++ - 'a' + 10) :
						((*in >= 'A') ? (*in++ - 'A' + 10) : (*in++ - '0')))
				<< 4;
		char ch2 = (
				(*in >= 'a') ?
						(*in++ - 'a' + 10) :
						((*in >= 'A') ? (*in++ - 'A' + 10) : (*in++ - '0')));// ((*in >= 'A') ? (*in++ - 'A' + 10) : (*in++ - '0'));
		*out++ = ch1 + ch2;
		ct -= 2;
	}
	return len;
}

// получить псевдослучайный массив длиной len
static char* get_psp(int len, guint32 seed) {
	int i;
	//guint32 seed = 0x4fa1;//неизменность seed гарантирует неизменность выходного массива
	GRand *rand = g_rand_new_with_seed(seed);
	char *masst = (char*) g_malloc0((len + 1) * sizeof(char));
	for (i = 0; i < len; i++) {
		char byte = 0;
		while (!byte)
			byte = (char) g_rand_int(rand);
		*(masst + i) = byte;
	}
	return masst;
}

// зашифровать строку
char* encode_string(const gchar *str) {
	gchar *str_in, *str_cr;
	if (!str)
		return NULL;
	str_in = g_strdup(str);
	int i, length = strlen(str);
	str_cr = get_psp(length, 0x4fa1);

	for (i = 0; i < length; i++) {
		str_in[i] ^= str_cr[i];
	}
	gchar *str_out = g_base64_encode((const guchar*) str_in, length);
	g_free(str_in);
	return str_out;
}

// расшифровать строку
char* decode_string(const gchar *str) {
	gchar *str_out, *str_cr;
	gsize i, length = 0;
	if (!str)
		return NULL;
	str_out = (gchar*) g_base64_decode(str, &length);
	str_cr = get_psp(length, 0x4fa1);
	for (i = 0; i < length; i++) {
		str_out[i] ^= str_cr[i];
	}
	g_free(str_cr);
	return str_out;
}

// сгенерировать уникальный sid пользователя (id сессии) - 16 байт
char* get_sid(const char *in_str, int in_int) {
	long long cur_time = my_time_get_cur_msec2000();
	char *str_psp_bin = get_psp(8, (guint32) cur_time);
	char *str_psp = g_base64_encode((const guchar*) str_psp_bin, 8);
	char *str_prepare = g_strdup_printf("%s%d%lld%s",
			(in_str) ? in_str : "ip.1", in_int, cur_time, str_psp);
	//char *str_enc = encode_string(str_prepare); 
	BYTE *str_md5 = (BYTE*) g_malloc0(17);
	findhash((BYTE*) str_prepare, str_md5);
	g_free(str_psp_bin);
	g_free(str_psp);
	g_free(str_prepare);
	//g_free(str_enc);
	return (char*) str_md5;
	/*/ комбинация изменяемых параметров
	 $id = uniqid();// генерирует уникальный ID
	 $u_a = $_SERVER['HTTP_USER_AGENT'];// браузер пользователя
	 $ip = $_SERVER['REMOTE_ADDR'];// адрес пользователя
	 $port = $_SERVER['REMOTE_PORT'];// порт пользователя
	 $time = gettimeofday(true);// текущее время с точностью до мс
	 $rand_str = $this->randAlphaNum(8);// случайная строка
	 */
	//return 0;// md5($id.$time.$u_a.$ip.$port.$rand_str);
}

