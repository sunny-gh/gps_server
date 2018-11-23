// net_func.h

#ifndef _NET_FUNC_H_
#define _NET_FUNC_H_
// Make sure we can call this stuff from C++.
#ifdef __cplusplus
extern "C" {
#endif
//#include <config.h>
//#include "parse_url.h"
#include <curl/curl.h>
#include <glib.h>

#define USER_AGENT	PACKAGE_STRING " %s"
//#define USER_AGENT "lavtrack "VERSION" (%s)"

// выход из функции при ошибке с присвоением кода ошибки
#define net_func_exit(ret) {if(err_code) *err_code = ret;return NULL;};
#define net_func_exit_if_err(ret) {if(ret!=CURLE_OK)	net_func_exit(ret)}

#ifdef WIN32
#define strcasecmp _stricmp
#endif

// протоколы
enum {
	PROTOCOL_FILE = 1,	// локальный файл
	PROTOCOL_HTTP,
	PROTOCOL_FTP
};

// типы прокси-серверов
enum {
	PROXY_NONE, PROXY_HTTP, PROXY_SOCKS4, PROXY_SOCKS4A, PROXY_SOCKS5
};

// Коды ошибок и статусы выполняемых процессов
enum {
	//eStatusOK=0,         // This is the good status
	//eStatusNetError,
	//eStatusNotResponding,
	//eStatusBadResponding,
	//eStatusNoDNSEntry,
	// высокоуровневые ошибки протоколов
	eStatusErr = -1,			// неизвестная ошибка
	eStatusBadAddr = 10,		// не удалось распарсить адрес
	eStatusUnsupportProto,	// неподдерживаемый протокол
	eStatusNoPermit,		// No permission (access rights)
	eStatusNoRes,			// нет такого ресурса (файла, страницы и т.д.)
	eStatusNoMem,			// недостаточно памяти
};

// описание прокси-сервера
typedef struct proxy_param_ {
	gboolean is_use;	// использовать ли прокси-сервер
	guchar type;	// тип прокси-сервера (PROXY_HTTP, PROXY_SOCK4, PROXY_SOCK5)
	char *host;		// адрес сервера
	int port;		// порт прокси-сервера
	gboolean is_auth;	// использовать ли авторизацию
	char *username;	// имя пользователя - не обязательно
	char *password;	// пароль пользователя - не обязательно
} proxy_param;

// описание соединения
typedef struct connect_param_ {
	CURL *curl;	// указатель для библиотеки curl
	gchar *url_addr;// адрес ресурса (http://example.com, file:///c/info.txt)
	// для http соединения
	char *url_refer;	// url_refer - ссылающаяся страница, м.б. = NULL
	char *user_agent;// user_agent - идентификатор броузера, м.б. = NULL ("Mozilla/5.0 (Windows NT 6.1; WOW64; rv:18.0) Gecko/20100101 Firefox/18.0")
	char *cookie;// cookie - куки, если не нужны = NULL, ("tab_site=0; tab_global=1")
	//guchar		 type;	// тип соединения (PROTOCOL_FILE, PROTOCOL_HTTP, PROTOCOL_FTP)
	//parsed_url	*url;	// распарсеный адрес сервера
	//proxy_param	*proxy;	// описание прокси-сервера, если он используется
	//SOCKET sock;		// сокет с установленной связью
} connect_param;

// содержимое http-заголовка  полученной страницы
/*
 HTTP/1.1 200 OK
 Date: Mon, 15 Apr 2013 07:15:09 GMT
 Server: Apache
 Keep-Alive: timeout=5, max=100
 Connection: Keep-Alive
 Transfer-Encoding: chunked
 Content-Type: text/html; charset=utf-8
 Content-Language: en
 Set-Cookie: sid=208c370eb490179a5a4b5d9f37f2f31c; expires=Fri, 17-May-2013 04:52:40 GMT
 */
typedef struct http_header_ {
	char *proto;			// протокол, "HTTP/1.1" (из строки HTTP/1.1 200 OK)
	int ret_code;			// код ответа сервера, 200, 404, ...
	char *ret_code_bin_str;	// код ответа сервера в текстовом виде, 200, 404, ...
	char *ret_code_str;	// тектовое описание кода ответа, 200="ОК", "404"="Not Found"
	int length;			// длина тела страницы в байтах, "Content-Length: 1290"
	time_t utc_time;// время по гринвичу, расшифровка "Date: Mon, 15 Apr 2013 07:15:09 GMT"
	char *server;			// "Server: Apache"
	char *connection;		// "Connection: Keep-Alive"
	char *conn_type;			// "Content-Type: text/html; charset=utf-8"
	char *keep_alive;		// "Keep-Alive: timeout=5, max=100"
	char *transfer_encoding;	// "Transfer-Encoding: chunked"
	char *accept_encoding;	// "Accept-Encoding: gzip, deflate"
	char *language;			// "Content-Language: en"
	char *cookie;// "Set-Cookie: sid=208c370eb490179a5a4b5d9f37f2f31c; expires=Fri, 17-May-2013 04:52:40 GMT"
	// для WebSocket
	char *upgrade;			// "Upgrade: websocket"
	// WebSocket (запросы клиента)
	char *web_socket_ver;	// "Sec-WebSocket-Version: 13"
	char *web_socket_key;	// "Sec-WebSocket-Key: 2U1Kg0ZXAm0OtYqwcNqj4A=="
	// WebSocket (ответы сервера)
	char *web_socket_accept;// "Sec-WebSocket-Accept	cD1KzWjEW5xhybsqpZgGPMr555k="
	// custom
	char *additional;
} http_header;

// содержимое страницы http
typedef struct http_page_ {
	char *raw_header;	// содержимое заголовка страницы
	char *raw_body;		// содержимое тела страницы
	size_t header_len;	// длина заголовка страницы
	size_t body_len;		// длина тела страницы
	http_header *header;		// расшифрованный заголовок
} http_page;

/*/ содержимое в формате json
 typedef struct json_data_ {
 int		count;	// число элементов с данными
 char	**name;	// массив названий элементов
 char	**value;	// массив значений элементов
 } json_data;*/

// копирование структуры proxy_param
void proxy_param_copy(proxy_param *proxy_to, const proxy_param *proxy_from);
// очистка структуры proxy_param
void proxy_param_clean(proxy_param *proxy);
// удаление структуры proxy_param
void proxy_param_free(proxy_param *proxy);
// удаление структуры connect_param
void net_connect_free(connect_param *connect);
// удаление структуры http_header
void http_header_free(http_header *header);
// удаление структуры http_page
void http_page_free(http_page *page);

// получить строку с описанием кода ошибки, строка требует удаления через g_free()
char *my_curl_easy_strerror(int error);

// вызывается один раз в самом начале работы
int net_init();
// вызывается один раз в самом конце работы
void net_destroy();

// создать сетевое соединение
// url_addr - адрес для соединения
// proxy - параметры прокси-сервера, если не нужен = NULL
// err_code - код ошибки, если не нужен = NULL
// return строка, которая требует удаления, или NULL
connect_param* net_connect(char *url_addr, proxy_param *proxy, int *err_code);

// установить новые timeout-ы
// t_timeout_connect_new - timeout при соединении, в сек.
// t_timeout_new - timeout при выполнении функций, в сек.
int net_set_timeout(int t_timeout_connect_new, int t_timeout_new);
// узнать timeout при соединении, в сек.
int net_get_timeout_connect();
// узнать timeout при выполнении функций, в сек.
int net_get_timeout();

// получить данные по http протоколу
// conn - параметры установленного соединения
// err_code - код ошибки, если не нужен м.б. = NULL
// return структура с данными страницы, которая требует удаления, или NULL в случае ошибки
http_page* http_get_page(connect_param *conn, int *err_code);

// отправить данные по http протоколу
// conn - параметры установленного соединения
// posd_data - данные для передачи на сервер, пример: "name=daniel&project=curl"
// posd_data_len - длина данных для передачи, если = -1, передаваемая строка заканчивается нулём.
// err_code - код ошибки, если не нужен = NULL
// return структура с данными страницы, которая требует удаления, или NULL в случае ошибки
http_page* http_post_page(connect_param *conn, char *posd_data,
		size_t posd_data_len, int *err_code);

// распарсить принятый http-заголовок
// page - полученная http-страница
// return структура с данными страницы, которая требует удаления, или NULL в случае ошибки
http_header* http_parse_header(http_page *page);

// Отправка запроса по адресу url_addr
// получить json ответ вида {"status": ... , "text": ...} по http протоколу
// url_addr - адресная строка
// proxy - структура с настройками прокси-сервера, если не нужен прокси = NULL
// str_req - строка запроса для отправки на сервер
// err_code - код ошибки, если не нужен = NULL
// err_str - текстовая расшифровка кода ошибки, если не нужена = NULL
// return полученные данные из поля "text" типа JsonNode*, которые требует удаления, или NULL в случае ошибки
char* http_post_json(char *url_addr, proxy_param *proxy, char *str_req,
		int *err_code, char **err_code_str);

#if __cplusplus
}  // End of the 'extern "C"' block
#endif

#endif // #ifndef _NET_FUNC_H_

