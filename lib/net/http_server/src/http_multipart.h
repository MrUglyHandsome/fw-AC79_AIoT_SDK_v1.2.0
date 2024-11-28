#ifndef __HTTP_MULTIPART__
#define __HTTP_MULTIPART__
#include "http_server.h"

typedef enum http_content_type {
    HTTP_CONTENT_TYPE_UNKNOWN,
    HTTP_CONTENT_TYPE_NONE,
    HTTP_CONTENT_TYPE_HTML,
    HTTP_CONTENT_TYPE_OCTET_STREAM,
    HTTP_CONTENT_TYPE_PDF,
    HTTP_CONTENT_TYPE_ZIP,
    HTTP_CONTENT_TYPE_GIF,
    HTTP_CONTENT_TYPE_JPEG,
    HTTP_CONTENT_TYPE_PNG,
    HTTP_CONTENT_TYPE_JS,
    HTTP_CONTENT_TYPE_PLAIN,
    HTTP_CONTENT_TYPE_CSS,
    HTTP_CONTENT_TYPE_JSON,
    HTTP_CONTENT_TYPE_APP_FORM,
    HTTP_CONTENT_TYPE_MULTIPART_FORM,
    HTTP_CONTENT_TYPE_WEBSOCK_TXT_DATA,
    HTTP_CONTENT_TYPE_WEBSOCK_BIN_DATA
} HTTP_CONTENT_TYPE;

typedef enum http_multipart_field {
    HTTP_MULTIPART_FIELD_NAME,
    HTTP_MULTIPART_FIELD_FILE_NAME,
    HTTP_MULTIPART_FIELD_UNKNOWN
} HTTP_MULTIPART_FIELD;

typedef enum  https_key_val_type {
    HTTPs_KEY_VAL_TYPE_PAIR,
    HTTPs_KEY_VAL_TYPE_VAL,
    HTTPs_KEY_VAL_TYPE_FILE,
    HTTPs_KEY_VAL_TYPE_UNKNOWN,
} HTTPs_KEY_VAL_TYPE;

enum https_conn_state {
    HTTPs_CONN_STATE_NULL,
    HTTPs_CONN_STATE_HEADER_PARSE,
    HTTPs_CONN_STATE_BODY_PARSE,
};

struct http_conn {
    enum http_content_type pContentType; //content类型
    enum http_multipart_field field; //name or filename
    enum https_conn_state state;
    enum https_conn_state state2;
    char fname[64]; //name or filename
    int  flen;
    char *boundary_begin;
    char *boundary_end;
};

struct https_io_intf {
    void *(*https_open)(char *fname);
    int (*https_write)(void *buf, int size, void *fd);
    void (*https_close)(void *fd);
    int (*respone_data)(void **data, int **len);
};

#define ASCII_CHAR_QUOTATION_MARK '\"'
#define SERVER_STRING "Server: JLhttpd/0.1.0\r\n"

void https_clean_conn(struct http_cli_t *cli);
void https_io_intf_register(struct https_io_intf *io_intf);
int https_send_respone(struct http_cli_t *cli, char *buf, int size);
#endif //__HTTP_MULTIPART__


