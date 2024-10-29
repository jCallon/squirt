#ifndef __STORAGE_H__
#define __STORAGE_H__

// Include ESP32 NVS API
// API: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/storage/nvs_flash.html
// Examples: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/storage/index.html
//           https://github.com/espressif/esp-idf/tree/a97a7b09/examples/storage/nvs_rw_value
#include "nvs_flash.h"

// Intialize NVS, wipe if it has an issue or there's a new version
bool storage_init(bool reinit);
// Get a handle to a namespace within NVS
bool storage_open(
    char *name,
    nvs_handle_t *nvs_handle);
// Close a handle to NVS
void storage_close(nvs_handle_t *nvs_handle);
// Wipe all NVS, reset device if desired to reinitialize all NVS handles
bool storage_wipe(bool reset);
// Get the value a key maps to in NVS
bool storage_get(
    nvs_handle_t nvs_handle,
    char *key,
    void *value,
    size_t num_value_bytes);
// Set the value a key maps to in NVS
bool storage_set(
    nvs_handle_t nvs_handle,
    char *key,
    void *value,
    size_t num_value_bytes);

#endif // __STORAGE_H__