#include "http_server.h"
#include "http_server.h"
#include "lwip/sockets.h"
#include "sock_api.h"
#include "fs/fs.h"
#include "stdlib.h"
#include "http_multipart.h"
#include "http_server.h"
#include "wifi/wifi_connect.h"
#include "system/includes.h"

#define CONFIG_STORAGE_PATH 	"storage/sd0"
#define CONFIG_ROOT_PATH     	CONFIG_STORAGE_PATH"/C/"

static int fd_count = 0;
void *httpsOpen(char *fname)
{
    char f_name[512];

    sprintf(f_name, CONFIG_ROOT_PATH"%d.mp3", fd_count);
    fd_count++;

    FILE *fd = fopen(f_name, "w+");
    if (NULL == fd) {
        printf("httpsOpen err!\n");
        return NULL;
    }

    printf("httpsOpen : %s, fd : 0x%x\n", f_name, fd);
    return fd;
}

void httpsClose(void *fd)
{
    printf("httpsClose : 0x%x\n", fd);
    fclose(fd);
}

int httpsWrite(void *buf, int size, void *fd)
{
    printf("httpsWrite : %d\n", size);
    int wlen;
    wlen = fwrite(buf, 1, size, fd);
    if (wlen != size) {
        printf("httpsWrite err!");
        return -1;
    }
    return wlen;
}

int responeData(void **data, int **len)
{
    char *dataPtr =
        "{\"msg\":\"OK\",\"code\":0,\"data\":[{\"fileName\":\"6f6f8015c7722a7bcfdc7b43dd57195a.mp3\",\"originalFilename\":\"ac motor.mp3\",\"url\":\"https://ydhardwarecommon.nosdn.127.net/6f6f8015c7722a7bcfdc7b43dd57195a.mp3\"},{\"fileName\":\"f6f748558704a7927a3dab93d413c031.mp3\",\"originalFilename\":\"ac hum.mp3\",\"url\":\"https://ydhardwarecommon.nosdn.127.net/f6f748558704a7927a3dab93d413c031.mp3\"},{\"fileName\":\"70e933b5c81046c57c67d9b8aeddc3f9.mp3\",\"originalFilename\":\"acoustooptic modulator.mp3\",\"url\":\"https://ydhardwarecommon.nosdn.127.net/70e933b5c81046c57c67d9b8aeddc3f9.mp3\"}]}";

    *data = dataPtr;
    *len = strlen(dataPtr);
    return 1;
}

static struct  https_io_intf iointf = {
    .https_open = httpsOpen,
    .https_write = httpsWrite,
    .https_close = httpsClose,
    .respone_data = responeData,
};

static void multipart_task(void *priv)
{
    https_io_intf_register(&iointf);
    http_get_server_init(32769);
}

static void multipart_start(void *priv)
{
    /* enum wifi_sta_connect_state state; */
    /* while (1) { */
    /* printf("Connecting to the network...\n"); */
    /* state = wifi_get_sta_connect_state() ; */
    /* if (WIFI_STA_NETWORK_STACK_DHCP_SUCC == state) { */
    /* printf("Network connection is successful!\n"); */
    /* break; */
    /* } */
    /* os_time_dly(100); */
    /* } */
    while (!storage_device_ready()) {//等待文件系统挂载完成
        os_time_dly(30);
    }

    printf(">>>SD card is ready <<<");

    if (thread_fork("multipart_task", 10, 1024, 0, NULL, multipart_task, NULL) != OS_NO_ERR) {
        printf("thread fork fail\n");
    }
}

void multipart_main(void *priv)
{
    if (thread_fork("multipart_start", 10, 512, 0, NULL, multipart_start, NULL) != OS_NO_ERR) {
        printf("thread fork fail\n");
    }
}

//late_initcall(c_main);



