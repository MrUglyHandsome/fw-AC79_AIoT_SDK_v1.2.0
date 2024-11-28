#include "sock_api/sock_api.h"
#include "os/os_api.h"
#include "server/net_server.h"
#include "server/server_core.h"
#include "server/ctp_server.h"
#include "server/simple_mov_unpkg.h"
#include "server/rt_stream_pkg.h"
#include "fs/fs.h"
#include "generic/list.h"
#include "packet.h"

#ifdef CONFIG_ENABLE_VLIST

#define VIDEO_PLAYBACK_TASK_NAME "video_playback"
#define VIDEO_PLAYBACK_TASK_STK  0x300
#define VIDEO_PLAYBACK_TASK_PRIO 10

struct __playback {
    u32 id;
    u8 state;
    void *video_playback_sock_hdl;
    int (*cb)(void *priv, u8 *data, size_t len);
    u8 kill_flag;
    OS_MUTEX mutex;
    struct list_head cli_head;
};

struct __playback playback_info;
#define playback_info_hander (&playback_info)
static u32 cout = 0;//thread_fork name;

struct __playback_cli_info {
    struct list_head entry;
    u32 msec;
    u32 state;
    struct sockaddr_in remote_addr;
    int pid;
    u8 stop;
    u8 kill_flag;
    u16 fast_ctrl;
    u32 tmp_is_30fps;
    struct __packet_info pinfo;
    OS_SEM   sem;

};
//找I帧第2方式，根据帧数
#ifdef CONFIG_NET_H264
static int find_idr_frame2(struct __packet_info *pinfo, u32 offset)
{
    int i = 0;
    char buf[5];
    int  sample_offset;
    int sample_size;
    int ret;

    i = offset;
    while (1) {

        if (i >= pinfo->info.video_sample_count) {
            //返回之后，会发结束帧,才不会导致APP快进花屏
            return pinfo->info.video_sample_count;
        }
        sample_size = get_sample_size(pinfo->info.stsz_tab, i);
        if (sample_size == -1) {
            printf("\n[Error] %s %d\n", __func__, __LINE__);
            return pinfo->info.video_sample_count;
        }
        sample_offset = get_chunk_offset(pinfo->info.stco_tab, i);
        if (sample_offset == -1) {
            printf("\n[Error] %s %d\n", __func__, __LINE__);
            return pinfo->info.video_sample_count;
        }
        if (false == fseek(pinfo->fd, sample_offset, SEEK_SET)) {
            printf("\n[Error] %s %d\n", __func__, __LINE__);
            return pinfo->info.video_sample_count;
        }
        ret = fread(pinfo->fd, buf, 5);
        if (ret != 5) {
            printf("\n[Error] %s %d\n", __func__, __LINE__);
            return pinfo->info.video_sample_count;
        }
        if (buf[4] != 0x67) {
            i++;
        } else {
            return i;
        }
    }
}
#endif

static int find_idr_frame(struct __packet_info *pinfo, u32 offset)
{
    int i = 0;
    char buf[5];
    int sample_offset;
    int sample_size;
    int ret;

#if (defined CONFIG_NET_H264)
    i = offset * pinfo->info.scale / (pinfo->info.sample_duration * 1000);
    while (1) {
        if (i >= pinfo->info.video_sample_count) {
            return pinfo->info.video_sample_count - 1;
        }
        sample_size = get_sample_size(pinfo->info.stsz_tab, i);
        if (sample_size == -1) {
            return -1;
        }
        sample_offset = get_chunk_offset(pinfo->info.stco_tab, i);
        if (sample_offset == -1) {
            return -1;
        }
        if (false == fseek(pinfo->fd, sample_offset, SEEK_SET)) {
            return -1;
        }
        ret = fread(pinfo->fd, buf, 5);
        if (ret != 5) {
            return -1;
        }
        if (buf[4] != 0x67) {
            i--;
        } else {
            return i;
        }
    }
#elif (defined CONFIG_NET_JPEG)
    return  avi_get_video_num(pinfo->fd, offset, pinfo->state); //返回seq
#endif
}

static int send_date_end_packet(struct __playback_cli_info *cli, u32 i)
{
    if (cli->tmp_is_30fps != i) {
        return send_date_packet(&cli->pinfo, 1);
    } else {
        return 0;
    }
}

static int send_date_per_fps(struct __playback_cli_info *cli, u32 i)
{
    int ret = 0;
    u32 sec = 0;
    int fps;
    u32 r;

#if (defined CONFIG_NET_H264)
    fps = cli->pinfo.info.scale / cli->pinfo.info.sample_duration;
#elif (defined CONFIG_NET_JPEG)
    fps = avi_get_fps(cli->pinfo.fd, cli->pinfo.state);
#endif


    sec = (i - cli->tmp_is_30fps) / fps;
    if (sec) {

        ret = send_date_packet(&cli->pinfo, sec);
#if (defined CONFIG_GPS_ENABLE)
        send_gps_data_packet(&cli->pinfo);
#endif
        r = (i - cli->tmp_is_30fps) % (fps);
        cli->tmp_is_30fps = i - r;
    }
    return ret;
}

static void video_playback_cli_thread(void *arg)
{
    int ret;
    FILE *fd;
    u8 name[32];
    char buf[64];
    int i = 0, j = 0;
    u32 msec = 0;
    int last_video_num = 0;
    u32 fast_num = 0;
    struct __playback_cli_info *cli = (struct __playback_cli_info *)arg;

    puts("start  video_playback_cli_thread\n");

    cli->pinfo.len = IMAGE_SIZE;
    cli->pinfo.data = (u8 *)zalloc(IMAGE_SIZE);
    if (cli->pinfo.data == NULL) {
        printf("\n[Error] %s %d malloc fali\n", __func__, __LINE__);
        CTP_CMD_COMBINED(NULL, CTP_OPEN_FILE, "TIME_AXIS_PLAY", "NOTIFY", buf);
        goto err1;
    }
    cli->pinfo.fd = fopen(cli->pinfo.file_name, "r");
    cli->fast_ctrl = 0;
    cli->pinfo.state = 1;//回放模式

    if (cli->pinfo.fd == NULL) {
        printf("\n[Error] %s %d FILE open fali\n", __func__, __LINE__);
        fget_name(cli->pinfo.fd, name, 32);
        sprintf(buf, "path:%s", name);
        CTP_CMD_COMBINED(NULL, CTP_OPEN_FILE, "TIME_AXIS_PLAY", "NOTIFY", buf);
        goto err1;
    }

#if (defined CONFIG_NET_H264)
    if (!is_vaild_mov_file(cli->pinfo.fd)) {
        printf("\n[Error] %s %d FILE is vaild\n", __func__, __LINE__);
        fget_name(cli->pinfo.fd, name, 32);
        sprintf(buf, "path:%s", name);
        CTP_CMD_COMBINED(NULL, CTP_OPEN_FILE, "TIME_AXIS_PLAY", "NOTIFY", buf);
        goto err1;
    }
    cli->pinfo.type = H264_TYPE_VIDEO;

#elif (defined CONFIG_NET_JPEG)
    avi_net_playback_unpkg_init(cli->pinfo.fd, cli->pinfo.state);
    if (!is_vaild_avi_file(cli->pinfo.fd, cli->pinfo.state)) {
        printf("\n[Error] %s %d FILE is vaild\n", __func__, __LINE__);
        fget_name(cli->pinfo.fd, name, 32);
        sprintf(buf, "path:%s", name);
        CTP_CMD_COMBINED(NULL, CTP_OPEN_FILE, "TIME_AXIS_PLAY", "NOTIFY", buf);
        goto err1;
    }
    cli->pinfo.type = JPEG_TYPE_VIDEO;
#endif

#if (defined CONFIG_GPS_ENABLE)
    if (find_gps_data(&cli->pinfo) < 0) {
        printf("\n[Error] %s %d  no gps data\n", __func__, __LINE__);
    }
#endif

    if (get_video_media_info(&cli->pinfo)) {
        printf("\n[Error] %s %d  get media info fail\n", __func__, __LINE__);
        goto err1;
    }
    i = find_idr_frame(&cli->pinfo, cli->msec);
    if (send_media_packet(&cli->pinfo) <= 0) {
        printf("\n[Error] %s %d  send media info fail\n", __func__, __LINE__);
        CTP_CMD_COMBINED(NULL, CTP_NET_ERR, "TIME_AXIS_PLAY", "NOTIFY", CTP_NET_ERR_MSG);
        goto err1;

    }
    incr_date_time(&cli->pinfo.time, cli->msec / 1000);
    cli->tmp_is_30fps = i;

#if (defined CONFIG_NET_H264)
    /*获取音频帧*/
    j = i / (cli->pinfo.info.video_sample_count / cli->pinfo.info.audio_chunk_num);
#elif (defined CONFIG_NET_JPEG)
    j = avi_video_base_to_get_audio_frame(i, cli->pinfo.state);
#endif

    while (1) {
        if (cli->stop) {
            os_sem_pend(&cli->sem, 0);
        }
        if (cli->kill_flag) {
            printf("[Msg] kill %s\n", __func__);
            goto err1;
        }

#if (defined CONFIG_NET_H264)
        if (cli->pinfo.info.video_sample_count && cli->pinfo.info.audio_chunk_num) {
            if ((i % (cli->pinfo.info.video_sample_count / cli->pinfo.info.audio_chunk_num) == 0)
                && j < cli->pinfo.info.audio_chunk_num) {
                if (!cli->fast_ctrl) {
                    //printf("@@@@@@ audio seq = %d\n",i);
                    ret = send_audio_packet(&cli->pinfo, j);
                    if (ret <= 0) {
                        goto err1;
                    }
                }

                j++;
            }
        }
#elif (defined CONFIG_NET_JPEG)
        if (cli->pinfo.info.video_sample_count && (i >= last_video_num || fast_num > 0)) {
            if (!cli->fast_ctrl) {
                if (j < cli->pinfo.info.audio_chunk_num && cli->pinfo.info.audio_chunk_num) {
                    if (fast_num) {
                        j = avi_video_base_to_get_audio_frame(i, cli->pinfo.state);
                        last_video_num = avi_audio_base_to_get_video_frame(j - 1, cli->pinfo.state);
                        if (i < last_video_num) {
                            i = last_video_num;
                        }
                        fast_num = 0;
                    }
                    ret = send_audio_packet(&cli->pinfo, j);
                    if (ret <= 0) {
                        goto send_video;
                    }
                    last_video_num = avi_audio_base_to_get_video_frame(j, cli->pinfo.state);
                    j++;
                }
            }
        }
#endif

send_video:

        if (i >= cli->pinfo.info.video_sample_count) {
next:
            cli->pinfo.fd = play_next(&cli->pinfo);//自动播放下一个文件
            send_end_packet(&cli->pinfo);
            if (cli->pinfo.fd == NULL) {
                printf("[Msg] video play back end\n\n");
                goto err1;
            }

            update_data(&cli->pinfo);
            cli->tmp_is_30fps = 0;
            i = 0;
            j = 0;
            last_video_num = 0;
#if (defined CONFIG_GPS_ENABLE)
            if (find_gps_data(&cli->pinfo) < 0) {
                printf("[Error] mov gps data err \n");
            }
#endif
            send_media_packet(&cli->pinfo);
            os_time_dly(50);
            if (cli->kill_flag) {//防止下载视频的时候自动播放下一个视频导致报错 by yyj
                printf("[Msg] kill %s\n", __func__);
                goto err1;
            }
        }


        ret = send_video_packet(&cli->pinfo, i);
        if (ret < 0) {
            CTP_CMD_COMBINED(NULL, CTP_NET_ERR, "TIME_AXIS_PLAY", "NOTIFY", CTP_NET_ERR_MSG);
            puts("send vidoe err!\n");
            goto err1;
        } else if (ret == 0) {
            goto next;
        }
        send_date_per_fps(cli, i);
        if (!cli->fast_ctrl) {
            i++;
        } else {
#if (defined CONFIG_NET_H264)
            i += cli->fast_ctrl;
            i = find_idr_frame2(&cli->pinfo, i);
            printf("\n i = %d\n", i);
            j = i / (cli->pinfo.info.video_sample_count / cli->pinfo.info.audio_chunk_num);
            j--;
#elif (defined CONFIG_NET_JPEG)
            i += cli->fast_ctrl / 16;
            fast_num += cli->fast_ctrl / 16;
#endif

        }
    }

err1:
#if (defined CONFIG_GPS_ENABLE)
    unfind_gps_data();
#endif

#if (defined CONFIG_NET_JPEG)
    avi_net_unpkg_exit(cli->pinfo.fd, cli->pinfo.state);
#endif
    if (cli->pinfo.info.stco_tab != NULL) {
        free(cli->pinfo.info.stco_tab);
        cli->pinfo.info.stco_tab = NULL;
    }
    if (cli->pinfo.info.stsz_tab != NULL) {
        free(cli->pinfo.info.stsz_tab);
        cli->pinfo.info.stsz_tab = NULL;
    }
    if (cli->pinfo.info.audio_stco_tab != NULL) {
        free(cli->pinfo.info.audio_stco_tab);
        cli->pinfo.info.audio_stco_tab = NULL;
    }
    if (cli->pinfo.fd != NULL) {
        fclose(cli->pinfo.fd);
        cli->pinfo.fd = NULL;
    }
    if (cli->pinfo.data) {
        free(cli->pinfo.data);
    }
    os_mutex_pend(&playback_info_hander->mutex, 0);
    list_del(&cli->entry);
    sock_unreg(cli->pinfo.sock);
    cli->pinfo.data = NULL;
    cli->kill_flag = 0;
    free(cli);
    os_mutex_post(&playback_info_hander->mutex);
}


int video_playback_req_handler2(void *msg)
{
    void *sock_hdl;
    char buf[128];
    char name[32];
    struct sockaddr_in remote_addr;
    fd_set rdset;
    struct timeval tv = {5, 0};
    int ret = 0;

    if (msg == NULL) {
        printf("\n[Error] %s msg == NULL\n", __func__);
        return -1;
    }

    struct net_req *req = (struct net_req *)msg;
    socklen_t addrlen = sizeof(remote_addr);
    sprintf(buf, "status:%d", 0);
    CTP_CMD_COMBINED(NULL, CTP_NO_ERR, "TIME_AXIS_PLAY", "NOTIFY", buf);

    while (1) {
        FD_ZERO(&rdset);
        FD_SET(sock_get_socket(playback_info_hander->video_playback_sock_hdl), &rdset);

        ret = sock_select(playback_info_hander->video_playback_sock_hdl, &rdset, NULL, NULL, &tv);
        if (ret < 0) {
            goto err1;
        } else if (ret == 0) {
            printf("[MSG] accept time out\n");
            CTP_CMD_COMBINED(NULL, CTP_REQUEST, "TIME_AXIS_PLAY", "NOTIFY", CTP_REQUEST_MSG);
            free(req);
            return -1;
        } else {
            sock_hdl = sock_accept(playback_info_hander->video_playback_sock_hdl, (struct sockaddr *)&remote_addr, &addrlen, NULL, NULL);
            if (!sock_hdl) {
                printf("%s ::%d sock_accept \n", __func__, __LINE__);
                goto err1;
            }
            struct __playback_cli_info *cli = zalloc(sizeof(struct __playback_cli_info));
            if (!cli) {
                printf("%s ::%d calloc \n", __func__, __LINE__);
                goto err1;
            }
            strcpy(cli->pinfo.file_name, req->playback.file_name);
            cli->msec = req->playback.msec;
            cli->pinfo.sock = sock_hdl;
            printf("old req->playback.filename : %s sec:%d\n", req->playback.file_name, req->playback.msec);
            printf("new req->playback.filename : %s sec:%d\n", cli->pinfo.file_name, cli->msec);
            cli->pinfo.data = NULL;
            cli->pinfo.len = 0;
            memcpy(&cli->remote_addr, &remote_addr, sizeof(struct sockaddr_in));
            os_sem_create(&cli->sem, 0);
            os_mutex_pend(&playback_info_hander->mutex, 0);
            list_add_tail(&cli->entry, &playback_info_hander->cli_head);
            os_mutex_post(&playback_info_hander->mutex);
            sprintf(name, "playback_cli_%d", cout++);
            ret = thread_fork(name, 25, 1024, 0, &cli->pid, video_playback_cli_thread, (void *)cli);
            if (ret != OS_NO_ERR) {
                printf("[Error] %s thread fork err =%d\n", __func__, ret);
                sock_unreg(sock_hdl);
                free(cli);
                goto err1;
            }
            free(req);
            return 0;

        }
    }
err1:
    free(req);
    sock_unreg(playback_info_hander->video_playback_sock_hdl);
    return -1;
}
static void time_axis_play_thread(void *arg)
{
    video_playback_req_handler2(arg);
}

int video_playback_post_msg(struct net_req *_req)
{
    int ret = 0;
    static u32 count = 0;
    char buf[64];
    struct net_req *req = (struct net_req *)calloc(1, sizeof(struct net_req));
    if (req == NULL) {
        return -1;
    }
    memcpy(req, _req, sizeof(struct net_req));
    sprintf(buf, "time_axis_play_thread%d", count++);
    ret = thread_fork(buf, 25, 0x1000, 0, 0, time_axis_play_thread, (void *)req);
    if (ret != OS_NO_ERR) {
        free(req);
        return -1;
    }
    return 0;
}

int playback_init(u16 port, int callback(void *priv, u8 *data, size_t len))
{
    puts("playback_init\n");
    int ret;
    struct sockaddr_in dest_addr;
    playback_info_hander->video_playback_sock_hdl = sock_reg(AF_INET, SOCK_STREAM, 0, NULL, NULL);

    if (playback_info_hander->video_playback_sock_hdl == NULL) {
        printf("%s %d->Error in socket()\n", __func__, __LINE__);
        goto EXIT;
    }

    if (sock_set_reuseaddr(playback_info_hander->video_playback_sock_hdl)) {
        printf("%s %d->Error in sock_set_reuseaddr(),errno=%d\n", __func__, __LINE__, errno);
        goto EXIT;
    }

    dest_addr.sin_family = AF_INET;
    dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr.sin_port = htons(port);
    puts("playback_init\n");
    ret = sock_bind(playback_info_hander->video_playback_sock_hdl, (struct sockaddr *)&dest_addr, sizeof(struct sockaddr));

    if (ret) {
        printf("%s %d->Error in bind(),errno=%d\n", __func__, __LINE__, errno);
        goto EXIT;
    }

    ret = sock_listen(playback_info_hander->video_playback_sock_hdl, 0xff);

    if (ret) {
        printf("%s %d->Error in listen()\n", __func__, __LINE__);
        goto EXIT;
    }


    if (callback == NULL) {
        playback_info_hander->cb = NULL;
    } else {
        playback_info_hander->cb = callback;
    }

    os_mutex_create(&playback_info_hander->mutex);
    INIT_LIST_HEAD(&playback_info_hander->cli_head);

    return 0;
EXIT:
    sock_unreg(playback_info_hander->video_playback_sock_hdl);
    playback_info_hander->video_playback_sock_hdl = NULL;
    return -1;
}

static int playback_disconnect_all_cli(void)
{
    puts("[Msg] playback_disconnect_all_cli\n\n");
    struct __playback_cli_info *cli = NULL;
    struct list_head *pos = NULL, *node = NULL;
    os_mutex_pend(&playback_info_hander->mutex, 0);
    if (list_empty(&playback_info_hander->cli_head)) {
        puts("[Msg]video_playback cli is emtry\n");
        os_mutex_post(&playback_info_hander->mutex);
        return -1;
    }
    list_for_each_safe(pos, node, &playback_info_hander->cli_head) {
        cli = list_entry(pos, struct __playback_cli_info, entry);
        cli->kill_flag = 1;
        if (cli->stop) {
            os_sem_post(&cli->sem);
        }

        sock_set_quit(cli->pinfo.sock);
    }

    os_mutex_post(&playback_info_hander->mutex);
    return -1;
}

void playback_uninit(void)
{
    playback_disconnect_all_cli();
    sock_unreg(playback_info_hander->video_playback_sock_hdl);
    playback_info_hander->video_playback_sock_hdl = NULL;
}


int playback_disconnect_cli(struct sockaddr_in *dst_addr)
{
    struct __playback_cli_info *cli = NULL;
    os_mutex_pend(&playback_info_hander->mutex, 0);
    struct list_head *pos = NULL, *node = NULL;
    if (list_empty(&playback_info_hander->cli_head)) {
        puts("[Msg]video_playback cli is emtry\n");
        os_mutex_post(&playback_info_hander->mutex);
        return -1;
    }
    list_for_each_safe(pos, node, &playback_info_hander->cli_head) {
        cli = list_entry(pos, struct __playback_cli_info, entry);
        if (dst_addr != NULL && cli->remote_addr.sin_addr.s_addr == dst_addr->sin_addr.s_addr) {
            cli->kill_flag = 1;
            if (cli->stop) {
                os_sem_post(&cli->sem);
            }
            sock_set_quit(cli->pinfo.sock);
        } else {
            printf("not find dst_addr:%s:%d\n", inet_ntoa(dst_addr->sin_addr.s_addr), ntohs(dst_addr->sin_port));
        }
        os_mutex_post(&playback_info_hander->mutex);
        return 0;
    }
    os_mutex_post(&playback_info_hander->mutex);
    return -1;
}

int playback_cli_pause(struct sockaddr_in *dst_addr)
{
    puts("-----------playback_cli_pause\n\n");
    struct __playback_cli_info *cli = NULL;
    struct list_head *pos = NULL, *node = NULL;
    os_mutex_pend(&playback_info_hander->mutex, 0);
    list_for_each_safe(pos, node, &playback_info_hander->cli_head) {
        cli = list_entry(pos, struct __playback_cli_info, entry);

        if (cli->remote_addr.sin_addr.s_addr == dst_addr->sin_addr.s_addr) {
            cli->stop = 1;
            os_mutex_post(&playback_info_hander->mutex);
            return 0;
        }
    }
    os_mutex_post(&playback_info_hander->mutex);
    return -1;
}

int playback_cli_continue(struct sockaddr_in *dst_addr)
{
    puts("---------------playback_cli_continue\n\n");
    struct __playback_cli_info *cli = NULL;
    struct list_head *pos = NULL, *node = NULL;
    os_mutex_pend(&playback_info_hander->mutex, 0);
    list_for_each_safe(pos, node, &playback_info_hander->cli_head) {
        cli = list_entry(pos, struct __playback_cli_info, entry);
        if (cli->remote_addr.sin_addr.s_addr == dst_addr->sin_addr.s_addr) {
            cli->stop = 0;
            os_sem_post(&cli->sem);
            os_mutex_post(&playback_info_hander->mutex);
            return 0;
        }
    }
    os_mutex_post(&playback_info_hander->mutex);
    return -1;

}
//快放
int playback_cli_fast_play(struct sockaddr_in *dst_addr, u32 speed)
{
    puts("---------------playback_cli_fast_play\n\n");
    struct __playback_cli_info *cli = NULL;
    struct list_head *pos = NULL, *node = NULL;
    os_mutex_pend(&playback_info_hander->mutex, 0);
    list_for_each_safe(pos, node, &playback_info_hander->cli_head) {
        cli = list_entry(pos, struct __playback_cli_info, entry);

        if (cli->remote_addr.sin_addr.s_addr == dst_addr->sin_addr.s_addr) {
            cli->fast_ctrl = speed;
            os_mutex_post(&playback_info_hander->mutex);
            return 0;
        }
    }
    os_mutex_post(&playback_info_hander->mutex);
    return -1;

}
#endif
