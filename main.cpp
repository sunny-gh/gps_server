// main.cpp : вместо демона для windows
//
#ifdef _WIN32

#include <string.h>
#include <glib.h>
#include "server_thread_ports.h"
#include "server_thread_dev.h"
#include "server_thread_api.h"
#include "base_lib.h"
#include "gps_server.h"
#include "daemon/log.h"

// только для Visual Studio (for VS2013 _MSC_VER=1800)
#if defined(_MSC_VER) //&& (_MSC_VER >= 1020)

#pragma comment(lib,"libglib-2.0-0.lib")
#pragma comment(lib,"libgobject-2.0-0.lib")
#pragma comment(lib,"libwinpthread-1.lib")
// для обработки json запросов
#pragma comment(lib,"libjson-glib-1.0-0.lib")
#pragma comment(lib,"zlib1.lib")

#pragma comment(lib,"libintl-8.lib")
// для сетевых протоколов http, ftp и т.д.
#pragma comment(lib,"libcurl-4.lib")

#endif //_MSC_VER

//$(MSBuildProjectDirectiry)/src;
/*/include;
 C:/msys32/mingw32/include/json-glib-1.0;
 C:\msys32\usr\local\pgsql\include;
 C:\msys32\mingw32\include\glib-2.0;
 C:\msys32\mingw32\lib\glib-2.0\include;
 $(VC_IncludePath);$(WindowsSDK_IncludePath);
 C:\msys32\mingw32\i686-w64-mingw32\include\sys;
 C:\msys32\mingw32\i686-w64-mingw32\include;
 C:\msys32\mingw32\include;
 */

// ожидание остановки потоков
void wait_stop_servers(void);

int main(int argc, char* argv[])
{
	log_init();
	init_work();
	//Sleep(50);
	//destroy_work();

	wait_stop_servers();
	return 0;
}

#endif
