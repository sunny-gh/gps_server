// sock_func.h

#ifndef _SOCK_FUNC_H_
#define _SOCK_FUNC_H_

// Make sure we can call this stuff from C++.
#if __cplusplus
extern "C" {
#endif

#define RESPONSE_TIMEOUT        13000	// время ожидания в милисекундах
#define MAX_REPLY_LEN			0xffff	// 0xffff=65535, 0x5B4 = 1460

#include <stdbool.h>

//#define BUF_SIZE	20480
// for Unix-like
#ifndef WIN32
#define WITH_POLL
#include <sys/socket.h>
#include <sys/socket.h> //for ntohs
//#include <netinet/in.h>  //for ntohs
#include <arpa/inet.h> // for inet_ntoa
#include <unistd.h> // for close
#include <fcntl.h>
#include <sys/poll.h>
#define closesocket	close
typedef void* LPVOID;
typedef long long LONGLONG;
typedef int SOCKET;
#ifndef SOCKET_ERROR
#define SOCKET_ERROR -1
#endif
#include <netdb.h>
#include <errno.h>
// for Windows
#else
//#include <winsock.h>
#include <winsock2.h>
#include <WS2TCPIP.H> 	//for typedef int socklen_t;
/* The following constants should be used for the second parameter of
 `shutdown'.  */
enum
{
	SHUT_RD = 0, /* No more receptions.  */
#define SHUT_RD		SHUT_RD
	SHUT_WR, /* No more transmissions.  */
#define SHUT_WR		SHUT_WR
	SHUT_RDWR /* No more receptions or transmissions.  */
#define SHUT_RDWR	SHUT_RDWR
};
#endif

// Коды ошибок и статусы выполняемых процессов
enum {
	eStatusOK = 0,         // This is the good status
	eStatusNetError,
	eStatusNotResponding,
	eStatusBadResponding,
	eStatusNoDNSEntry,
//eStatusDisabled,
};

// Возвращаем текст ошибки
char* get_err_text(int state);

// Иннициализация работы с сокетами
bool initSock(void);
// Окончание работы с сокетами
void closeSocks(void);

// определяем IP адрес по имени хоста
bool s_get_address(const char *lpszServerName, struct sockaddr_in *psa);

// соединение с сервером;
// name_server - адрес сайта ("localhost", "www.lavresearch.com")
// port - номер порта (ftp=21)
// sockfd - сюда запишется полученный номер сокета
// return: eStatusOK=0, а если ошибка - то другое
int s_connect(const char *name_server, int port, SOCKET *sockfd);

// отключение от сервера;
// sockfd - сюда номер сокета
// return: eStatusOK=0
int s_close(SOCKET sockfd);

// Проверка наличия данных на приём в сокете
// nSocket -  номер сокета. 
// timeout -  TimeOut в милисекундах. 
// return 1 - если дождались
// return zero if the time limit expired
// return SOCKET_ERROR [-1] if an error occurred
int s_pool(int nSocket, int timeout);

// Чтение из сокета до начала прихода заданной строки
// stop_str -строка, до которй будет продолжаться чтение
// del_stop_str - удалять ли строку для поиска в конце
// return: строка (если дождались конечных символов) или NULL, строка требует удаления через g_free()
char* s_get_next_str(SOCKET nSocket, int *dwLen, const char *stop_str,
		bool del_stop_str, int timeout);
// Чтение из сокета до конца строки
// return: строка (без "\r\n")
char* s_get_next_line(SOCKET nSocket, int *dwLen, int timeout);
// Чтение из сокета до конца строки (до первой встречи "\r\n")
// return: строка (c "\r\n")
char* s_get_next_line_rn(SOCKET nSocket, int *dwLen, int timeout);
char* s_get_next_line0(SOCKET nSocket, int *dwLen);

// Запись в сокет (с TimeOut-ом)
// return - число записанных байт
// timeout - в милисекундах
int s_send(SOCKET sock, unsigned char * buf, int bufsize, int timeout);

// Чтение из сокета заданного числа байт (с TimeOut-ом)
// return - число прочитанных байт
// timeout - в милисекундах
int s_recv(SOCKET sock, unsigned char *buf, int bufsize, int timeout);
//int sockrecvfrom(SOCKET sock,struct sockaddr_in *sin,unsigned char *buf, int bufsize, int timeout);

// Для функции poll()
#ifdef WITH_POLL
#include <poll.h>
#else
struct mypollfd
{
	SOCKET fd; /* file descriptor */
	short events; /* events to look for */
	short revents; /* events returned */
};
int mypoll(struct mypollfd *fds, unsigned int nfds, int timeout);
#ifndef POLLIN
#define POLLIN 1
#endif
#ifndef POLLOUT
#define POLLOUT 2
#endif
#ifndef POLLPRI
#define POLLPRI 4
#endif
#ifndef POLLERR
#define POLLERR 8
#endif
#ifndef POLLHUP
#define POLLHUP 16
#endif
#ifndef POLLNVAL
#define POLLNVAL 32
#endif
#define pollfd mypollfd
#define poll mypoll

#endif

// проверить сокет на валидность
int is_valid_socket(SOCKET sock);

#if __cplusplus
}  // End of the 'extern "C"' block
#endif

#endif // #ifndef _SOCK_FUNC_H_
