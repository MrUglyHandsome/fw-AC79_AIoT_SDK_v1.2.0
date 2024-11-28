#ifndef _AUDIO_VBASS_API_H_
#define _AUDIO_VBASS_API_H_
#include "system/includes.h"
// #include "media/audio_stream.h"
// #include "effects/VirtualBass_api.h"
#include "virtualbass/VirtualBass_api.h"

typedef struct _VirtualBassUdateParam {
    int ratio;
    int boost;
    int fc;
} VirtualBassUdateParam;

//虚拟低音
typedef struct _VirtualBass_TOOL_SET {
    int is_bypass; // 1-> byass 0 -> no bypass
    VirtualBassUdateParam parm;
} virtual_bass_param_tool_set;


typedef struct _vbass_hdl {
    void *workbuf;           //vbass 运行句柄及buf
    VirtualBassParam parm;
    u8 status;                           //内部运行状态机
    u8 update;                           //设置参数更新标志
    u8 out32bit;
} vbass_hdl;


int audio_vbass_run(void *priv, s16 *datain, int len, int sample_rate);
void *audio_vbass_open(void *priv, void *parm);
int audio_vbass_close(void *priv);
int audio_vbass_update_parm(void *priv, VirtualBassUdateParam *parm);
void audio_vbass_bypass(void *priv, u8 bypass);

#ifndef RUN_NORMAL
#define RUN_NORMAL  0
#endif

#ifndef RUN_BYPASS
#define RUN_BYPASS  1
#endif

#endif

