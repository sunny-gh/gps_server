// websocket_func.h

#ifndef _WEBSOCKET_FUNC_H_
#define _WEBSOCKET_FUNC_H_
// Make sure we can call this stuff from C++.
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <curl/curl.h>
#include <glib.h>

enum {
	WS_TYPE_TEXT = 0x1, // обозначает текстовый фрейм.
	WS_TYPE_BIN = 0x2, // обозначает двоичный фрейм.
	// 3 - 7 зарезервированы для будущих фреймов с данными.
	WS_TYPE_CLOSE = 0x8,	// обозначает закрытие соединения этим фреймом.
	WS_TYPE_PING = 0x9,	// обозначает PING.
	WS_TYPE_PONG = 0xA,	// обозначает PONG.
	// B - F зарезервированы для будущих управляющих фреймов.
	WS_TYPE_NEXT = 0x0,// обозначает фрейм - продолжение для фрагментированного сообщения. Он интерпретируется, исходя из ближайшего предыдущего ненулевого типа.
};

struct WS_PACKET {
	uint8_t frame_type;	//тип фрейма с сообщением

	gboolean is_last;	// последний ли это фрейм с сообщением
	gboolean is_mask;	// используется ли маска
	uint32_t mask;		// маска

	uint8_t *data;		// расшифрованные данные
	uint64_t data_len;	// длина расшифрованных данных

	uint8_t *raw;		// сырое содержимое пакета
	uint64_t raw_len;	// длина сырого пакета

	uint64_t need_read;	// сколько байт ещё требуется скачать
};

// проверка валидности номера фрейма
gboolean ws_valid_frame_number(uint8_t frame_name);

// очистить пакет
void ws_packet_clean(struct WS_PACKET *packet);
// удалить пакет
void ws_packet_free(struct WS_PACKET *packet);

// установка соединения (рукопожатия) websocket со стороны сервера
// header - полученый заголовок от от клиента, требует удаления через http_header_free(header)
gboolean ws_server_handshake(SOCKET sockfd, const char *server_name,
		http_header **header, int timeout);

// сформировать пакет для отправки 
// return: NULL или packet - сформированный пакет данных с текстовой строкой, удалять через ws_packet_free(packet);
struct WS_PACKET* ws_create_packet(uint8_t frame_type, uint8_t *data,
		uint64_t data_len);
// сделать пакет для отправки текстового сообщения
// если packet->need_read >0, значит ещё нужно докачать данных
// return: NULL или packet - сформированный пакет данных с текстовой строкой, удалять через ws_packet_free(packet);
struct WS_PACKET* ws_create_packet_from_string(const char *str);

// послать WebSocket сообщение
gboolean ws_send_mesage(SOCKET sockfd, uint8_t frame_type, uint8_t *data,
		uint64_t data_len, GMutex *mutex_rw, int timeout);
// послать WebSocket текстовое сообщение
gboolean ws_send_mesage_text(SOCKET sockfd, const char* str, GMutex *mutex_rw,
		int timeout);

// разбор пакета
// если packet->need_read >0, значит ещё нужно докачать данных
// packet - полученый пакет данных целиком
gboolean ws_parse_packet(struct WS_PACKET *packet);

// Клиентская часть

// описание соединения
typedef struct ws_param_ {
	CURL *curl;	// указатель для библиотеки curl
	gchar *url_addr;// адрес ресурса (http://example.com, file:///c/info.txt)
	SOCKET sock;		// сокет с установленной связью
} ws_param;

// установка соединения (рукопожатия) websocket со стороны клиента
// return: код ошибки, 0 успешное завершение сессии, иначе ошибка
int ws_client_handshake(const char *url_addr, proxy_param *proxy,
		SOCKET *ws_sock, void (*readheader_callback_func)(char*, size_t),
		int (*read_callback_func)(char*, size_t));

// установка локаольного соединения (рукопожатия) websocket со стороны клиента и отправка данных
// return: код ошибки, 0 успешное завершение сессии, иначе ошибка
int ws_client_handshake_fast(const char *url_addr, char *message_send);

#if __cplusplus
}  // End of the 'extern "C"' block
#endif

#endif // _WEBSOCKET_FUNC_H_

/*
 rfc6455 1.2.  Protocol Overview:
 The handshake from the client looks as follows:

 GET /chat HTTP/1.1
 Host: server.example.com
 Upgrade: websocket
 Connection: Upgrade
 Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==
 Origin: http://example.com
 Sec-WebSocket-Protocol: chat, superchat
 Sec-WebSocket-Version: 13

 The handshake from the server looks as follows:

 HTTP/1.1 101 Switching Protocols
 Upgrade: websocket
 Connection: Upgrade
 Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=
 Sec-WebSocket-Protocol: chat



 Запрос:
 GET /?encoding=text HTTP/1.1
 Host: echo.websocket.org
 User-Agent: Mozilla/5.0 (Windows NT 5.1; rv:46.0) Gecko/20100101 Firefox/46.0
 Accept: text/html,application/xhtml+xml,application/xml;q=0.9,* / *;q=0.8
 Accept-Language: ru-RU,ru;q=0.8,en-US;q=0.5,en;q=0.3
 Accept-Encoding: gzip, deflate
 DNT: 1
 Sec-WebSocket-Version: 13
 Origin: http://websocket.org
 Sec-WebSocket-Extensions: permessage-deflate
 Sec-WebSocket-Key: 2U1Kg0ZXAm0OtYqwcNqj4A==
 Cookie: _ga=GA1.2.2058967017.1465183826; _gat=1
 X-Compress: 1
 Connection: keep-alive, Upgrade
 Pragma: no-cache
 Cache-Control: no-cache
 Upgrade: websocket

 Ответ:
 Connection	Upgrade
 Date	Mon, 06 Jun 2016 08:34:33 GMT
 Sec-WebSocket-Accept	cD1KzWjEW5xhybsqpZgGPMr555k=
 Server	Kaazing Gateway
 Upgrade	websocket
 access-control-allow-cred...	true
 access-control-allow-head...	content-type, authorization, x-websocket-extensions, x-websocket-version, x-websocket-protocol
 access-control-allow-orig...	http://websocket.org

 */

/*
 Взаимодействие между клиентом и сервером начинается с запроса от клиента:

 GET /chat HTTP/1.1
 Host: server.example.com
 Upgrade: websocket
 Connection: Upgrade
 Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==
 Sec-WebSocket-Origin: http://example.com
 Sec-WebSocket-Protocol: chat, superchat
 Sec-WebSocket-Version: 7

 Ответ сервера имеет следующий вид:

 HTTP/1.1 101 Switching Protocols
 Upgrade: websocket
 Connection: Upgrade
 Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=
 Sec-WebSocket-Protocol: chat

 Ответ содержит заголовок Sec-WebSocket-Protocol с единственным протоколом, выбраным сервером (chat) из всех поддерживаемых клиентом (chat, superchat). Заголовок Sec-WebSocket-Accept формируется следующим образом:

 взять строковое значение из заголовка Sec-WebSocket-Key и объединить со строкой 258EAFA5-E914-47DA-95CA-C5AB0DC85B11 (в приведённом примере получится dGhlIHNhbXBsZSBub25jZQ==258EAFA5-E914-47DA-95CA-C5AB0DC85B11)
 вычислить бинарный хеш SHA-1 (бинарная строка из 20 символов) от полученной в первом пункте строки
 закодировать хеш в Base64 (s3pPLMBiTxaQ9kYGzzhZRbK+xOo=)

 Пример реализации вышеуказанного алгоритма на языке PHP:
 */