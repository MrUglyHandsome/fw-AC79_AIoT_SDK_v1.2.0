#ifndef __MATTER_KEYVALUE_ADAPTER_H
#define __MATTER_KEYVALUE_ADAPTER_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char ssid[64];
    char pwd[33];
} wifi_config_info;

int matter_kv_init(void);
int matter_kv_read(const char *key, void *value, size_t value_size);
int matter_kv_write(const char *key, const void *value, size_t value_size);
int matter_kv_delete(const char *key);
int matter_kv_eraseAll(void);
int wifi_set_config(wifi_config_info *cfg);
int wifi_get_config(wifi_config_info *cfg);
int wifi_connect_by_config(void);
void wifi_config_clean(void);
int is_wifi_config_connected(void);

#ifdef __cplusplus
}
#endif

#endif // __MATTER_KEYVALUE_ADAPTER_H


