#include "http_server.h"
#include "http_server.h"
#include "lwip/sockets.h"
#include "sock_api.h"
#include "fs/fs.h"
#include "stdlib.h"
#include "http_multipart.h"
#include "http_server.h"

#ifdef HTTP_MULTIPART_SUPPORT

static struct https_io_intf *pIoIntf = NULL;

void https_io_intf_register(struct https_io_intf *io_intf)
{
    pIoIntf = io_intf;
}

static void *https_get_io_intf(void)
{
    if ((NULL == pIoIntf) || (NULL == pIoIntf->https_open) || (NULL == pIoIntf->https_write)) {
        printf("https_get_io_intf err\n");
        return NULL;
    }

    return pIoIntf;
}

static int get_line(void *sock, char *buf, int size)
{
    int i = 0;
    char c = '\0';
    int n;

    while ((i < size - 1) && (c != '\n')) {
        n = sock_recv(sock, &c, 1, 0);
        if (n > 0) {
            if (c == '\r') {
                n = sock_recv(sock, &c, 1, MSG_PEEK);
                if ((n > 0) && (c == '\n')) {
                    sock_recv(sock, &c, 1, 0);
                } else {
                    c = '\n';
                }
            }

            buf[i] = c;
            i++;
        } else {
            c = '\n';
        }
    }

    buf[i] = '\0';

    return (i);
}

static int drop_garbage(void *sock, int size)
{
    char rn[2];
    sock_recv(sock, &rn, size, 0); //drop \r\n
    printf("drop_garbage : \n");
    put_buf(rn, 2);
}

static int httpd_parse_request(struct http_cli_t *cli, char *buf)
{
    char *find_p = NULL;
    int fileName[16];
    if (strncasecmp(buf, "Referer:", 8) == 0) {
    } else if (strncasecmp(buf, "Referrer:", 9) == 0) {
    } else if (strncasecmp(buf, "User-Agent:", 11) == 0) {
    } else if (strncasecmp(buf, "User-Agent:", 11) == 0) {
    } else if (strncasecmp(buf, "Host:", 5) == 0) {
    } else if (strncasecmp(buf, "Accept:", 7) == 0) {
    } else if (strncasecmp(buf, "Accept-Encoding:", 16) == 0) {
    } else if (strncasecmp(buf, "Accept-Language:", 16) == 0) {
    } else if (strncasecmp(buf, "If-Modified-Since:", 18) == 0) {
    } else if (strncasecmp(buf, "Cookie:", 7) == 0) {
    } else if (strncasecmp(buf, "Range:", 6) == 0) {
    } else if (strncasecmp(buf, "Range-If:", 9) == 0 ||
               strncasecmp(buf, "If-Range:", 9) == 0) {
    } else if (strncasecmp(buf, "Content-Type:", 13) == 0) {
        if (strstr(buf, "multipart/form-data")) {
            cli->http_con->boundary_begin = (char *)malloc(strlen(buf) + 2 + 1); //len + "--" + '\0'
            if (NULL == cli->http_con->boundary_begin) {
                return -1;
            }

            cli->http_con->boundary_end = (char *)malloc(strlen(buf) + 4 + 1); //len + "----" + '\0'
            if (NULL == cli->http_con->boundary_begin) {
                free(cli->http_con->boundary_begin);
                cli->http_con->boundary_begin = NULL;
                return -1;
            }

            if (find_p = strstr(buf, "boundary=")) {
                sprintf(cli->http_con->boundary_begin, "--%s",  find_p + strlen("boundary="));//格式/n/0
                memcpy(cli->http_con->boundary_end, cli->http_con->boundary_begin, strlen(cli->http_con->boundary_begin) - 1);
                cli->http_con->boundary_end[strlen(cli->http_con->boundary_begin) - 1] = '\0';
                sprintf(cli->http_con->boundary_end, "%s--\n", cli->http_con->boundary_end);
            }
            if (cli->http_con->state == HTTPs_CONN_STATE_HEADER_PARSE) {
                cli->http_con->pContentType = HTTP_CONTENT_TYPE_MULTIPART_FORM;
            }
        }

        if (strstr(buf, "application/octet-stream")) {
            if (cli->http_con->state == HTTPs_CONN_STATE_HEADER_PARSE) {
                cli->http_con->pContentType = HTTP_CONTENT_TYPE_OCTET_STREAM;
            }
        }
    } else if (strncasecmp(buf, "Content-Length:", 15) == 0) {
        cli->http_con->flen = atoi(buf + strlen("Content-Length:"));
        printf("filename: %s, len : %d\n", cli->http_con->fname, cli->http_con->flen);
    } else if (strncasecmp(buf, "Authorization:", 14) == 0) {
    } else if (strncasecmp(buf, "Connection:", 11) == 0) {
    } else if (strncasecmp(buf, "Content-Disposition:", 11) == 0) {
        if (find_p = strstr(buf, "filename=")) { //找filename
            strcpy(fileName, find_p + strlen("filename=") + 1);

            find_p = strstr(fileName, "\"");
            if (find_p) {
                *find_p = '\0';
            }

            printf("fileName : %s\n", fileName);

            strcpy(cli->http_con->fname, fileName);
            cli->http_con->field = HTTP_MULTIPART_FIELD_FILE_NAME;
        } else if (find_p = strstr(buf, "name=")) {
            strcpy(fileName, find_p + strlen("name=") + 1);

            find_p = strstr(fileName, "\"");
            if (find_p) {
                *find_p = '\0';
            }

            printf("fileName : %s\n", fileName);

            strcpy(cli->http_con->fname, fileName);
            cli->http_con->field = HTTP_MULTIPART_FIELD_NAME;
        } else {
            if (cli->http_con->pContentType == HTTP_CONTENT_TYPE_MULTIPART_FORM) {
                cli->http_con->field = HTTP_MULTIPART_FIELD_UNKNOWN;
                printf("HTTP_MULTIPART_FIELD_UNKNOWN : %s\n", buf);
            }
        }
    } else {
        if (cli->http_con->pContentType == HTTP_CONTENT_TYPE_MULTIPART_FORM) {
            cli->http_con->field = HTTP_MULTIPART_FIELD_UNKNOWN;
            printf("unknown request header: %s\n", buf);
            put_buf(buf, 2);
        }
    }

    return 0;
}

int httpd_parse_head(struct headers *hdr, struct http_cli_t *cli, char *buf, int size)
{
    char *find_n = NULL;
    char *find_p = NULL;
    void *sock_hdl = cli->sock_hdl;
    int numbytes;
    char *token = NULL;

    numbytes = get_line(sock_hdl, buf, size);
    if (numbytes == 0) {
        return -1;
    }

    find_p = buf;
    //获取请求mode
    memset(&hdr->metode, 0, sizeof(hdr->metode));
    find_n = strchr(find_p, ' ');
    if (!find_n) {
        return -1;
    }
    *(find_n++) = 0;
    strncpy(hdr->metode, find_p, sizeof(hdr->metode));
    hdr->metode[sizeof(hdr->metode) - 1] = 0;
    find_p = find_n;

    //find url
    memset(&hdr->uri, 0, sizeof(hdr->uri));
    find_n = strchr(find_p, ' ');
    if (!find_n) {
        return -1;
    }
    *(find_n++) = 0;
    strncpy(hdr->uri, find_p, sizeof(hdr->uri));
    hdr->uri[sizeof(hdr->uri) - 1] = 0;
    find_p = find_n;

    printf("mode: %s, uri : %s\n", hdr->metode, hdr->uri);
    if (!strcmp("POST", hdr->metode)) {
        cli->http_con = (struct http_conn *)malloc(sizeof(struct http_conn));
        if (NULL == cli->http_con) {
            return -1;
        }

        memset(cli->http_con, 0, sizeof(struct http_conn));
        cli->http_con->state = HTTPs_CONN_STATE_HEADER_PARSE;
    }

    //解析头部剩余部分
    numbytes = 1;
    while ((numbytes > 0) && strcmp("\n", buf)) {
        numbytes = get_line(sock_hdl, buf, size);
        if (numbytes > 0) {
            if (strcmp("\n", buf)) { // maybe \r\n ignore
                if (!strcmp("POST", hdr->metode)) {
                    httpd_parse_request(cli, buf);
                } else if (!strcmp("GET", hdr->metode)) {
                    hdr->lowRange = hdr->highRange = 0;
                    token = strstr(buf, "Range: bytes=");
                    if (token) {
                        hdr->lowRange = atoi(token + strlen("Range: bytes="));

                        hdr->highRange = -1;
                        if (!strstr(token, "-\r\n")) {
                            hdr->highRange = atoi(strstr(token, "-") + strlen("-"));
                        }
                    }
                } else {
                    return -1;
                }
            }
        } else {
            printf("httpd_parse_head err!\n");
            return -1;
        }
    }

    printf("httpd_parse_head end\n");

    return 0;
}

static int httpd_parse_form(struct http_cli_t *cli, char *buf, int size)
{
    int numbytes = 1;
    void *sock_hdl = cli->sock_hdl;
    while ((numbytes > 0) && strcmp("\n", buf)) {
        numbytes = get_line(sock_hdl, buf, size);
        if (numbytes > 0) {
            if (strcmp("\n", buf)) { // maybe \r\n ignore
                httpd_parse_request(cli, buf);
            }
        } else {
            printf("httpd_parse_form err!\n");
            return -1;
        }
    }

    return 0;
}

static int httpd_get_file(struct http_cli_t *cli, char *buf, const int buf_size, const int file_len)
{
    int readLen = 0;
    int remainLen = file_len;
    int rLen = 0;
    void *sock_hdl = cli->sock_hdl;
    struct https_io_intf *ioIntf = NULL;
    int err;

    ioIntf = https_get_io_intf();
    if (NULL == ioIntf) {
        return -1;
    }

    void *fd = ioIntf->https_open(cli->http_con->fname);
    if (NULL == fd) {
        printf("https_open err!\n");
        return -1;
    }

    while (remainLen) {
        rLen = remainLen > buf_size ? buf_size : remainLen;
        readLen = sock_recv(sock_hdl, buf, rLen, 0);
        if (readLen < 0) {
            printf("httpd_get_file error!\n");
            return -1;
        }

        err = ioIntf->https_write(buf, readLen, fd);
        if (err < 0) {
            printf("ioIntf->https_write error!\n");
            return -1;
        }

        remainLen -= readLen;
    }

    ioIntf->https_close(fd);

    return 0;
}

int httpd_get_key_value(struct http_cli_t *cli, char *buf, int buf_size)
{
    int numbytes = 0;
    void *sock_hdl = cli->sock_hdl;
    numbytes = get_line(sock_hdl, buf, buf_size);
    if (numbytes > 0) {
        printf("key :%s, value :%s, len : %d\n", cli->http_con->fname, buf, cli->http_con->flen);
        return 0;
    }

    return -1;
}

int form_multipart_parse_loop(struct http_cli_t *cli, char *buf, int size)
{
    int err;
    void *sock_hdl = cli->sock_hdl;
    for (;;) {
        err = httpd_parse_form(cli, buf, size);
        if (err) {
            break;
        }

        if (cli->http_con->field == HTTP_MULTIPART_FIELD_FILE_NAME) { //files-filename
            if (strchr(cli->http_con->fname, '.')) { //为文件需要保存
                err = httpd_get_file(cli, buf, size, cli->http_con->flen);
                if (err) {
                    printf("httpd_get_file err\n");
                    break;
                }
                cli->http_con->flen = 0;
                memset(cli->http_con->fname, 0, sizeof(cli->http_con->fname)); //need清掉
                drop_garbage(sock_hdl, 2);
            } else {
                printf("unknow file format\n");
                break;
            }
        } else if (cli->http_con->field == HTTP_MULTIPART_FIELD_NAME) { //为键值对
            err = httpd_get_key_value(cli, buf, size);
            if (err) {
                printf("httpd_get_key_value err!\n");
                break;
            }
            memset(cli->http_con->fname, 0, sizeof(cli->http_con->fname)); //need清掉
        } else { //'filename' or 'name' string MUST be found.
            printf("\'filename\' or \'name\' string MUST be found.\n");
            break;
        }

        //判断边界是否已经结束
        int numbytes = 0;
        numbytes = get_line(cli->sock_hdl, buf, size);
        if (numbytes > 0) {
            if (!strncmp(cli->http_con->boundary_begin, buf, numbytes)) {
                printf("find boundary: %s\n", buf);
                continue;
            } else if (!strncmp(cli->http_con->boundary_end, buf, numbytes)) {
                printf("find boundary end: %s\n", buf);
                return 0;
            } else {
                printf("unknown request line:%s, len : %d\n", buf, numbytes);
                put_buf(buf, 2);
            }
        } else {
            printf("form_multipart_parse_loop get_line err!\n");
            return -1;
        }
    }

    return -1;
}

static int https_form_multipart_parse(struct http_cli_t *cli, char *buf, int size)
{
    int numbytes = 0;
    void *sock_hdl = cli->sock_hdl;
    numbytes = get_line(sock_hdl, buf, size);//获取第一个boundary
    if (numbytes > 0) {
        if (!strncmp(cli->http_con->boundary_begin, buf, size)) {
            return form_multipart_parse_loop(cli, buf, size);
        }
    }

    return -1;
}

int https_req_body_form(struct http_cli_t *cli, char *buf, int size)
{
    int err;
    void *sock_hdl = cli->sock_hdl;
    if (cli->http_con->pContentType == HTTP_CONTENT_TYPE_MULTIPART_FORM) { //multipart/form-data
        err = https_form_multipart_parse(cli, buf,  size);
    }

    if (cli->http_con->pContentType == HTTP_CONTENT_TYPE_OCTET_STREAM) { //octet-stream
        err = httpd_get_file(cli, buf, size, cli->http_con->flen);
    }

    return err;
}

static void https_cannot_execute(struct http_cli_t *cli, char *buf, int size)
{
    void *sock_hdl = cli->sock_hdl;
    sprintf(buf, "HTTP/1.0 500 Internal Server Error\r\n");
    sock_send(sock_hdl, buf, strlen(buf), 0);
    sprintf(buf, "Content-type: text/html\r\n");
    sock_send(sock_hdl, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    sock_send(sock_hdl, buf, strlen(buf), 0);
    sprintf(buf, "<P>Error prohibited CGI execution.\r\n");
    sock_send(sock_hdl, buf, strlen(buf), 0);
}

static void https_bad_request(struct http_cli_t *cli, char *buf, int size)
{
    void *sock_hdl = cli->sock_hdl;
    sprintf(buf, "HTTP/1.0 400 BAD REQUEST\r\n");
    sock_send(sock_hdl, buf, strlen(buf), 0);
    sprintf(buf, "Content-type: text/html\r\n");
    sock_send(sock_hdl, buf, strlen(buf), 0);
    sprintf(buf, "Connection: close\r\n");
    sock_send(sock_hdl, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    sock_send(sock_hdl, buf, strlen(buf), 0);
    sprintf(buf, "<html><title>Bad Request</title><body><p>Your browser sent a bad request</p></body></html>\r\n");
    sock_send(sock_hdl, buf, strlen(buf), 0);
}

void https_clean_conn(struct http_cli_t *cli)
{
    if (NULL == cli || NULL == cli->http_con) {
        return;
    }

    if (cli->http_con->boundary_begin) {
        free(cli->http_con->boundary_begin);
        cli->http_con->boundary_begin = NULL;
    }

    if (cli->http_con->boundary_end) {
        free(cli->http_con->boundary_end);
        cli->http_con->boundary_end = NULL;
    }

    free(cli->http_con);
    cli->http_con = NULL;
}

int https_send_respone(struct http_cli_t *cli, char *buf, int size)
{
    void *client = cli->sock_hdl;
    char *respone_data = NULL;
    int respone_len = 0;
    struct https_io_intf *ioIntf = NULL;

    strcpy(buf, "HTTP/1.0 200 OK\r\n");
    sock_send(client, buf, strlen(buf), 0);

    strcpy(buf, SERVER_STRING);
    sock_send(client, buf, strlen(buf), 0);

    sprintf(buf, "Content-Type: application/json;charset=UTF-8\r\n");
    sock_send(client, buf, strlen(buf), 0);
    strcpy(buf, "\r\n");
    sock_send(client, buf, strlen(buf), 0);

    ioIntf = https_get_io_intf();
    if (ioIntf != NULL && ioIntf->respone_data != NULL) {
        ioIntf->respone_data(&respone_data, &respone_len);
        if (respone_data && respone_len) {
            /* 发送data */
            sock_send(client, respone_data, respone_len, 0);
            strcpy(buf, "\r\n\r\n");
            sock_send(client, buf, strlen(buf), 0);
        }
    }

    return 0;
}

#endif
