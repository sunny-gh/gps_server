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
#include <errno.h> // определяется здесь чтобы исключить определение errno.h после переопределения в этом файле кодов ошибок несовместимых с errno.h в win32
//#define BUF_SIZE	20480
// for Unix-like
#ifndef _WIN32
#define WITH_POLL	
//#include <netdb.h>
//#include <netinet/in.h>
//#include <netinet/tcp.h>
//#include <sys/ioctl.h>
//#include <sys/types.h>
#include <sys/socket.h> //for ntohs
//#include <netinet/in.h>  //for ntohs
#include <arpa/inet.h> // for inet_ntoa
#include <unistd.h> // for close
#include <fcntl.h>
#include <sys/poll.h>
#include <sys/select.h>
#define closesocket	close
typedef void* LPVOID;
typedef long long LONGLONG;
typedef int SOCKET;
#define SOCKET_ERROR	-1	// for win32 =  (-1)
#define INVALID_SOCKET	-1	// for win32 =  (SOCKET)(~0)
#include <netdb.h>
// for Windows
#else

#if !defined(_WIN32_WINNT) 
#undef _WIN32_WINNT
#endif
#define  _WIN32_WINNT   0x0601 // for WSAPoll() - since 0x0600
//#endif
//#include <winsock.h>
//#include <winsock2.h>
#include <winsock2.h> // for inet_addr()
#include <WS2tcpip.h> 	//for typedef int socklen_t;

/* The following constants should be used for the second parameter of `shutdown'.  */
enum
{
	SHUT_RD = 0, /* No more receptions.  */
#define SHUT_RD		SHUT_RD
	SHUT_WR, /* No more transmissions.  */
#define SHUT_WR		SHUT_WR
	SHUT_RDWR /* No more receptions or transmissions.  */
#define SHUT_RDWR	SHUT_RDWR
};

// коды ошибок для errno = WSAGetLastError(); - несовместимы с <errno.h>
#undef  EWOULDBLOCK
#define EWOULDBLOCK             WSAEWOULDBLOCK
#undef  EINPROGRESS
#define EINPROGRESS             WSAEINPROGRESS
#undef  EALREADY                
#define EALREADY                WSAEALREADY
#undef  ENOTSOCK                
#define ENOTSOCK                WSAENOTSOCK
#undef  EDESTADDRREQ            
#define EDESTADDRREQ            WSAEDESTADDRREQ
#undef  EMSGSIZE                
#define EMSGSIZE                WSAEMSGSIZE
#undef  EPROTOTYPE              
#define EPROTOTYPE              WSAEPROTOTYPE
#undef  ENOPROTOOPT             
#define ENOPROTOOPT             WSAENOPROTOOPT
#undef  EPROTONOSUPPORT         
#define EPROTONOSUPPORT         WSAEPROTONOSUPPORT
#undef	ESOCKTNOSUPPORT         
#define ESOCKTNOSUPPORT         WSAESOCKTNOSUPPORT
#undef	EOPNOTSUPP              
#define EOPNOTSUPP              WSAEOPNOTSUPP
#undef	EPFNOSUPPORT            
#define EPFNOSUPPORT            WSAEPFNOSUPPORT
#undef  EAFNOSUPPORT            
#define EAFNOSUPPORT            WSAEAFNOSUPPORT
#undef  EADDRINUSE              
#define EADDRINUSE              WSAEADDRINUSE
#undef  EADDRNOTAVAIL           
#define EADDRNOTAVAIL           WSAEADDRNOTAVAIL
#undef  ENETDOWN                
#define ENETDOWN                WSAENETDOWN
#undef  ENETUNREACH             
#define ENETUNREACH             WSAENETUNREACH
#undef  ENETRESET               
#define ENETRESET               WSAENETRESET
#undef  ECONNABORTED            
#define ECONNABORTED            WSAECONNABORTED
#undef  ECONNRESET              
#define ECONNRESET              WSAECONNRESET
#undef  ENOBUFS                 
#define ENOBUFS                 WSAENOBUFS
#undef  EISCONN                 
#define EISCONN                 WSAEISCONN
#undef  ENOTCONN                
#define ENOTCONN                WSAENOTCONN
#undef  ESHUTDOWN               
#define ESHUTDOWN               WSAESHUTDOWN
#undef  ETOOMANYREFS            
#define ETOOMANYREFS            WSAETOOMANYREFS
#undef  ETIMEDOUT               
#define ETIMEDOUT               WSAETIMEDOUT
#undef  ECONNREFUSED            
#define ECONNREFUSED            WSAECONNREFUSED
#undef  ELOOP                   
#define ELOOP                   WSAELOOP
#undef  ENAMETOOLONG            
#define ENAMETOOLONG            WSAENAMETOOLONG
#undef  EHOSTDOWN               
#define EHOSTDOWN               WSAEHOSTDOWN
#undef  EHOSTUNREACH            
#define EHOSTUNREACH            WSAEHOSTUNREACH
#undef  ENOTEMPTY               
#define ENOTEMPTY               WSAENOTEMPTY
#undef  EPROCLIM                
#define EPROCLIM                WSAEPROCLIM
#undef  EUSERS                  
#define EUSERS                  WSAEUSERS
#undef  EDQUOT                  
#define EDQUOT                  WSAEDQUOT
#undef  ESTALE                  
#define ESTALE                  WSAESTALE
#undef  EREMOTE                 
#define EREMOTE                 WSAEREMOTE
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
// port - номер порта
// sockfd - сюда запишется полученный номер сокета
// timeout_ms - таймаут в милисекундах, -1 по умолчанию
// return: eStatusOK=0, а если ошибка - то другое
int s_connect(const char *server_name, int port, SOCKET *sockfd,
		int timeout_ms);

// отключение от сервера;
// sockfd - сюда номер сокета
// timeout -  TimeOut в милисекундах
// return: eStatusOK=0
int s_close(SOCKET sockfd, int timeout);

// Проверка наличия данных на приём в сокете
// nSocket -  номер сокета. 
// timeout -  TimeOut в милисекундах. 
// return 1 - если дождались
// return zero if the time limit expired
// return SOCKET_ERROR [-1] if an error occurred
int s_pool(SOCKET nSocket, int timeout);

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

// Чтение из сокета не менее bufsize байт (с TimeOut-ом)
// return - число прочитанных байт (-1 err или -2 timeout)
// timeout - в милисекундах
int s_recv_full(SOCKET sock, unsigned char *buf, int bufsize, int timeout);

// проверить сокет на валидность
bool is_valid_socket(SOCKET sock);

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

#if __cplusplus
}
  // End of the 'extern "C"' block
#endif

#endif // #ifndef _SOCK_FUNC_H_
