// zlib_util.h : Упаковка/распаковка методом zlip deflate
//
#ifndef _ZLIB_UTIL_H_
#define _ZLIB_UTIL_H_

//#include <zlib.h>

// сжатие данных методом zlip deflate
// память под выходной буфер будет выделена, удаляется через g_free()
// level - степень сжатия, 0(без сжатия), 1(быстро)...9(сильно), Z_DEFAULT_COMPRESSION = -1 
// return: код ошибки, Z_OK(0) if success, Z_MEM_ERROR(-4) недостаточно памяти для сжатия, Z_BUF_ERROR(-5) недостаточно места в выходном буфере, Z_STREAM_ERROR(-2) уровень сжатия некорректный
int zlib_compress(uint8_t *str, uint32_t src_len, uint8_t **dst,
		uint32_t *dst_len_out, int level);

// распаковка данных методом zlip deflate
// память под выходной буфер будет выделена, удаляется через g_free()
// return: код ошибки, Z_OK(0) if success, Z_MEM_ERROR(-4) недостаточно памяти для сжатия, Z_BUF_ERROR(-5) недостаточно места в выходном буфере, Z_VERSION_ERROR(-6) если несовместима zlib library версия Z_STREAM_ERROR if the parameters are invalid, such as a null pointer to the structure.msg is set to null if there is no error message.
int zlib_uncompress(uint8_t *str, uint32_t src_len, uint8_t **dst,
		uint32_t *dst_len_out);

#endif // _ZLIB_UTIL_H_
