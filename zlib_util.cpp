// zlib_util.cpp : Упаковка/распаковка методом zlip deflate
//

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <zlib.h>

//../;C:/msys32/mingw32/include/enchant;C:/msys32/mingw32/include/gtkspell-3.0;C:/msys32/mingw32/include/curl;C:/msys32/mingw32/include/json-glib-1.0;C:/msys32/mingw32/include/gtksourceview-3.0;C:/msys32/mingw32/include/webkitgtk-3.0;C:/msys32/mingw32/include/gtk-3.0;C:/msys32/mingw32/include/cairo;C:/msys32/mingw32/include/pango-1.0;C:/msys32/mingw32/include/harfbuzz;C:/msys32/mingw32/include/pango-1.0;C:/msys32/mingw32/include/atk-1.0;C:/msys32/mingw32/include/cairo;C:/msys32/mingw32/include/pixman-1;C:/msys32/mingw32/include;C:/msys32/mingw32/include/libpng16;C:/msys32/mingw32/include/gdk-pixbuf-2.0;C:/msys32/mingw32/include/libpng16;C:/msys32/mingw32/include/libsoup-2.4;C:/msys32/mingw32/include/libxml2;C:/msys32/mingw32/include/webkitgtk-3.0;C:/msys32/mingw32/include/glib-2.0;C:/msys32/mingw32/lib/glib-2.0/include;$(VC_IncludePath);$(WindowsSDK_IncludePath);
//C:/msys32/mingw32/include/json-glib-1.0;C:\msys32\usr\local\pgsql\include;C:\msys32\mingw32\include\glib-2.0;C:\msys32\mingw32\lib\glib-2.0\include;$(VC_IncludePath);$(WindowsSDK_IncludePath);C:\msys32\mingw32\i686-w64-mingw32\include\sys;C:\msys32\mingw32\i686-w64-mingw32\include;C:\msys32\mingw32\include

// сжатие данных методом zlip deflate
// память под выходной буфер будет выделена, удаляется через g_free()
// level - степень сжатия, 0(без сжатия), 1(быстро)...9(сильно), Z_DEFAULT_COMPRESSION = -1 
// return: код ошибки, Z_OK(0) if success, Z_MEM_ERROR(-4) недостаточно памяти для сжатия, Z_BUF_ERROR(-5) недостаточно места в выходном буфере, Z_STREAM_ERROR(-2) уровень сжатия некорректный
int zlib_compress(uint8_t *str, uint32_t src_len, uint8_t **dst,
		uint32_t *dst_len_out, int level) {
	if (!dst)
		return Z_BUF_ERROR;
	uLongf dst_len = (uLongf)(src_len * 1.001 + 13);
	*dst = (Bytef *) g_malloc(dst_len);
	int ret = compress2(*dst, &dst_len, (const Bytef*) str, src_len, level);
	if (dst_len_out)
		*dst_len_out = dst_len;
	if (ret != Z_OK)
		g_free(*dst);
	return ret;
}

// распаковка данных методом zlip deflate
// память под выходной буфер будет выделена, удаляется через g_free()
// return: код ошибки, Z_OK(0) if success, Z_MEM_ERROR(-4) недостаточно памяти для сжатия, Z_BUF_ERROR(-5) недостаточно места в выходном буфере,
// Z_DATA_ERROR(-3), Z_VERSION_ERROR(-6) если несовместима zlib library версия Z_STREAM_ERROR if the parameters are invalid, such as a null pointer to the structure.msg is set to null if there is no error message.
int zlib_uncompress(uint8_t *str, uint32_t src_len, uint8_t **dst,
		uint32_t *dst_len_out) {
	if (!dst)
		return Z_BUF_ERROR;
	uLongf dst_len = (uLongf)(src_len * 2 + 13);
	*dst = (uint8_t *) g_malloc0(dst_len);
	int ret, count = 0;
	do {
		ret = uncompress(*dst, &dst_len, str, src_len);
		if (ret == Z_BUF_ERROR) {
			dst_len = (uLongf)(dst_len * 2);
			*dst = (uint8_t *) g_realloc(*dst, dst_len);
		}
		count++;
	} while (ret == Z_BUF_ERROR && count < 10);
	if (dst_len_out)
		*dst_len_out = dst_len;
	*dst = (uint8_t *) g_realloc(*dst, dst_len + 1);
	*(*dst + dst_len) = 0; // завершающий ноль для строки
	return ret;
}

/* ===========================================================================
 Compresses the source buffer into the destination buffer. The level
 parameter has the same meaning as in deflateInit.  sourceLen is the byte
 length of the source buffer. Upon entry, destLen is the total size of the
 destination buffer, which must be at least 0.1% larger than sourceLen plus
 12 bytes. Upon exit, destLen is the actual size of the compressed buffer.

 compress2 returns Z_OK if success, Z_MEM_ERROR if there was not enough
 memory, Z_BUF_ERROR if there was not enough room in the output buffer,
 Z_STREAM_ERROR if the level parameter is invalid.

 int ZEXPORT compress2(Bytef *dest, uLongf *destLen, const Bytef *source, uLong sourceLen, int level)
 {
 z_stream stream;
 int err;

 stream.next_in = (z_const Bytef *)source;
 stream.avail_in = (uInt)sourceLen;
 #ifdef MAXSEG_64K
 // Check for source > 64K on 16-bit machine:
 if ((uLong)stream.avail_in != sourceLen) return Z_BUF_ERROR;
 #endif
 stream.next_out = dest;
 stream.avail_out = (uInt)*destLen;
 if ((uLong)stream.avail_out != *destLen) return Z_BUF_ERROR;

 stream.zalloc = (alloc_func)0;
 stream.zfree = (free_func)0;
 stream.opaque = (voidpf)0;

 err = deflateInit(&stream, level);
 if (err != Z_OK) return err;

 err = deflate(&stream, Z_FINISH);
 if (err != Z_STREAM_END) {
 deflateEnd(&stream);
 return err == Z_OK ? Z_BUF_ERROR : err;
 }
 *destLen = stream.total_out;

 err = deflateEnd(&stream);
 return err;
 }

 int ZEXPORT compress(Bytef *dest, uLongf *destLen, const Bytef *source, uLong sourceLen)
 {
 return compress2(dest, destLen, source, sourceLen, Z_DEFAULT_COMPRESSION);
 }

 int uncompress1(Bytef *dest, uLongf *destLen, const Bytef *source, uLong sourceLen)
 {
 z_stream stream;
 int err;

 stream.next_in = (z_const Bytef *)source;
 stream.avail_in = (uInt)sourceLen;
 // Check for source > 64K on 16-bit machine:
 if ((uLong)stream.avail_in != sourceLen) return Z_BUF_ERROR;

 stream.next_out = dest;
 stream.avail_out = (uInt)*destLen;
 if ((uLong)stream.avail_out != *destLen) return Z_BUF_ERROR;

 stream.zalloc = (alloc_func)0;
 stream.zfree = (free_func)0;

 err = inflateInit(&stream);
 if (err != Z_OK) return err;

 err = inflate(&stream, Z_FINISH);
 if (err != Z_STREAM_END) {
 inflateEnd(&stream);
 if (err == Z_NEED_DICT || (err == Z_BUF_ERROR && stream.avail_in == 0))
 return Z_DATA_ERROR;
 return err;
 }
 *destLen = stream.total_out;

 err = inflateEnd(&stream);
 return err;
 }
 */

/*/ упаковка данных методом gzip deflate (для передачи в http ответе, надо будет добавить "Content-Encoding: gzip\r\n")
 // return: код ошибки, Z_OK(0) if success, Z_MEM_ERROR(-4) недостаточно памяти для сжатия, Z_BUF_ERROR(-5) недостаточно места в выходном буфере, Z_STREAM_ERROR(-2) уровень сжатия некорректный
 int zlib_compress_http(uint8_t *str, uint32_t src_len, uint8_t **dst, uint32_t *dst_len_out, int level)
 {
 if (!dst) return Z_BUF_ERROR;
 uLongf dst_len = (uLongf)(src_len*1.001) + 13;
 *dst = (Bytef *)g_malloc0(dst_len);
 int ret = compress2(*dst, &dst_len, (const Bytef*)str, src_len, level);
 if (dst_len_out)
 *dst_len_out = dst_len;
 //crc32();
 return ret;
 }

 #define CHECK_ERR(err, msg) { \
    if (err != Z_OK) { \
        fprintf(stderr, "%s error: %d\n", msg, err); \
        exit(1); \
	    } \
}

 // Test deflate() with large buffers and dynamic change of compression level

 void test_large_deflate(Byte *compr, uLong comprLen, Byte *uncompr, uLong uncomprLen)
 {
 z_stream c_stream; // compression stream 
 int err;

 c_stream.zalloc = (alloc_func)0;
 c_stream.zfree = (free_func)0;
 c_stream.opaque = (voidpf)0;

 err = deflateInit(&c_stream, Z_BEST_SPEED);
 CHECK_ERR(err, "deflateInit");

 c_stream.next_out = compr;
 c_stream.avail_out = (uInt)comprLen;

 // At this point, uncompr is still mostly zeroes, so it should compress
 // very well:
 //
 c_stream.next_in = uncompr;
 c_stream.avail_in = (uInt)uncomprLen;
 err = deflate(&c_stream, Z_NO_FLUSH);
 CHECK_ERR(err, "deflate");
 if (c_stream.avail_in != 0) {
 fprintf(stderr, "deflate not greedy\n");
 exit(1);
 }

 // Feed in already compressed data and switch to no compression: 
 deflateParams(&c_stream, Z_NO_COMPRESSION, Z_DEFAULT_STRATEGY);
 c_stream.next_in = compr;
 c_stream.avail_in = (uInt)comprLen / 2;
 err = deflate(&c_stream, Z_NO_FLUSH);
 CHECK_ERR(err, "deflate");

 // Switch back to compressing mode: 
 deflateParams(&c_stream, Z_BEST_COMPRESSION, Z_FILTERED);
 c_stream.next_in = uncompr;
 c_stream.avail_in = (uInt)uncomprLen;
 err = deflate(&c_stream, Z_NO_FLUSH);
 CHECK_ERR(err, "deflate");

 err = deflate(&c_stream, Z_FINISH);
 if (err != Z_STREAM_END) {
 fprintf(stderr, "deflate should report Z_STREAM_END\n");
 exit(1);
 }
 err = deflateEnd(&c_stream);
 CHECK_ERR(err, "deflateEnd");
 }

 void test_large_inflate (Byte *compr, uLong comprLen,Byte *uncompr, uLong uncomprLen)
 {
 int err;
 z_stream d_stream; // decompression stream 

 g_strlcpy((char*)uncompr, "garbage", uncomprLen);

 d_stream.zalloc = (alloc_func)0;
 d_stream.zfree = (free_func)0;
 d_stream.opaque = (voidpf)0;

 d_stream.next_in = compr;
 d_stream.avail_in = (uInt)comprLen;

 err = inflateInit(&d_stream);
 CHECK_ERR(err, "inflateInit");

 for (;;) {
 d_stream.next_out = uncompr;            // discard the output 
 d_stream.avail_out = (uInt)uncomprLen;
 err = inflate(&d_stream, Z_NO_FLUSH);
 if (err == Z_STREAM_END) break;
 CHECK_ERR(err, "large inflate");
 }

 err = inflateEnd(&d_stream);
 CHECK_ERR(err, "inflateEnd");

 if (d_stream.total_out != 2 * uncomprLen + comprLen / 2) {
 fprintf(stderr, "bad large inflate: %ld\n", d_stream.total_out);
 exit(1);
 }
 else {
 my_printf("large_inflate(): OK\n");
 }
 }

 */
