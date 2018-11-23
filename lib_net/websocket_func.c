// websocket_func.c
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <glib.h>
// libs
#include "net_func.h"
#include "sock_func.h"
#include "websocket_func.h"
#ifdef _WIN32
#include <io.h>		//for open, read
#else
#include <unistd.h>	//for unix
#endif

// Make sure we can call this stuff from C++.
#if __cplusplus
extern "C" {
#endif

// сгенерировать запрос "Sec-WebSocket-Key" по бинарной строке
static char *ws_calc_sec_key(uint8_t *data, int data_len) {
	GChecksum *checksum = g_checksum_new(G_CHECKSUM_SHA1);
	if (!checksum)
		return NULL;
	// 1. вычислить бинарный хеш SHA-1 (бинарная строка из 20 символов)
	g_checksum_update(checksum, data, data_len);
	gsize str2_len = 20;
	char *str2 = g_malloc(str2_len);
	g_checksum_get_digest(checksum, (guint8*) str2, &str2_len);
	g_checksum_free(checksum);
	// 2. закодировать хеш в Base64
	char *str3 = g_base64_encode((const guchar *) str2, str2_len);
	g_free(str2);
	return str3;
}
// вычислить ответ "Sec-WebSocket-Accept" на запрос "Sec-WebSocket-Key" или сгенерировать запрос по ключу
static char *ws_calc_sec_accept(char *key) {
	// пример: на запрос "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=" должен быть ответ "s3pPLMBiTxaQ9kYGzzhZRbK+xOo="
	// пример: на запрос "1G1jzhnoTKBcPqbIPrY6oA== " должен быть ответ "+jeM4qYd0ioe0rXJO1KEcpvk4Hk="
	GChecksum *checksum = g_checksum_new(G_CHECKSUM_SHA1);
	if (!checksum)
		return NULL;
	// 1. взять строковое значение из заголовка Sec-WebSocket-Key и объединить со строкой 258EAFA5-E914-47DA-95CA-C5AB0DC85B11
	char *str1 = g_strdup_printf("%s258EAFA5-E914-47DA-95CA-C5AB0DC85B11", key);
	// 2. вычислить бинарный хеш SHA-1 (бинарная строка из 20 символов) от полученной в первом пункте строки
	g_checksum_update(checksum, (const guchar *) str1, strlen(str1));
	gsize str2_len = 20;
	char *str2 = g_malloc(str2_len);
	g_checksum_get_digest(checksum, (guint8*) str2, &str2_len);
	g_checksum_free(checksum);
	// 3. закодировать хеш в Base64
	char *str3 = g_base64_encode((const guchar *) str2, str2_len);
	g_free(str1);
	g_free(str2);
	return str3;
}

// проверка валидности номера фрейма
gboolean ws_valid_frame_number(uint8_t frame_name) {
	if (frame_name < 0x1 || frame_name > 0xA
			|| (frame_name >= 0x3 && frame_name <= 0x7))
		return FALSE;
	return TRUE;
}

// очистить пакет
void ws_packet_clean(struct WS_PACKET *packet) {
	if (!packet)
		return;
	if (packet->raw)
		g_free(packet->raw);
	if (packet->data)
		g_free(packet->data);
}

// удалить пакет
void ws_packet_free(struct WS_PACKET *packet) {
	ws_packet_clean(packet);
	g_free(packet);
}

// установка соединения (рукопожатия) websocket со стороны сервера
// header - полученый заголовок от от клиента, требует удаления через http_header_free(header)
gboolean ws_server_handshake(SOCKET sockfd, const char *server_name,
		http_header **header, int timeout) {

	if (!header)
		return FALSE;
	while (1) {
		gboolean ret;
		int str_len;
		// ждём данных от клиента
		int is_data = s_pool(sockfd, timeout);
		// timeout
		if (!is_data)
			continue;
		// ошибка (м.б. сокет закрыт)
		if (is_data < 0)
			break;
		//int nRecv = 0;
		// принимаем http заголовок
		char *str_header = s_get_next_str(sockfd, &str_len, "\r\n\r\n", TRUE,
				timeout);
		//printf("[in header]\n%s\n", str_header);
		// расшифровываем заголовок
		http_page page;
		page.raw_header = str_header;
		page.header_len = (str_header) ? strlen(str_header) : 0;
		*header = http_parse_header(&page);
		g_free(str_header);
		// сформировать http заголовок для ответа
		if (!(*header) || !(*header)->web_socket_key || !(*header)->upgrade
				|| !(*header)->web_socket_ver) {
			// неподходящий запрос
			str_header =
					g_strdup_printf(
							"HTTP/1.1 400 Bad Request\r\nServer: %s\r\nUpgrade: websocket\r\n\r\n",
							server_name);
			ret = FALSE;
		} else {
			char *web_socket_accept =
					(*header) ?
							ws_calc_sec_accept((*header)->web_socket_key) :
							NULL;
			//Content-Encoding: gzip
			str_header =
					g_strdup_printf(
							"HTTP/1.1 101 Switching Protocols\r\nServer: %s\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: %s\r\nSec-WebSocket-Protocol: chat\r\n\r\n",
							server_name, web_socket_accept);
			g_free(web_socket_accept);
			ret = TRUE;
		}
		//printf("[out header]\n%s\n", str_header);
		// отправить заголовок ответа
		if (str_header) {
			s_send(sockfd, (unsigned char *) str_header, strlen(str_header),
					timeout);
			g_free(str_header);
		}
		return ret;
	}
	return FALSE;
}

/*/ разбор пакета
 // [in] header - первые байты сообщения
 // [in] header_len - длина заголовка, от 2 до 10 байт
 // [out] is_last - последний ли это фрейм с сообщением
 // [out] frame_type - тип фрейма с сообщением
 // [out] is_mask - используется ли маска
 // [out] len - длина данных (если=126 или 127, значит длину нужно будет уточнить)
 // header - полученый заголовок от от клиента, требует удаления через http_header_free(header)
 // return: сколько байт ещё нужно считать
 uint64_t ws_parse_packet_header(const char *header, int header_len, struct WS_PACKET *packet)
 {
 gboolean *is_last, int8_t *frame_type, gboolean *is_mask;
 uint64_t len = header[1] & 0x7F;// длина данных
 *is_last = header[0] & 0x80;// самый ставший бит
 *frame_type = header[0] & 0xF;// тип фрейма с сообщением
 *is_mask = header[1] & 0x80;// используется ли маска
 if (len == 126 && header_len >= 4)
 len = *(uint16_t*)(header + 2);
 if (len == 127 && header_len >= 10)
 len = *(uint64_t*)(header + 2);
 return len;
 }*/

// сформировать пакет для отправки 
// return: NULL или packet - сформированный пакет данных с текстовой строкой, удалять через ws_packet_free(packet);
struct WS_PACKET* ws_create_packet(uint8_t frame_type, uint8_t *data,
		uint64_t data_len) {
	gsize cur_pos = 0;
	if (!data && data_len > 0)// пустые пакеты допустимы, например закрытие сесии
		return NULL;
	struct WS_PACKET *packet = (struct WS_PACKET*) g_malloc0(
			sizeof(struct WS_PACKET));
	packet->data_len = data_len;
	packet->data = (uint8_t*) g_malloc0((gsize) packet->data_len);
	memcpy(packet->data, data, (size_t) data_len);
	packet->frame_type = frame_type;
	packet->is_last = TRUE;		// это последнее сообщение в цепочке
	packet->is_mask = (packet->data_len > 0) ? TRUE : FALSE;// накладывать ли маску на сообщение; если нет данных, то и маска не нужна
	packet->mask = (packet->is_mask) ? g_random_int() : 0;
	packet->raw_len = 2 + packet->data_len;		// заголовок + сообщение
	if (packet->is_mask)		// маска - 4 байта
		packet->raw_len += 4;
	if (packet->data_len > 125)	// длина сообщения занимает дополнительно 2 байта
		packet->raw_len += 2;
	if (packet->data_len > 0xffff)// длина сообщения занимает дополнительно 8 байт, если >65535
		packet->raw_len += 6;
	// сбор пакета
	packet->raw = (uint8_t*) g_malloc0((gsize) packet->raw_len);
	*(packet->raw + 0) |= (packet->is_last) ? 0x80 : 0;
	*(packet->raw + 0) |= packet->frame_type;
	*(packet->raw + 1) |= (packet->is_mask) ? 0x80 : 0;
	// какую длину указывать в пакете
	uint8_t send_data_len =
			(packet->data_len < 126) ? (uint8_t) packet->data_len :
			(packet->data_len > 0xffff) ? 127 : 126;
	*(packet->raw + 1) |= send_data_len;
	cur_pos = 2;
	if (send_data_len == 126) {
		memcpy(packet->raw + cur_pos, &packet->data_len, 2);
		cur_pos += 2;
	} else if (send_data_len == 127) {
		memcpy(packet->raw + cur_pos, &packet->data_len, 8);
		cur_pos += 8;
	}
	if (packet->is_mask)		// маска - 4 байта
	{
		memcpy(packet->raw + cur_pos, &packet->mask, 4);
		cur_pos += 4;
	}
	memcpy(packet->raw + cur_pos, packet->data, (size_t) packet->data_len);
	// наложение маски
	if (packet->is_mask) {
		uint64_t i;
		uint8_t *raw_msg = packet->raw + cur_pos;
		for (i = 0; i < packet->data_len; i++) {
			*(raw_msg + i) ^= *((uint8_t*) &packet->mask + i % 4);
		}
	}
	return packet;
}

// сделать пакет для отправки текстового сообщения
// если packet->need_read >0, значит ещё нужно докачать данных
// return: NULL или packet - сформированный пакет данных с текстовой строкой, удалять через ws_packet_free(packet);
struct WS_PACKET* ws_create_packet_from_string(const char *str) {
	if (!str)
		return NULL;
	return ws_create_packet(WS_TYPE_TEXT, (uint8_t *) str, strlen(str));
}

// послать WebSocket сообщение
gboolean ws_send_mesage(SOCKET sockfd, uint8_t frame_type, uint8_t *data,
		uint64_t data_len, GMutex *mutex_rw, int timeout) {
	int ret = 0;
	struct WS_PACKET *packet2 = ws_create_packet(frame_type, data, data_len);
	if (packet2) {
		printf("[ws_send_mesage_text start] %s\n", data);
		if (mutex_rw)
			g_mutex_lock(mutex_rw);
		ret = s_send(sockfd, (unsigned char *) packet2->raw,
				(int) packet2->raw_len, timeout);
		if (mutex_rw)
			g_mutex_unlock(mutex_rw);
		//ret = write(sockfd, (unsigned char *)packet2->raw, (int)packet2->raw_len);
		//printf("[ws_send_mesage end] %s\n", data);
	}
	ws_packet_free(packet2);
	return (ret > 0) ? TRUE : FALSE;
}

// послать WebSocket текстовое сообщение
gboolean ws_send_mesage_text(SOCKET sockfd, const char* str, GMutex *mutex_rw,
		int timeout) {
	int ret = 0;
	struct WS_PACKET *packet2 = ws_create_packet_from_string(str);
	if (packet2) {
		printf("[ws_send_mesage_text start] %s\n", str);
		if (mutex_rw)
			g_mutex_lock(mutex_rw);
		//		ret = fwrite((unsigned char *)packet2->raw, (int)packet2->raw_len,1, (FILE*)sockfd);
		ret = s_send(sockfd, (unsigned char *) packet2->raw,
				(int) packet2->raw_len, timeout);
		if (mutex_rw)
			g_mutex_unlock(mutex_rw);
		//printf("[ws_send_mesage_text end] %s\n", str);
	}
	ws_packet_free(packet2);
	return (ret > 0) ? TRUE : FALSE;
}

// разбор полученного пакета (packet->raw = > packet.data)
// если packet->need_read >0, значит ещё нужно докачать данных и заново запустить разбор пакета
// packet - полученый пакет данных частично или целиком
gboolean ws_parse_packet(struct WS_PACKET *packet) {
	uint64_t cur_pos = 0;
	if (!packet)
		return FALSE;
	packet->need_read = 0;
	// сколько байт уже скачано
	uint8_t *raw = packet->raw;	// сырое содержимое пакета
	uint64_t raw_len = packet->raw_len;	// длина сырого пакета
	if (raw_len < 2) {
		packet->need_read = 2 - raw_len;
		return TRUE;
	}
	packet->is_last = (raw[0] & 0x80) ? 1 : 0;	// последний ли это фрейм
	packet->frame_type = raw[0] & 0xF;	// тип фрейма с сообщением
	packet->data_len = raw[1] & 0x7F;	// длина данных
	if (packet->is_last != 1 || !ws_valid_frame_number(packet->frame_type))
		return FALSE;
	cur_pos = 2;
	if (packet->data_len == 126) {
		if (raw_len >= 4) {
			packet->data_len = *(uint16_t*) (raw + cur_pos);
			cur_pos += 2;
		} else {
			packet->need_read = 4 - raw_len;
			return TRUE;
		}
	} else if (packet->data_len == 127) {
		if (raw_len >= 10) {
			packet->data_len = *(uint64_t*) (raw + cur_pos);
			cur_pos += 8;
		} else {
			packet->need_read = 10 - raw_len;
			return TRUE;
		}
	}
	packet->is_mask = (raw[1] & 0x80) ? 1 : 0;	// используется ли маска
	int mask_len = 0;
	if (packet->is_mask)
		mask_len = 4;
	if (raw_len < cur_pos + mask_len + packet->data_len) {
		packet->need_read = cur_pos + mask_len + packet->data_len - raw_len;
		return TRUE;
	}
	if (packet->is_mask) {
		packet->mask = *(uint32_t*) (raw + cur_pos);
		cur_pos += 4;
	}
	packet->data = g_malloc0((gsize) packet->data_len + 1);
	memcpy(packet->data, raw + cur_pos, (gsize) packet->data_len);
	// наложение маски
	if (packet->is_mask) {
		uint64_t i;
		for (i = 0; i < packet->data_len; i++) {
			*(packet->data + i) ^= *((uint8_t*) &packet->mask + i % 4);
		}
	}

	// выставляем длину пакета, которую обработали
	packet->raw_len = cur_pos + packet->data_len;
	return TRUE;
}

// callback функция - создание сокета
curl_socket_t my_opensocketfunc_callback(void *clientp, curlsocktype purpose,
		struct curl_sockaddr *address) {
	printf("my_opensocketfunc_callback\n");
	SOCKET *ws_sock = (SOCKET*) clientp;
	SOCKET sock = socket(address->family, address->socktype, address->protocol);
	if (ws_sock)
		*ws_sock = sock;
	return sock;
}

// callback функция - получения заголовка
size_t my_headerfunc_callback(char *buffer, size_t size, size_t nitems,
		void *userdata) {
	//printf("my_headerfunc_callback\n");
	int (*callback_func)(char*, size_t) = (int(*)(char*, size_t))userdata;
	// выполнение пользовательской функции
	if (callback_func)
		callback_func(buffer, nitems * size);
	return nitems * size;
}

// callback функция - чтение из сокета
size_t my_writefunc_callback(char *ptr, size_t size, size_t nmemb,
		void *userdata) {
	//printf("my_writefunc_callback\n");
	int (*callback_func)(char*, size_t) = (int(*)(char*, size_t))userdata;
	// выполнение пользовательской функции
	if (callback_func)
		callback_func(ptr, nmemb * size);
	return nmemb * size;
}

// установка соединения (рукопожатия) websocket со стороны клиента
// 
// read_callback_func - обраная функция,вызываемая при чтении данных из сокета
// return: код ошибки, 0 успешное завершение сессии, иначе ошибка
int ws_client_handshake(const char *url_addr, proxy_param *proxy,
		SOCKET *ws_sock, void (*readheader_callback_func)(char*, size_t),
		int (*read_callback_func)(char*, size_t)) {
	//ws_param	*conn = NULL;
	CURL *curl = curl_easy_init();
	CURLcode ret;
	if (!curl)
		return -1;

#ifdef NDEBUG
	ret = curl_easy_setopt(curl, CURLOPT_VERBOSE, 0);// режим отладки (0 или 1)
#else
	ret = curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);// режим отладки (0 или 1)
#endif
	//ret = curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, net_get_timeout_connect());// timeout при соединении, в сек.
	// Default timeout is 0 (zero)which means it never times out during transfer.
	ret = curl_easy_setopt(curl, CURLOPT_TIMEOUT, 0);// timeout при выполнении функций, в сек.
	ret = curl_easy_setopt(curl, CURLOPT_URL, url_addr);// адрес для соединения
	ret = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);// удалённый сервер не будет проверять наш сертификат. В противном случае необходимо этот самый сертификат послать. (для wss)
	ret = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
	//ret = curl_easy_setopt(curl, CURLOPT_CONNECT_ONLY, 1L);// только соединиться
	// Add headers
	struct curl_slist *header_list_ptr;
	{
		// собрать заголовок
		header_list_ptr = curl_slist_append(NULL,
				"HTTP/1.1 101 WebSocket Protocol Handshake");
		header_list_ptr = curl_slist_append(header_list_ptr,
				"Upgrade: WebSocket");
		header_list_ptr = curl_slist_append(header_list_ptr,
				"Connection: Upgrade");
		header_list_ptr = curl_slist_append(header_list_ptr,
				"Sec-WebSocket-Version: 13");
		gdouble val = g_random_double();
		char *key = ws_calc_sec_key((uint8_t*) &val, sizeof(gdouble));
		char *ws_key = g_strdup_printf("Sec-WebSocket-Key: %s", key);
		header_list_ptr = curl_slist_append(header_list_ptr, ws_key);// "Sec-WebSocket-Key: x3JJHMbDL1EzLkh9GBhXDw=="
		g_free(key);
		g_free(ws_key);
		// установить заголовок
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list_ptr);
		//curl_slist_free_all(header_list_ptr);
	}
	// настройки прокси
	if (proxy && proxy->is_use) {
		ret = curl_easy_setopt(curl, CURLOPT_PROXY, proxy->host);// адрес прокси в формате "host:port"
		if (proxy->port)
			ret = curl_easy_setopt(curl, CURLOPT_PROXYPORT, proxy->port);// порт также можно ввести отдельно в виде числа
		switch (proxy->type) {
		case PROXY_HTTP:
			ret = curl_easy_setopt(curl, CURLOPT_PROXYTYPE, CURLPROXY_HTTP);
			ret = curl_easy_setopt(curl, CURLOPT_HTTPPROXYTUNNEL, 1L);//тунелирование через прокси, чтобы не выдавал ответы прокси, а сразу нужную страницу
			break;
		case PROXY_SOCKS4:
			ret = curl_easy_setopt(curl, CURLOPT_PROXYTYPE, CURLPROXY_SOCKS4);
			break;
		case PROXY_SOCKS4A:
			ret = curl_easy_setopt(curl, CURLOPT_PROXYTYPE, CURLPROXY_SOCKS4A);
			break;
		case PROXY_SOCKS5:
			ret = curl_easy_setopt(curl, CURLOPT_PROXYTYPE,
					CURLPROXY_SOCKS5_HOSTNAME);	//CURLPROXY_SOCKS5 - передавать ip-адрес, а не имя хоста (требуется DNS до отправки на прокси)
			break;
		}
		if (proxy->is_auth) {
			gchar *user_pass = g_strdup_printf("%s:%s",
					(proxy->username) ? proxy->username : "",
					(proxy->password) ? proxy->password : "");
			//ret = curl_easy_setopt(curl, CURLOPT_USERPWD, );
			ret = curl_easy_setopt(curl, CURLOPT_PROXYUSERPWD, user_pass);
			g_free(user_pass);
		}
	}
	// callback функции
	curl_easy_setopt(curl, CURLOPT_OPENSOCKETFUNCTION,
			my_opensocketfunc_callback);
	curl_easy_setopt(curl, CURLOPT_OPENSOCKETDATA, ws_sock);// передача доп. параметра в callback функцию my_opensocketfunc_callback()
	curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, my_headerfunc_callback);
	curl_easy_setopt(curl, CURLOPT_HEADERDATA, readheader_callback_func);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, my_writefunc_callback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, read_callback_func);// передача доп. параметра в callback функцию my_writefunc_callback()

	// создаём структуру с соединеним и заполняем её
	//conn = (ws_param *)g_malloc0(sizeof(ws_param));
	//conn->curl = curl;

	// выполнить запрос
	ret = curl_easy_perform(curl);

	curl_slist_free_all(header_list_ptr);
	//curl_easy_setopt(handle, CURLOPT_URL, url_addr);//"http ://echo.websocket.org"
	curl_easy_cleanup(curl);	// Завершает libcurl easy сессию 

	return ret;
}

// закрытие соединения
void ws_client_close(ws_param *conn) {
	if (!conn)
		return;
	//curl_easy_cleanup(conn->curl);// Завершает libcurl easy сессию 
	//conn->curl = NULL;
}

// установка локаольного соединения (рукопожатия) websocket со стороны клиента и отправка данных
// return: код ошибки, 0 успешное завершение сессии, иначе ошибка
int ws_client_handshake_fast(const char *url_addr, char *message_send) {
	//ws_param	*conn = NULL;
	CURL *curl = curl_easy_init();
	CURLcode ret;
	if (!curl)
		return -1;

#ifdef NDEBUG
	ret = curl_easy_setopt(curl, CURLOPT_VERBOSE, 0);// режим отладки (0 или 1)
#else
	ret = curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);// режим отладки (0 или 1)
#endif
	//ret = curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, net_get_timeout_connect());// timeout при соединении, в сек.
	// Default timeout is 0 (zero)which means it never times out during transfer.
	ret = curl_easy_setopt(curl, CURLOPT_TIMEOUT, 0);// timeout при выполнении функций, в сек.
	ret = curl_easy_setopt(curl, CURLOPT_URL, url_addr);// адрес для соединения
	ret = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);// удалённый сервер не будет проверять наш сертификат. В противном случае необходимо этот самый сертификат послать. (для wss)
	ret = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
	// Add headers
	struct curl_slist *header_list_ptr;
	{
		// собрать заголовок
		header_list_ptr = curl_slist_append(NULL,
				"HTTP/1.1 101 WebSocket Protocol Handshake");
		header_list_ptr = curl_slist_append(header_list_ptr,
				"Upgrade: WebSocket");
		header_list_ptr = curl_slist_append(header_list_ptr,
				"Connection: Upgrade");
		header_list_ptr = curl_slist_append(header_list_ptr,
				"Sec-WebSocket-Version: 13");
		char *custom_header = g_strdup_printf("Additional: %s", message_send);
		header_list_ptr = curl_slist_append(header_list_ptr, custom_header);
		g_free(custom_header);
		gdouble val = g_random_double();
		char *key = ws_calc_sec_key((uint8_t*) &val, sizeof(gdouble));
		char *ws_key = g_strdup_printf("Sec-WebSocket-Key: %s", key);
		header_list_ptr = curl_slist_append(header_list_ptr, ws_key);// "Sec-WebSocket-Key: x3JJHMbDL1EzLkh9GBhXDw=="
		g_free(key);
		g_free(ws_key);
		// установить заголовок
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list_ptr);
		//curl_slist_free_all(header_list_ptr);
	}
	// выполнить запрос
	ret = curl_easy_perform(curl);

	curl_slist_free_all(header_list_ptr);
	curl_easy_cleanup(curl);	// Завершает libcurl easy сессию 

	return ret;
}

#if __cplusplus
}  // End of the 'extern "C"' block
#endif

