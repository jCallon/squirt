// Include custom storage API
#include "storage.h"
// Include custom debug macros and compile flags
#include "flags.h"

// This storage API is unencrpyted.
// NOTE: NVS is not directly compatible with the ESP32 flash encryption system.
//       However, data can still be stored in encrypted form if NVS encryption
//       is used together with ESP32 flash encryption.
//       Please refer to NVS Encryption for more details.
//       NVS Encryption: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/storage/nvs_encryption.html

bool storage_init()
{
    // Initilaize NVS, if erasing first if necessary
    esp_err_t status = nvs_flash_init();
    if ((ESP_ERR_NVS_NO_FREE_PAGES == status) || (ESP_ERR_NVS_NEW_VERSION_FOUND == status))
    {
        // NVS partition was truncated and needs to be erased
        ESP_ERROR_RETURN_FALSE_IF_FAILED(status, nvs_flash_erase());
        ESP_ERROR_RETURN_FALSE_IF_FAILED(status, nvs_flash_init());
    }
    ESP_ERROR_RETURN_FALSE_IF_FAILED(status, status);

    // Return success
    return true;
}

bool storage_open(
    char *name,
    nvs_handle_t *nvs_handle)
{
    // Get NVS handle
    esp_err_t status = ESP_OK;
    ESP_ERROR_RETURN_FALSE_IF_FAILED(status, 
        nvs_open(
            /* const char *name = */ name,
            /* nvs_open_mode_t open_mode = */ NVS_READWRITE,
            /* nvs_handle_t *out_handle = */ nvs_handle));

    // Return success
    return true;
}

void storage_close(nvs_handle_t *nvs_handle)
{
    nvs_close(/* nvs_handle_t handle = */ *nvs_handle);
    *nvs_handle = 0;
}

bool storage_wipe(bool reset)
{
    esp_err_t status = ESP_OK;
    ESP_ERROR_RETURN_FALSE_IF_FAILED(status, nvs_flash_erase());
    if(true == reset)
    {
        esp_restart();
    }
    return true;
}

bool storage_get(
    nvs_handle_t nvs_handle,
    char *key,
    void *value,
    size_t num_value_bytes)
{
    // Get the number of bytes needed to store the value for the requested key
    // NOTE: "To get the size necessary to store the value, ...
    //        call nvs_get_blob with zero out_value and non-zero pointer to length"
    esp_err_t status = ESP_OK;
    size_t num_bytes_required = 0;
    ESP_ERROR_RETURN_FALSE_IF_FAILED(status,
        nvs_get_blob(
            /* nvs_handle_t handle = */ nvs_handle,
            /* const char *key = */ key,
            /* void *out_value = */ nullptr,
            /* size_t *length = */ &num_bytes_required));
    if(num_value_bytes < num_bytes_required)
    {
        return false;
    }

    // Now that we know the buffer to store the value is big enough,
    // we can safely try to copy it into the buffer
    ESP_ERROR_RETURN_FALSE_IF_FAILED(status,
        nvs_get_blob(
            /* nvs_handle_t handle = */ nvs_handle,
            /* const char *key = */ key,
            /* void *out_value = */ value,
            /* size_t *length = */ &num_bytes_required));
    return true;
}

bool storage_set(
    nvs_handle_t nvs_handle,
    char *key,
    void *value,
    size_t num_value_bytes)
{
    // Try to set the requested key value pair
    esp_err_t status = ESP_OK;
    ESP_ERROR_RETURN_FALSE_IF_FAILED(status,
        nvs_set_blob(
            /* nvs_handle_t handle = */ nvs_handle,
            /* const char *key = */ key,
            /* void *value = */ value,
            /* size_t length = */ num_value_bytes));

    // Commit changes to NVS
    ESP_ERROR_RETURN_FALSE_IF_FAILED(status, nvs_commit(/*nvs_handlt_t handle = */ nvs_handle));

    // Return success
    return true;
}