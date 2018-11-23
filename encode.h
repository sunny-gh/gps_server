// encode.h : шифрование и сжатие
//

#ifndef _ENCODE_H
#define _ENCODE_H

/* Convert binary data to binhex encoded data.
 ** The out[] buffer must be twice the number of bytes to encode.
 ** "len" is the size of the data in the in[] buffer to encode.
 ** Return the number of bytes encoded, or -1 on error.
 */
int bin2hex(char *out, const unsigned char *in, int len);

/* Convert binhex encoded data to binary data.
 ** "len" is the size of the data in the in[] buffer to decode, and must
 ** be even. The out[] buffer must be at least half of "len" in size.
 ** The buffers in[] and out[] can be the same to allow in-place decoding.
 ** Return the number of bytes encoded, or -1 on error.
 */
int hex2bin(char *out, const unsigned char *in, int len);

// зашифровать строку
char* encode_string(const gchar *str);

// расшифровать строку
char* decode_string(const gchar *str);

// сгенерировать уникальный sid пользователя (id сессии)
char* get_sid(const char *in_str, int in_int);

#endif	// _ENCODE_H
