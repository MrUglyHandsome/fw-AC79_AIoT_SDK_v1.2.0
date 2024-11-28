#ifndef  __PACKET_H__
#define  __PACKET_H__

#include "sock_api/sock_api.h"
#include "os/os_api.h"
#include "server/net_server.h"
#include "app_config.h"
#include "server/server_core.h"
#include "server/ctp_server.h"
#include "simple_mov_unpkg.h"
#include "simple_avi_unpkg.h"
#include "rt_stream_pkg.h"
#include "fs/fs.h"
#include "app_config.h"

#include <time.h>
#include "sys_time.h"

struct __packet_info {
    u32 len;
    void *sock;
    u8 *data;
    u32 data_len;
    FILE *fd;
    struct tm time;
    struct vfscan *fs;
    u8 type;
    u8 fast_play_mask;
    u8 state;//0 preview , 1 playback
    struct __mov_unpkg_info info;
    char file_name[64];
};
#if (__SDRAM_SIZE__ > (2 * 1024 * 1024))
#define IMAGE_SIZE	300*1024 //该长度必须大于一帧视频的数据长度
#else
#define IMAGE_SIZE	150*1024 //该长度必须大于一帧视频的数据长度
#endif

int get_video_media_info(struct __packet_info *pinfo);
FILE *play_next(struct __packet_info *pinfo);
FILE *play_prev(struct __packet_info *pinfo);
int send_video_packet(struct __packet_info  *pinfo, u32 i);
int send_audio_packet(struct __packet_info *pinfo, u32 j);
int send_media_packet(struct __packet_info *pinfo);
int send_date_packet(struct __packet_info *pinfo, u32 msec);
int send_end_packet(struct __packet_info *pinfo);
int update_data(struct __packet_info *pinfo);
void incr_date_time(struct tm *tm_time, int incr);
int send_gps_data_packet(struct __packet_info *pinfo);
int find_gps_data(struct __packet_info *pinfo);
int unfind_gps_data(void);

#endif  /*PACKET_H*/

