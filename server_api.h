// server_api.h : Протокол обмена с приложением
//

#ifndef _SERVER_API_H_
#define _SERVER_API_H_

// обработать api запрос и выдать ответ
// request_str - запрос в json формате
// client_ip_addr - ip адрес клиента
// client_port - порт клиента
// return: строка ответ на запрос в json-формате вида {"login":true,"param":xxx,...}, строка требует удаления через g_free()
char* api_prepare_answer(char *request_str, char *client_ip_addr,
		int client_port, int sock);

#endif	//_SERVER_API_H_
