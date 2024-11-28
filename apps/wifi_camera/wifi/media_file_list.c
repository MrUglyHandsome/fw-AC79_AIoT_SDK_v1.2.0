#include "app_config.h"
#ifdef CONFIG_ENABLE_VLIST
#include "vunpkg_server.h"
#include "system/includes.h"
#include "os/os_api.h"
#include "server/net_server.h"
#include "server/ctp_server.h"
#include "storage_device.h"
#include "server/simple_mov_unpkg.h"
#include "time.h"
#include "json_c/json.h"


extern void *memmem(const void *__haystack, size_t __haystacklen,
                    const void *__needle, size_t __needlelen);
extern int http_virfile_reg(const char *path, const char *contents, unsigned long len);

#define VIDEO_FILE_LIST_JSON_HEAD  	"{\"file_list\":["
#define VIDEO_FILE_LIST_JSON_END  	"]}"
#define VIDEO_JSON_MEM   "{\"y\":%d,\"f\":\"%s%s\",\"t\":\"%04d%02d%02d%02d%02d%02d\",\"d\":\"%d\",\"h\":%d,\"w\":%d,\"p\":%d,\"s\":\"%d\",\"c\":\"%d\",\"e\":\"%ld\"},"
#define VIDEO_JSON_APP   "{\"op\":\"NOTIFY\",\"param\":{\"status\":\"%d\",\"desc\":\"{\\\"y\\\":%d,\\\"f\\\":\\\"%s%s\\\",\\\"t\\\":\\\"%04d%02d%02d%02d%02d%02d\\\",\\\"d\\\":\\\"%d\\\",\\\"h\\\":%d,\\\"w\\\":%d,\\\"p\\\":%d,\\\"s\\\":\\\"%d\\\",\\\"c\\\":\\\"%d\\\",\\\"e\\\":\\\"%ld\\\"}\"}}"
#define PICTURE_JSON_MEM   "{\"f\":\"%s%s\",\"t\":\"%04d%02d%02d%02d%02d%02d\",\"h\":%d,\"w\":%d,\"s\":\"%d\",\"c\":\"%d\",\"e\":\"%ld\"},"
#define PICTURE_JSON_APP    "{\"op\":\"NOTIFY\",\"param\":{\"desc\":\"{\\\"f\\\":\\\"%s%s\\\",\\\"t\\\":\\\"%04d%02d%02d%02d%02d%02d\\\",\\\"h\\\":%d,\\\"w\\\":%d,\\\"s\\\":\\\"%d\\\",\\\"c\\\":\\\"%d\\\",\\\"e\\\":\\\"%ld\\\"}\"}}"

#define INFO_LEN 256
#define MAX_NUM  600
/* #define VF_LIST_CHECK */ //调试使用


const char __fs_arg[3][15] = {  "-tMOVAVI -st",
                                "-tJPG  -st",
                                "-tMOVAVIJPG -st"
                             };
//链表负载
struct file_info {
    struct list_head entry;
    u8 addlist: 1; //1 已在链表
    u16 id: 15;
    u16 len;
    u8(*fd)[INFO_LEN];
    u32 ftime;//2^32 = 4294967296 --> 2106/2/7 14:28:16, 最多纪录到2106/2/7号
};

#define FILE_INFO_SIZE	sizeof(struct file_info)

struct media_file_info {
    FILE *fd;
    u32  vaild;
    int _attr;
    struct vfs_attr attr;
    struct __mov_unpkg_info info;
    u8 is_emf;//1 紧急文件
    u8 channel; // 前后视区分
    u8 type;// 1MOV视频  2 图片 3 AVI视频  4，，，
    u8 namebuf[64];
    u16 height; //use in jpg
    u16 width; // use in jpg

};



static struct list_head forward_file_list_head = {NULL, NULL};
static struct list_head behind_file_list_head = {NULL, NULL};
static u32 initing = 0, init_flag = 0, file_info_num = 0;
static u8 forward_file_mem[MAX_NUM + 2][INFO_LEN] __attribute__((aligned(32)));
static u8 behind_file_mem[MAX_NUM + 2][INFO_LEN] __attribute__((aligned(32)));
static char emf_path[64] __attribute__((aligned(32)));
static struct file_info file_info_tab[MAX_NUM * 2]  __attribute__((aligned(32)));
static OS_MUTEX file_list_mutex;
static OS_MUTEX file_list_read_mutex;


static int send_json(struct media_file_info *__info, u32 status);

static int file_is_exit(struct file_info *info)
{
    char path[64];
    void *fd = NULL;
    char *begin = NULL, *end = NULL;

    begin = strstr(info->fd, "\"f\":\"");
    if (!begin) {
        goto exit;
    }
    begin += strlen("\"f\":\"");
    end = strchr(begin, '\"');
    if (!end) {
        goto exit;
    }
    memset(path, 0x0, sizeof(path));
    memcpy(path, begin, end - begin);
    /* printf("\n path = %s\n", path);  */
    fd = fopen(path, "r");
    if (!fd) {
        goto exit;
    }
    fclose(fd);
exit:
    return fd == NULL ? 0 : 1;
}


static void file_info_init()
{
    file_info_num = 0;
    memset(file_info_tab, 0, sizeof(file_info_tab));
}

static char *file_is_emf(char *dir, u8 is_emf)
{
    if (is_emf) {
#ifdef CONFIG_EMR_DIR_ENABLE
        memset(emf_path, 0, sizeof(emf_path));
        strcpy((char *)emf_path, dir);
        strcat((char *)emf_path, CONFIG_EMR_REC_DIR);
        return emf_path;
#endif
    }
    return dir;
}

static u8 *__find_emtry_block_f(struct file_info *info)
{
    u8 *str = NULL;
    int i = info->id - 1;
    os_mutex_pend(&file_list_mutex, 0);
    //通过id寻找内存块
    memset(forward_file_mem[i], 0, INFO_LEN);
    str = forward_file_mem[i];
    os_mutex_post(&file_list_mutex);
    return str;
}

static u8 *__find_emtry_block_b(struct file_info *info)
{
    u8 *str = NULL;
    int i = info->id - 1;
#if (defined CONFIG_VIDEO1_ENABLE || defined CONFIG_VIDEO3_ENABLE)
    os_mutex_pend(&file_list_mutex, 0);
    memset(behind_file_mem[i], 0, INFO_LEN);
    str = behind_file_mem[i];
    os_mutex_post(&file_list_mutex);
#endif
    return str;

}

static struct file_info *file_info_find(char behind)
{
    int i;
    int num = 0;
    u32 time = 0xffffffff;
    struct file_info *info = NULL;
    int start = behind ? MAX_NUM : 0;
    int end = behind ? 2 : 1;
    for (i = start; i < end * MAX_NUM; i++) {
        info = (struct file_info *)&file_info_tab[i];
        //如果当前块未被使用
        if (info->id == 0) {
            file_info_num++;
            info->id = i - start + 1;
            ASSERT((info->id > 0 && info->id <= MAX_NUM), "info->id err !!!!");
            return &file_info_tab[i];
        } else if (info->ftime < time) {
            //找到最旧的文件.对应的内存块
            time = info->ftime;
            num = i;
        }
    }
    info->id = num - start + 1;
    ASSERT((info->id > 0 && info->id <= MAX_NUM), "info->id err !!!!");
    return &file_info_tab[num];
}

static size_t forward_write_block(u8 *buffer, size_t len)
{
    const char *str = "\"e\":\"";
    char *pstr;
    struct file_info *info = (struct file_info *)file_info_find(0);

    if (info == NULL) {
        return -1;
    }
    pstr = strstr((const char *)buffer, str);
    if (pstr) {
        pstr += strlen(str);
        info->ftime = strtol((const char *)pstr, NULL, 10);
    } else {
        info->ftime = 0;
    }
    if (info->addlist) {
        info->len = len;
        memset(info->fd, 0, INFO_LEN);
        memcpy(info->fd, buffer, len);
        return len;
    }

    u8 *p = __find_emtry_block_f(info);
    memcpy(p, buffer, len);
    info->len = len;
    info->fd = (u8(*)[INFO_LEN])p;
    info->addlist = 1;
    os_mutex_pend(&file_list_mutex, 0);
    list_add_tail(&info->entry, &forward_file_list_head);
    os_mutex_post(&file_list_mutex);
    return len;

}

static int forward_remove_block(const char *fname)
{
    struct  file_info *__info = NULL;
    struct list_head *pos = NULL, *node = NULL;

    os_mutex_pend(&file_list_mutex, 0);
    if (list_empty(&forward_file_list_head)) {
        os_mutex_post(&file_list_mutex);
        return -1;
    }
    list_for_each_safe(pos, node, &forward_file_list_head) {
        __info = list_entry(pos, struct file_info, entry);
        if (__info) {
            if (strstr((const char *)__info->fd, fname)) {
                memset(__info->fd, 0, __info->len);
                list_del(&__info->entry);
                if (file_info_num > 0) {
                    file_info_num--;
                }
                __info->id = 0;
                __info->addlist = 0;
                os_mutex_post(&file_list_mutex);
                return 0;
            }
        }
    }
    os_mutex_post(&file_list_mutex);
    return -1;
}

static int forward_change_block(const char *fname, char attr)
{
    struct  file_info *__info = NULL;
    struct list_head *pos = NULL, *node = NULL;
    char *str = NULL;

    os_mutex_pend(&file_list_mutex, 0);
    if (list_empty(&forward_file_list_head)) {
        os_mutex_post(&file_list_mutex);
        return -1;
    }
    list_for_each_safe(pos, node, &forward_file_list_head) {
        __info = list_entry(pos, struct file_info, entry);
        if (__info) {
            if (memmem(__info->fd, __info->len, fname, strlen((const char *)fname))) {
                str = memmem(__info->fd, __info->len, "\"y\":", 4);
                printf("attr=%c \n", attr);
                if (str) {
                    *(str + 4) = attr;
                    os_mutex_post(&file_list_mutex);
                    return 0;
                }
            }
        }
    }
    os_mutex_post(&file_list_mutex);
    return -1;
}

static int forward_remove_block_all()
{
    struct  file_info *__info = NULL;
    struct list_head *pos = NULL, *node = NULL;
    os_mutex_pend(&file_list_mutex, 0);
    if (list_empty(&forward_file_list_head)) {
        os_mutex_post(&file_list_mutex);
        return -1;
    }

    list_for_each_safe(pos, node, &forward_file_list_head) {
        __info = list_entry(pos, struct file_info, entry);
        if (__info) {
            list_del(&__info->entry);
            memset(__info->fd, 0, INFO_LEN);
            file_info_num = 0;
            __info->id = 0;
            __info->addlist = 0;
        }
    }
    os_mutex_post(&file_list_mutex);
    return 0;

}


static size_t behind_write_block(u8 *buffer, size_t len)
{
    const char *str = "\"e\":\"";
    char *pstr;
    struct file_info *info = (struct file_info *)file_info_find(1);
    if (info == NULL) {
        return -1;
    }
    pstr = strstr((const char *)buffer, str);
    if (pstr) {
        pstr += strlen(str);
        info->ftime = strtol((const char *)pstr, NULL, 10);
    } else {
        info->ftime = 0;
    }
    if (info->addlist) {
        info->len = len;
        memset(info->fd, 0, INFO_LEN);
        memcpy(info->fd, buffer, len);
        return len;
    }
    u8 *p = __find_emtry_block_b(info);
    memcpy(p, buffer, len);
    info->len = len;
    info->fd = (u8(*)[INFO_LEN])p;
    info->addlist = 1;
    os_mutex_pend(&file_list_mutex, 0);
    list_add_tail(&info->entry, &behind_file_list_head);
    os_mutex_post(&file_list_mutex);
    return len;

}

static int behind_remove_block(const char *fname)
{
    struct  file_info *__info = NULL;
    struct list_head *pos = NULL, *node = NULL;
    os_mutex_pend(&file_list_mutex, 0);
    if (list_empty(&behind_file_list_head)) {
        os_mutex_post(&file_list_mutex);
        return -1;
    }

    list_for_each_safe(pos, node, &behind_file_list_head) {
        __info = list_entry(pos, struct file_info, entry);
        if (__info) {
            if (strstr((const char *)__info->fd, fname)) {
                memset(__info->fd, 0, __info->len);
                list_del(&__info->entry);
                if (file_info_num > 0) {
                    file_info_num--;
                }
                __info->id = 0;
                __info->addlist = 0;
                os_mutex_post(&file_list_mutex);
                return 0;
            }
        }
    }
    os_mutex_post(&file_list_mutex);
    return -1;
}

static int behind_change_block(const char *fname, char attr)
{
    struct  file_info *__info = NULL;
    struct list_head *pos = NULL, *node = NULL;
    char *str = NULL;
    os_mutex_pend(&file_list_mutex, 0);
    if (list_empty(&behind_file_list_head)) {
        os_mutex_post(&file_list_mutex);
        return -1;
    }
    list_for_each_safe(pos, node, &behind_file_list_head) {
        __info = list_entry(pos, struct file_info, entry);
        if (__info) {
            if (memmem(__info->fd, __info->len, fname, strlen(fname))) {
                str = memmem(__info->fd, __info->len, "\"y\":", 4);
                if (str) {
                    *(str + 4) = attr;
                    os_mutex_post(&file_list_mutex);
                    return 0;
                }
            }
        }
    }
    os_mutex_post(&file_list_mutex);
    return -1;
}




static int behind_remove_block_all()
{
    struct  file_info *__info = NULL;
    struct list_head *pos = NULL, *node = NULL;
    os_mutex_pend(&file_list_mutex, 0);
    if (list_empty(&behind_file_list_head)) {
        os_mutex_post(&file_list_mutex);
        return -1;
    }

    list_for_each_safe(pos, node, &behind_file_list_head) {
        __info = list_entry(pos, struct file_info, entry);
        if (__info) {
            list_del(&__info->entry);
            memset(__info->fd, 0, INFO_LEN);
            file_info_num = 0;
            __info->id = 0;
            __info->addlist = 0;
        }
    }
    os_mutex_post(&file_list_mutex);
    return 0;
}

void FILE_REMOVE_ALL()
{
    if (FILE_INITIND_CHECK()) {
        printf("\n %s fail\n", __func__);
        return;
    }
    forward_remove_block_all();
#if defined CONFIG_VIDEO3_ENABLE || defined CONFIG_VIDEO1_ENABLE
    behind_remove_block_all();
#endif
    init_flag = 0;
}

void __FILE_LIST_INIT(u8 is_forward, u32 file_num)
{
    FILE *fd = NULL;
    u32 count = 0;
    u32 flag = 0;
    char res[32];
    char path[64];
    struct vfscan *fs = NULL;
    struct media_file_info media_info;
    memset(res, 0, 32);

#ifdef CONFIG_ENABLE_VLIST
    if (is_forward) {
        strcpy((char *)res, get_rec_path_1());
        INIT_LIST_HEAD(&forward_file_list_head);
        memset(forward_file_mem, 0, (MAX_NUM + 2) * INFO_LEN);
        flag = 0;
    } else {
#if (defined CONFIG_VIDEO1_ENABLE || defined CONFIG_VIDEO3_ENABLE)
        strcpy((char *)res, get_rec_path_2());
        INIT_LIST_HEAD(&behind_file_list_head);
        memset(behind_file_mem, ' ', (MAX_NUM + 2) * INFO_LEN);
        flag = 1;
#else
        printf("\n [waring] no define CONFIG_VIDEO1_ENABLE || CONFIG_VIDEO3_ENABLE\n");
        return;
#endif
    }
    /*延时1s等SD卡稳定再初始化*/
    /* msleep(1000); */
#if   (defined CONFIG_NET_PKG_H264)
    fs = fscan(res, "-tMOVJPG -st", 9);
#elif (defined CONFIG_NET_PKG_JPEG)
    fs = fscan(res, "-tAVIJPG -st", 9);
#else
    fs = fscan(res, "-tJPG -st", 9);
#endif
    if (fs == NULL) {
        printf("\n%s %d fscan fail\n", __func__, __LINE__);
        return;
    }
    //扫描第一个文件
    fd = fselect(fs, FSEL_FIRST_FILE, 0);
    while (1) {
        if (fd == NULL) {
            break;
        }
        media_info.fd = fd;
        media_info.channel = flag;
        media_info.is_emf = 0;
        send_json(&media_info, 0x2);
        if (media_info.fd) {
            fclose(fd);
            media_info.fd = NULL;
        }
        count++;
        fd = fselect(fs, FSEL_NEXT_FILE, 0);
    }

#ifdef  CONFIG_EMR_DIR_ENABLE
    //扫描紧急文件夹
    fscan_release(fs);//先释放上一个
    memset(res, 0, sizeof(res));
    if (is_forward) {
        strcpy((char *)res, get_rec_path_1());
        strcat(res, CONFIG_EMR_REC_DIR);
        flag = 0;
    } else {

#if (defined CONFIG_VIDEO1_ENABLE || defined CONFIG_VIDEO3_ENABLE)
        strcpy((char *)res, get_rec_path_2());
        strcat(res, CONFIG_EMR_REC_DIR);
        flag = 1;
#else
        printf("\n [waring] no define CONFIG_VIDEO1_ENABLE || CONFIG_VIDEO3_ENABLE\n");
        return;
#endif
    }

#if   (defined  CONFIG_NET_PKG_H264 )
    fs = fscan(res, "-tMOVJPG -st");
#elif (defined ONFIG_NET_PKG_JPEG)
    fs = fscan(res, "-tAVIJPG -st");
#else
    fs = fscan(res, "-tJPG -st");
#endif
    if (fs == NULL) {
        printf("\n%s %d fscan fail\n", __func__, __LINE__);
        return;
    }
    count = 0;
    fd = fselect(fs, FSEL_FIRST_FILE, 0);
    while (1) {
        if (fd == NULL) {
            break;
        }
        media_info.fd = fd;
        media_info.channel = flag;
        media_info.is_emf = 1;

        send_json(&media_info, 0x2);
        if (media_info.fd) {
            fclose(fd);
            media_info.fd = NULL;
        }
        count++;
        fd = fselect(fs, FSEL_NEXT_FILE, 0);
    }
#endif
exit:
    fscan_release(fs);
#endif
}


void FILE_DELETE(char *__fname, u8 create_file)
{
    int err;
    u8 fname[128] = {0};
#ifdef CONFIG_ENABLE_VLIST
    /* puts("\nFILE_DELETF\n"); */
    if (FILE_INITIND_CHECK()) {
        printf("\n %s fail\n", __func__);
        /*         if(__fname){ */
        /* printf("\n %s __fname = %s\n",__func__,__fname); */
        /* } */
        return;
    }
    os_mutex_pend(&file_list_read_mutex, 0);
    if (__fname) {
        printf("FILE_DELETE file : %s \n", fname);
        strcpy((char *)fname, __fname);
        forward_remove_block((const char *)fname);
#if defined CONFIG_VIDEO3_ENABLE || defined CONFIG_VIDEO1_ENABLE
        behind_remove_block((const char *)fname);
#endif
    } else {
        printf("FILE_DELETE all file \n");
        forward_remove_block_all();
#if defined CONFIG_VIDEO3_ENABLE || defined CONFIG_VIDEO1_ENABLE
        behind_remove_block_all();
#endif
    }
    os_mutex_post(&file_list_read_mutex);
#endif
}

void FILE_CHANGE_ATTR(const char *fname, char attr)
{
    if (FILE_INITIND_CHECK()) {
        return;
    }
    if (fname == NULL) {
        printf("file name is NULL\n");
        return ;
    }
    if (strstr(fname, get_rec_path_1())) {
        forward_change_block(fname, attr);
    } else if (strstr(fname, get_rec_path_2())) {
#if defined CONFIG_VIDEO3_ENABLE || defined CONFIG_VIDEO1_ENABLE
        behind_change_block(fname, attr);
#endif
    }

}


extern u32 timer_get_ms(void);
void FILE_GEN(void *arg)
{
    FILE *fd = NULL, *fd2 = NULL;
    char *buff = NULL;
    char path[64];
    char buf[128];
    short enter = 0x0a0d;
    int i, j = 0;
    int len, readlen;
    int last_addr = 0;
    json_object *obj;
    struct list_head *pos = NULL, *node = NULL;
    struct  file_info *__info = NULL;
    int ret;

    os_mutex_pend(&file_list_read_mutex, 0);
    u32 t = timer_get_ms();
#ifdef CONFIG_ENABLE_VLIST

    u32 flag = *(u8 *)arg;
    free(arg);

    if (dev_online(SDX_DEV)) {
        os_mutex_pend(&file_list_mutex, 0);
        if (!flag || flag == 1) {
            sprintf(path, "%s%s", get_rec_path_1(), "VF_LIST.txt");
            fd = fopen(path, "r");
            if (fd) {
                fdelete(fd);
                fd = NULL;
            }
            fd = fopen(path, "w+");
            if (!fd) {
                printf("open err %s !!!\n\n", path);
                CTP_CMD_COMBINED(NULL, CTP_REQUEST, "FORWARD_MEDIA_FILES_LIST", "NOTIFY", CTP_REQUEST_MSG);
                goto exit1;
            }
            fwrite(VIDEO_FILE_LIST_JSON_HEAD, 1, strlen(VIDEO_FILE_LIST_JSON_HEAD), fd);
            fwrite(&enter, 1, sizeof(enter), fd);
            list_for_each_safe(pos, node, &forward_file_list_head) {
                __info = list_entry(pos, struct file_info, entry);
                if (__info) {
                    if (FILE_INITIND_CHECK() && !file_is_exit(__info)) {
                        memset(__info->fd, 0, __info->len);
                        list_del(&__info->entry);
                        if (file_info_num > 0) {
                            file_info_num--;
                        }
                        __info->id = 0;
                        __info->addlist = 0;
                        continue;

                    }
                    fwrite(__info->fd, 1, strlen((const char *)__info->fd), fd);
                    fwrite(&enter, 1, sizeof(enter), fd);
                    last_addr = ftell(fd);
                }
            }
            if (last_addr) {
                fseek(fd, last_addr - (sizeof(enter) + 1), SEEK_SET);
            }
            fwrite(&enter, 1, sizeof(enter), fd);
            fwrite(VIDEO_FILE_LIST_JSON_END, 1, strlen(VIDEO_FILE_LIST_JSON_END), fd);
            fclose(fd);
            last_addr = 0;
            pos = NULL;
            node = NULL;
            __info = NULL;

#ifdef VF_LIST_CHECK
redo1:
            /*校验VF_LIST文件*/
            sprintf(path, "%s%s", get_rec_path_1(), "VF_LIST.txt");
            fd = fopen(path, "r");
            ASSERT(fd != NULL, " %s %d check vf_list fail", __func__, __LINE__);
            len = flen(fd);
            buff = zalloc(len + 1);
            ASSERT(buff != NULL, " %s %d check vf_list fail", __func__, __LINE__);
            ret = fread(fd, buff, len);
            ASSERT(ret == len, " %s %d check vf_list fail", __func__, __LINE__);
            obj = json_tokener_parse(buff);
            ASSERT(obj != NULL, " %s %d check vf_list fail", __func__, __LINE__);
            json_object_put(obj);
            free(buff);
            fclose(fd);
#endif
            sprintf(buf, "type:1,path:%s%s", get_rec_path_1(), "VF_LIST.txt");
            CTP_CMD_COMBINED(NULL, CTP_NO_ERR, "FORWARD_MEDIA_FILES_LIST", "NOTIFY", buf);

        }

exit1:
        if (!flag || flag == 2) {
#if defined CONFIG_VIDEO3_ENABLE || defined CONFIG_VIDEO1_ENABLE
            sprintf(path, "%s%s", get_rec_path_2(), "VF_LIST.txt");
            fd2 = fopen(path, "r");
            if (fd2) {
                fdelete(fd2);
            }
            fd2 = fopen(path, "w+");
            if (!fd2) {
                printf("open err %s !!!\n\n", path);
                CTP_CMD_COMBINED(NULL, CTP_REQUEST, "FORWARD_MEDIA_FILES_LIST", "NOTIFY", CTP_REQUEST_MSG);
                goto exit2;
            }
            fwrite(fd2, VIDEO_FILE_LIST_JSON_HEAD, strlen(VIDEO_FILE_LIST_JSON_HEAD));
            fwrite(fd2, &enter, sizeof(enter));
            list_for_each_safe(pos, node, &behind_file_list_head) {
                __info = list_entry(pos, struct file_info, entry);
                if (__info) {
                    if (FILE_INITIND_CHECK() && !file_is_exit(__info)) {
                        memset(__info->fd, 0, __info->len);
                        list_del(&__info->entry);
                        if (file_info_num > 0) {
                            file_info_num--;
                        }
                        __info->id = 0;
                        __info->addlist = 0;
                        continue;
                    }
                    fwrite(fd2, __info->fd, strlen((const char *)__info->fd));
                    fwrite(fd2, &enter, sizeof(enter));
                    last_addr = fpos(fd2);
                }
            }
            if (last_addr) {
                fseek(fd2, last_addr - (sizeof(enter) + 1), SEEK_SET);
            }
            fwrite(fd2, &enter, sizeof(enter));
            fwrite(fd2, VIDEO_FILE_LIST_JSON_END, strlen(VIDEO_FILE_LIST_JSON_END));
            fclose(fd2);

            last_addr = 0;
            pos = NULL;
            node = NULL;
#ifdef VF_LIST_CHECK
redo2:
            //校验
            sprintf(path, "%s%s", get_rec_path_2(), "VF_LIST.txt");
            fd = fopen(path, "r");
            ASSERT(fd != NULL, " %s %d check vf_list fail", __func__, __LINE__);
            len = flen(fd);
            buff = zalloc(len + 1);
            ASSERT(buff != NULL, " %s %d check vf_list fail", __func__, __LINE__);
            ret = fread(fd, buff, len);
            ASSERT(ret == len, " %s %d check vf_list fail", __func__, __LINE__);
            obj = json_tokener_parse(buff);
            ASSERT(obj != NULL, "%s %d check vf_list fail", __func__, __LINE__);
            json_object_put(obj);
            free(buff);
            fclose(fd);
#endif

#if defined CONFIG_VIDEO3_ENABLE || defined CONFIG_VIDEO1_ENABLE
            sprintf(buf, "type:1,path:%s%s", get_rec_path_2(), "VF_LIST.txt");
            CTP_CMD_COMBINED(NULL, CTP_NO_ERR, "BEHIND_MEDIA_FILES_LIST", "NOTIFY", buf);
#endif

#endif

        }

exit2:
        os_mutex_post(&file_list_mutex);
        printf("----update file list\n");
    } else {
        CTP_CMD_COMBINED(NULL, CTP_REQUEST, "FORWARD_MEDIA_FILES_LIST", "NOTIFY", CTP_REQUEST_MSG);
        printf("SD card no ready !!!!!!!!!!\n\n");
    }
#endif
    printf("\n @@@@@@ make list time = %d\n", timer_get_ms() - t);
    os_mutex_post(&file_list_read_mutex);
}

void FILE_LIST_ADD(u32 status, const char *__path, u8 create_file)
{
    u32 len;
    struct media_file_info media_info = {0};
    FILE *fd = NULL;
    u8 flag = 0;
    u8 path[128] = {0};

#ifdef CONFIG_ENABLE_VLIST
    /* printf("FILE_LIST_ADD : %s\n", __path); */
    if (FILE_INITIND_CHECK()) {
        printf("\n %s fail\n", __func__);
        if (__path) {
            printf("\n %s __path = %s\n", __func__, __path);
        }
        return;
    }
    os_mutex_pend(&file_list_read_mutex, 0);

    len = strlen((const char *)__path);
    if (len < 3) {
        return;
    }
    strcpy((char *)path, __path);
    if (!storage_device_ready()) {
        printf("!!storage_device_ready err \n\n");
        return;
    }

    fd = fopen((const char *)path, "r");
    if (fd == NULL) {
        printf("err , open file :%s \n\n", path);
        return;
    }

    if (strstr((const char *)path, get_rec_path_1())) {
        flag = 0;
    } else {
        flag = 1;
    }

    media_info.is_emf = 0;
#ifdef CONFIG_EMR_DIR_ENABLE
    if (strstr((const char *)path, CONFIG_EMR_REC_DIR)) {
        media_info.is_emf = 1;
    }
#endif

    media_info.fd = fd;
    media_info.channel = flag;
    send_json(&media_info, status);
    if (media_info.fd) {
        fclose(fd);
        media_info.fd = NULL;
    }
    os_mutex_post(&file_list_read_mutex);
#endif
}

unsigned short __attribute__((weak))DUMP_PORT()
{
    return 0;
}

unsigned short __attribute__((weak))FORWARD_PORT()
{
    return 0;
}

unsigned short __attribute__((weak))BEHIND_PORT()
{
    return 0;
}

const char *__attribute__((weak))get_rec_path_1()
{
    return NULL;
}
const char *__attribute__((weak))get_rec_path_2()
{
    return NULL;
}
const char *__attribute__((weak))get_root_path()
{
    return NULL;
}

void __FILE_LIST_INIT(u8 is_forward, u32 file_num);

extern int send_ctp_string(int cmd_type, char *buf, char *_req, void *priv);


int get_media_file_info(struct media_file_info *__info)
{
    int ret = 0;
    char path[64];
    if (__info == NULL || __info->fd == NULL) {
        printf("file is not open\n");
        return -1;
    }
    fget_name(__info->fd, __info->namebuf, sizeof(__info->namebuf));
    ret = strlen((const char *)__info->namebuf) - 3;
    fget_attrs(__info->fd, &__info->attr);
    __info->attr.crt_time.sec += 1; //BUG BUG BUG
    fget_attr(__info->fd, &__info->_attr);
    if (!memcmp((__info->namebuf + ret), "MOV", 3)) {
#ifdef CONFIG_NET_PKG_H264
        if (is_vaild_mov_file(__info->fd)) {
            if (0 != read_stts(__info->fd, &__info->info)           ||
                0 != read_time_scale_dur(__info->fd, &__info->info) ||
                0 != read_height_and_length(__info->fd, &__info->info)
               ) {
                sprintf(path, "%s%s", __info->channel ? get_rec_path_2() : get_rec_path_1(), __info->namebuf);
                log_d("need recover path=>%s\n", path);
#if 0
                void *pkg = NULL;
                pkg = pkg_rcv_open(path, "mov");
                if (!pkg) {
                    log_e("rcv_fail\n\n");
                    __info->vaild = 0x0;
                    fdelete(__info->fd);//这种类型的文件多半是损坏的
                    __info->fd = NULL;
                    goto fail;
                }
                pkg_rcv(pkg);
                pkg_rcv_close(pkg);
#endif
            }

            if (__info->_attr & F_ATTR_RO) {
                __info->vaild = 0x2;
            } else {
                __info->vaild = 0x1;
            }
            __info->type = 0x1;
        } else {
            sprintf(path, "%s%s", __info->channel ? get_rec_path_2() : get_rec_path_1(), __info->namebuf);
            log_d("need recover path=>%s\n", path);
#if 0
            void *pkg = NULL;
            pkg = pkg_rcv_open(path, "mov");
            if (!pkg) {
                log_e("rcv_fail\n\n");
                __info->vaild = 0x0;
                fdelete(__info->fd);//这种类型的文件多半是损坏的
                __info->fd = NULL;
                goto fail;
            }
            pkg_rcv(pkg);
            pkg_rcv_close(pkg);
#endif
        }
#endif
    } else if (!memcmp((__info->namebuf + ret), "AVI", 3)) {
#ifdef CONFIG_NET_PKG_JPEG
        __info->type = 0x3;
        u8 state = 0;
        avi_net_preview_unpkg_init(__info->fd, state);
        if (!is_vaild_avi_file(__info->fd, state)) {
            __info->vaild = 0x0;
            printf("warning : %s , not vaild AVI file , delect file..... !!!!\n\n", __info->namebuf);
            goto fail;
        }

        if (avi_get_width_height(__info->fd, (void *)&__info->info, state)) {
            __info->vaild = 0x0;
            __info->fd = NULL;
            printf("warning : %s , not vaild AVI file , delect file..... !!!!\n\n", __info->namebuf);
            goto fail;
        }
        if (__info->_attr & F_ATTR_RO) {
            __info->vaild = 0x2;
        } else {
            __info->vaild = 0x1;
        }
        __info->info.scale = 1;
        __info->info.durition = avi_get_file_time(__info->fd, state);
        __info->info.sample_duration = avi_get_fps(__info->fd, state);
        avi_net_unpkg_exit(__info->fd, state);
#endif
    } else if (!memcmp((__info->namebuf + ret), "JPG", 3)) {
        __info->type = 0x2;
        fseek(__info->fd, 0x174, SEEK_SET);
        fread(&__info->height, 1, 2, __info->fd);
        fread(&__info->width, 1, 2, __info->fd);
        __info->height = lwip_htons(__info->height);
        __info->width  = lwip_htons(__info->width);
    } else {
        printf("FILE NAME:%s err!!!!!!\n", __info->namebuf);
        goto fail;
    }
    return 0;
fail:
    return -1;
}

static void incr_date_time(struct tm *tm_time, int incr)
{
    do {
        if (incr >= 60) {
            tm_time->tm_sec += 60;
            incr -= 60;
        } else {
            tm_time->tm_sec += incr;
            incr = 0;
        }

        if (tm_time->tm_sec > 59) {
            tm_time->tm_sec -= 60;
            tm_time->tm_min += 1;

            if (tm_time->tm_min > 59) {
                tm_time->tm_min -= 60;
                tm_time->tm_hour += 1;

                if (tm_time->tm_hour > 23) {
                    tm_time->tm_hour -= 24;
                    tm_time->tm_mday += 1;

                    if (tm_time->tm_mday > cal_days(tm_time->tm_year, tm_time->tm_mon)) {
                        tm_time->tm_mday = 1;
                        tm_time->tm_mon += 1;

                        if (tm_time->tm_mon > 12) {
                            tm_time->tm_mon = 1;
                            ++tm_time->tm_year;
                        }
                    }
                }
            }
        }
    } while (incr);
}
static int send_json(struct media_file_info *__info, u32 status)
{
    int ret = 0;

    struct tm _tm;
    char *buffer = malloc(256);
    if (buffer == NULL) {
        printf("malloc fail \n");
        return -1;

    }

    if (get_media_file_info(__info)) {
        /* printf("get_media_file_info fail\n"); */
        free(buffer);
        return -1;
    }

    int file_len = flen(__info->fd);
    _tm.tm_year  = __info->attr.crt_time.year - 1900;
    _tm.tm_mon  = __info->attr.crt_time.month - 1;
    _tm.tm_mday  = __info->attr.crt_time.day;
    _tm.tm_hour  = __info->attr.crt_time.hour;
    _tm.tm_min  = __info->attr.crt_time.min;
    _tm.tm_sec  = __info->attr.crt_time.sec;
    /*
    printf("start=%d-%d-%d-%d-%d-%d\n"
           , _tm.tm_year
           , _tm.tm_mon
           , _tm.tm_mday
           , _tm.tm_hour
           , _tm.tm_min
           , _tm.tm_sec);
    	   */
    switch (__info->type) {
    case 0x1:
        incr_date_time(&_tm, __info->info.durition / __info->info.scale);
        ret = sprintf(buffer, VIDEO_JSON_MEM
                      , __info->vaild
                      , __info->channel ? file_is_emf((char *)get_rec_path_2(), __info->is_emf) : file_is_emf((char *)get_rec_path_1(), __info->is_emf)
                      , __info->namebuf
                      , __info->attr.crt_time.year
                      , __info->attr.crt_time.month
                      , __info->attr.crt_time.day
                      , __info->attr.crt_time.hour
                      , __info->attr.crt_time.min
                      , __info->attr.crt_time.sec
                      , __info->info.durition / __info->info.scale
                      , __info->info.height
                      , __info->info.length
                      , __info->info.scale / __info->info.sample_duration
                      , file_len
                      , __info->channel
                      , mktime(&_tm)
                     );

        if (!__info->channel) {
            forward_write_block((u8 *)buffer, ret);
        } else {
            behind_write_block((u8 *)buffer, ret);
        }

        if (status <= 1 && !initing) {
            ret = sprintf(buffer, VIDEO_JSON_APP
                          , status
                          , __info->vaild
                          , __info->channel ? file_is_emf((char *)get_rec_path_2(), __info->is_emf) : file_is_emf((char *)get_rec_path_1(), __info->is_emf)
                          , __info->namebuf
                          , __info->attr.crt_time.year
                          , __info->attr.crt_time.month
                          , __info->attr.crt_time.day
                          , __info->attr.crt_time.hour
                          , __info->attr.crt_time.min
                          , __info->attr.crt_time.sec
                          , __info->info.durition / __info->info.scale
                          , __info->info.height >> 16
                          , __info->info.length >> 16
                          , __info->info.scale / __info->info.sample_duration
                          , file_len
                          , __info->channel
                          , mktime(&_tm)
                         );
            send_ctp_string(CTP_NOTIFY_COMMAND, buffer, "VIDEO_FINISH", NULL);
        } else {
            free(buffer);
        }
        break;
    case 0x2:
        ret = sprintf(buffer, PICTURE_JSON_MEM
                      , __info->channel ? file_is_emf((char *)get_rec_path_2(), __info->is_emf) : file_is_emf((char *)get_rec_path_1(), __info->is_emf)
                      , __info->namebuf
                      , __info->attr.crt_time.year
                      , __info->attr.crt_time.month
                      , __info->attr.crt_time.day
                      , __info->attr.crt_time.hour
                      , __info->attr.crt_time.min
                      , __info->attr.crt_time.sec
                      , __info->height
                      , __info->width
                      , file_len
                      , __info->channel
                      , mktime(&_tm)
                     );

        if (!__info->channel) {
            forward_write_block((u8 *)buffer, ret);
        } else {
            behind_write_block((u8 *)buffer, ret);
        }

        if (status <= 1 && !initing) {
            sprintf(buffer, PICTURE_JSON_APP
                    , __info->channel ? file_is_emf((char *)get_rec_path_2(), __info->is_emf) : file_is_emf((char *)get_rec_path_1(), __info->is_emf)
                    , __info->namebuf
                    , __info->attr.crt_time.year
                    , __info->attr.crt_time.month
                    , __info->attr.crt_time.day
                    , __info->attr.crt_time.hour
                    , __info->attr.crt_time.min
                    , __info->attr.crt_time.sec
                    , __info->height
                    , __info->width
                    , file_len
                    , __info->channel
                    , mktime(&_tm)
                   );
            send_ctp_string(CTP_NOTIFY_COMMAND, buffer, "PHOTO_CTRL", NULL);
        } else {
            free(buffer);
        }
        break;
    case 0x3:
        incr_date_time(&_tm, __info->info.durition / __info->info.scale);
        ret = sprintf(buffer, VIDEO_JSON_MEM
                      , __info->vaild
                      , __info->channel ? file_is_emf((char *)get_rec_path_2(), __info->is_emf) : file_is_emf((char *)get_rec_path_1(), __info->is_emf)
                      , __info->namebuf
                      , __info->attr.crt_time.year
                      , __info->attr.crt_time.month
                      , __info->attr.crt_time.day
                      , __info->attr.crt_time.hour
                      , __info->attr.crt_time.min
                      , __info->attr.crt_time.sec
                      , __info->info.durition / __info->info.scale
                      , __info->info.height
                      , __info->info.length
                      , __info->info.sample_duration
                      , file_len
                      , __info->channel
                      , mktime(&_tm)
                     );

        if (!__info->channel) {
            forward_write_block((u8 *)buffer, ret);
        } else {
            behind_write_block((u8 *)buffer, ret);
        }

        if (status <= 1 && !initing) {
            ret = sprintf(buffer, VIDEO_JSON_APP
                          , status
                          , __info->vaild
                          , __info->channel ? file_is_emf((char *)get_rec_path_2(), __info->is_emf) : file_is_emf((char *)get_rec_path_1(), __info->is_emf)
                          , __info->namebuf
                          , __info->attr.crt_time.year
                          , __info->attr.crt_time.month
                          , __info->attr.crt_time.day
                          , __info->attr.crt_time.hour
                          , __info->attr.crt_time.min
                          , __info->attr.crt_time.sec
                          , __info->info.durition / __info->info.scale
                          , __info->info.height
                          , __info->info.length
                          , __info->info.sample_duration
                          , file_len
                          , __info->channel
                          , mktime(&_tm)
                         );
            send_ctp_string(CTP_NOTIFY_COMMAND, buffer, "VIDEO_FINISH", NULL);//内部已free掉buffer
        } else {
            free(buffer);
        }
        break;
    default:
        free(buffer);
        break;
    }
    return 0;
}
void FILE_LIST_INIT_SMALL(u32 file_num)
{
    __FILE_LIST_INIT(2, file_num);
#if defined CONFIG_VIDEO2_ENABLE || defined CONFIG_VIDEO1_ENABLE
    __FILE_LIST_INIT(3, file_num);
#endif
}

int FILE_LIST_EXIT(void)
{
    if (FILE_INITIND_CHECK()) {
        printf("\n %s fail\n", __func__);
        return -1;
    }
    FILE_DELETE(NULL, 0);



    return 0;
}

int FILE_LIST_INIT(u32 flag)
{
#ifdef CONFIG_ENABLE_VLIST
    os_mutex_pend(&file_list_read_mutex, 0);
    if (initing) {
        printf("File list in doing, Please waiting\n");
        return -1;
    }
    initing = 1;
    file_info_init();
    u32 t = timer_get_ms();
    __FILE_LIST_INIT(1, MAX_NUM);
#if defined CONFIG_VIDEO3_ENABLE || defined CONFIG_VIDEO1_ENABLE
    __FILE_LIST_INIT(0, MAX_NUM);
#endif
    printf("\n @@@@@@ FILE_LIST_INIT = %d\n", timer_get_ms() - t);
    if (flag) {
        void FILE_GEN_ASYNC(u8 flag);
        FILE_GEN_ASYNC(0);
        init_flag = 1;
    }
    initing = 0;
    os_mutex_post(&file_list_read_mutex);
#endif
    return 0;
}



int vf_list(u8 type, u8 isforward, char *dir)
{
    struct vfscan *fs = NULL;
    FILE *vf_fd_forward = NULL;
    FILE *vf_fd_behind = NULL;
    FILE *fd = NULL;
    u32 open_count = 0;
    union vunpkg_req req;
    u8 namebuf[128];
    char *tmp_buf = NULL;
    const char *fs_arg = NULL;
    int err;
    int ret;
    int vaild;
    int file_sz = 0;
    char path[64];
    if (!storage_device_ready()) {
        return -1;
    }

    tmp_buf = (char *)malloc(512);

    if (tmp_buf == NULL) {
        printf("err %s  %d malloc fail\n", __func__, __LINE__);
        return -1;
    }

#if 1

    if (isforward) {

        sprintf(path, "%s%s", get_rec_path_1(), "VF_LIST.txt");
        vf_fd_forward = fopen(path, "w+");

        if (vf_fd_forward == NULL) {
            printf("%s  %d fopen fail\n", __func__, __LINE__);

            return -1;
        }

        file_sz += fwrite(VIDEO_FILE_LIST_JSON_HEAD, 1,  strlen(VIDEO_FILE_LIST_JSON_HEAD), vf_fd_forward);
        strcpy((char *)dir, path);
    } else {

        sprintf(path, "%s%s", get_rec_path_2(), "VF_LIST.txt");
        vf_fd_behind = fopen(path, "w+");

        if (vf_fd_behind == NULL) {
            printf("%s  %d fopen fail\n", __func__, __LINE__);

            return -1;
        }

        file_sz += fwrite(VIDEO_FILE_LIST_JSON_HEAD, 1, strlen(VIDEO_FILE_LIST_JSON_HEAD), vf_fd_behind);
        strcpy((char *)dir, path);
    }

#endif
    //copy double dir
    switch (type) {
    case VID_JPG:
        fs_arg = __fs_arg[2];
        break;
    case VIDEO:
        fs_arg = __fs_arg[0];
        break;
    case JPG:
        fs_arg = __fs_arg[1];
        break;
    default:
        printf("%s:%d type err \n\n", __func__, __LINE__);
        break;

    }

    if (isforward) {
        fs = fscan(get_rec_path_1(), fs_arg, 9);

        while (1) {
            if (fd == NULL) {
                fd = fselect(fs, FSEL_FIRST_FILE, 0);
            } else {
                fd = fselect(fs, FSEL_NEXT_FILE, 0);
            }

            if (fd == NULL) {
                printf("open_count->%d fs_arg->%s\n", open_count, fs_arg);
                if (open_count > 0) {
                    fseek(vf_fd_forward, file_sz - 1, SEEK_SET);
                }


                fwrite("]}", 1, 2, vf_fd_forward);
                fclose(vf_fd_forward);
                fscan_release(fs);
                malloc_stats();
                free(tmp_buf);
                return type;
                /* break; */
            }

            fget_name(fd, namebuf, 128);
            printf("---> file name : %s \n", namebuf);
            ret = strlen((const char *)namebuf) - 3;
            struct vfs_attr attr;
            int _attr;
            fget_attrs(fd, &attr);
            fget_attr(fd, &_attr);

            if (!memcmp((namebuf + ret), "mov", 3)) {

#ifdef CONFIG_NET_PKG_H264
                struct __mov_unpkg_info info;

                if (is_vaild_mov_file(fd)) {
                    if (read_stts(fd, &info) != 0) {
                        vaild = 0;
                        fclose(fd);
                        continue;

                    }
                    if (0 != read_time_scale_dur(fd, &info)) {
                        vaild = 0;
                        fclose(fd);
                        continue;
                    }
                    if (0 != read_height_and_length(fd, &info)) {
                        vaild = 0;
                        fclose(fd);
                        continue;
                    }
                    vaild = 1;

                    if (_attr & F_ATTR_RO) {
                        vaild = 2;
                    }
                } else {
                    FILE_DELETE((char *)namebuf, 0);
                    fdelete(fd);
                    vaild = 0;
                    /*fclose(fd);*/
                    continue;
                }

                ret = sprintf(tmp_buf, "{\"y\":%d,\"f\":\"%s%s\",\"t\":\"%04d%02d%02d%02d%02d%02d\",\"d\":\"%d\",\"h\":%d,\"w\":%d,\"p\":%d,\"s\":\"%d\"},"
                              , vaild
                              , get_rec_path_1()
                              , namebuf
                              , attr.crt_time.year
                              , attr.crt_time.month
                              , attr.crt_time.day
                              , attr.crt_time.hour
                              , attr.crt_time.min
                              , attr.crt_time.sec
                              , info.durition / info.scale
                              , info.height >> 16
                              , info.length >> 16
                              , info.scale / info.sample_duration
                              , flen(fd)
                             );
#endif
            } else if (!memcmp((namebuf + ret), "AVI", 3)) {
#ifdef CONFIG_NET_PKG_JPEG
                struct __mov_unpkg_info info;
                u8 state = 0;
                avi_net_preview_unpkg_init(fd, state);
                if (!is_vaild_avi_file(fd, state)) {
                    vaild = 0x0;
                    printf("err : vf_list not vaild AVI file , and delect this file!!!!\n\n");
                    FILE_DELETE((char *)namebuf, 0);
                    fdelete(fd);
                    /*fclose(fd);*/
                    continue;
                }
                if (avi_get_width_height(fd, (void *)&info, state)) {
                    vaild = 0x0;
                    printf("err : vf_list read AVI width and heigt fail !!!!\n\n");
                    fclose(fd);
                    continue;
                }
                if (_attr & F_ATTR_RO) {
                    vaild = 0x2;
                } else {
                    vaild = 0x1;
                }
                info.scale = 1;
                info.durition = avi_get_file_time(fd, state);
                info.sample_duration = avi_get_fps(fd, state);
                avi_net_unpkg_exit(fd, state);
                ret = sprintf(tmp_buf, "{\"y\":%d,\"f\":\"%s%s\",\"t\":\"%04d%02d%02d%02d%02d%02d\",\"d\":\"%d\",\"h\":%d,\"w\":%d,\"p\":%d,\"s\":\"%d\"},"
                              , vaild
                              , get_rec_path_1()
                              , namebuf
                              , attr.crt_time.year
                              , attr.crt_time.month
                              , attr.crt_time.day
                              , attr.crt_time.hour
                              , attr.crt_time.min
                              , attr.crt_time.sec
                              , info.durition / info.scale
                              , info.height
                              , info.length
                              , info.sample_duration
                              , flen(fd)
                             );
#endif
            } else {
#if 1
                //  puts("picture\n");
                u16 height, width;
                fseek(fd, 0x174, SEEK_SET);
                fread(&height, 1, 2, fd);
                fread(&width, 1, 2, fd);
                height = lwip_htons(height);
                width  = lwip_htons(width);
                ret = sprintf(tmp_buf, "{\"f\":\"%s%s\",\"t\":\"%04d%02d%02d%02d%02d%02d\",\"h\":%d,\"w\":%d,\"s\":\"%d\"},"
                              , get_rec_path_1()
                              , namebuf
                              , attr.crt_time.year
                              , attr.crt_time.month
                              , attr.crt_time.day
                              , attr.crt_time.hour
                              , attr.crt_time.min
                              , attr.crt_time.sec
                              , height
                              , width
                              , flen(fd)
                             );
#endif
            }

            open_count++;
#if 1
            file_sz += fwrite(tmp_buf, 1, ret, vf_fd_forward);
#endif
            fclose(fd);
        }

    } else {

//////////////////////////////////////////////////////////////
//////////////////////////2号文件夹////////////////////////////////////
#if 1
        open_count = 0;
        fs = fscan(get_rec_path_2(), fs_arg, 9);

        while (1) {
            if (fd == NULL) {
                fd = fselect(fs, FSEL_FIRST_FILE, 0);
            } else {
                fd = fselect(fs, FSEL_NEXT_FILE, 0);
            }


            if (fd == NULL) {
                if (open_count > 0) {
                    fseek(vf_fd_behind, file_sz - 1, SEEK_SET);
                } else {
                    return NONE;
                }

                fwrite("]}", 1, 2, vf_fd_behind);
                fclose(vf_fd_behind);
                fscan_release(fs);
                return type;
            }

            fget_name(fd, namebuf, 128);
            printf("---> file name : %s \n", namebuf);
            int _attr;
            struct vfs_attr attr;
            fget_attrs(fd, &attr);

            fget_attr(fd, &_attr);
            if (!memcmp((namebuf + ret), "mov", 3)) {
                struct __mov_unpkg_info info;

#ifdef CONFIG_NET_PKG_H264
                if (is_vaild_mov_file(fd)) {
                    read_stts(fd, &info);
                    read_time_scale_dur(fd, &info);
                    read_height_and_length(fd, &info);
                    vaild = 1;

                    if (_attr & F_ATTR_RO) {
                        vaild = 2;
                    }

                } else
#endif
                {

                    vaild = 0;
                    FILE_DELETE((char *)namebuf, 0);
                    fdelete(fd);
                    /*fclose(fd);*/
                    continue;
                }

                ret = sprintf(tmp_buf, "{\"y\":%d,\"f\":\"%s%s\",\"t\":\"%04d%02d%02d%02d%02d%02d\",\"d\":\"%d\",\"h\":%d,\"w\":%d,\"p\":%d,\"s\":\"%d\"},"
                              , vaild
                              , get_rec_path_2()
                              , namebuf
                              , attr.crt_time.year
                              , attr.crt_time.month
                              , attr.crt_time.day
                              , attr.crt_time.hour
                              , attr.crt_time.min
                              , attr.crt_time.sec
                              , info.durition / info.scale
                              , info.height >> 16
                              , info.length >> 16
                              , info.scale / info.sample_duration
                              , flen(fd)
                             );
            } else if (!memcmp((namebuf + ret), "AVI", 3)) {
#ifdef CONFIG_NET_PKG_JPEG
                struct __mov_unpkg_info info;
                u8 state = 0;
                avi_net_preview_unpkg_init(fd, state);
                if (!is_vaild_avi_file(fd, state)) {
                    vaild = 0x0;
                    printf("err : vf_list not vaild AVI file !!!!\n\n");
                    FILE_DELETE((char *)namebuf, 0);
                    fdelete(fd);
                    /*fclose(fd);*/
                    continue;
                }
                if (avi_get_width_height(fd, (void *)&info, state)) {
                    vaild = 0x0;
                    printf("err : vf_list read AVI width and heigt fail !!!!\n\n");
                    fclose(fd);
                    continue;
                }
                if (_attr & F_ATTR_RO) {
                    vaild = 0x2;
                } else {
                    vaild = 0x1;
                }
                info.scale = 1;
                info.durition = avi_get_file_time(fd, state);
                info.sample_duration = avi_get_fps(fd, state);
                avi_net_unpkg_exit(fd, state);
                ret = sprintf(tmp_buf, "{\"y\":%d,\"f\":\"%s%s\",\"t\":\"%04d%02d%02d%02d%02d%02d\",\"d\":\"%d\",\"h\":%d,\"w\":%d,\"p\":%d,\"s\":\"%d\"},"
                              , vaild
                              , get_rec_path_2()
                              , namebuf
                              , attr.crt_time.year
                              , attr.crt_time.month
                              , attr.crt_time.day
                              , attr.crt_time.hour
                              , attr.crt_time.min
                              , attr.crt_time.sec
                              , info.durition / info.scale
                              , info.height
                              , info.length
                              , info.sample_duration
                              , flen(fd)
                             );
#endif
            } else {
                //       puts("picture\n");
                u16 height, width;
                fseek(fd, 0x174, SEEK_SET);
                fread(&height, 1, 2, fd);
                fread(&width, 1, 2, fd);
                height = lwip_htons(height);
                width  = lwip_htons(width);
                ret = sprintf(tmp_buf, "{\"f\":\"%s%s\",\"t\":\"%04d%02d%02d%02d%02d%02d\",\"h\":%d,\"w\":%d,\"s\":\"%d\"},"
                              , get_rec_path_2()
                              , namebuf
                              , attr.crt_time.year
                              , attr.crt_time.month
                              , attr.crt_time.day
                              , attr.crt_time.hour
                              , attr.crt_time.min
                              , attr.crt_time.sec
                              , height
                              , width
                              , flen(fd)
                             );
            }

            open_count++;
            file_sz += fwrite(tmp_buf, 1, ret, vf_fd_behind);
            fclose(fd);
        }

#endif
    }

    return NONE;
}



void FILE_TEST()
{
    puts("FILE_TEST\n\n\n\n");
    __FILE_LIST_INIT(1, MAX_NUM);
    __FILE_LIST_INIT(0, MAX_NUM);
    puts("FILE_TEST\n\n\n\n");
}
int FILE_INITIND_CHECK()
{
    return initing;
}

//根据现有链表生成文件
void FILE_GEN_ASYNC(u8 flag)
{
#ifdef CONFIG_ENABLE_VLIST
    static u32 count = 0;
    char buf[32];
    void *__flag = malloc(sizeof(u32));
    if (!__flag) {
        CTP_CMD_COMBINED(NULL, CTP_REQUEST, "FORWARD_MEDIA_FILES_LIST", "NOTIFY", CTP_REQUEST_MSG);
        printf("\n [ %s - %d ] fail\n", __func__, __LINE__);
        return;
    }
    *(u32 *)__flag = flag;
    sprintf(buf, "file_gen_thread%d", count++);
    thread_fork(buf, 28, 0x1000, 0, 0, FILE_GEN, __flag);
#endif
}
//暂不支持异步创建
#if 1
void file_list_thread(void *arg)
{
    /*生成文件列表*/
    if (storage_device_ready()) {
#if defined CONFIG_ENABLE_VLIST
        FILE_LIST_INIT(*(u32 *)arg);
#endif
    }
    free(arg);
}

/*异步生成文件列表*/
void FILE_LIST_IN_MEM(u32 flag)
{
#ifdef CONFIG_ENABLE_VLIST
    char buf[32];
    static u32 count = 0;
    void *__flag = malloc(sizeof(u32));
    if (!__flag) {
        printf("\n [ %s - %d ] fail\n", __func__, __LINE__);
        return;
    }
    *(u32 *)__flag = flag;
    sprintf(buf, "file_list_thread%d", count++);
    thread_fork(buf, 28, 0x1000, 0, 0, file_list_thread, __flag);
#endif
}
#endif
/*同步生成文件列表*/
void FILE_LIST_SCAN(void)
{
    if (storage_device_ready()) {
#if defined CONFIG_ENABLE_VLIST
        FILE_LIST_INIT(1);
#endif
    }
}

int vflist_mutex_init(void)
{
    int err;
    err = os_mutex_create(&file_list_mutex);
    if (err != OS_NO_ERR) {
        printf("os mutex create fail \n");
        initing = 0;
        return -1;
    }
    err = os_mutex_create(&file_list_read_mutex);
    if (err != OS_NO_ERR) {
        printf("os fiile mutex create fail \n");
        initing = 0;
        return -1;
    }
    INIT_LIST_HEAD(&forward_file_list_head);
    INIT_LIST_HEAD(&behind_file_list_head);
    return 0;
}
late_initcall(vflist_mutex_init);
#endif
