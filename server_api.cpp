// server_api.c : Протокол обмена с приложением
//

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <glib.h>
#include <json-glib/json-glib.h>
#include "base_lib.h"
//#include "crc16.h"
#include "my_time.h"
#include "server_api.h"
#include "server_thread_dev.h" // for get_protocol_num()
#include "zlib_util.h"
#include "lib_protocol/crc16.h"
#include "lib_protocol/protocol.h"
#include "lib_protocol/protocol_parse.h"

// типы запросов
enum {
	ACTION_NONE, ACTION_LOGIN,	// авторизация соединения
	ACTION_LOGOUT,	// деавторизация соединения
	ACTION_GET_DATA,	// получение данных
	ACTION_CMD,	// получение команды

//	ACTION_MONITOR,
	ACTION_COUNT	// число возможных типов запросов
};

// разобрать api запрос
// request_str - запрос в json формате
// return: тип запроса
static int api_parse_request(char *request_str, JsonParser *parser) {
	int ret = ACTION_NONE;
	if (!request_str)
		return ACTION_NONE;

	// расшифровка строки с запросом клиента в формате Json
	const char *action_str = NULL;
	// распарсить пришедшую строку
	if (!parser || !json_parser_load_from_data(parser, request_str, -1, NULL)) {
		return ACTION_NONE;
	}
	JsonReader *reader = json_reader_new(json_parser_get_root(parser));
	// перейти в поле 'action' и в нём д.б. строка
	if (!reader || !json_reader_read_member(reader, "action")
			|| !json_reader_is_value(reader))
		ret = ACTION_NONE;
	else
	// читаем содержимое поля 'action'
	{
		action_str = json_reader_get_string_value(reader);
		json_reader_end_element(reader);
	}
	if (!action_str)
		ret = ACTION_NONE;
	else {
		if (!g_strcmp0(action_str, "login"))
			ret = ACTION_LOGIN;
		else if (!g_strcmp0(action_str, "logout"))
			ret = ACTION_LOGOUT;
		else if (!g_strcmp0(action_str, "getdata"))
			ret = ACTION_GET_DATA;
		else if (!g_strcmp0(action_str, "cmd"))
			ret = ACTION_CMD;
//		if (!g_strcmp0(action_str, "monitor"))
//			ret = ACTION_MONITOR;
	}
	g_object_unref(reader);
	return ret;
}

// обработать api запрос "login" и выдать ответ
// request_str - запрос в json формате
// client_ip_addr - ip адрес клиента
// client_port - порт клиента
//строка запрос в json-формате вида {"action":login, "name":xx, "password":xx} или {"action":login, "sid":xx}
// return: строка ответ на запрос в json-формате вида {"action":login,"is_login":true, "devs"=["dev1","dev2",...],"devs_dev1"=["protocol","passw","status"], ...,"full_user_name":xx} 
// или {"is_login":false","err_code":0...-3}, строка требует удаления через g_free()
static char* api_prepare_answer_for_login(char *request_str,
		char *client_ip_addr, int client_port, JsonParser *parser, int sockfd,
		gboolean *is_login) {
	gboolean is_sid_auth = FALSE;	// авторизация по sid-у
	char *user_name = NULL;
	const char *user_passwd = NULL;
	char *user_sid = NULL;// идентификатор пользователя, которому разрушен вход на сервер
	char *user_devs = NULL;	// список устройств, которые доступны этому пользователю
	char *full_user_name = NULL;	// полное имя пользователя
	// return: 0 - успешная авторизация, -4 ошибка выполнения авторизации, -1 нет такого пользователя, -2 неверный пароль, -3 пользователь заблокирован
	int err_code = -4;
	// читаем переменные name и password
	{
		JsonReader *reader = json_reader_new(json_parser_get_root(parser));
		//printf("[api_prepare_answer_for_login] json_reader_new() reader=%x\n", reader);
		if (reader) {
			// перейти в поле 'sid' и в нём д.б. строка
			if (json_reader_read_member(reader, "sid")
					&& json_reader_is_value(reader)) {
				is_sid_auth = TRUE;
				user_sid = g_strdup(
						(char*) json_reader_get_string_value(reader));
				// авторизация пользователя по sid-у
				err_code = base_check_user_sid(user_sid, client_ip_addr,
						&user_name, &user_devs, &full_user_name);
			}
			json_reader_end_element(reader);
			// если нет sid-а, то проверяем логин и пароль
			if (!user_sid || (user_sid && !strlen(user_sid))) {
				// перейти в поле 'name' и в нём д.б. строка
				if (json_reader_read_member(reader, "name")
						&& json_reader_is_value(reader)) {
					user_name = g_strdup(json_reader_get_string_value(reader));
				}
				json_reader_end_element(reader);
				// авторизация пользователя по логину и паролю
				if (json_reader_read_member(reader, "password")
						&& json_reader_is_value(reader)) {
					user_passwd = json_reader_get_string_value(reader);
					json_reader_end_element(reader);
				}
				// авторизация по паролю
				err_code = base_check_user(user_name, user_passwd,
						client_ip_addr, client_port, &user_sid, &user_devs,
						&full_user_name);
			}

			g_object_unref(reader);
		}
	}
	printf("[api_prepare_answer_for_login] err_str=%d\n", err_code);
	// return: 1 - успешная авторизация, 0 ошибка выполнения авторизации, -1 нет такого пользователя, -2 неверный пароль, -3 пользователь заблокирован
	// ошибка авторизации
	char *err_str = NULL;
	switch (err_code) {
	case -4:
		err_str = g_strdup("Authorisation Error");
		break;
	case -5:
		err_str = g_strdup("Open database error");
		break;
	case -1:
		err_str = g_strdup("No such user");
		break;
	case -2:
		err_str = g_strdup("Wrong password");
		break;
	case -3:
		err_str = g_strdup("User is blocked");
		break;
	}
	if (err_str)
		printf("%s\n", err_str);
	g_free(err_str);

	// сформировать выходную json строку
	JsonBuilder *builder = json_builder_new();
	json_builder_begin_object(builder);
	json_builder_set_member_name(builder, "action");
	json_builder_add_string_value(builder, "login");
	json_builder_set_member_name(builder, "is_login");
	if (err_code) {
		json_builder_add_boolean_value(builder, FALSE);
		json_builder_set_member_name(builder, "err_code");
		json_builder_add_int_value(builder, err_code);
	} else {
		// разбираем список устройств пользователя
		char **list_devs = (user_devs) ? g_strsplit(user_devs, ";", -1) : NULL;
		int devs_n = (list_devs) ? g_strv_length(list_devs) : 0;
		// добавить/обновить к активному соединению параметр
		{
			struct USER_INFO *uinfo = (struct USER_INFO*) g_malloc(
					sizeof(struct USER_INFO));
			uinfo->name = g_strdup(user_name);
			uinfo->devs_list = list_devs;
			uinfo->devs_n = devs_n;
			add_connection_param(sockfd, uinfo);
		}
		// выходной параметр
		if (is_login)
			*is_login = TRUE;
		json_builder_add_boolean_value(builder, TRUE);
		// если авторизация была по sid-y
		if (is_sid_auth) {
			json_builder_set_member_name(builder, "user_name");
			json_builder_add_string_value(builder, user_name);
		}
		// если создан sid, то отправляем его пользователю
		else {
			json_builder_set_member_name(builder, "sid");
			json_builder_add_string_value(builder, user_sid);
		}
		// добавляем список устройств, доступных пользователю
		{
			if (devs_n > 0) {
				json_builder_set_member_name(builder, "devs");
				json_builder_begin_array(builder);
				for (int i = 0; i < devs_n; i++) {
					//json_builder_add_string_value(builder, *(list_devs + i));
					// узнать параметры устройства по imei
					char **str_array = base_get_dev_param(*(list_devs + i));
					if (str_array) {
						int j = 0;
						//json_builder_begin_object(builder);
						//json_builder_set_member_name(builder, *(list_devs + i));
						json_builder_begin_array(builder);
						while (*(str_array + j)) {
							json_builder_add_string_value(builder,
									*(str_array + j));
							j++;
						}
						// добавить текущее состояние устройства (online,offline)
						{
							// поиск активного DEV соединения для устройства imei
							// return: номер соединения(сокета), 0 - не нашли
							const char *imei = *(list_devs + i);
							int sock = find_dev_connection(imei, NULL, NULL,
									NULL);
							const char *cur_state_str = (sock) ? "1" : "0";
							json_builder_add_string_value(builder,
									cur_state_str);
						}
						json_builder_end_array(builder);
						//json_builder_end_object(builder);
						g_strfreev(str_array);
					}

				}
				json_builder_end_array(builder);
			}
			//if(list_devs) g_strfreev(list_devs);// сохранится в USER_INFO, удалится в del_connection()
		}
		/*/ добавляем параметры всех устройств //"devs_dev1" = ["protocol", "passw", "status"]
		 if (0)
		 {
		 if (devs_n > 0)
		 {
		 for (int i = 0; i < devs_n; i++)
		 {
		 // узнать параметры устройства по imei
		 char **str_array = base_get_dev_param(*(list_devs + i));
		 if (str_array)
		 {
		 int j = 0;
		 json_builder_set_member_name(builder, *(list_devs + i));
		 json_builder_begin_array(builder);
		 while(*(str_array + j))
		 {
		 json_builder_add_string_value(builder, *(str_array + j));
		 j++;
		 }
		 json_builder_end_array(builder);
		 g_strfreev(str_array);
		 }
		 }
		 }
		 }*/
		// добавляем полное имя пользователя
		if (full_user_name) {
			json_builder_set_member_name(builder, "full_user_name");
			json_builder_add_string_value(builder, full_user_name);
		}
	}

	json_builder_end_object(builder);

	g_free(user_name);
	g_free(user_sid);
	g_free(user_devs);
	g_free(full_user_name);
	// получаем выходную json строку
	JsonGenerator *gen = json_generator_new();
	JsonNode * root = json_builder_get_root(builder);
	json_generator_set_root(gen, root);
	char *ret_str = json_generator_to_data(gen, NULL);

	json_node_free(root);
	g_object_unref(gen);
	g_object_unref(builder);
	return ret_str;
}

enum {
	CMD_NONE, CMD_ALIAS, CMD_TO_DEV,

	CMD_COUNT
};

// потоковая функция отправки больших порций данных устройству
static void* thread_send_data_do_dev(void *args) {
	gpointer *arg = (gpointer*) args;
	int sockfd = GPOINTER_TO_INT(arg[0]);							// сокет
	unsigned char *buf = (unsigned char *) arg[1];		// указатель на данные
	int bufsize = GPOINTER_TO_INT(arg[2]);						// длина данных
	GMutex *mutex_w = (GMutex*) arg[3];							// мьютекс

	if (buf && bufsize > 0) {
		if (mutex_w)
			g_mutex_lock(mutex_w);
		s_send(sockfd, buf, bufsize, 5000);		// TIMEOUT_MS = 5000 (5 сек.)
		if (mutex_w)
			g_mutex_unlock(mutex_w);
	}
	g_free(buf);
	return NULL;
}

// отправить данные в отдельном потоке
int api_send_thread(SOCKET sock, unsigned char * buf1, int bufsize1,
		unsigned char * buf2, int bufsize2, GMutex *mutex_w) {
	// приготовить данные для потока
	gpointer *arg = (gpointer*) g_malloc0(4 * sizeof(gpointer));
	arg[0] = GINT_TO_POINTER((int) sock);							// сокет
	arg[1] = (gpointer) g_malloc(bufsize1 + bufsize2);	// указатель на данные
	arg[2] = GINT_TO_POINTER(bufsize1 + bufsize2);				// длина данных
	arg[3] = (gpointer) mutex_w;							// мьютекс
	unsigned char *bur_recv = (unsigned char *) arg[1];
	memcpy(bur_recv, buf1, bufsize1);
	memcpy(bur_recv + bufsize1, buf2, bufsize2);
	pthread_t thread;
	// создать поток
	return pthread_create(&thread, NULL, thread_send_data_do_dev, (void*) arg);
}

// обработать api запрос "cmd" и выдать ответ
// request_str - запрос в json формате
// client_ip_addr - ip адрес клиента
// client_port - порт клиента
//строка запрос в json-формате вида  {"action":"cmd","name":name,"params":[param1,param2,...]}
// return: строка ответ на запрос, если требуется, в json-формате вида {"action":"cmd", "name":name, "res"=TRUE/FALSE, "message":"текстовое сообщение о выполнении команды" ... }, строка требует удаления через g_free()
static char* api_prepare_answer_for_cmd(char *request_str, char *client_ip_addr,
		JsonParser *parser) {
	// сформировать выходную json строку
	JsonBuilder *builder = json_builder_new();
	json_builder_begin_object(builder);
	json_builder_set_member_name(builder, "action");
	json_builder_add_string_value(builder, "cmd");

	JsonReader *reader = json_reader_new(json_parser_get_root(parser));
	//printf("[api_prepare_answer_for_cmd] reader=%x\n", reader);
	if (reader) {
		int cmd_num = CMD_NONE;							// какая команда
		const char *cmd_name = NULL;							// какая команда
		int params_count = 0;							// число параметров
		const char **params_array = NULL;					// список параметров
		// перейти в поле 'name' и в нём д.б. строка
		if (json_reader_read_member(reader, "name")
				&& json_reader_is_value(reader)) {
			cmd_name = json_reader_get_string_value(reader);
			//printf("[api_prepare_answer_for_cmd] cmd_name=%s\n", cmd_name);
			if (!g_strcmp0(cmd_name, "new_alias"))
				cmd_num = CMD_ALIAS;
			else if (!g_strcmp0(cmd_name, "to_dev"))
				cmd_num = CMD_TO_DEV;
		}
		json_reader_end_element(reader);
		// перейти в поле 'params' и в нём д.б. массив
		if (json_reader_read_member(reader, "params")
				&& json_reader_is_array(reader)) {
			int j;
			params_count = json_reader_count_elements(reader);
			params_array = (const char **) g_malloc0(
					params_count * sizeof(const char*));
			for (j = 0; j < params_count; j++) {
				json_reader_read_element(reader, j);
				*(params_array + j) = json_reader_get_string_value(reader);
				json_reader_end_element(reader);
			}
		}
		json_reader_end_element(reader);

		switch (cmd_num) {
		case CMD_ALIAS: // 2 параметра: imei, new_alias
			if (params_count == 2) {
				char *err_msg = NULL;
				gboolean ret = base_set_new_dev_alias(*(params_array + 0),
						*(params_array + 1), &err_msg);
				// "new alias has been set"
				json_builder_set_member_name(builder, "res");
				json_builder_add_boolean_value(builder, ret);
				json_builder_set_member_name(builder, "name");
				json_builder_add_string_value(builder, cmd_name);
				json_builder_set_member_name(builder, "imei");
				json_builder_add_string_value(builder, *(params_array + 0));
				json_builder_set_member_name(builder, "message");
				if (ret)
					json_builder_add_string_value(builder, "Ok"); //alias has been set
				else {
					char *text_out = g_strdup_printf("can't set: %s.",
							(err_msg) ? err_msg : "");
					g_free(err_msg);
					json_builder_add_string_value(builder, text_out);
				}
			} else {
				json_builder_set_member_name(builder, "res");
				json_builder_add_boolean_value(builder, FALSE);
				json_builder_set_member_name(builder, "name");
				json_builder_add_string_value(builder, cmd_name);
				json_builder_set_member_name(builder, "message");
				json_builder_add_string_value(builder, "bad input parameters");
			}
			break;
		case CMD_TO_DEV:
			// 2 или 3 параметра: imei, cmd_text, cmd_param
			if (params_count == 2 || params_count == 3) {
				const char *imei = *(params_array + 0);
				const char *cmd_text = *(params_array + 1);
				const char *cmd_param =
						(params_count == 3) ? *(params_array + 2) : NULL;
				printf("[api_prepare_answer_for_cmd] imei=%s params_count=%d\n",
						imei, params_count);
				printf("[api_prepare_answer_for_cmd] cmd_text=%s\n", cmd_text);
				printf("[api_prepare_answer_for_cmd] cmd_param=%x\n",
						cmd_param);

				// поиск активного DEV соединения для устройства imei
				// return: номер соединения(сокета), 0 - не нашли
				GMutex *mutex_w = NULL;
				int protocol = PROTOCOL_NA;
				uint16_t ver = 0;
				int sockfd = find_dev_connection(imei, &protocol, &ver,
						&mutex_w);
				printf("[api_prepare_answer_for_cmd] sockfd=%d\n", sockfd);
				if (sockfd) {
					if (params_count == 2) {
						/*/ заменяем название команды на её содержимое
						 if (!g_strcmp0(cmd_text, "cmd_ping"))
						 {
						 if (protocol == PROTOCOL_WIALON)
						 cmd_text = "#P#\r\n";
						 else
						 cmd_text = NULL;
						 }*/
						// сформировать команду для отправки устройству
						int cmd_text_to_dev_len = 0;
						unsigned char *cmd_text_to_dev =
								packet_prepare_cmd_to_dev(protocol, cmd_text,
										&cmd_text_to_dev_len);
						printf(
								"[api_prepare_answer_for_cmd] cmd_text_to_dev=%s cmd_text_to_dev_len=%d\n\r",
								cmd_text_to_dev, cmd_text_to_dev_len);
						// перенаправляем команду напрямую устройству
						if (cmd_text_to_dev && cmd_text_to_dev_len > 0) {
							if (mutex_w)
								g_mutex_lock(mutex_w);
							s_send(sockfd, (unsigned char *) cmd_text_to_dev,
									cmd_text_to_dev_len, 5000);	// TIMEOUT_MS = 5000 (5 сек.)
							if (mutex_w)
								g_mutex_unlock(mutex_w);
							g_free(cmd_text_to_dev);
						}

					}
					if (params_count == 3) {
						gsize cmd_param_dec_len = 0;
						// переводим в бинарный вид из текстового
						guchar *cmd_param_dec = g_base64_decode(cmd_param,
								&cmd_param_dec_len);
						gchar *cmd_text_new = NULL;
						if (!g_strcmp0(cmd_text, "cmd_message")) {
							if (protocol == PROTOCOL_WIALON) {
								if (ver == 11)
									cmd_text_new = g_strdup_printf("#M#%s\r\n",
											cmd_param_dec);
								if (ver == 20) {
									// вычисляем контрольную сумму
									uint16_t crc16_calc = crc16(
											(uint8_t*) cmd_param,
											cmd_param_dec_len);
									cmd_text_new = g_strdup_printf(
											"#M#%s;%X\r\n", cmd_param_dec,
											crc16_calc);
								}
							}
							api_send_thread(sockfd,
									(unsigned char *) cmd_text_new,
									strlen(cmd_text_new), NULL, 0, mutex_w);// TIMEOUT_MS = 5000 (5 сек.)
							//if (mutex_w) g_mutex_lock(mutex_w);
							//if (cmd_text_new)
							//s_send(sockfd, (unsigned char *)cmd_text_new, strlen(cmd_text_new), 5000);// TIMEOUT_MS = 5000 (5 сек.)
							//if (mutex_w) g_mutex_unlock(mutex_w);
							g_free(cmd_text_new);
						} else if (!g_strcmp0(cmd_text, "cmd_firmware")) {
							printf(
									"cmd_firmware sockfd=%d imei=%s protocol=%d ver=%d mutex_w=%x\n\r",
									sockfd, imei, protocol, ver, mutex_w);
							if (protocol == PROTOCOL_WIALON) {
								//Служит для отправки новой прошивки на контроллер.
								//#US#sz\r\nBIN или #US#sz;crc16\r\nBIN
								if (ver == 11)
									cmd_text_new = g_strdup_printf(
											"#US#%lu\r\n", cmd_param_dec_len);
								if (ver == 20) {
									// вычисляем контрольную сумму
									uint16_t crc16_calc = crc16(
											(uint8_t*) cmd_param_dec,
											cmd_param_dec_len);
									cmd_text_new = g_strdup_printf(
											"#US#%lu;%X\r\n", cmd_param_dec_len,
											crc16_calc);
								}
								printf("cmd_text_new=%s\n", cmd_text_new);

								if (cmd_text_new) {
									// отправить данные устройству в отдельном потоке, чтобы не задерживать обмен по API
									api_send_thread(sockfd,
											(unsigned char *) cmd_text_new,
											strlen(cmd_text_new),
											(unsigned char *) cmd_param_dec,
											cmd_param_dec_len, mutex_w);
									//if (mutex_w) g_mutex_lock(mutex_w);
									//s_send(sockfd, (unsigned char *)cmd_text_new, strlen(cmd_text_new), 5000);// TIMEOUT_MS = 5000 (5 сек.)
									//if (mutex_w) g_mutex_unlock(mutex_w);
									g_free(cmd_text_new);
								}
							}
						} else if (!g_strcmp0(cmd_text, "cmd_configure")) {
							if (protocol == PROTOCOL_WIALON) {
								// Служит для отправки файла конфигурации на контроллер. 
								// #UC#sz\r\nBIN или #UC#sz;crc16\r\nBIN
								if (ver == 11)
									cmd_text_new = g_strdup_printf(
											"#UC#%lu\r\n", cmd_param_dec_len);
								if (ver == 20) {
									// вычисляем контрольную сумму
									uint16_t crc16_calc = crc16(
											(uint8_t*) cmd_param_dec,
											cmd_param_dec_len);
									cmd_text_new = g_strdup_printf(
											"#UC#%lu;%X\r\n", cmd_param_dec_len,
											crc16_calc);
								}
								if (cmd_text_new) {
									// отправить данные устройству в отдельном потоке, чтобы не задерживать обмен по API
									api_send_thread(sockfd,
											(unsigned char *) cmd_text_new,
											strlen(cmd_text_new),
											(unsigned char *) cmd_param_dec,
											cmd_param_dec_len, mutex_w);
									g_free(cmd_text_new);
								}
								/*if (cmd_text_new)
								 {
								 if (mutex_w) g_mutex_lock(mutex_w);
								 s_send(sockfd, (unsigned char *)cmd_text_new, strlen(cmd_text_new), 5000);// TIMEOUT_MS = 5000 (5 сек.)
								 if (mutex_w) g_mutex_unlock(mutex_w);
								 }
								 if (cmd_param_dec && cmd_param_dec_len>0)
								 api_send_thread(sockfd, (unsigned char *)cmd_param_dec, cmd_param_dec_len, mutex_w);// TIMEOUT_MS = 5000 (5 сек.)
								 g_free(cmd_text_new);*/
							}
						} else
							sockfd = 0;				// нераспознанная команда
						g_free(cmd_param_dec);
					}
				}
				json_builder_set_member_name(builder, "res");		// результат
				json_builder_add_boolean_value(builder,
						(sockfd > 0) ? TRUE : FALSE);
				json_builder_set_member_name(builder, "name");
				json_builder_add_string_value(builder, cmd_name);
				json_builder_set_member_name(builder, "imei");
				json_builder_add_string_value(builder, imei);
				json_builder_set_member_name(builder, "message");
				if (sockfd > 0)
					json_builder_add_string_value(builder, "TRUE");
				else
					json_builder_add_string_value(builder, "FALSE");
			} else {
				json_builder_set_member_name(builder, "res");
				json_builder_add_boolean_value(builder, FALSE);
				json_builder_set_member_name(builder, "message");
				json_builder_add_string_value(builder, "bad input parameters");
			}
		}
		g_free(params_array);

		g_object_unref(reader);
	}

	json_builder_end_object(builder);
	// получаем выходную json строку
	JsonGenerator *gen = json_generator_new();
	JsonNode * root = json_builder_get_root(builder);
	json_generator_set_root(gen, root);
	char *ret_str = json_generator_to_data(gen, NULL);
	json_node_free(root);
	g_object_unref(gen);

	g_object_unref(builder);
	printf("[api_prepare_answer_for_cmd] ret_str=%s\n", ret_str);
	return ret_str;
}

// обработать api запрос "getdata" и выдать ответ
// request_str - запрос в json формате
// client_ip_addr - ip адрес клиента
// client_port - порт клиента
//строка запрос в json-формате вида {"action":getdata, "sid":xx, "dev":xx, "data1":xx, "data2":xx}
// return: строка ответ на запрос в json-формате вида {"action":"getdata", "protocol":xx, "imei":xx, "rows":xx, "data":xx }, строка требует удаления через g_free()
static char* api_prepare_answer_for_getdata(char *request_str,
		char *client_ip_addr, JsonParser *parser) {
	int ret_records = 0;
	char *out_str = NULL;
	// сформировать выходную json строку
	JsonBuilder *builder = json_builder_new();
	json_builder_begin_object(builder);
	json_builder_set_member_name(builder, "action");
	json_builder_add_string_value(builder, "getdata");
	// получаем содержимое protocol_num, imei, rows и data
	// заполяем ret_records, out_str
	{
		JsonReader *reader = json_reader_new(json_parser_get_root(parser));
		if (reader) {
			const char *imei = NULL;
			uint64_t id1 = 0, id2 = 0;

			// перейти в поле 'dev' и в нём д.б. строка
			if (json_reader_read_member(reader, "dev")
					&& json_reader_is_value(reader))
				imei = json_reader_get_string_value(reader);
			json_reader_end_element(reader);
			// перейти в поле 'id1' и в нём д.б. число
			if (json_reader_read_member(reader, "id1")
					&& json_reader_is_value(reader))
				id1 = json_reader_get_int_value(reader);
			json_reader_end_element(reader);
			// перейти в поле 'id2' и в нём д.б. число
			if (json_reader_read_member(reader, "id2")
					&& json_reader_is_value(reader))
				id2 = json_reader_get_int_value(reader);
			json_reader_end_element(reader);

			int protocol_num = 0;
			int64_t rotal_records = 0;
			// прочитать пакеты из БД
			// return: число пакетов, -5 ошибка открытия БД
			ret_records = base_get_data_packets((char*) imei, id1, id2,
					&out_str, &protocol_num, &rotal_records);

			// выходные параметры
			json_builder_set_member_name(builder, "protocol");
			json_builder_add_int_value(builder, protocol_num);
			json_builder_set_member_name(builder, "imei");
			json_builder_add_string_value(builder, imei);
			json_builder_set_member_name(builder, "rows");
			json_builder_add_int_value(builder, ret_records);
			json_builder_set_member_name(builder, "total_rows");
			json_builder_add_int_value(builder, rotal_records);
			g_object_unref(reader);
		}
	}
	//json_builder_set_member_name(builder, "data");
	//json_builder_add_string_value(builder, str);
	if (ret_records > 0) {
		json_builder_set_member_name(builder, "data");
		int len = strlen(out_str);
		gchar *str_base64 = g_base64_encode((const guchar*) out_str, len);
		//int len2 = strlen(str_base64);
		json_builder_add_string_value(builder, (const char*) str_base64);
		g_free(str_base64);
	}
	json_builder_end_object(builder);
	g_free(out_str);
	// получаем выходную json строку
	JsonGenerator *gen = json_generator_new();
	JsonNode * root = json_builder_get_root(builder);
	json_generator_set_root(gen, root);
	char *ret_str = json_generator_to_data(gen, NULL);
	json_node_free(root);
	g_object_unref(gen);

	g_object_unref(builder);
	return ret_str;
}

// обработать api запрос и выдать ответ
// request_str - запрос в json формате
// client_ip_addr - ip адрес клиента
// client_port - порт клиента
// return: строка ответ на запрос в json-формате вида {"login":true,"param":xx,...}, строка требует удаления через g_free()
char* api_prepare_answer(char *request_str, char *client_ip_addr,
		int client_port, int sockfd) {
	char *ret_str = NULL;
	int action;
	// расшифровка строки с ответом сервера в формате Json
	JsonParser *parser = json_parser_new();
	action = api_parse_request(request_str, parser);
	switch (action) {
	case ACTION_NONE:
	default:
		printf("[api_prepare_answer] unknown action=%d\n", action);
		return NULL;
	case ACTION_LOGIN: {
		printf("[api_prepare_answer] ACTION_LOGIN\n");
		gboolean is_login = FALSE;
		ret_str = api_prepare_answer_for_login(request_str, client_ip_addr,
				client_port, parser, sockfd, &is_login);
		if (is_login) {
			// авторизовать соединение
			auth_connection(sockfd);
		}
	}
		break;
	case ACTION_LOGOUT:
		printf("[api_prepare_answer] ACTION_LOGOUT\n");
		// авторизовать соединение
		deauth_connection(sockfd);
		break;
	case ACTION_GET_DATA:
		printf("[api_prepare_answer] ACTION_GET_DATA\n");
		if (is_auth_connection(sockfd))
			ret_str = api_prepare_answer_for_getdata(request_str,
					client_ip_addr, parser);
		break;
	case ACTION_CMD:
		printf("[api_prepare_answer] ACTION_CMD\n");
		if (is_auth_connection(sockfd)) {
			printf("[api_prepare_answer] is_auth_connection=TRUE sockfd=%d\n",
					sockfd);
			ret_str = api_prepare_answer_for_cmd(request_str, client_ip_addr,
					parser);
		} else
			printf("[api_prepare_answer] is_auth_connection= FALSE sockfd=%d\n",
					sockfd);
		break;
//	case ACTION_MONITOR:
//		if (is_auth_connection(sockfd))
//			ret_str = api_prepare_answer_for_monitor(request_str, client_ip_addr, client_port, parser);
//		break;
	}

	if (parser)
		g_object_unref(parser);

	// коды ответов
	// 100 Continue — сервер удовлетворён начальными сведениями о запросе, клиент может продолжать пересылать заголовки. Появился в HTTP/1.1.
	// 200 OK — успешный запрос.Если клиентом были запрошены какие - либо данные, то они находятся в заголовке и / или теле сообщения.Появился в HTTP / 1.0.
	//400 Bad Request — сервер обнаружил в запросе клиента синтаксическую ошибку.Появился в HTTP / 1.0.
	//403 Forbidden — сервер понял запрос, но он отказывается его выполнять из - за ограничений в доступе для клиента к указанному ресурсу.
	//Если для доступа к ресурсу требуется аутентификация средствами HTTP, то сервер вернёт ответ 401, или 407 при использовании прокси
	//404 Not Found — самая распространённая ошибка при пользовании Интернетом, основная причина — ошибка в написании адреса Web - страницы.
	//503 Service Unavailable — сервер временно не имеет возможности обрабатывать запросы по техническим причинам (обслуживание, перегрузка и прочее). В поле Retry-After заголовка сервер может указать время, через которое клиенту рекомендуется повторить запрос.
//	if (code_str) *code_str = g_strdup("200 OK");
	return ret_str;
}

