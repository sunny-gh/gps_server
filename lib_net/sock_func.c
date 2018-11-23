// sock_func.c

//#ifdef WIN32
//#include "../glib_mini.h"
//#else
#include <malloc.h>
#include <glib.h>
#include <pthread.h>
//#endif
#ifdef _WIN32
#pragma comment(lib,"Ws2_32.lib")
#endif

#include <string.h>
#include <stdio.h>
#include "sock_func.h"

// Make sure we can call this stuff from C++.
#if __cplusplus
extern "C" {
#endif

// Локализация;
//#include <libintl.h>
//#define _(String) gettext (String)
//#ifndef _(String)
#define _A(String) (String)
//#endif

// Возвращаем текст ошибки, строка требует удаления
char* get_err_text(int state) {
	char *text;
	switch (state) {
	//Winsock Error
	case eStatusOK:
		text = g_strdup_printf("%s", _A("Ok"));
		break;
		//"Не удалось создать сокет"
	case eStatusNetError:
		text = g_strdup_printf("%s", _A("Net error"));
		break;
		//"Нет ответа" за запрос connect
	case eStatusNotResponding:
		text = g_strdup_printf("%s", _A("No Response"));
		break;
		//"Неверный ответ сервера"
	case eStatusBadResponding:
		text = g_strdup_printf("%s", _A("Invalid Response"));
		break;
		//"Нет ответа DNS сервера"
	case eStatusNoDNSEntry:
		text = g_strdup_printf("%s", _A("No DNS entry"));
		break; //text = g_strdup_printf("%s",_("DNS сервер не отвечает"));break;
		/*	case eStatusNotSupportProtocol:
		 text = g_strdup_printf("%s",_("Unsupported protocol"));break;// Неподдерживаемый протокол
		 case eStatusNotConnected:
		 text = g_strdup_printf("%s",_("Cannot open data connection"));break;// не удалось подключится к серверу по протоколу ftp, http и т.д.
		 case eStatusNotConnection_aborted:
		 text = g_strdup_printf("%s",_("Connection closed, transer aborted"));break;
		 case eStatusLoginFailure:
		 text = g_strdup_printf("%s",_("Not logged in. Login incorrect"));break;// непонятная ошибка, код ничему не соответствует (Unknown error)
		 case eStatusExtendedError:
		 text = g_strdup_printf("%s",_("Unknown error"));break;
		 case eStatusNoMemory:
		 text = g_strdup_printf("%s",_("No Memory"));break;// ошибка выделения памяти
		 case eStatusCmdErr:
		 text = g_strdup_printf("%s",_("Error of performing the last command"));break;// ошибка выполнения команды
		 */

	default:
		text = g_strdup_printf("%s", _A("Unknown error happen"));
		break; // этого не д.б.
	}
	return text;
}

// разделить строку вида: www.host/index.html на адрес сайта и имя страницы
// name_server - адрес сайта [память выделяется здесь] ("localhost", "www.lavresearch.com")
// addr - имя объекта [память выделяется здесь] ("index.html", "image.png")
// return: 1 - если успешно, иначе 0
int separate_address(char *str_url_in, char **name_server, char **addr) {
	int i = 0;
	gchar *str_url = g_strdup(str_url_in);
	int len = (int) strlen(str_url);
	for (i = 0; i < len - 1; i++) {
		if (*(str_url + i) == '/') {
			*(str_url + i) = 0;
			*name_server = g_strdup(str_url + 0);
			*(str_url + i) = '/';
			*addr = g_strdup(str_url + i);
			i = len + 10;
			break;
		}
	}

	// если не нашли '/', значит корневая папка
	if (i != (len + 10)) {
		*name_server = str_url;
		*addr = g_strdup("/");
	} else
		g_free(str_url);
	return 1;
}

#ifdef _WIN32
#include <windows.h>
// Initialize Windows sockets version 1.01
bool initSock(void)
{
	static BOOL first_success_start = FALSE;
	WSADATA WSAData;
	if(first_success_start)
	return TRUE;
	//if(!WSAStartup(0x202, &WSAData))
	if(!WSAStartup(0x101, &WSAData))
	first_success_start = TRUE;
	return first_success_start;
}
// for Unux-like
#else
bool initSock(void) {
	return true;
}
#endif

// Окончание работы с сокетами
void closeSocks(void) {
#ifdef _WIN32
	WSACleanup();
#endif
}

// для доступа к потоконебезопасной функции gethostbyname()
static pthread_mutex_t mutex_gethostbyname = PTHREAD_MUTEX_INITIALIZER;

// определяем IP адрес по имени хоста
bool s_get_address(const char *lpszServerName, struct sockaddr_in *psa) {
	struct hostent *phe;

	// если lpszServerName ip адрес, а не имя
#ifdef _WIN32
	unsigned long ulAddr = INADDR_NONE;
#else  // Unix-like
	in_addr_t ulAddr = INADDR_NONE;
#endif
	ulAddr = inet_addr(lpszServerName); //inet_pton(AF_INET, ); для win32

	if (ulAddr != INADDR_NONE) {
#ifdef _WIN32
		psa->sin_addr.S_un.S_addr = ulAddr;
#else  // Unix-like
		psa->sin_addr.s_addr = ulAddr;
#endif
		psa->sin_family = AF_INET;
		return TRUE;
	}

	// если lpszServerName не ip адрес, а имя
	pthread_mutex_lock(&mutex_gethostbyname);
	phe = gethostbyname(lpszServerName); //inet_addr();
	if (!phe) {
		pthread_mutex_unlock(&mutex_gethostbyname);
		return FALSE;
	}
	memcpy((char *) &psa->sin_addr, (char *) phe->h_addr,
			(size_t) phe->h_length);
	psa->sin_family = phe->h_addrtype;
	pthread_mutex_unlock(&mutex_gethostbyname);
	return TRUE;
}

// сделать сокет неблокирующим/блокирующим
// is_nonblock - (true) сделать неблокирующим / (false) блокирующим
static void set_sock_nonblock(SOCKET sockfd, bool is_nonblock) {
// for Windows
#ifdef _WIN32
	unsigned long arg = is_nonblock;
	ioctlsocket(sockfd, FIONBIO, &arg);
// for Unix-like
#else
	int arg = fcntl(sockfd, F_GETFL, NULL);
	if (is_nonblock)
		arg |= O_NONBLOCK;
	else
		arg |= ~O_NONBLOCK;
	fcntl(sockfd, F_SETFL, arg);
	// fcntl(*sockfd,F_SETFL,fcntl(*sockfd,F_GETFL) | ~O_NONBLOCK);
#endif
}

// соединение с сервером;
// name_server - адрес сайта ("localhost", "www.lavresearch.com")
// port - номер порта
// sockfd - сюда запишется полученный номер сокета
// timeout_ms - таймаут в милисекундах, -1 по умолчанию
// return: eStatusOK=0, а если ошибка - то другое
int s_connect(const char *server_name, int port, SOCKET *sockfd,
		int timeout_ms) {
	int err;
	struct sockaddr_in server;
	if (!server_name)
		return eStatusNoDNSEntry;
	memset(&server, 0, sizeof(struct sockaddr_in));
#if defined(__QNXNTO__)
	server.sin_len = sizeof(server);
#endif
	server.sin_family = AF_INET;
	server.sin_port = htons(port);
	// ищем IP по имени
	if (!s_get_address(server_name, &server))
		return eStatusNoDNSEntry;
	// IP есть, пытаемся соеденится
	//state = eProgressConnecting;
	// создаем сокет;

	if ((*sockfd = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
		return eStatusNetError;
	// установка соединения
	//if((err=connect(*sockfd,(struct sockaddr*)&server,sizeof(struct sockaddr_in))) == SOCKET_ERROR)
	//	return eStatusNotResponding;
	// делаем сокет неблокирующим
	if (timeout_ms != -1)
		set_sock_nonblock(*sockfd, true);
	// установка соединения
	if ((err = connect(*sockfd, (struct sockaddr*) &server,
			sizeof(struct sockaddr_in))) == SOCKET_ERROR) {
// for Windows
#ifdef WIN32
		int err = WSAGetLastError();
// for Unix-like
#else
		int err = errno;
#endif		
		// соединение уже установлено
		if (err == EISCONN)
			return eStatusOK;
		// если errno=EINPROGRESS, то сокет неблокирующий, соединение в процессе установки
		if (err != EINPROGRESS && err != EWOULDBLOCK) {
			//errno;
			//#define EADDRNOTAVAIL   249                /* Can't assign requested address */
			//#define ECONNREFUSED    261                /* Connection refused */
			s_close(*sockfd, timeout_ms);
			*sockfd = INVALID_SOCKET;
			return eStatusNotResponding;
		}
		int ret;
		// Wait for connection
		//ret = s_pool(*sockfd,timeout_ms);
		{
			struct pollfd fds;
			fds.fd = *sockfd;
			fds.events = POLLOUT | POLLIN;// ждём, когда в сокет можно писать или по нему пришли данные
			ret = poll(&fds, 1, timeout_ms);
			// если событие произошло, но не то
			if (ret > 0 && !(fds.revents & (POLLOUT | POLLIN)))
				ret = -1;
		}
		if (ret == 0)		// timeout
				{
			s_close(*sockfd, timeout_ms);
			*sockfd = INVALID_SOCKET;
			return eStatusNotResponding;
		}
		if (ret == -1)		// error
				{
			s_close(*sockfd, timeout_ms);
			*sockfd = 0;
			return eStatusNetError;
		}
		// определяем состояние сокета
		int so_error = -1;
		socklen_t optlen = 4;
#ifdef _WIN32 // for Windows
		if (getsockopt(*sockfd, SOL_SOCKET, SO_ERROR, (char*)&so_error, &optlen) == SOCKET_ERROR)
#else		// Unix-like
		if (getsockopt(*sockfd, SOL_SOCKET, SO_ERROR, &so_error, &optlen)
				== SOCKET_ERROR)
#endif	
				{
			//int err = errno;//EBADF = 9
			s_close(*sockfd, timeout_ms);
			*sockfd = INVALID_SOCKET;
			return eStatusNetError;
		}
		//printf("[sock] so_error=%d optlen=%d \n\n",so_error,optlen);
		if (so_error != 0 || !optlen)//#define ECONNREFUSED    261                /* Connection refused */
				{
			s_close(*sockfd, timeout_ms);
			*sockfd = INVALID_SOCKET;
			return eStatusNotResponding;
		}
		/*
		 struct timeval	tv;
		 fd_set			fds;
		 tv.tv_sec = 5;
		 tv.tv_usec = 0;
		 FD_ZERO(&fds);
		 FD_SET(*sockfd, &fds);
		 ret = select(*sockfd + 1, NULL, &fds, NULL, &tv);
		 if(!ret)// timeout
		 if (getsockopt(*sockfd, SOL_SOCKET, SO_ERROR, &ret, &len) == -1)
		 ;*/
	}
	// опять делаем сокет блокирующим
	if (timeout_ms != -1)
		set_sock_nonblock(*sockfd, false);
	// Соединение есть, выходим
	return eStatusOK;
}

// отключение от сервера;
// sockfd - сюда номер сокета
// timeout -  TimeOut в милисекундах
// return: eStatusOK=0, -1 err
int s_close(SOCKET sockfd, int timeout) {
	// закрываем соединение на чтение и запись
#ifdef WIN32
	// The shutdown function does not close the socket. Any resources attached to the socket will not be freed until closesocket is invoked.
	int cs = shutdown(sockfd, SD_BOTH);
	cs = closesocket(sockfd);
	return cs;
#else
	int cs = shutdown(sockfd, SHUT_RDWR);
#endif
	// если shutdown успешна, иначе соединения уже вероятно закрыто
	if (!cs) {
		// ждём закрытия соединения другой стороной
		cs = s_pool(sockfd, timeout);
		//cs = s_recv(sockfd, NULL, 0, timeout);
	}
	cs = closesocket(sockfd);
	return cs;
}

// Чтение из сокета до начала прихода заданной строки
// stop_str -строка, до которй будет продолжаться чтение
// del_stop_str - удалять ли строку для поиска в конце
// return: строка (если дождались конечных символов) или NULL, строка требует удаления через g_free()
char* s_get_next_str(SOCKET nSocket, int *dwLen, const char *stop_str,
		bool del_stop_str, int timeout) {
	gboolean bSuccess = FALSE;
	int nRecv = 0;		// число принятых символов
	int stop_str_len = (stop_str) ? strlen(stop_str) : 0;
	// если нечего искать
	if (!stop_str_len)
		return NULL;
	int lpszBuffer_len = 256;
	char *lpszBuffer = g_malloc(lpszBuffer_len);
	// принятая строка не будет больше, чем MAX_REPLY_LEN
	while (1)		//nRecv < MAX_REPLY_LEN)
	{
		// читаем 1 байт
		int ret = s_recv(nSocket, (unsigned char *) (lpszBuffer + nRecv), 1,
				timeout);
		//int ret = recv(nSocket,lpszBuffer+nRecv,1, 0);
		if (ret <= 0) {
			break;
		}
		nRecv += ret;
		//printf("%02x",*(lpszBuffer + nRecv));
		while ((nRecv + 1) >= lpszBuffer_len) {
			lpszBuffer_len *= 2;
			lpszBuffer = (char*) g_realloc(lpszBuffer, lpszBuffer_len);
		}
		// поиск требуемой строки
		if (nRecv >= stop_str_len) {
			// нашли требуемую строку
			if (!g_ascii_strncasecmp(lpszBuffer + nRecv - stop_str_len,
					stop_str, stop_str_len)) {
				bSuccess = TRUE;
				break;
			}
		}

	};
	// конец чтения
	if (bSuccess) {
		// удаляем искомую строку
		if (del_stop_str) {
			lpszBuffer[nRecv - stop_str_len] = '\0';
			if (dwLen)
				*dwLen = nRecv - stop_str_len;
		} else {
			lpszBuffer[nRecv] = '\0';
			if (dwLen)
				*dwLen = nRecv;
		}
		lpszBuffer = g_realloc(lpszBuffer, *dwLen + 1);
		return lpszBuffer;
	}
	// в случае ошибки или ненайденой строки
	if (dwLen)
		*dwLen = 0;
	g_free(lpszBuffer);
	return NULL;
}

// Чтение из сокета до конца строки (до первой встречи  "\r\n"="0d0a" или "\n"="0a")
// return: строка (без "\r\n") или NULL, если строка без "\r\n" на конце, строка требует удаления через g_free()
char* s_get_next_line(SOCKET nSocket, int *dwLen, int timeout) {
	gboolean bSuccess = FALSE;
	int nRecv = 0;		// число принятых символов
	int lpszBuffer_len = 256;
	//char *lpszBuffer = malloc(lpszBuffer_len);
	char *lpszBuffer = g_malloc0(lpszBuffer_len);
	// принятая строка не будет больше, чем MAX_REPLY_LEN
	while (lpszBuffer)		//nRecv < MAX_REPLY_LEN)
	{
		// читаем 1 байт
		int ret = s_recv(nSocket, (unsigned char *) (lpszBuffer + nRecv), 1,
				timeout);
		//int ret = recv(nSocket,lpszBuffer+nRecv,1, 0);
		if (ret <= 0) {
			break;
		}
		//printf("%02x",*(lpszBuffer + nRecv));
		while ((nRecv + 1) >= lpszBuffer_len) {
			lpszBuffer_len *= 2;
			lpszBuffer = (char*) g_realloc(lpszBuffer, lpszBuffer_len);
			//lpszBuffer=(char*)realloc(lpszBuffer,lpszBuffer_len);
		}
		if (nRecv > 0 && lpszBuffer[nRecv] == '\n')	// && lpszBuffer[nRecv - 1] == '\r')
				{
			bSuccess = TRUE;
			nRecv++;
			break;
		}
		//if (lpszBuffer[nRecv] != '\r')
		nRecv++;
	};
	//printf("\n");

	// конец чтения
	if (bSuccess) {
		// проверяем какой тип конца строки присутвует \r\n или \n
		int dlen = 0;
		if (nRecv > 0 && lpszBuffer[nRecv - 1] == '\n')	// && lpszBuffer[nRecv - 1] == '\r')
			dlen++;
		if (nRecv > 1 && lpszBuffer[nRecv - 2] == '\r')
			dlen++;
		// удаляем конец строки, если он есть, а он должен быть
		if (dlen)
			lpszBuffer[nRecv - dlen] = '\0';		// выстаявляем конец строки
		if (dwLen)
			*dwLen = nRecv - dlen;

		//lpszBuffer[nRecv-2] = '\0';// выстаявляем конец строки
		//if(dwLen) *dwLen = nRecv - 2;
		return lpszBuffer;
	}
	// в случае ошибки
	if (dwLen)
		*dwLen = 0;
	//free(lpszBuffer);
	g_free(lpszBuffer);
	return NULL;
}

// Чтение из сокета до конца строки (до первой встречи "\r\n")
// return: строка (c "\r\n") или NULL, если строка без "\r\n" на конце, строка требует удаления через g_free()
char* s_get_next_line_rn(SOCKET nSocket, int *dwLen, int timeout) {
	gboolean bSuccess = FALSE;
	int nRecv = 0;		// число принятых символов
	int lpszBuffer_len = 256;
	char *lpszBuffer = g_malloc(lpszBuffer_len);
	// принятая строка не будет больше, чем MAX_REPLY_LEN
	while (lpszBuffer)		//nRecv < MAX_REPLY_LEN)
	{
		// читаем 1 байт
		int ret = s_recv(nSocket, (unsigned char *) (lpszBuffer + nRecv), 1,
				timeout);
		//int ret = recv(nSocket,lpszBuffer+nRecv,1, 0);
		if (ret <= 0) {
			break;
		}
		//printf("%02x",*(lpszBuffer + nRecv));
		while ((nRecv + 2) >= lpszBuffer_len) {
			lpszBuffer_len *= 2;
			lpszBuffer = (char*) g_realloc(lpszBuffer, lpszBuffer_len);
		}
		if (nRecv > 0 && lpszBuffer[nRecv - 1] == '\r'
				&& lpszBuffer[nRecv] == '\n') {
			bSuccess = TRUE;
			nRecv++;
			break;
		}
		//if (lpszBuffer[nRecv] != '\r')
		nRecv++;
	};
	printf("\n");

	// конец чтения
	if (bSuccess) {
		lpszBuffer[nRecv] = '\0';		// выстаявляем конец строки
		if (dwLen)
			*dwLen = nRecv;
		return lpszBuffer;
	}
	// в случае ошибки
	if (dwLen)
		*dwLen = 0;
	g_free(lpszBuffer);
	return NULL;
}

// Чтение из сокета до конца строки (до первой встречи "\r\n")
// return: строка(вместе с "\r\n") или NULL, если строка без "\r\n" на конце, строка требует удаления через g_free()
char* s_get_next_line0(SOCKET nSocket, int *dwLen) {
	gboolean bSuccess = FALSE;
	int nRecv = 0;		// число принятых символов
#ifdef _WIN32
	int send_flags = 0;
#else // Unix Like
	// Если во время пеердачи получатель данных закроет сокет на той стороне,
	// то этой программе будет послан сигнал SIGPIPE и она завершится, если сигнал не перехватывать.
	// флаг MSG_NOSIGNAL не даёт посылать сигнал SIGPIPE, взамен send() вернёт -1 и в errno будет записано EPIPE(32).
	int send_flags = MSG_NOSIGNAL;
#endif
	int lpszBuffer_len = 3000;
	char *lpszBuffer = g_malloc(lpszBuffer_len);
	// принятая строка не будет больше, чем MAX_REPLY_LEN
	while (lpszBuffer)	//nRecv < MAX_REPLY_LEN)
	{
		int i, ret, n_part = 10;	// число частей для деления периода ожидания
		for (i = 0; i < n_part; i++) {
			ret = s_pool((int) nSocket, RESPONSE_TIMEOUT / n_part);
			// дождались данные
			if (ret > 0)
				break;
			// ошибка связи
			if (ret < 0)
				break;
			// timeout
			if (ret == 0) {
				ret = send((int) nSocket, (const char*) "1", 0, send_flags);
				if (ret < 0)
					break;
				else
					g_usleep(G_USEC_PER_SEC / 100);
			}
		}
		// есть данные во входном буфере
		if (ret > 0) {
			if (recv(nSocket, lpszBuffer + nRecv, 1, 0) <= 0) {
				//ERROR_FTP_TRANSFER_IN_PROGRESS
				break;
			}
			while ((nRecv + 1) >= lpszBuffer_len) {
				lpszBuffer_len *= 2;
				lpszBuffer = (char*) g_realloc(lpszBuffer, lpszBuffer_len);
			}

			if (*(lpszBuffer + nRecv) == '\n') {
				bSuccess = TRUE;
				break;
			}
			if (*(lpszBuffer + nRecv) != '\r')
				nRecv++;
		} else {
			//ERROR_INTERNET_TIMEOUT
			break;
		}
	}

	// конец чтения
	if (bSuccess) {
		lpszBuffer[nRecv] = '\0';
		if (dwLen)
			*dwLen = nRecv;	// - 1;
		return lpszBuffer;
	}
	// в случае ошибки
	if (dwLen)
		*dwLen = 0;
	g_free(lpszBuffer);
	return NULL;
}

// Запись в сокет (с TimeOut-ом)
// return - число записанных байт, или (-1 err или -2 timeout)
// timeout - в милисекундах
int s_send(SOCKET sock, unsigned char * buf, int bufsize, int timeout) {
	int sent = 0;
	int res;
#ifdef _WIN32
	int send_flags = 0;
#else // Unix Like
	// Если во время пеердачи получатель данных закроет сокет на той стороне,
	// то этой программе будет послан сигнал SIGPIPE и она завершится, если сигнал не перехватывать.
	// флаг MSG_NOSIGNAL не даёт посылать сигнал SIGPIPE, взамен send() вернёт -1 и в errno будет записано EPIPE(32).
	int send_flags = MSG_NOSIGNAL;
#endif

	struct pollfd fds;
	fds.fd = sock;
	fds.events = POLLOUT;	// | POLLNVAL | POLLHUP;// | POLLERR | POLLPRI;
	do {
		//if(timetoexit) return 0;
		res = poll(&fds, 1, timeout);
		//printf("[s_send] pool=%d fds.revents=%d (%x)!\n",res, fds.revents,fds.revents);
		if (res == 1 && fds.revents != POLLOUT)
			return -1;
		//if(res < 0 && (errno == EAGAIN || errno == EINTR)) continue;
		if (res < 1)
			break;	// 0-timeout, -1 ошибка
		// отправка данных
		res = send(sock, (const char *) (buf + sent), bufsize - sent,
				send_flags);
		if (res < 0) {
			printf("[s_send] bufsize=%d sent=%d res=%d errno=%d\n", bufsize,
					sent, res, errno);
			//if(errno == EAGAIN || errno == EINTR) continue;
			break;
		}
		sent += res;
		if (bufsize != sent)
			printf("[s_send] bufsize=%d sent=%d res=%d\n", bufsize, sent, res);
	} while (sent < bufsize);
	if (!sent && res < 1)
		return res;
	return sent;
}

// Чтение из сокета (с TimeOut-ом)
// return - число прочитанных байт (-1 err или -2 timeout)
// timeout - в милисекундах
int s_recv(SOCKET sock, unsigned char *buf, int bufsize, int timeout) {
	struct pollfd fds;
	int res;
//	int res_errno;

	//printf("sockrecvfrom\n");

	fds.fd = sock;
	fds.events = POLLIN;// | POLLNVAL | POLLHUP | POLLERR | POLLPRI;// | POLLRDNORM;//POLLOUT |
	res = poll(&fds, 1, timeout);
	//printf("s_recv: fds.revents=%d (%x)!\n", fds.revents,fds.revents);
	// т.к. POLLIN=(POLLRDNORM | POLLRDBAND), то может быть revents=POLLRDNORM
	if (res == 1 && !(fds.revents & POLLIN))
		return -1;
	//printf("poll=%d\n",res);
	if (!res)	// timeout
		return -2;
	if (res < 1) {
		return -1;
	}
	//printf("[s_recv] **** thread_id=%d before recv() buf=%x(%c) bufsize=%d\n",pthread_self(), buf, *buf, bufsize);
	res = recv(sock, (char*) buf, bufsize, 0);	//MSG_WAITALL
	//printf("[s_recv] **** thread_id=%d after recv() buf=%x(%c) bufsize=%d res=%d\n", pthread_self(), buf, *buf, bufsize, res);
	if (res <= 0) {	//EINTR=4  ENOENT=2 EINVAL=22 ECONNRESET=254
		printf("[s_recv] recv()=%d errno=%d\n", res, errno);
	}
	return res;
}

// Чтение из сокета не менее bufsize байт (с TimeOut-ом)
// return - число прочитанных байт (-1 err или -2 timeout)
// timeout - в милисекундах
int s_recv_full(SOCKET sock, unsigned char *buf, int bufsize, int timeout) {
	unsigned char *buf_cur = buf;
	int rest = bufsize;	// сколько байт осталось прочитать
	int ret = 0;
	do {
		ret = s_recv(sock, buf_cur, rest, timeout);
		if (ret > 0) {
			buf_cur += ret;
			rest -= bufsize;
		} else
			break;	// -2 timeout, -1 ошибка
	} while (rest > 0);
	// если ничего не считали
	if (rest == bufsize)
		return ret;
	return bufsize - rest;
}

// проверить сокет на валидность (существуемость)
bool is_valid_socket(SOCKET sock) {
	struct pollfd fds;
	fds.fd = sock;
	fds.events = POLLIN;
	// return: -1 err, 0 timeout, 1 дождались
	int count_desc = poll(&fds, 1, 0);
	// ошибка
	if (count_desc == -1)
		return false;
	// пришло событие с кодом ошибки
	if (count_desc > 0) {
		// признак разрыва соединения в Windows
		// в Windows на закрытом сокете fds.revents=POLLHUP, в Unix на закрытом сокете fds.events = POLLIN
		if (fds.revents & (POLLERR | POLLHUP | POLLNVAL))
			return false;
		// признак разрыва соединения в Unix (QNX)
		// в Windows на закрытом сокете res=0, в Unix res=-1
		char buf[2];
		int res = recv(sock, buf, 1, MSG_PEEK);	// MSG_PEEK 	Получать данные с начала очереди получения без удаления их из очереди.
		if (res < 0)
			return false;
		if (!res && (fds.revents & POLLIN))	// данные в буфере якобы есть, а считалось 0 байт
			return false;
	}
	return true;
}

// Проверка наличия данных на приём в сокете
// nSocket -  номер сокета.
// timeout -  TimeOut в милисекундах.
// [Specifying a negative value in timeout means an infinite timeout.]
// [Specifying a timeout of zero causes poll() to return immediately, even if no file descriptors are ready.]
// return 1 - если дождались
// return zero if the time limit expired
// return: >0, если можно считывать данные, 0 timeout, -1 ошибка(м.б. сокет закрыт)
int s_pool(SOCKET nSocket, int timeout) {
	struct pollfd fds;
	int res;
	fds.fd = nSocket;
	// POLLIN - пришли данные
	// POLLNVAL - закрыли сокет на своей стороне
	// POLLHUP - закрыли сокет на другой стороне (не работает!, приходит POLLIN и при последующем чтении возвращается 0 байт)
	fds.events = POLLIN;	// | | POLLNVAL | POLLHUP | POLLERR | POLLPRI
	res = poll(&fds, 1, timeout);

	//for win32>=Vista: WSAPoll(struct pollfd *fds, nfds_t nfds, int timeout);

	/*struct timespec timeout_ts;
	 / Блокируем сигнал SIGINT
	 sigset_t set, orig;
	 sigemptyset(&set);
	 sigaddset(&set, SIGINT);
	 sigemptyset(&orig);
	 pthread_sigmask(SIG_BLOCK, &set, &orig);
	 timeout_ts.tv_sec = timeout/1000;
	 timeout_ts.tv_nsec = (timeout%1000)*1000000;
	 //res = ppoll(&fds, 1, &timeout_ts, &orig);
	 printf("s_pool: fds.revents=%d (%x) timeout=%d:%d!\n", fds.revents,fds.revents,(int)timeout_ts.tv_sec,(int)timeout_ts.tv_nsec);
	 */

	// т.к. POLLIN=(POLLRDNORM | POLLRDBAND), то может быть revents=POLLRDNORM
	if (res == 1 && !(fds.revents & POLLIN))//if(res==1 && fds.revents!=POLLIN)
		return -1;
	return res;
}

// с использованием select-a
int s_pool_sel(int nSocket, int timeout) {
	int count_desc;
	fd_set fdsr;
	struct timeval tt;
	tt.tv_sec = 0;
	tt.tv_usec = timeout * 1000;	//in microseconds
	FD_ZERO(&fdsr);
	FD_SET(nSocket, &fdsr);
	//ограничение, не более FD_SETSIZE сокетов
	count_desc = select(nSocket + 1, &fdsr, NULL, NULL, &tt);
	return count_desc;
}

#ifndef WITH_POLL
#ifdef _WIN32
// timeout - в милисекундах, -1 ждать безконечно или пока сокет не закроется
// return: -1 err, 0 timeout, 1 дождались или сокет закрылся
int mypoll(struct mypollfd *fds, unsigned int nfds, int timeout)
{
	return WSAPoll((WSAPOLLFD*)fds, nfds, timeout);	// from Windows Vista
}
#else
int mypoll(struct mypollfd *fds, unsigned int nfds, int timeout) {
	fd_set readfd;
	fd_set writefd;
	fd_set oobfd;
	struct timeval tv;
	unsigned i;
	int num;
	SOCKET maxfd = 0;

	//printf("my_pool start\n");

	tv.tv_sec = timeout / 1000;
	tv.tv_usec = 1000 * (timeout % 1000);
	FD_ZERO(&readfd);
	FD_ZERO(&writefd);
	FD_ZERO(&oobfd);
	for (i = 0; i < nfds; i++) {
		if ((fds[i].events & POLLIN))
			FD_SET(fds[i].fd, &readfd);
		if ((fds[i].events & POLLOUT))
			FD_SET(fds[i].fd, &writefd);
		if ((fds[i].events & POLLPRI))
			FD_SET(fds[i].fd, &oobfd);
		fds[i].revents = 0;
		if (fds[i].fd > maxfd)
			maxfd = fds[i].fd;
	}
//	if(fds[0].events&POLLIN)
//		if((num = select((int)maxfd+1, &readfd, NULL, NULL, &tv)) < 1)
//			return num;
//	if(fds[0].events&POLLOUT)
//		if((num = select((int)maxfd+1, NULL, &writefd, NULL, &tv)) < 1)
//			return num;

	if ((num = select((int) maxfd + 1, &readfd, &writefd, &oobfd, &tv)) < 1)
		return num;
	for (i = 0; i < nfds; i++) {
		if (FD_ISSET(fds[i].fd, &readfd))
			fds[i].revents |= POLLIN;
		if (FD_ISSET(fds[i].fd, &writefd))
			fds[i].revents |= POLLOUT;
		if (FD_ISSET(fds[i].fd, &oobfd))
			fds[i].revents |= POLLPRI;
	}
	return num;
}
#endif	// #ifdef _WIN32
#endif	// #ifndef WITH_POLL

#if __cplusplus
}  // End of the 'extern "C"' block
#endif
