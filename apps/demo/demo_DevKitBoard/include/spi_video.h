#ifndef __SPI_VIDEO_H__
#define __SPI_VIDEO_H__

/*+++++++使用例子只能使能一个+++++++*/
#if 1
#define USE_TEST_ONLY_Y_OUT          DISABLE
#define USE_TEST_YUV_OUT             ENABLE
#else
#define USE_TEST_ONLY_Y_OUT          ENABLE
#define USE_TEST_YUV_OUT             DISABLE
#endif

/**********************************说明****************************************************
使用镜头开源版SPI摄像头gc0310
提供演示如下
提供单线双线spi的以下使用例子
Y数据输出    60fps 大小128*128 场景用于扫描笔 并显示到屏
yuv数据输出  15fps 大小VGA pclk 80M xclk40M 并显示到屏 13fps

说明：spi极限速度80M dvp硬件同样支持bt656单线双线以及4线spi bt656时序 参考dvp使用例子
如果需要YUV422数据使用硬件DVP接口
bt656时序介绍：
单线摄像头PCLK输出同步时钟该时钟需要有以下特征 有数据的时候才有时钟空闲时候时钟无
pclk时钟频率：100k-96M 时钟均能正确接收数据
数据d0：数据输出是按一行一行的数据输出例如下图
Pclk: 640*480
--    --    --    --     --    --    --            --    --    --
  |  |  |  |  |  |         |  |  |  |  |  |          |  |  |  |  |  |
   --    --    --           --    --    --             --    --    --
包特征:
frist_line_data: ff0000ab+ff000080+data(yuv422)+ff00009d
line_data:                ff000080+data(yuv422)+ff00009d
end_line_data:            ff000080+data(yuv422)+ff00009d+ff0000b6
d0: all
frist_line_data         (480-2) x line_data        end_line_data

大概的时间特征 行于行之间间隔很小 170us 需要很快的响应不能有别的中断打断 一般需要使用不可屏蔽中断规避
如果是按整包接收就可以不要不可屏蔽 包与包之间间隔比较长一般 看数据帧率 如果14帧  14ms
*******************************************************************************************/

/**********定义spi使用接口***********/
//iic使用通用摄像头流程初始化摄像头
#define JL_SPI                       JL_SPI2  //与spi设备对应：JL_SPI0 JL_SPI1 JL_SPI2
#define JL_SPI_NAME                  "spi2"   //与spi设备对应："spi1", "spi2"
//摄像头的XCLK_PORT，使用SPI接收时，board_xxx.c板级配置就不能配置xclk（配置在本文件有益于动态更改时钟IO）
//改例子硬件接口在此文件定义
#define CAM_XCLK_PORT   IO_PORTH_02

/**********定义摄像头长宽************/
#define CAMERA_W			         640
#define CAMERA_H				     480

/***********定义一帧YUV422数据长度***/
#define YUV422_PACK_LEN              (CAMERA_W*CAMERA_H*2)
#define LBUF_NUMB                    5
#define YUV422_LBUF_LEN              (YUV422_PACK_LEN*LBUF_NUMB)
/***********定义一帧Y数据长度***/
#define Y_PACK_LEN                   (CAMERA_W*CAMERA_H)

/**********定义bt6565摄像头时序******/
#define CAM_VSTART_VALUE                 ntohl(0xff0000ab)  //帧头
#define CAM_LSTART_VALUE                 ntohl(0xff000080)  //行头
#define CAM_FHEAD_SIZE	             4     //帧头长
#define CAM_FEND_SIZE	             4     //帧尾长
#define CAM_LHEAD_SIZE	             4     //行头长
#define CAM_LEND_SIZE	             4     //行尾长

#if USE_TEST_ONLY_Y_OUT                    //纯Y
#define CAM_LINE_SIZE                CAMERA_W       //定义有效数据长度
#else if(USE_TEST_YUV_OUT)                 //yuv422
#define CAM_LINE_SIZE                (CAMERA_W * 2) //定义有效数据长度
#endif

//frist_line_data 1292
#define DATA_FIRST_PACK_SIZE         (CAM_FHEAD_SIZE + CAM_LHEAD_SIZE + CAM_LINE_SIZE + CAM_LEND_SIZE)
//(480-2) x line_data 1288
#define DATA_LINE_PACK_SIZE                            (CAM_LHEAD_SIZE + CAM_LINE_SIZE + CAM_LEND_SIZE)
//end_line_data 1292
#define DATA_END_PACK_SIZE                             (CAM_LHEAD_SIZE + CAM_LINE_SIZE + CAM_LEND_SIZE + CAM_FEND_SIZE)
//all_data_size 618248
#define DATA_ALL_PACK_SIZE           (DATA_FIRST_PACK_SIZE + DATA_LINE_PACK_SIZE * (CAMERA_H-2) + DATA_END_PACK_SIZE)



#define ENABLE                       1
#define DISABLE                      0

/**********定义数据接收缓冲行数**********/
#define RECV_DATA_LINE_PACK_TIME     32     //按16行数据接收 需要单独接收一行包头 一行包尾
/* #define RECV_ALL_DATA                      //一次性接收完所有包注意数据包尽量不要太大 收存Y数据使用 */

/**********定义数据缓存**************/
#ifdef RECV_DATA_LINE_PACK_TIME
#define LINE_LBUF_SIZE               ((RECV_DATA_LINE_PACK_TIME * DATA_FIRST_PACK_SIZE)) //按最长的行包为基准
#endif

#ifdef RECV_ALL_DATA
#define LINE_LBUF_SIZE               (DATA_ALL_PACK_SIZE) //定义3个整帧包
#endif


/* #define SPI_IO_DBG */
#ifdef SPI_IO_DBG
#define DBG_PORT_BIT                 BIT(1)
#define DBG_PORT                     JL_PORTA
#endif

/**********spi_video数据结构体*******/
struct spi_video {
    OS_SEM sem;
    OS_SEM rgb1;
    OS_SEM rgb2;
    OS_SEM rgb1_ok;
    OS_SEM rgb2_ok;
    u8 camera_reinit;
    u8 kill;
    u16 recv_move_cnt;
    u8 *frame_addr_move_cnt;
    u8 *video;
    u8 *spi;
    u8 *buf;
    u8 *yuv422_buf;
    u8 *cpy_addr;
    u8 *y_buf;
    u8 *irq_recv_buf;
    u8 *irq_last_recv_buf;
    u8 *real_data_addr;
    u8 *get_frame;
    u8 frame_addr_use_flag;
    u8 *next_frame_addr;
};


#endif


