#include "init.h"
#include "packet.h"
#include "gpio.h"
#include "usb_device.h"
#include "main.h"
#include "partition.h"
#include "nvs.h"
#include "crc.h"

extern uint32_t toggle_time_r;
extern uint32_t toggle_time_g;
extern uint32_t toggle_time_b;

typedef enum flasher_op {
    OP_NOP         = 0x00,
    OP_GET_STATUS  = 0x01,
    OP_ERASE       = 0x02,
    OP_WRITE_START = 0x03,
    OP_WRITE_DATA  = 0x04,
    OP_WRITE_END   = 0x05,
    OP_RESET       = 0x06,
} flasher_op_t;

typedef enum flasher_result {
    ACK  = 0x79,
    NACK = 0x1F
} flasher_result_t;

typedef struct write_state {
    uint8_t started;
    firm_partition_t target_partition;
    uint32_t written_bytes;
} write_state_t;

static void get_status(packet_t const* p)
{
    packet_t sp = { 0 };
    sp.type = Response;
    sp.sender_id = p->destination_id;
    sp.packet_id = p->packet_id;
    sp.op        = p->op;
    sp.len       = 5;
    uint16_t id = (uint16_t)(DBGMCU->IDCODE & 0xFFF); //Retrieves MCU ID from DEBUG interface
    uint16_t size = 0;
    uint8_t current_run_mode = APP_MODE;
    uint8_t current_firm     = FIRM0;
    if (nvs_get("RUN_MODE", &current_run_mode, &size, 1) != NVS_OK ||
        nvs_get("RUN_FIRM", &current_firm, &size, 1)     != NVS_OK)
    {
        sp.data[0] = NACK;
        sp.data[1] = (id)      & 0xFF;
        sp.data[2] = (id >> 8) & 0xFF;
        sp.data[3] = 0;
        sp.data[4] = 0;
    } else {
        sp.data[0] = ACK;
        sp.data[1] = (id)      & 0xFF;
        sp.data[2] = (id >> 8) & 0xFF;
        sp.data[3] = current_run_mode;
        sp.data[4] = current_firm;
    }
    USB_SendQueuePacket(&sp);
    toggle_time_b = 8;
}

static uint32_t erase_firm(firm_partition_t firm)
{
    FLASH_EraseInitTypeDef erase_info;
    uint32_t error_page = 0;
    switch (firm) {
    case FIRM0:
    default:
        erase_info.PageAddress = FIRM0_START_ADDRESS;
        erase_info.NbPages     = (FIRM0_SIZE + 4) / (2 * 0x400);
        break;
    case FIRM1:
        erase_info.PageAddress = FIRM1_START_ADDRESS;
        erase_info.NbPages     = (FIRM1_SIZE + 4) / (2 * 0x400);
        break;
    }
    erase_info.TypeErase = FLASH_TYPEERASE_PAGES;
    HAL_FLASH_Unlock();
    if (HAL_FLASHEx_Erase(&erase_info, &error_page) != HAL_OK) {
        HAL_FLASH_Lock();
        return error_page;
    }
    HAL_FLASH_Lock();
    return 0;
}

static void erase(packet_t const* p)
{
    firm_partition_t firm = (firm_partition_t)(p->data[0]);
    uint32_t error_page = erase_firm(firm);
    packet_t sp = { 0 };
    sp.type = Response;
    sp.sender_id = p->destination_id;
    sp.packet_id = p->packet_id;
    sp.op        = p->op;
    sp.len       = 2;
    sp.data[0]   = (error_page == 0) ? ACK : NACK;
    sp.data[1]   = error_page & 0xFF;
    USB_SendQueuePacket(&sp);
}

static write_state_t write_state = {
    .started          = 0,
    .target_partition = FIRM0,
    .written_bytes    = 0,
};

static void write_start(packet_t const* p)
{
    firm_partition_t partition = (firm_partition_t)(p->data[0]);
    write_state.started = 1;
    write_state.target_partition = partition;
    write_state.written_bytes = 0;
    packet_t sp = { 0 };
    sp.type = Response;
    sp.sender_id = p->destination_id;
    sp.packet_id = p->packet_id;
    sp.op        = p->op;
    sp.len       = 1;
    uint8_t next_run_mode = APP_MODE;
    if (nvs_put("RUN_MODE", &next_run_mode, 1, 1) == NVS_OK &&
        nvs_commit()                              == NVS_OK)
    {
        sp.data[0]   = ACK;
        USB_SendQueuePacket(&sp);
    } else {
        sp.data[0] = NACK;
        USB_SendQueuePacket(&sp);
    }
    return;
}

static void write_data(packet_t const* p)
{
    uint32_t base_address = 0;
    packet_t sp = { 0 };
    sp.type = Response;
    sp.sender_id = p->destination_id;
    sp.packet_id = p->packet_id;
    sp.op        = p->op;
    sp.len       = 1;

    if (!write_state.started) {
        goto send_nak;
    }

    switch (write_state.target_partition) {
    case FIRM0:
        base_address = FIRM0_START_ADDRESS;
        break;
    case FIRM1:
        base_address = FIRM1_START_ADDRESS;
        break;
    default:
        goto send_nak;
    }

    if ((p->len % 4) != 0) {
        goto send_nak;
    }

    HAL_FLASH_Unlock(); //Unlocks the flash memory

    /* Decode the sent bytes using AES-128 ECB */
    // aes_enc_dec((uint8_t*) pucData, AES_KEY, 1);

    uint32_t d = 0;
    for (uint32_t i = 0; i < p->len; i+=4) {
        memcpy(&d, p->data+i, sizeof(uint32_t));
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, base_address + write_state.written_bytes, d) != HAL_OK)
        {
            HAL_FLASH_Lock();
            goto send_nak;
        }
        write_state.written_bytes += 4;
    }

    HAL_FLASH_Lock(); //Locks again the flash memory

    sp.data[0] = ACK;
    USB_SendQueuePacket(&sp);
    return;

send_nak:
    sp.data[0] = NACK;
    USB_SendQueuePacket(&sp);

}

static void write_end(packet_t const* p)
{
    uint32_t firm_address = 0;
    uint32_t firm_size = 0;
    uint32_t crc_address = 0;
    uint32_t calc_crc = 0;
    packet_t sp = { 0 };
    sp.type = Response;
    sp.sender_id = p->destination_id;
    sp.packet_id = p->packet_id;
    sp.op        = p->op;
    sp.len       = 1;

    if (!write_state.started) {
        goto send_nak;
    }

    switch (write_state.target_partition) {
    case FIRM0:
        firm_address = FIRM0_START_ADDRESS;
        firm_size = FIRM0_SIZE;
        crc_address = FIRM0_CRC_ADDRESS;
        break;
    case FIRM1:
        firm_address = FIRM1_START_ADDRESS;
        firm_size = FIRM1_SIZE;
        crc_address = FIRM1_CRC_ADDRESS;
        break;
    default:
        goto send_nak;
    }

    calc_crc = calc_crc32((uint32_t*)(firm_address), firm_size);

    HAL_FLASH_Unlock(); //Unlocks the flash memory

    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, crc_address, calc_crc) != HAL_OK)
    {
        HAL_FLASH_Lock();
        goto send_nak;
    }

    HAL_FLASH_Lock();

    sp.data[0] = ACK;
    USB_SendQueuePacket(&sp);
    return;

send_nak:
    sp.data[0] = NACK;
    USB_SendQueuePacket(&sp);

}

static void reset(packet_t const* p)
{
    packet_t sp = { 0 };
    sp.type = Response;
    sp.sender_id = p->destination_id;
    sp.packet_id = p->packet_id;
    sp.op        = p->op;
    sp.len       = 1;
    if (p->len != 2) {
        sp.data[0]   = NACK;
        USB_SendQueuePacket(&sp);
    } else {
        uint8_t next_run_mode = (run_mode_t)(p->data[0]);
        uint8_t next_firm     = (firm_partition_t)(p->data[1]);
        if (nvs_put("RUN_MODE", &next_run_mode, 1, 1) == NVS_OK &&
            nvs_put("RUN_FIRM", &next_firm, 1, 1)     == NVS_OK &&
            nvs_commit()                              == NVS_OK)
        {
            sp.data[0] = ACK;
            USB_SendQueuePacket(&sp);
            HAL_Delay(100);
            NVIC_SystemReset();
            HAL_Delay(1000);
        } else {
            sp.data[0] = NACK;
            USB_SendQueuePacket(&sp);
        }
    }
}

void dispatch_packet(packet_t const* p)
{
    switch (p->op) {
    case OP_GET_STATUS:
        get_status(p);
        break;
    case OP_ERASE:
        erase(p);
        break;
    case OP_WRITE_START:
        write_start(p);
        break;
    case OP_WRITE_DATA:
        write_data(p);
        break;
    case OP_WRITE_END:
        write_end(p);
        break;
    case OP_RESET:
        reset(p);
        break;
    default:
        break;
    }
}
