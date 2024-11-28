#include "app_config.h"

#if CONFIG_MP3_DEC_ENABLE
//需要添加

#include "device/device.h"//u8
#include "system/includes.h"

#include "server/audio_server.h"

/*********播放flash中的mp3资源****用于按键播放提示音和开机音乐**************************/

/*=调用该函数即可完成flash中的资源播放 例如post_msg_play_flash_mp3("music_test.mp3",50)50是播放音量最大100=*/
void post_msg_play_flash_mp3(char *file_name, u8 dec_volume);

static OS_MUTEX mutex_mp3;
static OS_SEM dec_ready_sem;

#define SHUTDOWN_CMD    (1001)

struct flash_mp3_hdl {
    struct server *dec_server;
    char *file_name;
    char  file_path[64];
    u8 dec_volume;
    int task_pid;
    FILE *file;
    u8 dec_open_flog;
};

static struct flash_mp3_hdl *mp3_info = {0};

static void dec_server_event_handler(void *priv, int argc, int *argv)
{
    int msg = 0;
    switch (argv[0]) {
    case AUDIO_SERVER_EVENT_ERR:
        printf("AUDIO_SERVER_EVENT_ERR\n");
        break;
    case AUDIO_SERVER_EVENT_END:
        printf("AUDIO_SERVER_EVENT_END\n");
        msg = SHUTDOWN_CMD;
        os_taskq_post(os_current_task(), 1, msg);
        break;
    case AUDIO_SERVER_EVENT_CURR_TIME:
        printf("play_time: %d\n", argv[1]);
        break;
    default:
        break;
    }
}

static void play_mp3_task(void *priv)
{
    int msg[32] = {0};
    int err;
    printf("<<<<<<<<<<<<<<<<<<<path = %s", mp3_info->file_path);
    mp3_info->file = fopen(mp3_info->file_path, "r");
    if (!mp3_info->file) {
        puts("no this mp3!\n");
    }

    mp3_info->dec_server = server_open("audio_server", "dec");
    if (!mp3_info->dec_server) {
        puts("play_music open audio_server fail!\n");
        goto __err;
    }

    server_register_event_handler(mp3_info->dec_server, NULL, dec_server_event_handler);

    union audio_req req = {0};

    req.dec.cmd             = AUDIO_DEC_OPEN;
    req.dec.volume          = mp3_info->dec_volume;
    req.dec.output_buf      = NULL;
    req.dec.output_buf_len  = 12 * 1024;
    req.dec.channel         = 0;
    req.dec.sample_rate     = 0;
    req.dec.priority        = 1;
    req.dec.vfs_ops         = NULL;
    req.dec.file            = mp3_info->file;
    req.dec.dec_type		= "mp3";
    req.dec.sample_source   = "dac";
    //打开解码器
    if (server_request(mp3_info->dec_server, AUDIO_REQ_DEC, &req) != 0) {
        puts("1open_audio_dec_err!!!");
        return ;
    }
    //开始解码
    req.dec.cmd = AUDIO_DEC_START;
    if (server_request(mp3_info->dec_server, AUDIO_REQ_DEC, &req) != 0) {
        puts("2open_audio_dec_err!!!");
        return ;
    }

    mp3_info->dec_open_flog = 1;
    os_sem_post(&dec_ready_sem);

    os_task_pend("taskq", msg, ARRAY_SIZE(msg));
    //关闭音频
    req.dec.cmd = AUDIO_DEC_STOP;
    //关闭解码器
    printf("stop dec.\n");
    server_request(mp3_info->dec_server, AUDIO_REQ_DEC, &req);
    server_close(mp3_info->dec_server);

    if (mp3_info->file) {
        fclose(mp3_info->file);
    }
    printf(">>>>>dec_server stop");
    os_sem_post(&dec_ready_sem);
    mp3_info->dec_open_flog = 0;
    return ;

__err:
    fclose(mp3_info->file);
    server_close(mp3_info->dec_server);
    return;

}

/*=调用该函数即可完成flash中的资源播放 例如post_msg_play_flash_mp3("music_test.mp3",50)50是播放音量最大100=*/
void post_msg_play_flash_mp3(char *file_name, u8 dec_volume)
{
    os_mutex_pend(&mutex_mp3, 0);//加互斥放防止异步操作

    int msg = 0;
    snprintf(mp3_info->file_path, 64, CONFIG_VOICE_PROMPT_FILE_PATH"%s", file_name);
    mp3_info->dec_volume = dec_volume;

    if (mp3_info->dec_open_flog  ==  0) { //音频解码器没有被打开过
        thread_fork("play_mp3_task", 10, 1024, 32, &mp3_info->task_pid, play_mp3_task, NULL);
        os_sem_pend(&dec_ready_sem, 0);//等待解码器打开成功
    } else {//如果之前的音频没有播放完
        msg = SHUTDOWN_CMD;
        os_taskq_post("play_mp3_task", 1, msg);
        os_sem_pend(&dec_ready_sem, 0);//等待解码器关闭
        thread_kill(&mp3_info->task_pid, KILL_WAIT);//确认上次线程是否关闭
        thread_fork("play_mp3_task", 10, 1024, 32, &mp3_info->task_pid, play_mp3_task, NULL);
    }

    os_time_dly(30);//限制最快播放的时间间隔 因此延时300ms 这里延时很小的时候会有异常没有解决 可能是开关线程频繁 底层开了解码器 还没关又打开次数多了就异常了
    os_mutex_post(&mutex_mp3);
}

static void flash_mp3_open(void)
{
    os_sem_create(&dec_ready_sem, 0);
    os_mutex_create(&mutex_mp3);
    mp3_info = (struct flash_mp3_hdl *)calloc(1, sizeof(struct flash_mp3_hdl));
}
late_initcall(flash_mp3_open);

/***************上面为提示音播放部分*********************************************/

#endif //CONFIG_MP3_DEC_ENABLE
