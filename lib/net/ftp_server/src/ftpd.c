/*
 * Copyright (c) 2002 Florian Schulze.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the authors nor the names of the contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * ftpd.c - This file is part of the FTP daemon for lwIP
 *
 */

#include "lwip/tcp.h"
#include "lwip/debug.h"
#include "lwip/stats.h"
#include "ftpd.h"

#include <stdarg.h>
#include <ctype.h>
#include <string.h>

#include "vfs.h"

#ifndef EINVAL
#define EINVAL 1
#define ENOMEM 2
#define ENODEV 3
#endif

#define FTP_SEND_DOUBLE_BUFFER //send double buffer

#define FTPD_DEBUG LWIP_DBG_ON

#define msg110 "110 MARK %s = %s."
/*
         110 Restart marker reply.
             In this case, the text is exact and not left to the
             particular implementation; it must read:
                  MARK yyyy = mmmm
             Where yyyy is User-process data stream marker, and mmmm
             server's equivalent marker (note the spaces between markers
             and "=").
*/
#define msg120 "120 Service ready in nnn minutes."
#define msg125 "125 Data connection already open; transfer starting."
#define msg150 "150 File status okay; about to open data connection."
#define msg150recv "150 Opening BINARY mode data connection for %s (%i bytes)."
#define msg150stor "150 Opening BINARY mode data connection for %s."
#define msg200 "200 Command okay."
#define msg202 "202 Command not implemented, superfluous at this site."
#define msg211 "211 System status, or system help reply."
#define msg212 "212 Directory status."
#define msg213 "213 File status."
#define msg214 "214 %s."
/*
             214 Help message.
             On how to use the server or the meaning of a particular
             non-standard command.  This reply is useful only to the
             human user.
*/
#define msg214SYST "214 %s system type."
/*
         215 NAME system type.
             Where NAME is an official system name from the list in the
             Assigned Numbers document.
*/
#define msg220 "220 lwIP FTP Server ready."
/*
         220 Service ready for new user.
*/
#define msg221 "221 Goodbye."
/*
         221 Service closing control connection.
             Logged out if appropriate.
*/
#define msg225 "225 Data connection open; no transfer in progress."
#define msg226 "226 Closing data connection."
/*
             Requested file action successful (for example, file
             transfer or file abort).
*/
#define msg227 "227 Entering Passive Mode (%i,%i,%i,%i,%i,%i)."
/*
         227 Entering Passive Mode (h1,h2,h3,h4,p1,p2).
*/
#define msg230 "230 User logged in, proceed."
#define msg250 "250 Requested file action okay, completed."
#define msg257PWD "257 \"%s\" is current directory."
#define msg257 "257 \"%s\" created."
/*
         257 "PATHNAME" created.
*/
#define msg331 "331 User name okay, need password."
#define msg332 "332 Need account for login."
#define msg350 "350 Requested file action pending further information."
#define msg421 "421 Service not available, closing control connection."
/*
             This may be a reply to any command if the service knows it
             must shut down.
*/
#define msg425 "425 Can't open data connection."
#define msg426 "426 Connection closed; transfer aborted."
#define msg450 "450 Requested file action not taken."
/*
             File unavailable (e.g., file busy).
*/
#define msg451 "451 Requested action aborted: local error in processing."
#define msg452 "452 Requested action not taken."
/*
             Insufficient storage space in system.
*/
#define msg500 "500 Syntax error, command unrecognized."
/*
             This may include errors such as command line too long.
*/
#define msg501 "501 Syntax error in parameters or arguments."
#define msg502 "502 Command not implemented."
#define msg503 "503 Bad sequence of commands."
#define msg504 "504 Command not implemented for that parameter."
#define msg530 "530 Not logged in."
#define msg532 "532 Need account for storing files."
#define msg550 "550 Requested action not taken."
/*
             File unavailable (e.g., file not found, no access).
*/
#define msg551 "551 Requested action aborted: page type unknown."
#define msg552 "552 Requested file action aborted."
/*
             Exceeded storage allocation (for current directory or
             dataset).
*/
#define msg553 "553 Requested action not taken."
#define msgLoginFailed "503 Login incorrect. Login failed."
/*
             File name not allowed.
*/

enum ftpd_state_e {
    FTPD_USER,
    FTPD_PASS,
    FTPD_IDLE,
    FTPD_NLST,
    FTPD_LIST,
    FTPD_RETR,
    FTPD_RNFR,
    FTPD_STOR,
    FTPD_QUIT
};

static const char *month_table[12] = {
    "Jan",
    "Feb",
    "Mar",
    "Apr",
    "May",
    "Jun",
    "Jul",
    "Aug",
    "Sep",
    "Oct",
    "Nov",
    "Dec"
};
/*
------------------------------------------------------------
	SFIFO 1.3
------------------------------------------------------------
 * Simple portable lock-free FIFO
 * (c) 2000-2002, David Olofson
 *
 * Platform support:
 *	gcc / Linux / x86:		Works
 *	gcc / Linux / x86 kernel:	Works
 *	gcc / FreeBSD / x86:		Works
 *	gcc / NetBSD / x86:		Works
 *	gcc / Mac OS X / PPC:		Works
 *	gcc / Win32 / x86:		Works
 *	Borland C++ / DOS / x86RM:	Works
 *	Borland C++ / Win32 / x86PM16:	Untested
 *	? / Various Un*ces / ?:		Untested
 *	? / Mac OS / PPC:		Untested
 *	gcc / BeOS / x86:		Untested
 *	gcc / BeOS / PPC:		Untested
 *	? / ? / Alpha:			Untested
 *
 * 1.2: Max buffer size halved, to avoid problems with
 *	the sign bit...
 *
 * 1.3:	Critical buffer allocation bug fixed! For certain
 *	requested buffer sizes, older version would
 *	allocate a buffer of insufficient size, which
 *	would result in memory thrashing. (Amazing that
 *	I've manage to use this to the extent I have
 *	without running into this... *heh*)
 */

/*
 * Porting note:
 *	Reads and writes of a variable of this type in memory
 *	must be *atomic*! 'int' is *not* atomic on all platforms.
 *	A safe type should be used, and  sfifo should limit the
 *	maximum buffer size accordingly.
 */
typedef int sfifo_atomic_t;
#ifdef __TURBOC__
#	define	SFIFO_MAX_BUFFER_SIZE	0x7fff
#else /* Kludge: Assume 32 bit platform */
#	define	SFIFO_MAX_BUFFER_SIZE	0x7fffffff
#endif

typedef struct sfifo_t {
    char *buffer;
    int size;			/* Number of bytes */
    sfifo_atomic_t readpos;		/* Read position */
    sfifo_atomic_t writepos;	/* Write position */
} sfifo_t;

#define SFIFO_SIZEMASK(x)	((x)->size - 1)

#define sfifo_used(x)	(((x)->writepos - (x)->readpos) & SFIFO_SIZEMASK(x))
#define sfifo_space(x)	((x)->size - 1 - sfifo_used(x))

static FTPD_LOGIN *ftp_login = 0;
FTPD_OTA *ftp_ota = 0;
static u16_t sfifo_size = 4096;

/*
 * Alloc buffer, init FIFO etc...
 */
static int sfifo_init(sfifo_t *f, int size)
{
    memset(f, 0, sizeof(sfifo_t));

    if (size > SFIFO_MAX_BUFFER_SIZE) {
        return -EINVAL;
    }

    /*
     * Set sufficient power-of-2 size.
     *
     * No, there's no bug. If you need
     * room for N bytes, the buffer must
     * be at least N+1 bytes. (The fifo
     * can't tell 'empty' from 'full'
     * without unsafe index manipulations
     * otherwise.)
     */
    f->size = 1;
    for (; f->size <= size; f->size <<= 1)
        ;
    f->size >>= 1;

    /* Get buffer */
    if (0 == (f->buffer = (void *)ftp_malloc(f->size))) {
        return -ENOMEM;
    }

    return 0;
}

/*
 * Dealloc buffer etc...
 */
static void sfifo_close(sfifo_t *f)
{
    if (f->buffer) {
        ftp_free(f->buffer);
    }
}

/*
 * Get Write FIFO writepos
 * Return writepos address
 */
static void *sfifo_buffer_writepos_get(sfifo_t *f, int *len)
{
    if (!f->buffer) {
        *len = 0;
        return NULL;    /* No buffer! */
    }
    int space = sfifo_space(f);
    int maxsize = SFIFO_SIZEMASK(f);
    if (space >= maxsize) {
        f->writepos = 0;
        f->readpos = 0;
    } else if (space == 0) {
        *len = 0;
        return NULL;
    }
    if (f->writepos >= f->readpos && (maxsize - f->writepos) > 0) {
        *len = maxsize - f->writepos;
    } else {
        *len = 0;
        return NULL;
    }
    return (void *)(f->buffer + f->writepos);
}
/*
 * Write FIFO writepos
 * Return write size
 */
static int sfifo_buffer_writepos_set(sfifo_t *f, int len)
{
    int total;
    int i;

    if (!f->buffer) {
        return 0;    /* No buffer! */
    }
    total = f->size - f->writepos;
    if (len >= total) {
        len = total;
    }
    f->writepos += len;
    return len;
}
/*
 * Write bytes to a FIFO
 * Return number of bytes written, or an error code
 */
static int sfifo_write(sfifo_t *f, const void *_buf, int len)
{
    int total;
    int i;
    const char *buf = (const char *)_buf;

    if (!f->buffer) {
        return -ENODEV;    /* No buffer! */
    }

    /* total = len = min(space, len) */
    total = sfifo_space(f);
    LWIP_DEBUGF(FTPD_DEBUG, ("sfifo_space() = %d\n", total));
    if (len > total) {
        len = total;
    } else {
        total = len;
    }

    i = f->writepos;
    if (i + len >= f->size) {
        memcpy(f->buffer + i, buf, f->size - i);
        buf += f->size - i;
        len -= f->size - i;
        i = 0;
    }
    memcpy(f->buffer + i, buf, len);
    f->writepos = i + len;

    return total;
}

struct ftpd_datastate {
    char connected;
    unsigned int file_sum_check;
    unsigned int file_size;
    unsigned int read_size;
    vfs_dir_t *vfs_dir;
    vfs_dirent_t *vfs_dirent;
    void *vfs_file;
    sfifo_t fifo;
    struct tcp_pcb *msgpcb;
    struct ftpd_msgstate *msgfs;
};

struct ftpd_msgstate {
    char passive;
    enum ftpd_state_e state;
    sfifo_t fifo;
    struct ip4_addr dataip;
    u16_t dataport;
    struct tcp_pcb *datalistenpcb;
    struct tcp_pcb *datapcb;
    struct ftpd_datastate *datafs;
    char *renamefrom;
};

static void send_msg(struct tcp_pcb *pcb, struct ftpd_msgstate *fsm, char *msg, ...);

static void ftpd_dataerr(void *arg, err_t err)
{
    struct ftpd_datastate *fsd = arg;

    LWIP_DEBUGF(FTPD_DEBUG, ("ftpd_dataerr: %s (%i)\n", lwip_strerr(err), err));
    if (fsd == NULL) {
        return;
    }
    fsd->msgfs->datafs = NULL;
    fsd->msgfs->state = FTPD_IDLE;
    ftp_free(fsd);
}

static void ftpd_dataclose(struct tcp_pcb *pcb, struct ftpd_datastate *fsd)
{
    tcp_arg(pcb, NULL);
    tcp_sent(pcb, NULL);
    tcp_recv(pcb, NULL);

    if (fsd->msgfs->datalistenpcb) {
        tcp_arg(fsd->msgfs->datalistenpcb, NULL);
        tcp_accept(fsd->msgfs->datalistenpcb, NULL);
        tcp_close(fsd->msgfs->datalistenpcb);
        fsd->msgfs->datalistenpcb = NULL;
    }

    if (fsd->vfs_file) {
        vfs_close(fsd->vfs_file, 0);
        fsd->vfs_file = NULL;
    }
    fsd->msgfs->datafs = NULL;
    sfifo_close(&fsd->fifo);
    ftp_free(fsd);
    tcp_arg(pcb, NULL);
    tcp_close(pcb);
}

static int send_data(struct tcp_pcb *pcb, struct ftpd_datastate *fsd)
{
    err_t err;
    u16_t len;

    if (sfifo_used(&fsd->fifo) > 0) {
        int i;

        /* We cannot send more data than space available in the send
           buffer. */
        if (tcp_sndbuf(pcb) < sfifo_used(&fsd->fifo)) {
            len = tcp_sndbuf(pcb);
        } else {
            len = (u16_t) sfifo_used(&fsd->fifo);
        }

        i = fsd->fifo.readpos;
        if ((i + len) >= fsd->fifo.size && (fsd->fifo.size - i)) {
            err = tcp_write(pcb, fsd->fifo.buffer + i, (u16_t)(fsd->fifo.size - i), 1);
            if (err != ERR_OK) {
                LWIP_DEBUGF(FTPD_DEBUG, ("send_data: error writing!\n"));
                return -EINVAL;
            }
            len -= fsd->fifo.size - i;
            fsd->fifo.readpos = 0;
            i = 0;
        }
        if (len > 0) {
            err = tcp_write(pcb, fsd->fifo.buffer + i, len, 1);
            if (err != ERR_OK) {
                LWIP_DEBUGF(FTPD_DEBUG, ("send_data: error writing!\n"));
                return -EINVAL;
            }
            fsd->fifo.readpos += len;
        }
    }
    return 0;
}
static void send_file(struct ftpd_datastate *fsd, struct tcp_pcb *pcb)
{
    int err = 0;
    if (!fsd->connected) {
        return;
    }

    if (fsd->vfs_file) {
        int len;
        unsigned char *buffer = NULL;
#ifdef FTP_SEND_DOUBLE_BUFFER
        int size = fsd->fifo.size;
        buffer = ftp_malloc(size);
        if (!buffer) {
            printf("err in %s no mem \r\n", __func__);
            goto exit;
        }
#endif
        /* first, check fifo is no empty, and send fifo to tcp */
        if (sfifo_used(&fsd->fifo) > 0) {
            err = send_data(pcb, fsd);
            if (buffer) {
                ftp_free(buffer);
                buffer = NULL;
            }
            return;
        }
        /* second, check sfifo space */
        len = sfifo_space(&fsd->fifo);
        if (len == 0) {
            err = send_data(pcb, fsd);
            if (buffer) {
                ftp_free(buffer);
                buffer = NULL;
            }
            return;
        }
        unsigned int last_percent = 0;
        if (fsd->file_size) {
            last_percent = (unsigned long long)fsd->read_size * 100 / fsd->file_size;
        }
#ifdef FTP_SEND_DOUBLE_BUFFER
        if (len > size) {
            len = size;
        }
        len = vfs_read(buffer, 1, len, fsd->vfs_file);
        if (len == 0) {
            vfs_close(fsd->vfs_file, 0);
            fsd->vfs_file = NULL;
            fsd->read_size = 0;;
            fsd->file_size = 0;;
            printf("--->FTP send file complete, file_sum_check = 0x%x\n", fsd->file_sum_check);
            fsd->file_sum_check = 0;
            goto exit;
        }
        fsd->read_size += len;
        for (int i = 0; i < len; i++) {
            fsd->file_sum_check += buffer[i];
        }
        sfifo_write(&fsd->fifo, buffer, len);
        ftp_free(buffer);
        buffer = NULL;
#else
        /* Get fifo address , and read file writ to it */
        buffer = sfifo_buffer_writepos_get(&fsd->fifo, &len);
        if (buffer && len) {
            len = vfs_read(buffer, 1, len, fsd->vfs_file);
            if (len == 0) {
                vfs_close(fsd->vfs_file, 0);
                fsd->vfs_file = NULL;
                fsd->read_size = 0;;
                fsd->file_size = 0;;
                printf("--->FTP send file complete, file_sum_check = 0x%x\r\n", fsd->file_sum_check);
                fsd->file_sum_check = 0;
                goto exit;
            }
            fsd->read_size += len;
            for (int i = 0; i < len; i++) {
                fsd->file_sum_check += buffer[i];
            }
            sfifo_buffer_writepos_set(&fsd->fifo, len);
        }
        err = send_data(pcb, fsd);
#endif
        err = send_data(pcb, fsd);
        if (fsd->file_size) {
            unsigned int percent = (unsigned long long)fsd->read_size * 100 / fsd->file_size;
            if (percent != last_percent) {
                printf("--->FTP send file >> %d%% \r\n", percent);
            }
        }
    } else {
exit:
        ;
        struct ftpd_msgstate *fsm;
        struct tcp_pcb *msgpcb;

        /* first, check fifo is no empty, and send fifo to tcp */
        if (sfifo_used(&fsd->fifo) > 0) {
            err = send_data(pcb, fsd);
        }
        fsm = fsd->msgfs;
        msgpcb = fsd->msgpcb;

        ftpd_dataclose(pcb, fsd);
        fsm->datapcb = NULL;
        fsm->datafs = NULL;
        fsm->state = FTPD_IDLE;
        send_msg(msgpcb, fsm, msg226);
        return;
    }
}

static void send_next_directory(struct ftpd_datastate *fsd, struct tcp_pcb *pcb, int shortlist)
{
    char *buffer = ftp_malloc(256);
    int len;
    int fnum = 0;
    void *vfs = NULL;

    if (!buffer) {
        printf("err in %s no mem \r\n", __func__);
        goto exit;
    }
    while (1) {
        if (fsd->vfs_dirent == NULL && fsd->vfs_dir) {
            fsd->vfs_dirent = vfs_readdir(fsd->vfs_dir, &vfs, fnum);
        }

        if (fsd->vfs_dirent) {
            fnum++;
            if (shortlist) {
                len = sprintf(buffer, "%s\r\n", fsd->vfs_dirent->name);
                if (sfifo_space(&fsd->fifo) < len) {
                    send_data(pcb, fsd);
                    ftp_free(buffer);
                    vfs_close(vfs, 0);
                    return;
                }
                sfifo_write(&fsd->fifo, buffer, len);
                fsd->vfs_dirent = NULL;
                vfs_close(vfs, 0);
            } else {
                vfs_stat_t st;
                time_t current_time;
                int current_year;
                int current_mon;
                struct tm s_time;

                /* platform must notice !!! */
                time(&current_time);
                gmtime_r(&current_time, &s_time);
                s_time.tm_year += 1900;
                s_time.tm_mon += 1;
                current_year = s_time.tm_year;
                current_mon = s_time.tm_mon;

                /*vfs_stat(fsd->msgfs->vfs, fsd->vfs_dirent->name, &st);*/
                vfs_stat(vfs, fsd->vfs_dirent->name, &st);
                gmtime_r(&st.st_mtime, &s_time);
                s_time.tm_year += 1900;
                s_time.tm_mon += 1;
                if (s_time.tm_year >= current_year) {
                    if (s_time.tm_mon > current_mon) {
                        s_time.tm_year--;
                    }
                    len = sprintf(buffer, "-rw-rw-rw-   1 user     ftp  %11ld %s %02d %02d:%02d %s\r\n", st.st_size, month_table[s_time.tm_mon - 1], s_time.tm_mday, s_time.tm_hour, s_time.tm_min, fsd->vfs_dirent->name);
                } else {
                    len = sprintf(buffer, "-rw-rw-rw-   1 user     ftp  %11ld %s %02d %5d %s\r\n", st.st_size, month_table[s_time.tm_mon - 1], s_time.tm_mday, s_time.tm_year, fsd->vfs_dirent->name);
                }

                if (VFS_ISDIR(st.st_mode)) {
                    buffer[0] = 'd';
                }
                printf("--->FTP ls : %s", buffer);
                if (sfifo_space(&fsd->fifo) < len) {
                    send_data(pcb, fsd);
                    ftp_free(buffer);
                    vfs_close(vfs, 0);
                    return;
                }
                sfifo_write(&fsd->fifo, buffer, len);
                fsd->vfs_dirent = NULL;
                vfs_close(vfs, 0);
            }
        } else {
exit:
            ;
            struct ftpd_msgstate *fsm;
            struct tcp_pcb *msgpcb;

            if (sfifo_used(&fsd->fifo) > 0) {
                send_data(pcb, fsd);
            }
            fsm = fsd->msgfs;
            msgpcb = fsd->msgpcb;

            vfs_closedir(fsd->vfs_dir);
            fsd->vfs_dir = NULL;
            ftpd_dataclose(pcb, fsd);
            fsm->datapcb = NULL;
            fsm->datafs = NULL;
            fsm->state = FTPD_IDLE;
            send_msg(msgpcb, fsm, msg226);
            if (buffer) {
                ftp_free(buffer);
            }
            vfs_close(vfs, 0);
            return;
        }
    }
}

static err_t ftpd_datasent(void *arg, struct tcp_pcb *pcb, u16_t len)
{
    struct ftpd_datastate *fsd = arg;
    (void) len; /* suppress unused warning */

    switch (fsd->msgfs->state) {
    case FTPD_LIST:
        send_next_directory(fsd, pcb, 0);
        break;
    case FTPD_NLST:
        send_next_directory(fsd, pcb, 1);
        break;
    case FTPD_RETR:
        send_file(fsd, pcb);
        break;
    default:
        break;
    }

    return ERR_OK;
}

static err_t ftpd_datarecv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err)
{
    struct ftpd_datastate *fsd = arg;

    if (err == ERR_OK && p != NULL) {
        struct pbuf *q;
        u16_t tot_len = 0;

        for (q = p; q != NULL; q = q->next) {
            int len;

            len = vfs_write(q->payload, 1, q->len, fsd->vfs_file);
            tot_len += len;
            if (len != q->len) {
                break;
            }
        }

        /* Inform TCP that we have taken the data. */
        tcp_recved(pcb, tot_len);

        pbuf_free(p);
    }
    if (err == ERR_OK && p == NULL) {
        struct ftpd_msgstate *fsm;
        struct tcp_pcb *msgpcb;

        fsm = fsd->msgfs;
        msgpcb = fsd->msgpcb;

        if (fsd->vfs_file) {
            vfs_close(fsd->vfs_file, 0);
            fsd->vfs_file = NULL;
        }
        ftpd_dataclose(pcb, fsd);
        fsm->datapcb = NULL;
        fsm->datafs = NULL;
        fsm->state = FTPD_IDLE;
        send_msg(msgpcb, fsm, msg226);
    }

    return ERR_OK;
}

static err_t ftpd_dataconnected(void *arg, struct tcp_pcb *pcb, err_t err)
{
    struct ftpd_datastate *fsd = arg;
    (void) err; /* suppress unused warning */

    fsd->msgfs->datapcb = pcb;
    fsd->connected = 1;

    /* Tell TCP that we wish to be informed of incoming data by a call
       to the http_recv() function. */
    tcp_recv(pcb, ftpd_datarecv);

    /* Tell TCP that we wish be to informed of data that has been
       successfully sent by a call to the ftpd_sent() function. */
    tcp_sent(pcb, ftpd_datasent);

    tcp_err(pcb, ftpd_dataerr);

    switch (fsd->msgfs->state) {
    case FTPD_LIST:
        send_next_directory(fsd, pcb, 0);
        break;
    case FTPD_NLST:
        send_next_directory(fsd, pcb, 1);
        break;
    case FTPD_RETR:
        send_file(fsd, pcb);
        break;
    default:
        break;
    }

    return ERR_OK;
}

static err_t ftpd_dataaccept(void *arg, struct tcp_pcb *pcb, err_t err)
{
    struct ftpd_datastate *fsd = arg;

    (void) err; /* suppress unused warning */

    fsd->msgfs->datapcb = pcb;
    fsd->connected = 1;

    /* Tell TCP that we wish to be informed of incoming data by a call
       to the http_recv() function. */
    tcp_recv(pcb, ftpd_datarecv);

    /* Tell TCP that we wish be to informed of data that has been
       successfully sent by a call to the ftpd_sent() function. */
    tcp_sent(pcb, ftpd_datasent);

    tcp_err(pcb, ftpd_dataerr);

    switch (fsd->msgfs->state) {
    case FTPD_LIST:
        send_next_directory(fsd, pcb, 0);
        break;
    case FTPD_NLST:
        send_next_directory(fsd, pcb, 1);
        break;
    case FTPD_RETR:
        send_file(fsd, pcb);
        break;
    default:
        break;
    }

    return ERR_OK;
}

static int open_dataconnection(struct tcp_pcb *pcb, struct ftpd_msgstate *fsm)
{
    if (fsm->passive) {
        return 0;
    }

    /* Allocate memory for the structure that holds the state of the
       connection. */
    fsm->datafs = ftp_malloc(sizeof(struct ftpd_datastate));

    if (fsm->datafs == NULL) {
        LWIP_DEBUGF(FTPD_DEBUG, ("open_dataconnection: Out of memory\n"));
        send_msg(pcb, fsm, msg451);
        return 1;
    }
    memset(fsm->datafs, 0, sizeof(struct ftpd_datastate));
    fsm->datafs->msgfs = fsm;
    fsm->datafs->msgpcb = pcb;

    if (sfifo_init(&fsm->datafs->fifo, sfifo_size) != 0) {
        ftp_free(fsm->datafs);
        send_msg(pcb, fsm, msg451);
        return 1;
    }

    fsm->datapcb = tcp_new();

    if (fsm->datapcb == NULL) {
        sfifo_close(&fsm->datafs->fifo);
        ftp_free(fsm->datafs);
        send_msg(pcb, fsm, msg451);
        return 1;
    }

    /* Tell TCP that this is the structure we wish to be passed for our
       callbacks. */
    tcp_arg(fsm->datapcb, fsm->datafs);
    ip_addr_t dataip;
    IP_SET_TYPE_VAL(dataip, IPADDR_TYPE_V4);
    ip4_addr_copy(*ip_2_ip4(&dataip), fsm->dataip);
    tcp_connect(fsm->datapcb, &dataip, fsm->dataport, ftpd_dataconnected);

    return 0;
}

static void cmd_user(const char *arg, struct tcp_pcb *pcb, struct ftpd_msgstate *fsm)
{
    (void) arg; /* suppress unused warning */

    /*send_msg(pcb, fsm, msg331);*/
    /*fsm->state = FTPD_PASS;*/
    if (ftp_login) {
        if (ftp_login->user && !strcmp(ftp_login->user, arg)) {
            send_msg(pcb, fsm, msg331);
            fsm->state = FTPD_PASS;
        } else {
            send_msg(pcb, fsm, msgLoginFailed);
            fsm->state = FTPD_QUIT;
        }
    } else {
        send_msg(pcb, fsm, msg331);
        fsm->state = FTPD_PASS;
    }
}

static void cmd_pass(const char *arg, struct tcp_pcb *pcb, struct ftpd_msgstate *fsm)
{
    (void) arg; /* suppress unused warning */
    /*send_msg(pcb, fsm, msg230);*/
    /*fsm->state = FTPD_IDLE;*/
    if (ftp_login) {
        if (ftp_login->pass && !strcmp(ftp_login->pass, arg)) {
            send_msg(pcb, fsm, msg230);
            fsm->state = FTPD_IDLE;
        } else {
            send_msg(pcb, fsm, msgLoginFailed);
            fsm->state = FTPD_QUIT;
        }
    } else {
        send_msg(pcb, fsm, msgLoginFailed);
        fsm->state = FTPD_QUIT;
    }
}

static void cmd_port(const char *arg, struct tcp_pcb *pcb, struct ftpd_msgstate *fsm)
{
    int nr;
    unsigned pHi, pLo;
    unsigned ip[4];

    nr = sscanf(arg, "%u,%u,%u,%u,%u,%u", &(ip[0]), &(ip[1]), &(ip[2]), &(ip[3]), &pHi, &pLo);
    if (nr != 6) {
        send_msg(pcb, fsm, msg501);
    } else {
        IP4_ADDR(&fsm->dataip, (u8_t) ip[0], (u8_t) ip[1], (u8_t) ip[2], (u8_t) ip[3]);
        fsm->dataport = ((u16_t) pHi << 8) | (u16_t) pLo;
        send_msg(pcb, fsm, msg200);
    }
}

static void cmd_quit(const char *arg, struct tcp_pcb *pcb, struct ftpd_msgstate *fsm)
{
    (void) arg; /* suppress unused warning */
    send_msg(pcb, fsm, msg221);
    fsm->state = FTPD_QUIT;
}

static void cmd_cwd(const char *arg, struct tcp_pcb *pcb, struct ftpd_msgstate *fsm)
{
    /*if (!vfs_chdir(fsm->vfs, arg)) {*/
    if (!vfs_chdir(arg)) {
        send_msg(pcb, fsm, msg250);
    } else {
        send_msg(pcb, fsm, msg550);
    }
}

static void cmd_cdup(const char *arg, struct tcp_pcb *pcb, struct ftpd_msgstate *fsm)
{
    (void) arg; /* suppress unused warning */
    /*if (!vfs_chdir(fsm->vfs, "..")) {*/
    if (!vfs_chdir("..")) {
        send_msg(pcb, fsm, msg250);
    } else {
        send_msg(pcb, fsm, msg550);
    }
}

static void cmd_pwd(const char *arg, struct tcp_pcb *pcb, struct ftpd_msgstate *fsm)
{
    char *path;
    (void) arg; /* suppress unused warning */

    /*if ((path = vfs_getcwd(fsm->vfs, NULL, 0))) {*/
    if ((path = vfs_getcwd(NULL, 0))) {
        send_msg(pcb, fsm, msg257PWD, path);
        ftp_free(path);
    }
}

static void cmd_list_common(const char *arg, struct tcp_pcb *pcb, struct ftpd_msgstate *fsm, int shortlist)
{
    vfs_dir_t *vfs_dir;
    char *cwd;
    (void) arg; /* suppress unused warning */

    /*cwd = vfs_getcwd(fsm->vfs, NULL, 0);*/
    cwd = vfs_getcwd(NULL, 0);
    if ((!cwd)) {
        send_msg(pcb, fsm, msg451);
        return;
    }
    /*vfs_dir = vfs_opendir(fsm->vfs, cwd);*/
    vfs_dir = vfs_opendir(cwd);
    ftp_free(cwd);
    /*if (!vfs_dir) {*/
    if (!vfs_dir && !ftp_ota) { //支持无SD卡的OTA遍历目录列表，如返回遍历目录失败则，会出现多次调用升级接口问题
        send_msg(pcb, fsm, msg451);
        return;
    }

    if (open_dataconnection(pcb, fsm) != 0) {
        vfs_closedir(vfs_dir);
        return;
    }

    fsm->datafs->vfs_dir = vfs_dir;
    fsm->datafs->vfs_dirent = NULL;
    if (shortlist != 0) {
        fsm->state = FTPD_NLST;
    } else {
        fsm->state = FTPD_LIST;
    }

    send_msg(pcb, fsm, msg150);
}

static void cmd_nlst(const char *arg, struct tcp_pcb *pcb, struct ftpd_msgstate *fsm)
{
    cmd_list_common(arg, pcb, fsm, 1);
}

static void cmd_list(const char *arg, struct tcp_pcb *pcb, struct ftpd_msgstate *fsm)
{
    cmd_list_common(arg, pcb, fsm, 0);
}

static void cmd_retr(const char *arg, struct tcp_pcb *pcb, struct ftpd_msgstate *fsm)
{
    /*vfs_file_t *vfs_file;*/
    void *vfs_file;
    vfs_stat_t st;

    /*vfs_stat(fsm->vfs, arg, &st);*/
    vfs_stat(NULL, arg, &st);
    if (!VFS_ISREG(st.st_mode)) {
        send_msg(pcb, fsm, msg550);
        return;
    }
    /*vfs_file = vfs_open(fsm->vfs, arg, "rb");*/
    vfs_file = vfs_open(arg, "rb");
    if (!vfs_file) {
        send_msg(pcb, fsm, msg550);
        return;
    }

    send_msg(pcb, fsm, msg150recv, arg, st.st_size);

    if (open_dataconnection(pcb, fsm) != 0) {
        vfs_close(vfs_file, 1);
        return;
    }

    fsm->datafs->file_size = st.st_size;
    fsm->datafs->read_size = 0;
    fsm->datafs->vfs_file = vfs_file;
    fsm->state = FTPD_RETR;
}

static void cmd_stor(const char *arg, struct tcp_pcb *pcb, struct ftpd_msgstate *fsm)
{
    /*vfs_file_t *vfs_file;*/
    void *vfs_file;

    /*vfs_file = vfs_open(fsm->vfs, arg, "wb");*/
    vfs_file = vfs_open(arg, "wb");
    if (!vfs_file) {
        send_msg(pcb, fsm, msg550);
        return;
    }

    send_msg(pcb, fsm, msg150stor, arg);

    if (open_dataconnection(pcb, fsm) != 0) {
        vfs_close(vfs_file, 1);
        return;
    }

    fsm->datafs->vfs_file = vfs_file;
    fsm->state = FTPD_STOR;
}

static void cmd_noop(const char *arg, struct tcp_pcb *pcb, struct ftpd_msgstate *fsm)
{
    (void) arg; /* suppress unused warning */
    send_msg(pcb, fsm, msg200);
}

static void cmd_syst(const char *arg, struct tcp_pcb *pcb, struct ftpd_msgstate *fsm)
{
    (void) arg; /* suppress unused warning */
    send_msg(pcb, fsm, msg214SYST, "UNIX");
}

static void cmd_pasv(const char *arg, struct tcp_pcb *pcb, struct ftpd_msgstate *fsm)
{
    static u16_t port = 4096;
    static u16_t start_port = 4096;
    struct tcp_pcb *temppcb;
    (void) arg; /* suppress unused warning */

    /* Allocate memory for the structure that holds the state of the
       connection. */
    fsm->datafs = ftp_malloc(sizeof(struct ftpd_datastate));

    if (fsm->datafs == NULL) {
        LWIP_DEBUGF(FTPD_DEBUG, ("cmd_pasv: Out of memory\n"));
        send_msg(pcb, fsm, msg451);
        return;
    }
    memset(fsm->datafs, 0, sizeof(struct ftpd_datastate));

    if (sfifo_init(&fsm->datafs->fifo, sfifo_size) != 0) {
        ftp_free(fsm->datafs);
        send_msg(pcb, fsm, msg451);
        return;
    }

    fsm->datalistenpcb = tcp_new();

    if (fsm->datalistenpcb == NULL) {
        ftp_free(fsm->datafs);
        sfifo_close(&fsm->datafs->fifo);
        send_msg(pcb, fsm, msg451);
        return;
    }

    start_port = port;

    while (1) {
        err_t err;

        if (++port > 0x7fff) {
            port = 4096;
        }

        fsm->dataport = port;
        err = tcp_bind(fsm->datalistenpcb, (ip_addr_t *)&pcb->local_ip, fsm->dataport);
        if (err == ERR_OK) {
            break;
        }
        if (start_port == port) {
            err = ERR_CLSD;
        }
        if (err == ERR_USE) {
            continue;
        } else {
            ftpd_dataclose(fsm->datalistenpcb, fsm->datafs);
            fsm->datalistenpcb = NULL;
            fsm->datafs = NULL;
            return;
        }
    }

    temppcb = tcp_listen(fsm->datalistenpcb);
    if (!temppcb) {
        LWIP_DEBUGF(FTPD_DEBUG, ("cmd_pasv: tcp_listen failed\n"));
        ftpd_dataclose(fsm->datalistenpcb, fsm->datafs);
        fsm->datalistenpcb = NULL;
        fsm->datafs = NULL;
        return;
    }
    fsm->datalistenpcb = temppcb;

    fsm->passive = 1;
    fsm->datafs->connected = 0;
    fsm->datafs->msgfs = fsm;
    fsm->datafs->msgpcb = pcb;

    /* Tell TCP that this is the structure we wish to be passed for our
       callbacks. */
    tcp_arg(fsm->datalistenpcb, fsm->datafs);
    tcp_accept(fsm->datalistenpcb, ftpd_dataaccept);
    send_msg(pcb, fsm, msg227, ip4_addr1(ip_2_ip4(&pcb->local_ip)), ip4_addr2(ip_2_ip4(&pcb->local_ip)), ip4_addr3(ip_2_ip4(&pcb->local_ip)), ip4_addr4(ip_2_ip4(&pcb->local_ip)), (fsm->dataport >> 8) & 0xff, (fsm->dataport) & 0xff);
}

static void cmd_abrt(const char *arg, struct tcp_pcb *pcb, struct ftpd_msgstate *fsm)
{
    (void) arg; /* suppress unused warning */
    if (fsm->datafs != NULL) {
        tcp_arg(fsm->datapcb, NULL);
        tcp_sent(fsm->datapcb, NULL);
        tcp_recv(fsm->datapcb, NULL);
        tcp_abort(pcb);
        sfifo_close(&fsm->datafs->fifo);
        ftp_free(fsm->datafs);
        fsm->datafs = NULL;
    }
    fsm->state = FTPD_IDLE;
}

static void cmd_type(const char *arg, struct tcp_pcb *pcb, struct ftpd_msgstate *fsm)
{
    LWIP_DEBUGF(FTPD_DEBUG, ("Got TYPE -%s-\n", arg));

    /*if (arg && strcmp(arg, "I") != 0) {*/
    /*send_msg(pcb, fsm, msg502);*/
    /*return;*/
    /*}*/
    send_msg(pcb, fsm, msg200);
}

static void cmd_mode(const char *arg, struct tcp_pcb *pcb, struct ftpd_msgstate *fsm)
{
    LWIP_DEBUGF(FTPD_DEBUG, ("Got MODE -%s-\n", arg));
    send_msg(pcb, fsm, msg502);
}

static void cmd_rnfr(const char *arg, struct tcp_pcb *pcb, struct ftpd_msgstate *fsm)
{
    if (arg == NULL) {
        send_msg(pcb, fsm, msg501);
        return;
    }
    if (*arg == '\0') {
        send_msg(pcb, fsm, msg501);
        return;
    }
    if (fsm->renamefrom) {
        ftp_free(fsm->renamefrom);
    }
    fsm->renamefrom = ftp_malloc(strlen(arg) + 1);
    if (fsm->renamefrom == NULL) {
        LWIP_DEBUGF(FTPD_DEBUG, ("cmd_rnfr: Out of memory\n"));
        send_msg(pcb, fsm, msg451);
        return;
    }
    strcpy(fsm->renamefrom, arg);
    fsm->state = FTPD_RNFR;
    send_msg(pcb, fsm, msg350);
}

static void cmd_rnto(const char *arg, struct tcp_pcb *pcb, struct ftpd_msgstate *fsm)
{
    if (fsm->state != FTPD_RNFR) {
        send_msg(pcb, fsm, msg503);
        return;
    }
    fsm->state = FTPD_IDLE;
    if (arg == NULL) {
        send_msg(pcb, fsm, msg501);
        return;
    }
    if (*arg == '\0') {
        send_msg(pcb, fsm, msg501);
        return;
    }
    /*if (vfs_rename(fsm->vfs, fsm->renamefrom, arg)) {*/
    if (vfs_rename(fsm->renamefrom, arg)) {
        send_msg(pcb, fsm, msg450);
    } else {
        send_msg(pcb, fsm, msg250);
    }
}

static void cmd_mkd(const char *arg, struct tcp_pcb *pcb, struct ftpd_msgstate *fsm)
{
    if (arg == NULL) {
        send_msg(pcb, fsm, msg501);
        return;
    }
    if (*arg == '\0') {
        send_msg(pcb, fsm, msg501);
        return;
    }
    /*if (vfs_mkdir(fsm->vfs, arg, VFS_IRWXU | VFS_IRWXG | VFS_IRWXO) != 0) {*/
    if (vfs_mkdir(arg, VFS_IRWXU | VFS_IRWXG | VFS_IRWXO) != 0) {
        send_msg(pcb, fsm, msg550);
    } else {
        send_msg(pcb, fsm, msg257, arg);
    }
}

static void cmd_rmd(const char *arg, struct tcp_pcb *pcb, struct ftpd_msgstate *fsm)
{
    vfs_stat_t st;

    if (arg == NULL) {
        send_msg(pcb, fsm, msg501);
        return;
    }
    if (*arg == '\0') {
        send_msg(pcb, fsm, msg501);
        return;
    }
    /*if (vfs_stat(fsm->vfs, arg, &st) != 0) {*/
    if (vfs_stat(NULL, arg, &st) != 0) {
        send_msg(pcb, fsm, msg550);
        return;
    }
    if (!VFS_ISDIR(st.st_mode)) {
        send_msg(pcb, fsm, msg550);
        return;
    }
    /*if (vfs_rmdir(fsm->vfs, arg) != 0) {*/
    if (vfs_rmdir(arg) != 0) {
        send_msg(pcb, fsm, msg550);
    } else {
        send_msg(pcb, fsm, msg250);
    }
}

static void cmd_dele(const char *arg, struct tcp_pcb *pcb, struct ftpd_msgstate *fsm)
{
    vfs_stat_t st;

    if (arg == NULL) {
        send_msg(pcb, fsm, msg501);
        return;
    }
    if (*arg == '\0') {
        send_msg(pcb, fsm, msg501);
        return;
    }
    /*if (vfs_stat(fsm->vfs, arg, &st) != 0) {*/
    if (vfs_stat(NULL, arg, &st) != 0) {
        send_msg(pcb, fsm, msg550);
        return;
    }
    if (!VFS_ISREG(st.st_mode)) {
        send_msg(pcb, fsm, msg550);
        return;
    }
    /*if (vfs_remove(fsm->vfs, arg) != 0) {*/
    if (vfs_remove(arg) != 0) {
        send_msg(pcb, fsm, msg550);
    } else {
        send_msg(pcb, fsm, msg250);
    }
}

struct ftpd_command {
    char *cmd;
    void (*func)(const char *arg, struct tcp_pcb *pcb, struct ftpd_msgstate *fsm);
};

const static struct ftpd_command ftpd_commands[] = {
    {"USER", cmd_user},
    {"PASS", cmd_pass},
    {"PORT", cmd_port},
    {"QUIT", cmd_quit},
    {"CWD", cmd_cwd},
    {"CDUP", cmd_cdup},
    {"PWD", cmd_pwd},
    {"XPWD", cmd_pwd},
    {"NLST", cmd_nlst},
    {"LIST", cmd_list},
    {"RETR", cmd_retr},
    {"STOR", cmd_stor},
    {"NOOP", cmd_noop},
    {"SYST", cmd_syst},
    {"ABOR", cmd_abrt},
    {"TYPE", cmd_type},
    {"MODE", cmd_mode},
    {"RNFR", cmd_rnfr},
    {"RNTO", cmd_rnto},
    {"MKD", cmd_mkd},
    {"XMKD", cmd_mkd},
    {"RMD", cmd_rmd},
    {"XRMD", cmd_rmd},
    {"DELE", cmd_dele},
    {"PASV", cmd_pasv},
    {NULL, NULL}
};

static void send_msgdata(struct tcp_pcb *pcb, struct ftpd_msgstate *fsm)
{
    err_t err;
    u16_t len;

    if (sfifo_used(&fsm->fifo) > 0) {
        int i;

        /* We cannot send more data than space available in the send
           buffer. */
        if (tcp_sndbuf(pcb) < sfifo_used(&fsm->fifo)) {
            len = tcp_sndbuf(pcb);
        } else {
            len = (u16_t) sfifo_used(&fsm->fifo);
        }

        i = fsm->fifo.readpos;
        if ((i + len) >= fsm->fifo.size && (fsm->fifo.size - i)) {
            err = tcp_write(pcb, fsm->fifo.buffer + i, (u16_t)(fsm->fifo.size - i), 1);
            if (err != ERR_OK) {
                LWIP_DEBUGF(FTPD_DEBUG, ("send_msgdata: error writing!\n"));
                return;
            }
            len -= fsm->fifo.size - i;
            fsm->fifo.readpos = 0;
            i = 0;
        }
        err = tcp_write(pcb, fsm->fifo.buffer + i, len, 1);
        if (err != ERR_OK) {
            LWIP_DEBUGF(FTPD_DEBUG, ("send_msgdata: error writing!\n"));
            return;
        }
        fsm->fifo.readpos += len;
    }
}

static void send_msg(struct tcp_pcb *pcb, struct ftpd_msgstate *fsm, char *msg, ...)
{
    va_list arg;
    char *buffer = ftp_malloc(255);
    int len;

    if (!buffer) {
        printf("err in %s no mem \r\n", __func__);
        return;
    }
    va_start(arg, msg);
    vsprintf(buffer, msg, arg);
    va_end(arg);
    strcat(buffer, "\r\n");
    len = strlen(buffer);
    if (sfifo_space(&fsm->fifo) < len) {
        ftp_free(buffer);
        return;
    }
    sfifo_write(&fsm->fifo, buffer, len);
    LWIP_DEBUGF(FTPD_DEBUG, ("response: %s", buffer));
    send_msgdata(pcb, fsm);
    ftp_free(buffer);
}

static void ftpd_msgerr(void *arg, err_t err)
{
    struct ftpd_msgstate *fsm = arg;

    LWIP_DEBUGF(FTPD_DEBUG, ("ftpd_msgerr: %s (%i)\n", lwip_strerr(err), err));
    if (fsm == NULL) {
        return;
    }
    if (fsm->datafs) {
        ftpd_dataclose(fsm->datapcb, fsm->datafs);
    }
    sfifo_close(&fsm->fifo);
    if (fsm->renamefrom) {
        ftp_free(fsm->renamefrom);
    }
    fsm->renamefrom = NULL;
    ftp_free(fsm);
}

static void ftpd_msgclose(struct tcp_pcb *pcb, struct ftpd_msgstate *fsm)
{
    tcp_arg(pcb, NULL);
    tcp_sent(pcb, NULL);
    tcp_recv(pcb, NULL);
    if (fsm->datafs) {
        ftpd_dataclose(fsm->datapcb, fsm->datafs);
    }
    sfifo_close(&fsm->fifo);
    if (fsm->renamefrom) {
        ftp_free(fsm->renamefrom);
    }
    fsm->renamefrom = NULL;
    ftp_free(fsm);
    tcp_arg(pcb, NULL);
    tcp_close(pcb);
    vfs_chdir_close(NULL);
}

static err_t ftpd_msgsent(void *arg, struct tcp_pcb *pcb, u16_t len)
{
    struct ftpd_msgstate *fsm = arg;
    (void) len; /* suppress unused warning */

    if ((sfifo_used(&fsm->fifo) == 0) && (fsm->state == FTPD_QUIT)) {
        ftpd_msgclose(pcb, fsm);
        return ERR_OK;
    }

    if (pcb->state <= ESTABLISHED) {
        send_msgdata(pcb, fsm);
    }
    return ERR_OK;
}

static err_t ftpd_msgrecv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err)
{
    char *text;
    struct ftpd_msgstate *fsm = arg;

    if (err == ERR_OK && p != NULL) {

        /* Inform TCP that we have taken the data. */
        tcp_recved(pcb, p->tot_len);

        text = ftp_malloc(p->tot_len + 255);
        if (text) {
            char cmd[5];
            struct pbuf *q;
            char *pt = text;
            struct ftpd_command *ftpd_cmd;

            for (q = p; q != NULL; q = q->next) {
                bcopy(q->payload, pt, q->len);
                pt += q->len;
            }
            *pt = '\0';

            pt = &text[strlen(text) - 1];
            while (((*pt == '\r') || (*pt == '\n')) && pt >= text) {
                *pt-- = '\0';
            }

            LWIP_DEBUGF(FTPD_DEBUG, ("query: %s\n", text));

            strncpy(cmd, text, 4);
            for (pt = cmd; isalpha(*pt) && pt < &cmd[4]; pt++) {
                *pt = toupper(*pt);
            }
            *pt = '\0';

            for (ftpd_cmd = ftpd_commands; ftpd_cmd->cmd != NULL; ftpd_cmd++) {
                if (!strcmp(ftpd_cmd->cmd, cmd)) {
                    break;
                }
            }

            if (strlen(text) < (strlen(cmd) + 1)) {
                pt = "";
            } else {
                pt = &text[strlen(cmd) + 1];
            }

            if (ftpd_cmd->func) {
                printf("ftp cmd : %s \r\n", ftpd_cmd->cmd);
                ftpd_cmd->func(pt, pcb, fsm);
            } else {
                send_msg(pcb, fsm, msg502);
            }

            ftp_free(text);
        }
        pbuf_free(p);
    } else if (err == ERR_OK && p == NULL) {
        ftpd_msgclose(pcb, fsm);
    }

    return ERR_OK;
}

static err_t ftpd_msgpoll(void *arg, struct tcp_pcb *pcb)
{
    struct ftpd_msgstate *fsm = arg;
    (void) pcb; /* suppress unused warning */

    if (fsm == NULL) {
        return ERR_OK;
    }

    if (fsm->datafs) {
        if (fsm->datafs->connected) {
            switch (fsm->state) {
            case FTPD_LIST:
                send_next_directory(fsm->datafs, fsm->datapcb, 0);
                break;
            case FTPD_NLST:
                send_next_directory(fsm->datafs, fsm->datapcb, 1);
                break;
            case FTPD_RETR:
                send_file(fsm->datafs, fsm->datapcb);
                break;
            default:
                break;
            }
        }
    }

    return ERR_OK;
}

static err_t ftpd_msgaccept(void *arg, struct tcp_pcb *pcb, err_t err)
{
    LWIP_PLATFORM_DIAG(("ftpd_msgaccept called"));
    struct ftpd_msgstate *fsm;
    (void) err; /* suppress unused warning */
    (void) arg;

    /* Allocate memory for the structure that holds the state of the
       connection. */
    fsm = ftp_malloc(sizeof(struct ftpd_msgstate));

    if (fsm == NULL) {
        LWIP_DEBUGF(FTPD_DEBUG, ("ftpd_msgaccept: Out of memory\n"));
        return ERR_MEM;
    }
    memset(fsm, 0, sizeof(struct ftpd_msgstate));

    /* Initialize the structure. */
    if (sfifo_init(&fsm->fifo, sfifo_size) != 0) {
        ftp_free(fsm);
        return ERR_MEM;
    }
    fsm->state = FTPD_IDLE;
    /*fsm->vfs = vfs_openfs();*/
    /*if (fsm->vfs == NULL) {*/
    /*sfifo_close(&fsm->fifo);*/
    /*ftp_free(fsm);*/
    /*return ERR_CLSD;*/
    /*}*/

    /* Tell TCP that this is the structure we wish to be passed for our
       callbacks. */
    tcp_arg(pcb, fsm);

    /* Tell TCP that we wish to be informed of incoming data by a call
       to the http_recv() function. */
    tcp_recv(pcb, ftpd_msgrecv);

    /* Tell TCP that we wish be to informed of data that has been
       successfully sent by a call to the ftpd_sent() function. */
    tcp_sent(pcb, ftpd_msgsent);

    tcp_err(pcb, ftpd_msgerr);

    tcp_poll(pcb, ftpd_msgpoll, 1);

    send_msg(pcb, fsm, msg220);

    return ERR_OK;
}

void ftpd_server_init(const char *user, const char *pass, const char *ota_name, int fifo_size)
{
    struct tcp_pcb *pcb;
    if (user && pass) {
        ftp_login = ftp_malloc(sizeof(*ftp_login));
        if (!ftp_login) {
            printf("ftpd_server init err \r\n");
            return;
        }
        memset(ftp_login, 0, sizeof(*ftp_login));
        ftp_login->user = user;
        ftp_login->pass = pass;
    }
    if (ota_name) {
        ftp_ota = ftp_malloc(sizeof(*ftp_ota));
        if (!ftp_ota) {
            printf("ftp_ota alloc err \r\n");
        } else {
            memset(ftp_ota, 0, sizeof(*ftp_ota));
            ftp_ota->fname = ota_name;
        }
    }
    if (fifo_size > 0) {
        sfifo_size = fifo_size;
    }

    pcb = tcp_new();
    LWIP_DEBUGF(FTPD_DEBUG, ("ftpd_init: pcb: %lx\n", (unsigned long) pcb));
    if (!pcb) {
        printf("ftp tcp_new err\n");
        return;
    }
    int r = tcp_bind(pcb, IP_ADDR_ANY, 21);
    LWIP_DEBUGF(FTPD_DEBUG, ("ftpd_init: tcp_bind: %d\n", r));
    pcb = tcp_listen(pcb);
    LWIP_DEBUGF(FTPD_DEBUG, ("ftpd_init: listen-pcb: %lx\n", (unsigned long) pcb));
    if (!pcb) {
        printf("ftp tcp_listen err\n");
        return;
    }
    tcp_accept(pcb, ftpd_msgaccept);
    printf("ftpd_server init ok\n\n");
}
