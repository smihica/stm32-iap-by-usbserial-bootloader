#ifndef __NVS_H
#define __NVS_H

#include <stdint.h>
#include <stdlib.h>

typedef enum nvs_result {
    NVS_OK                             = 0x00,
    KEY_NOT_FOUND                      = 0x01,
    CAPACITY_NOT_ENOUGH                = 0x02,
    CAPACITY_MISMATCH                  = 0x03,
    DST_SIZE_NOT_ENOUGH                = 0x04,
    KEY_IS_TOO_LONG                    = 0x05,
    VALUE_SIZE_IS_LARGER_THAN_CAPACITY = 0x09,
    UNKNOWN_ERROR                      = 0xFF,
} nvs_result_t;

nvs_result_t nvs_init();
nvs_result_t nvs_commit();
nvs_result_t nvs_clear();
nvs_result_t nvs_get(char const* key, uint8_t* dst, uint16_t* value_size, uint16_t dst_size);
nvs_result_t nvs_put(char const* key, uint8_t const* value, uint16_t value_size, uint16_t capacity_size);
nvs_result_t nvs_del(char const* key);

#endif



