#include "system/includes.h"
#include "server/video_server.h"
#include "app_config.h"
#include "asm/debug.h"
#include "asm/osd.h"
#include "asm/isc.h"
#include "app_database.h"
#include "storage_device.h"
#include "os/os_api.h"
#include "camera.h"
#include "yuv_soft_scalling.h"

#ifdef CONFIG_VIDEO1_ENABLE
/*============================================================================================================*/
/********************BT656格式的-1/2/4bit摄像头只获取YUV使用说明*****************************************
请注意：下列说明关系到是否正确收到BT656-1/2/4bit摄像头的YUV数据

MSB正向：MSB=D7,LSB=D0，（D7为Sensor_D7的IO，D0为Sensor_D0的IO）
MSB反向：MSB=D0,LSB=D7，（D7为Sensor_D7的IO，D0为Sensor_D0的IO）
MSB反向则移位为左移，MSB正向则移位右移
遵循：MSB反向左移，MSB正向右移；先反向，再左右移原则
协议：BT656-1/2/4bit，PCLK有时钟则有数据，PCLK无时钟则无数据，协议满足SPI-1/2/4bit协议
	  帧起始：ff0000ab
	  帧结束：ff0000b6
	  行起始：ff000080
	  行结束：ff00009d

1、1bit情况下，不用关心移位和反向问题（内部已经处理好）

2、2bit/4bit情况下
关键函数：bt656_one_line_databits_config()
void bt656_one_line_databits_config(u32 databit, u8 msb_lsb_rev, u8 shift);
//databit:SEN_MBUS_DATA_WIDTH_2B/SEN_MBUS_DATA_WIDTH_1B/SEN_MBUS_DATA_WIDTH_4B
//msb_lsb_rev:0正向，1反向
//shift：移位（0-7）

（1）使用bt656_one_line_xxx函数，则PCLK只能选择IO_PORTC_08和IO_PORTH_04
①在PCLK选择IO_PORTC_08下：数据位只能从IO_PORTC_00到IO_PORTC_07，对应的YUV-ISC硬件模块的输入IO的数据位对应为：D7->D0对应PC0->PC7（与IO芯片IO顺序刚好相反）。
如摄像头为2bit两线数据线，PC0、PC1连接了摄像头D0和D1数据，摄像头D0为低位数据，D1为高位数据，由于YUV-ISC硬件模块输入高位MSB对应的IO-D7优先输入，
则需要如下步骤进行分析：
YUV-ISC硬件模块的输入到IO为MSB先输入，再依次输入低位，由于是2bit，所以D7先输入高位（PC0-高位），D6再输入低位（PC1-低位），
那么IO高低位和摄像头高低位相反（需求：PC0、PC1连接了摄像头D0和D1数据，D0为低位数据，D1为高位数据），因此需要设置反向输入。
因此需要选反向输入再移位，如下（遵循：先MSB-LSB反向，再左右移原则，MSB反向为左移，MSB正向为右移）：

首先：设置反向输入，即MSB=D0,LSB=D7，此时2bit下输入为D0和D1（PC7和PC6），高位在D0，低位在D1（高位在PC7，低位在PC6）（bt656_one_line_databits_config函数的第二个参数为1）。

再次：左移6位，上步骤后下输入为D0和D1，左移6位后，输入为D(0+6)和D(1+6)，即输入为D6和D7（高位在PC1，低位在PC0），达到符合的需求：PC0、PC1连接了摄像头D0和D1数据，D0为低位数据，D1为高位数据（bt656_one_line_databits_config函数的第三个参数为6）。

②在PCLK选择IO_PORTH_04下：数据位只能为IO_PORTH_03, IO_PORTH_02, IO_PORTH_06, IO_PORTH_05，对应的YUV硬件模块的输出到IO的数据位对应为：D0->PH3、D1->PH2、D3->PH6、D4->PH5、D5到D7无对应IO。
如摄像头为2bit两线数据线，PH3、PH2连接了摄像头D0和D1数据，摄像头D0为低位数据，D1为高位数据，由于YUV-ISC硬件模块输入高位MSB对应的IO-D7优先输入，
则需要如下步骤进行分析：
YUV-ISC硬件模块的输入到IO为MSB先输入，再依次输入低位，由于是2bit，所以D7先输入高位，D6再输入低位，由于没有D7和D6的IO，因此需要右移6位，
把输入移到D1和D0，刚好高低位符合需求：PH3、PH2连接了摄像头D0和D1数据，摄像头D0为低位数据，D1为高位数据。

因此只需要移位（遵循：先MSB-LSB反向，再左右移原则，MSB反向为左移，MSB正向为右移），即：bt656_one_line_databits_config函数的第二个参数为0，第三个参数为6）

***********************************************************************************************/

//******************摄像头时钟输出IO配置********************//
#define CAMERA_VIDEO_POWERDOWN_IO  	IO_PORTH_09 //powedown引脚
#define CAMERA_VIDEO_RESET_IO  		IO_PORTH_07 //reset引脚
#define CAMERA_VIDEO_XMCLK_IO  		IO_PORTH_08 //xmclk引脚

//******************BT656硬件接收IO配置********************//
#define CAMERA_VIDEO_PCLK_PORT		IO_PORTH_04	//pclk选择的IO
#define CAMERA_VIDEO_START_IO		IO_PORTH_03 //数据位的低位IO，2/4bit下也只需设置1个IO，但是必须是对应的最低位

/*#define CAMERA_VIDEO_PCLK_PORT	IO_PORTC_08*/
/*#define CAMERA_VIDEO_START_IO		IO_PORTC_05*/

//******************摄像头分辨率**************************//
#define CAMERA_VIDEO_W				CONFIG_VIDEO1_IMAGE_W //摄像头的分辨率-宽，需要和摄像头驱动设置的输出分辨率寄存器配置一致
#define CAMERA_VIDEO_H				CONFIG_VIDEO1_IMAGE_H //摄像头的分辨率-高，需要和摄像头驱动设置的输出分辨率寄存器配置一致
#define CAMERA_ONFRAM_YUV420_SIZE	(CAMERA_VIDEO_W * CAMERA_VIDEO_H * 3 / 2)//一帧YUV420大小

#define IIC_DEV_ID					1 //isp_dev参数：0则打开"iic0"设备，1则打开"iic1"设备，board.c需要配置好IIC的IO
/*============================================================================================================*/

//******************yuv vdeo状态**************************//
#define CAMERA_VIDEO_IDEL			0x0
#define CAMERA_VIDEO_OPEN			0x1
#define CAMERA_VIDEO_CLOSE			0x3
#define CAMERA_VIDEO_YUV_USE_DMA   	1   //1使用硬件DMA拷贝，0使用memcpy

struct bt656_video {
    u8 frame;
    u8 init;
    u8 state;
    u8 frame_done;
    u8 dma_copy_id;
    u16 width;
    u16 height;
    u8 *app_yuv_buf;
    u8 *yuv_buf;
    u32 frame_size;
    u32 pid;
    void (*yuv_cb)(u8 *buf, u32 size, int width, int height);
    OS_SEM sem;
} bt656_video_info = {0};

void sdfile_save_test(char *buf, int len, char one_file, char close);

/*
 * 硬件接收完成回调函数，需要进行数据拷贝到应用层的BUF
 */
static int bt656_one_line_framedone_callback(void *priv, int id, void *blk_info)
{
    struct yuv_block_info *block_info = (struct yuv_block_info *)blk_info;//获取回调参数
    char *buf = (char *)block_info->y;//获取YUV地址
    int len = block_info->ylen + block_info->ulen + block_info->vlen;//获取YUV长度

    if (!bt656_video_info.frame_done && buf) {
        /*复制YUV到应用层缓冲区*/
#if CAMERA_VIDEO_YUV_USE_DMA
        bt656_video_info.dma_copy_id = dma_copy_async(bt656_video_info.app_yuv_buf, buf, len);
#else
        memcpy(bt656_video_info.app_yuv_buf, buf, len);
#endif
        bt656_video_info.frame_done = TRUE;
        os_sem_post(&bt656_video_info.sem);
        return 0;
    }
    return -EINVAL;
}
/*
 * 用户的摄像头检测和打开函数：主要为配置摄像头寄存器
 */
static int camera_device_open(void)
{
    int err = 0;
    if (bt656_video_info.state == CAMERA_VIDEO_OPEN) {
        printf("bt656 video is open \n");
        return 0;
    }

    /* 1. 设置摄像头的IO和输出时钟给摄像头 */
    gpio_direction_output(CAMERA_VIDEO_POWERDOWN_IO, 0);	//设置powedown引脚正常工作电平，一般摄像头要求低电平，需要留意数据手册要求
    gpio_direction_output(CAMERA_VIDEO_RESET_IO, 1);		//设置reset引脚正常工作电平，一般摄像头要求高电平，需要留意数据手册要求
    gpio_output_channle(CAMERA_VIDEO_XMCLK_IO, CH3_PLL_24M);//输出时钟给摄像头

    /* 在2/4bit情况下：如果不清楚设置最低IO，则应用层需要设置IO的输入模式，打开上下拉和数字输入输出模式，如下2bit-PH3-PH2例子*/
    /*
    #if (CONFIG_SPI_ONE_LINE_ENABLE == 0)
    gpio_direction_input(IO_PORTH_03);//输入
    gpio_set_pull_up(IO_PORTH_03, 0);//关上拉
    gpio_set_pull_down(IO_PORTH_03, 0);//关下拉
    gpio_set_die(IO_PORTH_03, 1);//设置数字模式

    gpio_direction_input(IO_PORTH_02);//输入
    gpio_set_pull_up(IO_PORTH_02, 0);//关上拉
    gpio_set_pull_down(IO_PORTH_02, 0);//关下拉
    gpio_set_die(IO_PORTH_02, 1);//设置数字模式
    #endif
    */


    /* 2. 检测摄像头是否正常，GC0310_check函数的isp_dev参数：0则打开iic0设备，1则打开iic1设备，board.c需要配置好IIC的IO */
    err = GC0310_check(IIC_DEV_ID, CAMERA_VIDEO_RESET_IO, CAMERA_VIDEO_POWERDOWN_IO);
    if (err) {
        printf("GC0310_check err\n");
        return -1;
    }

    /* 3. 初始化摄像头，配置摄像头寄存器 */
    err = GC0310_init(IIC_DEV_ID, NULL, NULL, NULL, NULL);
    if (err) {
        printf("GC0310_init err\n");
        return -1;
    }
    bt656_video_info.state = CAMERA_VIDEO_OPEN;
    printf("camera open ok\n");
    return 0;
}
/*
 * 用户的摄像头关闭函数：一般的摄像头为关闭xmclk或者进行reset引脚复位
 */
static int camera_device_close(void)
{
    if (!bt656_video_info.init) {
        printf("bt656 video no init \n");
        return -EINVAL;
    }
    if (bt656_video_info.state == CAMERA_VIDEO_CLOSE) {
        puts("spi video is close \n");
        return 0;
    }
    /* 1. 复位摄像头和关闭时钟输出  */
    gpio_direction_output(CAMERA_VIDEO_RESET_IO, 0);//设置reset引脚正常工作电平，一般摄像头要求高电平，需要留意数据手册要求
    os_time_dly(1);
    gpio_direction_output(CAMERA_VIDEO_RESET_IO, 1);//设置reset引脚正常工作电平，一般摄像头要求高电平，需要留意数据手册要求
    gpio_clear_output_channle(CAMERA_VIDEO_XMCLK_IO, CH3_PLL_24M);//关闭输出时钟给摄像头

    /* 2. 标记video状态  */
    bt656_video_info.state = CAMERA_VIDEO_CLOSE;
    return 0;
}
/*
 * 用户的摄像头YUV获取任务
 */
static void bt656_video_yuv_task(void *priv)
{
    int ret;
    int msg[4];

    /* 1. 创建信号量、初始化参数*/

    bt656_video_info.width 				= CAMERA_VIDEO_W;
    bt656_video_info.height 			= CAMERA_VIDEO_H;
    bt656_video_info.frame_done 		= FALSE;
    bt656_video_info.frame_size 		= CAMERA_ONFRAM_YUV420_SIZE;
    if (!os_sem_valid(&bt656_video_info.sem)) {
        os_sem_create(&bt656_video_info.sem, 0);
    }
    os_sem_set(&bt656_video_info.sem, 0);

    /* 2. 申请buf*/
    if (!bt656_video_info.yuv_buf) {
        bt656_video_info.yuv_buf = malloc(bt656_video_info.frame_size * 2);//申请2帧数据缓存大小，1帧给硬件接收，1帧给应用层使用
        if (!bt656_video_info.yuv_buf) {
            printf("no mem bt656 video \n");
            goto exit;
        }
        bt656_video_info.app_yuv_buf = bt656_video_info.yuv_buf + bt656_video_info.frame_size;
    }

    /* 2. bt656协议参数初始化*/
    bt656_one_line_init((u32)bt656_video_info.yuv_buf,	//硬件接收的YUV地址
                        bt656_video_info.frame_size,	//传一帧YUV420大小
                        bt656_video_info.width,			//宽
                        bt656_video_info.height,		//高
                        SEN_IN_FORMAT_YUYV);			//YUYV格式
    /* 3. bt656-IO初始化*/
#if (CONFIG_SPI_ONE_LINE_ENABLE == 0)
    bt656_one_line_io_init(CAMERA_VIDEO_PCLK_PORT,		//摄像头pclk的IO，固定IO_PORTC_08和IO_PORTH_04
                           SEN_MBUS_PCLK_SAMPLE_FALLING,//摄像头pclk下降沿采样
                           0,							//pclk是否进过硬件滤波器（默认0经过，1不经过）
                           CAMERA_VIDEO_START_IO,		//数据线最低位IO
                           2);							//数据线占2个IO，用于初始化IO，如上行参数为IO_PORTC_00，则会初始化PC0和PC1

#else
    bt656_one_line_io_init(CAMERA_VIDEO_PCLK_PORT,		//摄像头pclk的IO，固定IO_PORTC_08和IO_PORTH_04
                           SEN_MBUS_PCLK_SAMPLE_FALLING,//摄像头pclk下降沿采样
                           0,							//pclk是否进过硬件滤波器（默认0经过，1不经过）
                           CAMERA_VIDEO_START_IO,		//数据线最低位IO
                           1);							//数据线占1个IO，用于初始化IO，如上行参数为IO_PORTC_00，则会初始化PC0
#endif
    /* 4. bt656-帧完成回调函数配置*/
    bt656_one_line_framedone_reg((int *)bt656_one_line_framedone_callback, NULL);

    /* 5. 打开摄像头并配置输出分辨率等寄存器*/
    if (camera_device_open()) {
        printf("camera_device_open err \n");
        goto exit;
    }

    /* 6. 使能硬件接收，在2/4bit下，最后一步在配置MSB正反向和移位（因为camera_device_open有可能会对MSB正反向和移位进行重置）*/
#if (CONFIG_SPI_ONE_LINE_ENABLE == 0)
    /*
     * 2/4bit下的bt656-数据IO移位配置，默认不配置则是单线的1bit，放在最后一步再设置反向和移位
     * 移位和反向和需要输入的IO相关，需要注意！！！！！！！！！
     */
    bt656_one_line_databits_config(SEN_MBUS_DATA_WIDTH_2B, 0, 6);//按照移位来配置对应输入IO
#endif
    bt656_one_line_en(1);//最后一步再打开

    bt656_video_info.init = TRUE;
    while (1) {
        os_taskq_accept(ARRAY_SIZE(msg), msg);
        ret = os_sem_pend(&bt656_video_info.sem, 100);
        if (thread_kill_req()) {
            break;
        }
        if (!ret && bt656_video_info.frame_done) {
            putchar('R');
#if CAMERA_VIDEO_YUV_USE_DMA
            dma_copy_async_wait(bt656_video_info.dma_copy_id);
#endif
            u8 *read_buf = bt656_video_info.app_yuv_buf;
            u32 wlen = bt656_video_info.frame_size;
            /* 7. 使用地址:read_buf，长度:wlen，可以在这里进行YUV的算法或者编码JPEG*/
            /*
            static u32 cnt = 0;
            cnt++;
            sdfile_save_test(read_buf, wlen, 1, cnt % 100 == 0);
            */
            if (bt656_video_info.yuv_cb) { //yuv数据回调
                bt656_video_info.yuv_cb(read_buf, wlen, bt656_video_info.width, bt656_video_info.height);
            }

            //使用结束需要清frame_done
            bt656_video_info.frame_done = FALSE;
        } else {
            printf("yuv recv timeout \n");
        }
    }

    /* 8. 关闭硬件模块和关闭摄像头*/
exit:
    bt656_one_line_exit();
    camera_device_close();
    if (bt656_video_info.yuv_buf) {
        free(bt656_video_info.yuv_buf);
        bt656_video_info.yuv_buf = NULL;
    }
    bt656_video_info.init = FALSE;
    printf("\n\n bt656_video_yuv_task exit \n\n");
}

/*
 * 用户的摄像头YUV获取任务创建：参数为YUV回调函数
 */
int bt656_video_yuv_task_create(void (*cb)(u8 *buf, u32 size, int width, int height))
{
    bt656_video_info.yuv_cb = cb;
    return thread_fork("bt656_yuv_task", 10, 512, 64, &bt656_video_info.pid, bt656_video_yuv_task, NULL);
}

/*
 * 用户的摄像头YUV获取任务删除
 */
void bt656_video_yuv_task_kill(void)
{
    if (bt656_video_info.init) {
        os_sem_post(&bt656_video_info.sem);
        thread_kill(&bt656_video_info.pid, 0);//关闭再重新打开
        while (bt656_video_info.init) {
            os_time_dly(100);
            printf("bt656_video_yuv_task_kill err\n");
        }
    }
}
#endif

