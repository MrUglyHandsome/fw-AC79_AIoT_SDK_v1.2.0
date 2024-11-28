#include "device/device.h"
#include "system/includes.h"
#include "fdb_def.h"
#include "fdb_cfg.h"
#include "matter_keyValue_adapter.h"

static OS_MUTEX mutex;
#define mutex_init()   os_mutex_create(&mutex)
#define mutex_enter()  os_mutex_pend(&mutex, 0)
#define mutex_exit()   os_mutex_post(&mutex)

static struct fdb_kvdb kvdb = { 0 };
static int kv_init_already = 0;
static struct fdb_default_kv_node default_kv_table[] = {
};

static void lock(fdb_db_t db)
{
    mutex_enter();
}

static void unlock(fdb_db_t db)
{
    mutex_exit();
}

int matter_kv_init(void)
{
    if (kv_init_already) {
        return 0;
    }
    struct fdb_default_kv default_kv;
    fdb_err_t result;

    default_kv.kvs = default_kv_table;
    default_kv.num = sizeof(default_kv_table) / sizeof(default_kv_table[0]);
    result = fdb_kvdb_init(&kvdb, "env", "fdb_kvdb1", &default_kv, NULL);
    if (result != FDB_NO_ERR) {
        printf("matter_kv_init err!\n");
        return -1;
    }

    kv_init_already = 1;
    mutex_init();
    fdb_kvdb_control(&kvdb, FDB_KVDB_CTRL_SET_LOCK, lock);
    fdb_kvdb_control(&kvdb, FDB_KVDB_CTRL_SET_UNLOCK, unlock);
    return 0;
}

int matter_kv_read(const char *key, void *value, size_t value_size)
{
    if (!kv_init_already) {
        printf("matter kv area not init!\n");
        return -1;
    }

    struct fdb_blob blob;
    fdb_kv_get_blob(&kvdb, key, fdb_blob_make(&blob, value, value_size));
    return blob.saved.len;
}

int matter_kv_write(const char *key, const void *value, size_t value_size)
{
    if (!kv_init_already) {
        printf("matter kv area not init!\n");
        return -1;
    }

    struct fdb_blob blob;
    fdb_err_t err = fdb_kv_set_blob(&kvdb, key, fdb_blob_make(&blob, value, value_size));
    if (FDB_NO_ERR != err) {
        return -1;
    }
    return value_size;
}

int matter_kv_delete(const char *key)
{
    fdb_kv_del(&kvdb, key);
    return 0;
}

int matter_kv_eraseAll(void)
{
    printf("[%s]Not supported yet!", __FUNCTION__, __LINE__);
    return 0;
}

int wifi_set_config(wifi_config_info *cfg)
{
    if (NULL == cfg) {
        return -1;
    }

    return matter_kv_write("wifi_cfg", cfg, sizeof(wifi_config_info));
}

int wifi_get_config(wifi_config_info *cfg)
{
    if (NULL == cfg) {
        return -1;
    }

    return matter_kv_read("wifi_cfg", cfg, sizeof(wifi_config_info));
}

static int connected_flag = 0; //指示已经在连接配置的wifi info

//指示是否wifi已经在连接配置的wifi info
int is_wifi_config_connected(void)
{
    return connected_flag;
}

//如果连接失败需要清除配置
void wifi_config_clean(void)
{
    connected_flag = 0;
    matter_kv_delete("wifi_cfg");
}

int wifi_connect_by_config(void)
{
    wifi_config_info cfg;
    if (wifi_get_config(&cfg) != sizeof(wifi_config_info)) {
        return -1;
    }

    wifi_enter_sta_mode(cfg.ssid, cfg.pwd);
    connected_flag = 1;
    return 0;
}
