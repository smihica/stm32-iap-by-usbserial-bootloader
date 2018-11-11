#include "init.h"
#include <string.h>
#include "nvs.h"
#include "partition.h"
#include "gpio.h"

/*

# Entry structure
- [KEY 8byte], [ENTRY_CAP_SIZE 2byte], [VALUE_SIZE 2byte], [ENTRY ENTRY_CAP_SIZEbytes]

*/

static inline uint16_t read16(void const* p)
{
    uint8_t const* x = (uint8_t const*)p;
    return x[0]|(x[1]<<8);
}

static inline uint64_t read64(void const* p)
{
    uint8_t const* x = (uint8_t const*)p;
    return x[0]|(x[1]<<8)|(x[2]<<16)|((uint32_t)x[3]<<24)|( (uint64_t)(x[4]|(x[5]<<8)|(x[6]<<16)|((uint32_t)x[7]<<24)) <<32);
}

static inline void write16(void* p, uint16_t w)
{
    uint8_t* x = (uint8_t*)p;
    x[0] = w & 0xFF;
    x[1] = (w>>8) & 0xFF;
}

static inline void write64(void* p, uint64_t w)
{
    uint8_t* x = (uint8_t*)p;
    x[0] = w & 0xFF;
    x[1] = (w>>8) & 0xFF;
    x[2] = (w>>16) & 0xFF;
    x[3] = (w>>24) & 0xFF;
    x[4] = (w>>32) & 0xFF;
    x[5] = (w>>40) & 0xFF;
    x[6] = (w>>48) & 0xFF;
    x[7] = (w>>56) & 0xFF;
}


static uint8_t nvs_memory[NVS_SIZE];

nvs_result_t nvs_init()
{
    memcpy(nvs_memory, (uint32_t const*)(NVS_START_ADDRESS), NVS_SIZE);
    return NVS_OK;
}

nvs_result_t nvs_commit()
{
    FLASH_EraseInitTypeDef erase_info = { 0 };
    uint32_t error_page = 0;
    erase_info.PageAddress = NVS_START_ADDRESS;
    erase_info.NbPages     = NVS_SIZE / ( 2 * 0x400 );
    erase_info.TypeErase   = FLASH_TYPEERASE_PAGES;
    HAL_FLASH_Unlock();
    if (HAL_FLASHEx_Erase(&erase_info, &error_page) != HAL_OK) {
        // error_block has the number of bad block.
        HAL_FLASH_Lock();
        return UNKNOWN_ERROR;
    }
    uint32_t const* itr = (uint32_t const*)(nvs_memory);
    for (uint32_t i = 0; i < (NVS_SIZE / 4); i++) {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, NVS_START_ADDRESS + ( i * 4 ), itr[i]) != HAL_OK) {
            HAL_FLASH_Lock();
            return UNKNOWN_ERROR;
        }
    }
    HAL_FLASH_Lock();
    return NVS_OK;
}

nvs_result_t nvs_clear()
{
    memset(nvs_memory, 0, NVS_SIZE);
    return NVS_OK;
}

nvs_result_t nvs_get(char const* key, uint8_t* dst, uint16_t* value_size, uint16_t dst_size)
{
    if (strlen(key) > 8) return KEY_IS_TOO_LONG;
    char key_data[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    memcpy(key_data, key, strlen(key));
    uint64_t const k = read64(key_data);
    uint8_t const* ptr = nvs_memory;
    while (ptr < (nvs_memory + NVS_SIZE - 12))
    {
        uint64_t const current_key = read64(ptr);
        if (current_key == 0) {
            // last
            break;
        } else if (current_key == k) {
            // found
            uint16_t vs = read16(ptr + 10);
            if (dst_size < vs) {
                return DST_SIZE_NOT_ENOUGH;
            }
            if (ptr + 12 + vs > nvs_memory + NVS_SIZE) {
                break;
            }
            memcpy(dst, ptr + 12, vs);
            *value_size = vs;
            return NVS_OK;
        } else {
            // check next
            uint16_t const entry_cap_size = read16(ptr + 8);
            ptr = ptr + 12 + entry_cap_size;
        }
    }
    return KEY_NOT_FOUND;
}

nvs_result_t nvs_put(char const* key, uint8_t const* value, uint16_t value_size, uint16_t capacity_size)
{
    if (strlen(key) > 8) return KEY_IS_TOO_LONG;
    if (value_size > capacity_size) return VALUE_SIZE_IS_LARGER_THAN_CAPACITY;
    char key_data[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    memcpy(key_data, key, strlen(key));
    uint64_t const k = read64(key_data);
    uint8_t* ptr = nvs_memory;
    while (ptr < (nvs_memory + NVS_SIZE - 12))
    {
        uint64_t const current_key = read64(ptr);
        if (current_key == 0) {
            if ((ptr + 12 + capacity_size) > (nvs_memory + NVS_SIZE)) {
                break;
            }
            write64(ptr, k);
            write16(ptr + 8, capacity_size);
            write16(ptr + 10, value_size);
            memcpy(ptr + 12, value, value_size);
            return NVS_OK;
        } else if (current_key == k) {
            // found
            uint16_t const entry_cap_size = read16(ptr + 8);
            if (capacity_size > entry_cap_size) return CAPACITY_MISMATCH;
            write16(ptr + 10, value_size);
            memcpy(ptr + 12, value, value_size);
            return NVS_OK;
        } else {
            // check next
            uint16_t const entry_cap_size = read16(ptr + 8);
            ptr = ptr + 12 + entry_cap_size;
        }
    }
    return CAPACITY_NOT_ENOUGH;
}

nvs_result_t nvs_del(char const* key)
{
    if (strlen(key) > 8) return KEY_IS_TOO_LONG;
    char key_data[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    memcpy(key_data, key, strlen(key));
    uint64_t const k = read64(key_data);
    uint8_t* ptr = nvs_memory;
    while (ptr < nvs_memory + NVS_SIZE - 12)
    {
        uint64_t const current_key = read64(ptr);
        if (current_key == 0) {
            // last
            break;
        } else if (current_key == k) {
            // found
            uint16_t const entry_cap_size = read16(ptr + 8);
            if (ptr + 12 + entry_cap_size > nvs_memory + NVS_SIZE) {
                break;
            }
            while (ptr + 12 + entry_cap_size < nvs_memory + NVS_SIZE) {
                *ptr = *(ptr + 12 + entry_cap_size);
                ptr++;
            }
            while (ptr < nvs_memory + NVS_SIZE) {
                *ptr = 0;
                ptr++;
            }
            return NVS_OK;
        } else {
            // check next
            uint16_t const entry_cap_size = read16(ptr + 8);
            ptr = ptr + 12 + entry_cap_size;
        }
    }
    return KEY_NOT_FOUND;
}
