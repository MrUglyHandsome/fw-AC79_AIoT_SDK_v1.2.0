#include "system/includes.h"
#include "asm/sfc_norflash_api.h"
#include <fal.h>

#define USER_FLASH_SPACE_PATH "mnt/sdfile/EXT_RESERVED/user"
#define FLASHDB_CONFIG_SIZE     (16 * 1024)
#define FLASH_BLOCK_SIZE        (4 * 1024)

static u32 flashdb_addr = 0xFFFFFFFF;
static u32 flashdb_size = 0;

struct vfs_attr {
    u8 attr;					/*!< 文件属性标志位 */
    u32 fsize;					/*!< 文件大小 */
    u32 sclust;					/*!< 最小分配单元 */
    struct sys_time crt_time;	/*!< 文件创建时间 */
    struct sys_time wrt_time;	/*!< 文件最后修改时间 */
    struct sys_time acc_time;   /*!< 文件最后访问时间 */
};

static int flashdb_flash_init(void)
{
    u32 addr;
    FILE *profile_fp = fopen(USER_FLASH_SPACE_PATH, "r");
    if (profile_fp == NULL) {
        puts("__flashDB_mount ERROR!!!\r\n");
        return -1;
    }

    struct vfs_attr file_attr;
    fget_attrs(profile_fp, &file_attr);
    addr = sdfile_cpu_addr2flash_addr(file_attr.sclust);
    fclose(profile_fp);

    flashdb_addr = addr;
    flashdb_size = file_attr.fsize;

    printf("__flashDB_mount_addr = 0x%x, size = 0x%x \r\n", addr, file_attr.fsize);
    if (file_attr.fsize < FLASHDB_CONFIG_SIZE) {
        while (1) {
            printf("USER_LEN < FLASHDB_CONFIG_SIZE\n");
            os_time_dly(30);
        }
    }

    return 0;
}

static int flashdb_flash_read(long offset, u8 *buf, u32 size)
{
    return norflash_read(NULL, buf, size, flashdb_addr + offset);
}

static int flashdb_flash_write(long offset, u8 *buf, u32 size)
{
    return norflash_write(NULL, buf, size, flashdb_addr + offset);
}

static int flashdb_flash_erase(long offset, u32 size)
{
    long addr = 0;
    u32	 erase_addr, erase_block = 0, i = 0;

    addr = flashdb_addr + offset;

    if (size % FLASH_BLOCK_SIZE) {
        ASSERT(0, "[flashDB]4k byte alignment is required\n");
    } else {
        erase_block = size / FLASH_BLOCK_SIZE;
    }

    for (i = 1, erase_addr = addr; i <= erase_block; i++, erase_addr += 4 * 1024) {
        norflash_ioctl(NULL, IOCTL_ERASE_SECTOR, erase_addr);
    }

    return size;
}

void factory_reset(void)
{
    if (0xFFFFFFFF == flashdb_addr) {
        return;
    }

    u32 addr = flashdb_addr;
    u32 block_num = (flashdb_size / FLASH_BLOCK_SIZE);

    for (int i = 0; i < block_num; i++) {
        norflash_ioctl(NULL, IOCTL_ERASE_SECTOR, addr);
        addr += FLASH_BLOCK_SIZE;
    }

    printf("cpu system_reset ....\n\n");
    system_reset();
}

const struct fal_flash_dev AC79_onchip_norflash = {
    .name       = "AC79_onchip_norflash",
    .addr       = 0,
    .len        = FLASHDB_CONFIG_SIZE,
    .blk_size   = FLASH_BLOCK_SIZE,
    .ops        = {flashdb_flash_init, flashdb_flash_read, flashdb_flash_write, flashdb_flash_erase},
    .write_gran = 1
};


