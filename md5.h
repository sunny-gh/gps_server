/*
 вычисление md5 для строки или файла 
 */

#ifndef WORD
typedef unsigned short WORD;
#endif
#ifndef DWORD
typedef unsigned long DWORD;
#endif
#ifndef BYTE
typedef unsigned char BYTE;
#endif
#ifndef BOOL
typedef int BOOL;
#endif
#ifndef FALSE
#define FALSE	0
#endif
#ifndef TRUE
#define TRUE	1
#endif

typedef unsigned int UINT4;

typedef BYTE* MD5;
struct MD11 {
	BYTE hash[16];
};

/* Data structure for MD5 (Message-Digest) computation */
typedef struct {
	UINT4 i[2]; /* number of _bits_ handled mod 2^64 */
	UINT4 buf[4]; /* scratch buffer */
	unsigned char in[64]; /* input buffer */
	unsigned char digest[16]; /* actual digest after MD5Final call
	 */
} MD5_CTX;

void MD5Init(MD5_CTX *);
void MD5Update(MD5_CTX *, unsigned char *, unsigned int);
void MD5Final(MD5_CTX *);
void Transform(UINT4 *, UINT4 *);

//+вход:
//str - входная строка
//len - длина входной строки
//+выход:
//hash - 16 байт функция md5 
void findhash_n(BYTE *str, int len, BYTE *hash);

//+вход:
//str - входная строка (длина строки определяется автоматически по 0 на конце)
//len - длина входной строки
//+выход:
//hash - 16 байт функция md5 
void findhash(BYTE *str, BYTE *hash);

//+вход:
//name - имя входного файла
//+выход:
//hash - 16 байт функция md5 
//+return: true - ОК, false - файл не открыт или не найден
BOOL findhashfile(char *name, BYTE *hash);

