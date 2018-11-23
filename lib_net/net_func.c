// net_func.c
#define _CRT_SECURE_NO_WARNINGS
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <glib.h>
//# include <direct.h>	//for mkdir 
#ifdef _WIN32
#include <io.h>		//for open, read
#else
#include <unistd.h>	//for unix
#endif
#include <fcntl.h>		//for _O_BINARY & _O_TEXT  
#include <errno.h>
#include <json-glib/json-glib.h>
//#include "sock_func.h"
// libs
#include "net_func.h"

// константы
//#include <config.h>
// локализация 
#include "../package_locale.h" // определение для _(), N_(), C_(), NC_()

// узнать версию ОС - для user-agenta
#ifdef _WIN32
#define inline __inline 
#if !defined(_WIN32_WINNT)
#define _WIN32_WINNT _WIN32_WINNT_WINXP //0x0501     // Выберите значение, указывающее на другие версии Windows.
#endif
#include <Windows.h>
typedef BOOL (WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);
LPFN_ISWOW64PROCESS fnIsWow64Process;

// Make sure we can call this stuff from C++.
#if __cplusplus
extern "C" {
#endif

// from http://msdn.microsoft.com/en-us/library/ms684139%28v=vs.85%29.aspx
// return TRUE, если процесс запущен в эмуляторе - значит 32-битное приложение в 64 разрядной системе
// return FALSE, если процесс запущен в родной среде - 32 или 64
	BOOL IsWow64()
	{
		BOOL bIsWow64 = FALSE;

		//IsWow64Process is not available on all supported versions of Windows.
		//Use GetModuleHandle to get a handle to the DLL that contains the function
		//and GetProcAddress to get a pointer to the function if available.
		fnIsWow64Process = (LPFN_ISWOW64PROCESS) GetProcAddress(
				GetModuleHandle(TEXT("kernel32")),"IsWow64Process");
		if(NULL != fnIsWow64Process)
		{
			if (!fnIsWow64Process(GetCurrentProcess(),&bIsWow64))
			{
				//handle error
			}
		}
		return bIsWow64;
	}

// Определяем разрядность OC, независимо от разрядности приложения
	BOOL is_64_bit()
	{
#ifdef _WIN64
		return TRUE; // если это 64-битное приложение
#endif
		if(IsWow64()) return TRUE; // если это 32-битное приложение в 64-битной среде
		return FALSE;
	}

	const char* get_os_version()
	{
		static gchar *str = NULL;
		OSVERSIONINFO VersionInfo;
		if(!str)
		{
			//VerifyVersionInfo();
			ZeroMemory(&VersionInfo, sizeof(OSVERSIONINFO));
			VersionInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
			if(GetVersionEx(&VersionInfo))
			// $S - для строк WideChar (пример: L"Service Pack 1")
			str=g_strdup_printf("win%s ver.%d.%d build %d %s",(is_64_bit())?"64":"32",VersionInfo.dwMajorVersion,VersionInfo.dwMinorVersion,VersionInfo.dwBuildNumber,(VersionInfo.szCSDVersion)?VersionInfo.szCSDVersion:"");
		}
		return str; // например: "win64 ver.6.1 build 7601 Service Pack 1"
	};
#else // для Linux
#include <sys/utsname.h>
const char* get_os_version() {
	static gchar *str = NULL;
	if (!str) {
		struct utsname buf;
		int ret = uname(&buf);
		if (!ret)
			str = g_strdup_printf("%s %s ver.%s", buf.sysname, buf.release,
					buf.version);
		else
			str = g_strdup("other");
	}
	return str; // например: "Linux 2.6.28 ver.1.4"
}
#endif

// копирование структуры proxy_param
void proxy_param_copy(proxy_param *proxy_to, const proxy_param *proxy_from) {
	if (!proxy_to || !proxy_from)
		return;
	memcpy(proxy_to, proxy_from, sizeof(proxy_param));
	proxy_to->host = g_strdup(proxy_from->host);
	proxy_to->username = g_strdup(proxy_from->username);
	proxy_to->password = g_strdup(proxy_from->password);
}
;

// очистка структуры proxy_param
void proxy_param_clean(proxy_param *proxy) {
	if (!proxy)
		return;
	if (proxy->host)
		g_free(proxy->host);
	//if(proxy->port)		g_free(proxy->port);
	if (proxy->username)
		g_free(proxy->username);
	if (proxy->password)
		g_free(proxy->password);
}
;

// удаление структуры proxy_param
void proxy_param_free(proxy_param *proxy) {
	proxy_param_clean(proxy);
	g_free(proxy);
}
;

// удаление структуры connect_param
void net_connect_free(connect_param *conn) {
	if (!conn)
		return;
	//if(connect->url)	 parsed_url_free(connect->url);
	//if(connect->proxy) proxy_param_free(connect->proxy);
	if (conn->curl)
		curl_easy_cleanup(conn->curl);	// Завершает libcurl easy сессию 
	if (conn->url_addr)
		g_free(conn->url_addr);
	if (conn->url_refer)
		g_free(conn->url_refer);
	if (conn->user_agent)
		g_free(conn->user_agent);
	if (conn->cookie)
		g_free(conn->cookie);
	g_free(conn);
}

// удаление структуры http_header
void http_header_free(http_header *header) {
	if (!header)
		return;
	if (header->proto)
		g_free(header->proto);
	if (header->ret_code_bin_str)
		g_free(header->ret_code_bin_str);
	if (header->ret_code_str)
		g_free(header->ret_code_str);
	if (header->server)
		g_free(header->server);
	if (header->connection)
		g_free(header->connection);
	if (header->conn_type)
		g_free(header->conn_type);
	if (header->keep_alive)
		g_free(header->keep_alive);
	if (header->transfer_encoding)
		g_free(header->accept_encoding);
	if (header->accept_encoding)
		g_free(header->accept_encoding);
	if (header->language)
		g_free(header->language);
	if (header->cookie)
		g_free(header->cookie);
	// websocket
	if (header->upgrade)
		g_free(header->upgrade);
	if (header->web_socket_ver)
		g_free(header->web_socket_ver);
	if (header->web_socket_key)
		g_free(header->web_socket_key);
	if (header->web_socket_accept)
		g_free(header->web_socket_accept);
	// custom
	if (header->additional)
		g_free(header->additional);
	g_free(header);

}

// удаление структуры http_page
void http_page_free(http_page *page) {
	if (!page)
		return;
	if (page->raw_header)
		g_free(page->raw_header);
	if (page->raw_body)
		g_free(page->raw_body);
	if (page->header)
		http_header_free(page->header);
	g_free(page);
}

/*/ удаление структуры json_data
 void json_data_free(json_data *json)
 {
 int i;
 if(!json) return;
 if(json->name)
 for(i=0;i<json->count;i++)
 g_free(json->name[i]);
 if(json->value)
 for(i=0;i<json->count;i++)
 g_free(json->value[i]);
 if(json->name)		g_free(json->name);
 if(json->value)		g_free(json->value);
 g_free(json);
 }*/

// узнать номер порта по умолчанию для выбранного типа прокси-сервера
int get_default_proxy_port(int type) {
	int port;
	if (type == PROXY_HTTP)
		port = 3128;	//или 8080
	else
		port = 1080;	//if(type==PROXY_SOCKS4 || type==PROXY_SOCKS5)
	return port;
}

// мьютекс для защиты от одновременного выполнения из нескольких потоков curl_easy_strerror()
G_LOCK_DEFINE (my_curl_easy_strerror);

// получить строку с описанием кода ошибки, строка требует удаления через g_free()
char *my_curl_easy_strerror(int error) {
	G_LOCK (my_curl_easy_strerror);
	const char *err_str = NULL;
	switch (error) {
	case CURLE_UNSUPPORTED_PROTOCOL:
		err_str = _("Unsupported protocol");//"Unknown protocol was specified"; - glib.po/glib.mo
	case CURLE_URL_MALFORMAT:
		err_str = _("URL using bad/illegal format or missing URL");
	case CURLE_NOT_BUILT_IN:
		err_str = _("A requested feature, protocol or option was not found");
	case CURLE_COULDNT_RESOLVE_PROXY:
		err_str = _("Couldn't resolve proxy name");
	case CURLE_COULDNT_RESOLVE_HOST:
		err_str = _("Couldn't resolve host name");
	case CURLE_COULDNT_CONNECT:
		err_str = _("Couldn't connect to server");// Не удалось подключиться к серверу
	case CURLE_REMOTE_ACCESS_DENIED:
		err_str = _("Access denied to remote resource");
	case CURLE_OUT_OF_MEMORY:
		err_str = g_dgettext("glib20", "out of memory");//"закончилась память"
	case CURLE_OPERATION_TIMEDOUT:
		err_str = g_dgettext("glib20", "Timeout was reached");//"Время ожидания истекло"
	case CURLE_SEND_ERROR:
		err_str = _("Failed sending data to the peer");
	case CURLE_RECV_ERROR:
		err_str = _("Failure when receiving data from the peer");
	}
	// возвращаем текст ошибки из стандартной функции - только на англ. языке
	if (!err_str)
		err_str = curl_easy_strerror((CURLcode) error);
	char *ret_err_str = g_strdup(err_str);
	G_UNLOCK(my_curl_easy_strerror);

	return ret_err_str;
}

/*
 // узнать номер порта по умолчанию для выбранного протокола
 static int get_default_port(int proto)
 {
 int port = 0;
 if(proto==PROTOCOL_HTTP)		port =80;//или 3128
 else if(proto==PROTOCOL_FTP)	port =21;
 return port;
 }

 // создать сетевое соединение
 // url_addr - адрес страницы содержимое которй нужно получить
 // url_refer - ссылающаяся страница, м.б. = NULL (только для HTTP протокола)
 // proxy - параметры прокси-сервера, если не нужен м.б. = NULL
 // err_code - код ошибки, если не нужен м.б. = NULL
 // return строка, которая требует удаления, или NULL
 connect_param* net_connect(char *url_addr,proxy_param *proxy, int *err_code)
 {
 connect_param *conn = NULL;
 parsed_url *url;
 guchar connect_type;
 SOCKET sockfd;
 // распарсивание адреса
 url = parse_url((const char *)url_addr);
 if(!url) {if(err_code) *err_code = eStatusBadAddr;return NULL;}
 // тип соединения;
 if(!strcmp(url->scheme,"http"))
 connect_type = PROTOCOL_HTTP;
 else if(!strcmp(url->scheme,"file"))
 connect_type = PROTOCOL_FILE;
 else if(!strcmp(url->scheme,"ftp"))
 connect_type = PROTOCOL_FTP;
 else
 {
 parsed_url_free(url);
 if(err_code) *err_code = eStatusUnsupportProto;
 return NULL;
 }
 
 // устанавливаем соединение
 {
 int ret_err;
 int port = 0;//(url->port)?atoi(url->port):0;
 char *server_name;
 // определяем имя сервера
 if(proxy)
 server_name = proxy->host;
 else
 server_name = url->host;
 // если порт не задан, то берём по умолчанию
 if(!url->port)
 url->port = get_default_port(connect_type);
 if(proxy && !proxy->port)	
 proxy->port = get_default_proxy_port(proxy->type);
 // выбираем порт для тек. соединения
 if(proxy)	port = proxy->port;
 else		port = url->port;
 // установка соединения
 switch(connect_type)
 {
 case PROTOCOL_FILE: // локальный файл
 sockfd = open(url->path,_O_RDONLY | O_RAW);// _O_RDWR, O_RAW = _O_BINARY
 if((int)sockfd<0)
 {
 //errno_t errn_code = errno;
 switch(errno) 
 {
 case EPERM: // нет прав доступа
 case EACCES:// наверное то же самое
 if(err_code) *err_code = eStatusNoPermit;break;
 case ENOENT: // нет такого файла
 default:// остальные ошибки
 if(err_code) *err_code = eStatusNoRes;break;
 }
 return NULL;
 }
 break;
 default: // сетевое соединение
 ret_err = s_connect(server_name,port,&sockfd);
 if(ret_err!=eStatusOK)// варианты: eStatusOK, eStatusNoDNSEntry, eStatusNetError, eStatusNotResponding
 {
 if(err_code) *err_code = ret_err;
 return NULL;
 }
 }
 }
 // создаём структуру с соединеним и заполняем её
 conn = g_malloc0(sizeof(*conn));
 conn->type = connect_type;
 conn->url = url;
 conn->sock = sockfd;
 if(proxy) conn->proxy = proxy;
 if(err_code) *err_code = eStatusOK;
 return conn;
 }


 // закрыть сетевое соединение
 // conn - параметры установленного соединения
 void net_close(connect_param *conn)
 {
 if(conn->type==PROTOCOL_FILE)
 close(conn->sock);// закрыть файл
 else
 s_close(conn->sock);// закрыть сокет
 conn->sock = 0;
 }



 // получить данные
 // conn - параметры установленного соединения
 // data_len - число принятых данных
 // err_code - код ошибки, если не нужен м.б. = NULL
 // return строка, которая требует удаления, или NULL в случае ошибки
 char* net_get_data(connect_param *conn, int *data_len, int *err_code)
 {
 int buf_len = 0;// сколько считано
 char *buf = NULL;
 *data_len = 0;// пока ничего не получили
 if(!conn)
 {
 if(err_code) *err_code = eStatusErr;
 return NULL;
 }
 switch(conn->type)
 {
 case PROTOCOL_FILE: // получаем из локального файла
 {
 // через seek узнавать buf_len_max!
 int n_byte;
 int buf_len_max = 32768;// максимальный блок, считанный за раз
 buf = g_malloc0(buf_len_max);
 if(!buf)// не удалось выделить память
 {
 if(err_code) *err_code = eStatusNoMem;
 return NULL;
 }
 do 
 {
 buf = g_realloc(buf,buf_len+buf_len_max);
 n_byte = read(conn->sock,buf,buf_len_max);
 if(n_byte>0)
 buf_len+=n_byte;
 }
 while(n_byte>0);
 buf = g_realloc(buf,buf_len);
 }
 break;
 case PROTOCOL_HTTP: // получаем по http протоколу
 buf = http_get_content(conn,NULL,&buf_len,err_code);
 break;
 case PROTOCOL_FTP: // получаем по ftp протоколу
 break;
 }
 // ничего не считали
 if(buf_len<=0)
 {
 g_free(buf);
 buf = NULL;
 }
 return buf;
 }
 */

// вызывается один раз в самом начале работы
int net_init() {
	CURLcode ret = curl_global_init(CURL_GLOBAL_DEFAULT);
	return (int) ret;
}

// вызывается один раз в самом конце работы
void net_destroy() {
	curl_global_cleanup();
}

// timeout при соединении, в сек.
static int t_timeout_connect = 4;
//  timeout при выполнении функций, в сек.
static int t_timeout = 7;

// установить новые timeout-ы
// t_timeout_connect_new - timeout при соединении, в сек.
// t_timeout_new - timeout при выполнении функций, в сек.
int net_set_timeout(int t_timeout_connect_new, int t_timeout_new) {
	t_timeout_connect = t_timeout_connect_new;
	t_timeout = t_timeout_new;
	return 1;
}

// узнать timeout при соединении, в сек.
int net_get_timeout_connect() {
	return t_timeout_connect;
}
// узнать timeout при выполнении функций, в сек.
int net_get_timeout() {
	return t_timeout;
}

// создать сетевое соединение
// url_addr - адрес страницы содержимое которой нужно считать/изменить (пример: http://example.com)
// proxy - параметры прокси-сервера, если не нужен м.б. = NULL
// err_code - код ошибки, если не нужен м.б. = NULL
/* коды ошибок: -1 недопустимый параметр(моя ошибка)
 CURLE_UNSUPPORTED_PROTOCOL(1) - 
 CURLE_COULDNT_CONNECT(7) - авторизация прокси SOCKS5 требуется или пароль не подошел
 прокси или url_addr не отвечает
 CURLE_RECV_ERROR(56) - авторизация прокси HTTP требуется или пароль не подошел
 CURLE_COULDNT_RESOLVE_PROXY(5) - DNS не нашёл ip прокcи по адресу
 CURLE_COULDNT_RESOLVE_HOST(6) - DNS не нашёл ip url_addr по адресу
 */
// return строка, которая требует удаления, или NULL
connect_param* net_connect(char *url_addr, proxy_param *proxy, int *err_code) {
	connect_param *conn = NULL;
	//guchar			 connect_type;
	//parsed_url		*url;
	CURL *curl = curl_easy_init();
	CURLcode ret;
	if (!curl)
		net_func_exit(-1);	//{if(err_code) *err_code = -1;return NULL;}
	/*/ распарсивание адреса
	 url = parse_url((const char *)url_addr);
	 if(!url) {if(err_code) *err_code = eStatusBadAddr;return NULL;}
	 // определяем тип соединения;
	 if(!strcmp(url->scheme,"http"))
	 connect_type = PROTOCOL_HTTP;
	 else if(!strcmp(url->scheme,"file"))
	 connect_type = PROTOCOL_FILE;
	 else if(!strcmp(url->scheme,"ftp"))
	 connect_type = PROTOCOL_FTP;
	 else
	 {
	 parsed_url_free(url);
	 net_func_exit_if_err(CURLE_UNSUPPORTED_PROTOCOL);
	 return NULL;
	 }*/
	// установка соединения
	ret = curl_easy_setopt(curl, CURLOPT_VERBOSE, 0);// режим отладки (0 или 1)
	ret = curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT,
			net_get_timeout_connect());	// timeout при соединении, в сек.
	ret = curl_easy_setopt(curl, CURLOPT_TIMEOUT, net_get_timeout());// timeout при выполнении функций, в сек.
	ret = curl_easy_setopt(curl, CURLOPT_URL, url_addr);// адрес для соединения
	ret = curl_easy_setopt(curl, CURLOPT_CONNECT_ONLY, 1L);	// только соединиться
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
	ret = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);// удалённый сервер не будет проверять наш сертификат. В противном случае необходимо этот самый сертификат послать.
	ret = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
	ret = curl_easy_perform(curl);
	if (ret != CURLE_OK)
		curl_easy_cleanup(curl);
	net_func_exit_if_err(ret);
	ret = curl_easy_setopt(curl, CURLOPT_CONNECT_ONLY, 0L);	// отключить режим - только соединение
	if (ret != CURLE_OK)
		curl_easy_cleanup(curl);
	net_func_exit_if_err(ret);//if(ret!=CURLE_OK)	{if(err_code) *err_code = ret;return NULL;}

	// создаём структуру с соединеним и заполняем её
	conn = (connect_param *) g_malloc0(sizeof(*conn));
	//conn->type = connect_type;
	//conn->url = url;
	//conn->sock = sockfd;
	conn->curl = curl;
	//if(proxy) conn->proxy = proxy;
	if (err_code)
		*err_code = 0;
	return conn;
}

// добавить к одному массиву другой
// memory - массив с данными, куда копировать
// add_mem - массив с данными, откуда копировать
// memory_len - размер массива memory
// add_size - размер массива add_mem
static int add_mem_data(char **memory, size_t *memory_len, char *add_mem,
		size_t add_size) {
	*memory = (char*) g_realloc(*memory, *memory_len + add_size + 1);
	//out of memory!
	if (*memory == NULL) {
		//printf("not enough memory (realloc returned NULL)\n");
		return 0;
	}
	memcpy((*memory + *memory_len), add_mem, add_size);
	// увеличиваем число принятых байт
	*memory_len += add_size;
	// последний байт обнуляем
	*(*memory + *memory_len) = 0;
	return add_size;
}
// запись полученного заголовка по сети или из файла в память
static size_t WriteHttpMemoryHeadCallback(void *contents, size_t size,
		size_t nmemb, http_page *page)	//void *user_data)
		{
	if (!page)
		return 0;
	// добавить к заголовку принятые данные
	return add_mem_data(&page->raw_header, &page->header_len, (char*) contents,
			size * nmemb);
}

/*static size_t ReadHttpMemoryCallback(void *contents, size_t size, size_t nmemb, http_page *page)//void *user_data)
 {
 return size * nmemb;
 }*/

// запись полученных данных по сети или из файла в память
static size_t WriteHttpMemoryCallback(void *contents, size_t size, size_t nmemb,
		http_page *page)	//void *user_data)
		{
	if (!page)
		return 0;
	// добавить к телу документа принятые данные
	return add_mem_data(&page->raw_body, &page->body_len, (char*) contents,
			size * nmemb);
	/*char **memory;
	 size_t *len;
	 size_t realsize = size * nmemb;
	 if(!page) return 0;
	 // последняя посылка заголовка
	 if(!page->body_len && realsize==2 && !strncmp(contents,"\r\n",2))
	 {
	 page->body = g_malloc0(1);
	 return realsize;
	 }
	 // получаем заголовок
	 if(!page->body) 
	 {
	 len = &page->header_len;
	 memory = &page->header;
	 }
	 // получаем тело страницы
	 else
	 {
	 len = &page->body_len;
	 memory = &page->body;
	 }
	 *memory = g_realloc(*memory, *len + realsize + 1);
	 //out of memory!
	 if (*memory == NULL) 
	 {
	 printf("not enough memory (realloc returned NULL)\n");
	 return 0;
	 }
	 memcpy((*memory+*len), contents, realsize);
	 // увеличиваем число принятых байт
	 *len += realsize;
	 // последний байт обнуляем
	 *(*memory+*len) = 0;
	 return realsize;*/
}

// установка опций для http соединения
static CURLcode set_http_opt(connect_param *conn, http_page *page) {
	CURLcode ret;
	CURL *curl;
	if (!conn || !conn->curl)
		return (CURLcode)(-1);
	curl = conn->curl;
	// установка параметров
	//curl_easy_reset(curl);
	ret = curl_easy_setopt(curl, CURLOPT_HEADER, 0L);// не получать заголовок в теле документа
	//для индикатора прогресса(curlgtk.c) ->	//ret = curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);ret = curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, my_progress_func);ret = curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, buf);
	ret = curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "gzip, deflate");	// разрешаем приём сжатых данных
	// callback функции на приём данных
	ret = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
			WriteHttpMemoryCallback);	// обратный вызов для чтения данных
	ret = curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION,
			WriteHttpMemoryHeadCallback);// обратный вызов для чтения заголовка
	// передача параметра в callback функцию на приём данных
	ret = curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) page);
	ret = curl_easy_setopt(curl, CURLOPT_HEADERDATA, (void *) page);
	if (conn->url_addr)
		ret = curl_easy_setopt(curl, CURLOPT_URL, conn->url_addr);
	if (conn->url_refer)
		ret = curl_easy_setopt(curl, CURLOPT_REFERER, conn->url_refer);
	if (conn->user_agent)
		ret = curl_easy_setopt(curl, CURLOPT_USERAGENT, conn->user_agent);
	if (conn->cookie)
		ret = curl_easy_setopt(curl, CURLOPT_COOKIE, conn->cookie);
	return ret;
}

// получить данные по http протоколу
// conn - параметры установленного соединения
// err_code - код ошибки, если не нужен м.б. = NULL
// return структура с данными страницы, которая требует удаления, или NULL в случае ошибки
http_page* http_get_page(connect_param *conn, int *err_code) {
	CURLcode ret;
	CURL *curl;
	http_page *page = (http_page*) g_malloc0(sizeof(*page));
	// установка опций для http соединения
	ret = set_http_opt(conn, page);
	if (ret != CURLE_OK) {
		g_free(page);
		net_func_exit(ret);
	}
	curl = conn->curl;
	// установка дополнительных параметров
	ret = curl_easy_setopt(curl, CURLOPT_HTTPGET, 1);	// GET запрос
	ret = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);// удалённый сервер не будет проверять наш сертификат. В противном случае необходимо этот самый сертификат послать.
	ret = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
	// запуск получения страницы (заполнение page)
	ret = curl_easy_perform(curl);
	net_func_exit_if_err(ret);
	// распарсить принятый http-заголовок
	page->header = http_parse_header(page);
	return page;
}

// отправить данные по http протоколу
// conn - параметры установленного соединения
// post_data - данные для передачи на сервер, пример: "name=daniel&project=curl"
// post_data_len - длина данных для передачи, если = -1, передаваемая строка заканчивается нулём.
// err_code - код ошибки, если не нужен = NULL
// return структура с данными страницы, которая требует удаления, или NULL в случае ошибки
http_page* http_post_page(connect_param *conn, char *post_data,
		size_t post_data_len, int *err_code) {
	CURLcode ret;
	CURL *curl;
	http_page *page = (http_page*) g_malloc0(sizeof(*page));
	// установка опций для http соединения
	ret = set_http_opt(conn, page);
	if (ret != CURLE_OK) {
		g_free(page);
		net_func_exit(ret);
	}
	curl = conn->curl;
	// установка дополнительных параметров
	//ret = curl_easy_setopt(curl, CURLOPT_HTTPPOST, &post);// POST запрос - передача структуры curl_httppost
	ret = curl_easy_setopt(curl, CURLOPT_POST, 1);// POST запрос - необязательно, если будет CURLOPT_POSTFIELDS
	if (post_data)
		ret = curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);// данные для POST запроса
	if (post_data_len >= 0)
		ret = curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE,
				(long) post_data_len); // если надо много передать: CURLOPT_POSTFIELDSIZE_LARGE
	// запуск получения страницы (заполнение page)
	ret = curl_easy_perform(curl);
	net_func_exit_if_err(ret);
	// распарсить принятый http-заголовок
	page->header = http_parse_header(page);
	return page;
}

// распарсить принятый http-заголовок (из page->raw_header)
// page - полученная http-страница
// return структура с данными страницы, которая требует удаления, или NULL в случае ошибки
http_header* http_parse_header(http_page *page) {
	char **str_lines;	// строки с ответами
	if (!page || !page->raw_header || page->header_len < 1)
		return NULL;	// нечего парсить
	str_lines = g_strsplit(page->raw_header, "\r\n", -1);
	if (str_lines) {
		int i = 0;
		http_header *header = (http_header*) g_malloc0(sizeof(http_header));
		while (str_lines[i]) {
			if (!i || (header->ret_code == 100 && i == 2))// первая строка ответа
					{
				char **str_one_line = g_strsplit(str_lines[i], " ", 3);	// на 3 части разбиваем строку, в третьей подстроке пробелы не учитываются
				if (header->proto) {
					g_free(header->proto);
					header->proto = NULL;
				}
				if (header->ret_code_str) {
					g_free(header->ret_code_str);
					header->ret_code_str = NULL;
				}
				if (str_one_line) {
					int j = 0;
					while (str_one_line[j]) {
						if (j == 0)
							header->proto = g_strdup(str_one_line[j]);
						if (j == 1) {
							header->ret_code = atoi(str_one_line[j]);
							header->ret_code_bin_str = g_strdup(
									str_one_line[j]);
						}
						if (j == 2)
							header->ret_code_str = g_strdup(str_one_line[j]);
						j++;
					}
				}
				g_strfreev(str_one_line);
			} else	// остальные строки
			{
				char **str_one_line = g_strsplit(str_lines[i], ": ", 2);// на 2 части разбиваем строку
				if (str_one_line) {
					if (str_one_line[0] && str_one_line[1]) {
						if (!strcasecmp(str_one_line[0], "Content-Length"))
							header->length = atoi(str_one_line[1]);
						else if (!strcasecmp(str_one_line[0], "Date"))
							header->utc_time = (long) curl_getdate(
									str_one_line[1], NULL);
						else if (!strcasecmp(str_one_line[0], "Server"))
							header->server = g_strdup(str_one_line[1]);
						else if (!strcasecmp(str_one_line[0], "Connection"))
							header->connection = g_strdup(str_one_line[1]);
						else if (!strcasecmp(str_one_line[0], "Content-Type"))
							header->conn_type = g_strdup(str_one_line[1]);
						else if (!strcasecmp(str_one_line[0], "Keep-Alive"))
							header->keep_alive = g_strdup(str_one_line[1]);
						else if (!strcasecmp(str_one_line[0],
								"Transfer-Encoding"))
							header->transfer_encoding = g_strdup(
									str_one_line[1]);
						else if (!strcasecmp(str_one_line[0],
								"Accept-Encoding"))
							header->accept_encoding = g_strdup(str_one_line[1]);
						else if (!strcasecmp(str_one_line[0],
								"Content-Language"))
							header->language = g_strdup(str_one_line[1]);
						else if (!strcasecmp(str_one_line[0], "Set-Cookie"))
							header->cookie = g_strdup(str_one_line[1]);
						// для WebSocket
						else if (!strcasecmp(str_one_line[0], "Upgrade"))
							header->upgrade = g_strdup(str_one_line[1]);
						else if (!strcasecmp(str_one_line[0],
								"Sec-WebSocket-Version"))
							header->web_socket_ver = g_strdup(str_one_line[1]);
						else if (!strcasecmp(str_one_line[0],
								"Sec-WebSocket-Key"))
							header->web_socket_key = g_strdup(str_one_line[1]);
						else if (!strcasecmp(str_one_line[0],
								"Sec-WebSocket-Accept"))
							header->web_socket_accept = g_strdup(
									str_one_line[1]);
						// custom
						else if (!strcasecmp(str_one_line[0], "Additional"))
							header->web_socket_accept = g_strdup(
									str_one_line[1]);
					}
					g_strfreev(str_one_line);
				}
			}

			i++;
		};
		g_strfreev(str_lines);
		return header;
	}
	return NULL;
}

/*/ получить дату в формате Fri, 17-Apr-2013 07:34:21 GMT
 static char* get_text_date_time(time_t *time)
 {
 gchar *wday_str = NULL;
 gchar *buf = (gchar*)g_malloc0(60);
 struct tm *tm_gmt = gmtime(time);
 switch(tm_gmt->tm_wday)
 {
 case 0: wday_str="Sun";break;
 case 1: wday_str="Mon";break;
 case 2: wday_str="Tue";break;
 case 3: wday_str="Wed";break;
 case 4: wday_str="Thu";break;
 case 5: wday_str="Fri";break;
 case 6: wday_str="Sat";break;
 }
 if(wday_str)
 strcpy(buf,wday_str);
 strftime(buf+strlen(wday_str),60,", %d-%b-%Y %X GMT",tm_gmt);
 return buf;
 }*/

/*/ получить дату в формате Fri, 17-Apr-2013 07:34:21 GMT
 // тек. дата со сдвигом shift_day
 static char* get_text_date_time_sh(int shift_day)
 {
 time_t rawtime;
 time (&rawtime);
 rawtime+=shift_day*24*3600;// сдвиг переводим в секунды
 return get_text_date_time(&rawtime);
 }*/

// получить куки-переменную по имени val, строка требует удаления
char* http_cookie_get(http_header *header, char *name) {
	gint i = 0;
	gchar *str_ret = NULL;
	gchar **all_cookies;
	if (!header || !header->cookie)
		return NULL;
	all_cookies = g_strsplit(header->cookie, ";", 0);
	if (all_cookies)
		while (all_cookies[i]) {
			gchar **one_cookies = g_strsplit(all_cookies[i], "=", 2);
			// нашли нужную строку в куках
			if (one_cookies[0] && one_cookies[1]
					&& !strcmp(one_cookies[0], name))
				str_ret = g_strdup(one_cookies[1]);
			g_strfreev(one_cookies);
			i++;
			if (str_ret)
				break;
		}
	g_strfreev(all_cookies);
	return str_ret;
}

// Отправка запроса по адресу url_addr
// получить json ответ вида {"status": ... , "text": ...} по http протоколу
// url_addr - адресная строка
// proxy - структура с настройками прокси-сервера, если не нужен прокси = NULL
// str_req - строка запроса для отправки на сервер
// err_code - код ошибки, если не нужен = NULL
// err_str - текстовая расшифровка кода ошибки, если не нужена = NULL
// return полученные данные из поля "text" типа JsonNode*, которые требует удаления, или NULL в случае ошибки
char* http_post_json(char *url_addr, proxy_param *proxy, char *str_req,
		int *err_code, char **err_code_str) {
	int err = 0;
	char *err_str = NULL;
	gchar *ret_val = NULL;
	connect_param *conn;
	http_page *page = NULL;
	conn = net_connect(url_addr, proxy, &err);
	while (conn) {
		//int post_data_len=0;// - длина данных для передачи, если = -1, передаваемая строка заканчивается нулём.
		conn->user_agent = g_strdup_printf(USER_AGENT, get_os_version());
		conn->url_addr = g_strdup_printf("%s", url_addr);
		page = http_post_page(conn, str_req, -1, &err);
		if (!page)
			break;
		if (!page->header) {
			err = -1;
			break;
		}
		// 100 Continue — сервер удовлетворён начальными сведениями о запросе, клиент может продолжать пересылать заголовки. Появился в HTTP/1.1.
		// 200 OK — успешный запрос.Если клиентом были запрошены какие - либо данные, то они находятся в заголовке и / или теле сообщения.Появился в HTTP / 1.0.
		if (page->header
				&& (page->header->ret_code != 200
						&& page->header->ret_code != 100)) {
			err = -1;
			err_str = g_strdup_printf("http response: %d (%s)",
					page->header->ret_code, page->header->ret_code_str);
			break;
		}
		// возвращаем ответ сервера, т.к. все проверки прошли успешно
		ret_val = g_strdup(page->raw_body);

		/*/ расшифровка ответа
		 {
		 GError *error = NULL;
		 JsonParser *parser = json_parser_new();
		 JsonReader *reader;
		 char *str_val;
		 // распарсить пришедшую строку
		 if(!json_parser_load_from_data(parser,page->raw_body,-1,&error))
		 {
		 err = -1;
		 err_str = g_strdup_printf(_("Unable to parse http response: %s"),(error)?error->message:_("Unknown error"));
		 g_error_free(error);
		 g_object_unref(parser);
		 break;
		 }
		 reader = json_reader_new (json_parser_get_root (parser));
		 // перейти в поле 'status'
		 if(!reader || !json_reader_read_member (reader, "status"))
		 {
		 const GError *error = json_reader_get_error (reader);
		 err_str = g_strdup_printf(_("Bad http response: %s"),(error)?error->message:_("Unknown error"));
		 err = -1;
		 g_object_unref(reader);
		 g_object_unref(parser);
		 break;
		 }
		 // прочитать поле 'status' c кодом ответа (OK или ERROR или FORBIDDEN)
		 str_val = (char*)json_reader_get_string_value(reader);
		 json_reader_end_member (reader);
		 if(!str_val)
		 {
		 err_str = g_strdup_printf(_("Bad http response"));
		 err = -1;
		 g_object_unref(reader);
		 g_object_unref(parser);
		 break;
		 }
		 // какая-то ошибка на сервере или запрещённое действие
		 if (!strcasecmp(str_val, "ERROR") || !strcasecmp(str_val, "FORBIDDEN"))
		 {
		 err_str = g_strdup_printf(str_val);
		 err = -1;
		 g_object_unref(reader);
		 g_object_unref(parser);
		 break;
		 }
		 // неизвестный ответ от сервера
		 if(strcasecmp(str_val,"OK"))// любой ответ, кроме OK, ERROR или FORBIDDEN
		 {
		 err_str = g_strdup_printf("%s, status=%s",_("Bad http response"),str_val);
		 err = -1;
		 g_object_unref(reader);
		 g_object_unref(parser);
		 break;
		 }
		 // возвращаем ответ сервера, т.к. все проверки прошли успешно
		 ret_val = g_strdup(page->raw_body);
		 g_object_unref(reader);
		 g_object_unref(parser);
		 }*/
		break;
	};
	if (err != CURLE_OK && !err_str)
		err_str = my_curl_easy_strerror(err);
	// завершение соединения и чистка памяти
	net_connect_free(conn);
	http_page_free(page);
	// передача параметров на выход 
	if (err_code)
		*err_code = err;
	if (err_code_str)
		*err_code_str = err_str;
	else
		g_free(err_str);
	return ret_val;
}

/*
 if(0)
 {
 int i, err_code;
 connect_param *conn;
 proxy_param proxy;
 char a[11];
 http_page *page=NULL;
 //proxy.type = PROXY_HTTP;
 proxy.type = PROXY_SOCKS5;
 proxy.host = "localhost";//"pc";
 proxy.host = "pc";
 proxy.port = 3128;//8080; - http
 proxy.port = 1080;// socks
 proxy.is_auth = TRUE;
 proxy.username = "sunny";
 proxy.password = "pass";
 
 for(i=0;i<10;i++)
 a[i]='a'+i;
 a[5]=0;
 
 //conn =  net_connect("http://workhorse",&proxy,&err_code);
 //conn =  net_connect("http://localhost",&proxy,&err_code); 
 //conn =  net_connect("http://217.19.112.4",&proxy,&err_code); 
 //conn =  net_connect("http://miass.ru",&proxy,&err_code); 
 conn =  net_connect("http://workhorse/site/parse.php?p=/adm/site",NULL,&err_code);
 //conn =  net_connect("file:///D:/Programm/openserver/domains/workhorse/app/WorkHorse/WorkHorse/src/ReadMe.txt",NULL,&err_code);
 
 
 if(conn)
 {
 conn->url_refer = "http://parent";
 conn->user_agent = "mozilla";
 conn->cookie = "fasfs=123&fs=456";
 page = http_post_page(conn,a,-1,&err_code);
 page = http_get_page(conn,&err_code);
 }
 //char *b = http_get_content("ftp://example.com",NULL,NULL);
 http_page_free(page);
 err_code=0;
 exit(0);
 }*/

#if __cplusplus
}  // End of the 'extern "C"' block
#endif

