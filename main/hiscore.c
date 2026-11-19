#include <stdbool.h>
#include "nvs_flash.h"
#include "nvs.h"
#include "hiscore.h"

static nvs_handle_t nvs;
static bool ok;

void hiscore_init(void)
{
    esp_err_t e = nvs_flash_init();
    // NVS partition full or written by a newer IDF version → wipe and start over
    if (e == ESP_ERR_NVS_NO_FREE_PAGES || e == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        e = nvs_flash_init();
    }
    ok = e == ESP_OK && nvs_open("hiscore", NVS_READWRITE, &nvs) == ESP_OK;
}

int32_t hiscore_load(const char *key)
{
    int32_t v = 0, tmp;
    if (ok && nvs_get_i32(nvs, key, &tmp) == ESP_OK) v = tmp;
    return v;
}

void hiscore_save(const char *key, int32_t value)
{
    // Called only on a new record: flash has limited erase cycles, so never write every frame.
    if (!ok) return;
    nvs_set_i32(nvs, key, value);
    nvs_commit(nvs);
}
