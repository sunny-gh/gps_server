// server_thread_ports.h: Основной поток сервера, ожидающий клиентов
//
#ifndef _SERVER_THREAD_PORTS_H
#define _SERVER_THREAD_PORTS_H

#include "lib_net/server_thread.h"

// номера портов для разных устройств
enum {
	PORT_HTTP = 80,			// HTTP сервер
	PORT_API = 8090,		// API - WebSocket протокол
	PORT_WIALON = 9001,		// Wialon ipc протокол
	PORT_OSMAND = 9002,		// Traccar web протокол
	PORT_TRACCAR = 9003,	// Traccar текстовый протокол
	PORT_GT06 = 9004,
	PORT_BABYWATCH = 9005,	// протокол часов baby watch Q90

	PORT_MAX = 9100
};

#endif	//_SERVER_THREAD_PORTS_H
