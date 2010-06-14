/* cgi-lib.h - header file for cgi-lib.c
   Eugene Kim, <eekim@eekim.com>
   $Id: cgi-lib.h,v 1.3 2010/06/14 22:12:00 wes Exp $

   Copyright (C) 1996, 1997 Eugene Eric Kim
   All Rights Reserved
*/

#ifndef _CGI_LIB
#define _CGI_LIB 1

#include <stdlib.h>
#ifdef __cplusplus
extern "C" {
#endif
#include "cgi-llist.h"

#if 0
/* change this if you are using HTTP upload */
#ifndef UPLOADDIR
#define UPLOADDIR "/tmp"
#endif
#endif

/* CGI Environment Variables */
#define SERVER_SOFTWARE getenv("SERVER_SOFTWARE")
#define SERVER_NAME getenv("SERVER_NAME")
#define GATEWAY_INTERFACE getenv("GATEWAY_INTERFACE")

#define SERVER_PROTOCOL getenv("SERVER_PROTOCOL")
#define SERVER_PORT getenv("SERVER_PORT")
#define REQUEST_METHOD getenv("REQUEST_METHOD")
#define PATH_INFO getenv("PATH_INFO")
#define PATH_TRANSLATED getenv("PATH_TRANSLATED")
#define SCRIPT_NAME getenv("SCRIPT_NAME")
#define QUERY_STRING getenv("QUERY_STRING")
#define REMOTE_HOST getenv("REMOTE_HOST")
#define REMOTE_ADDR getenv("REMOTE_ADDR")
#define AUTH_TYPE getenv("AUTH_TYPE")
#define REMOTE_USER getenv("REMOTE_USER")
#define REMOTE_IDENT getenv("REMOTE_IDENT")
#define CONTENT_TYPE getenv("CONTENT_TYPE")
#define CONTENT_LENGTH getenv("CONTENT_LENGTH")

#define HTTP_USER_AGENT getenv("HTTP_USER_AGENT")

short accept_image(void);

/* form processing routines */
void unescape_url(char *url);
/* #define read_cgi_input(entries...)	_read_cgi_input(##entries, NULL) */
#define read_cgi_input(cx, entries...) _read_cgi_input(cx, entries, NULL)
    int _read_cgi_input(JSContext *cx, llist* entries, const char *uploadDir, ...);
char *cgi_val(llist l, const char *name);
char **cgi_val_multi(llist l, char *name);
char *cgi_name(llist l,char *value);
char **cgi_name_multi(llist l, char *value);

/* miscellaneous CGI routines */
int parse_cookies(llist *entries);
void print_cgi_env(JSContext *cx);
void print_entries(JSContext *cx, llist l);
char *escape_input(char *str);

/* boolean functions */
short is_form_empty(llist l);
short is_field_exists(llist l, char *str);
short is_field_empty(llist l, char *str);
#ifdef __cplusplus
}
#endif

#endif
