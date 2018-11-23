/*_
 * Copyright 2010 Scyphus Solutions Co. Ltd.  All rights reserved.
 *
 * Authors:
 *      Hirochika Asai
 */

#ifndef _URL_PARSER_H
#define _URL_PARSER_H

/*
 * URL storage
 * <scheme>://<user>:<password>@<host>:<port>/<path>?<query>#<fragment>
 */

typedef struct parsed_url_ {
	char *scheme; /* mandatory */
	char *host; /* mandatory */
	int port; /* optional */
	char *path; /* optional */
	char *query; /* optional */
	char *fragment; /* optional */
	char *username; /* optional */
	char *password; /* optional */
} parsed_url;

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Declaration of function prototypes
 */
parsed_url * parse_url(const char *);
void parsed_url_free(parsed_url *);

#ifdef __cplusplus
}
#endif

#endif /* _URL_PARSER_H */
