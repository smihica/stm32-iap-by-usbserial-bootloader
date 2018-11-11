#include "init.h"
#include "packet.h"
#include "crc.h"
#include "ring_buffer.h"
#include "gpio.h"

static uint8_t rx_buffer[0x400] = { 0 };
static ring_buffer_t rx = { 0 };

void packet_parser_init()
{
    rx.buffer          = rx_buffer;
    rx.buffer_len_mask = 0x400 - 1; // 0x3FF = 1023 = 0011 1111 1111
    rx.lead_ptr        = 0;         // read_ptr
    rx.follow_ptr      = 0;         // follow_ptr
    rx.save_ptr        = 0;         // save_ptr
}

void packet_parser_reset()
{
    rx.lead_ptr        = 0;         // read_ptr
    rx.follow_ptr      = 0;         // follow_ptr
    rx.save_ptr        = 0;         // save_ptr
}

void packet_parser_bulk_push(uint8_t const* src, uint32_t size)
{
    for (uint32_t i = 0; i < size; i++) {
        ring_buffer_push(&rx, src[i]);
    }
}

typedef enum parser_state {
    PREAMBLE_1     = 0x00,
    PREAMBLE_2     = 0x01,
    TYPE           = 0x02,
    SENDER_ID      = 0x03,
    DESTINATION_ID = 0x04,
    PACKET_ID      = 0x05,
    OPERATOR       = 0x06,
    LENGTH         = 0x07,
    DATA           = 0x08,
    CRC_1          = 0x09,
    CRC_2          = 0x0A,
    CRC_3          = 0x0B,
    CRC_4          = 0x0C,
} parser_state_t;

uint8_t packet_parse(packet_t* p)
{
    if (ring_buffer_length(&rx) == 0) return 0;
    uint32_t tmp = 0;
    uint32_t crc32 = 0;
    uint8_t type, sender_id, destination_id, packet_id, op, len;
    parser_state_t state = PREAMBLE_1;
    ring_buffer_save_point(&rx);
    while (1) {
        int32_t x = ring_buffer_pop(&rx);
        if (x < 0) {
            ring_buffer_revert_save_point(&rx);
            break;
        }
        uint8_t c = (uint8_t)x;
        switch (state) {
        case PREAMBLE_1: {
            if (c == 0xAA) {
                state = PREAMBLE_2;
            } else {
                ring_buffer_save_point(&rx); // eat 1
            }
            break;
        }
        case PREAMBLE_2: {
            if (c == 0x55) {
                state = TYPE;
            } else {
                ring_buffer_revert_save_point(&rx);
                ring_buffer_pop(&rx);
                ring_buffer_save_point(&rx); // eat 1
                state = PREAMBLE_1;
                continue;
            }
            break;
        }
        case TYPE: {
            if (c != 0x00 && c != 0x01) {
                // this is not a type
                // this must not be a packet. zap to first.
                ring_buffer_revert_save_point(&rx);
                ring_buffer_pop(&rx);
                ring_buffer_save_point(&rx); // eat 1
                state = PREAMBLE_1;
                continue;
            } else {
                type = c;
                state = SENDER_ID;
            }
            break;
        }
        case SENDER_ID: {
            sender_id = c;
            state = DESTINATION_ID;
            break;
        }
        case DESTINATION_ID: {
            destination_id = c;
            state = PACKET_ID;
            break;
        }
        case PACKET_ID: {
            packet_id = c;
            state = OPERATOR;
            break;
        }
        case OPERATOR: {
            op = c;
            state = LENGTH;
            break;
        }
        case LENGTH: {
            if (c > (0x100 - 8)) { // header size is 8.
                // length too long
                // this must not be a packet. zap to first.
                ring_buffer_revert_save_point(&rx);
                ring_buffer_pop(&rx);
                ring_buffer_save_point(&rx); // eat 1
                state = PREAMBLE_1;
                continue;
            } else {
                len = c;
                tmp = 0;
                if (len > 0) {
                    state = DATA;
                } else {
                    state = CRC_1;
                }
            }
            break;
        }
        case DATA: {
            tmp++;
            if (tmp >= len) {
                state = CRC_1;
            }
            break;
        }
        case CRC_1: {
            crc32 = c;
            state = CRC_2;
            break;
        }
        case CRC_2: {
            tmp = c;
            crc32 |= (tmp << 8);
            state = CRC_3;
            break;
        }
        case CRC_3: {
            tmp = c;
            crc32 |= (tmp << 16);
            state = CRC_4;
            break;
        }
        case CRC_4: {
            tmp = c;
            crc32 |= (tmp << 24);
            for (uint16_t i = 0; i < len + 8; i++) {
                p->data[i] = ring_buffer_index_with_offset(&rx, i, rx.save_ptr);
            }
            uint32_t crc32_calc = calc_crc32((uint32_t*)(&p->data[0]), len + 8);
            if (crc32 == crc32_calc) {
                p->type = type;
                p->sender_id = sender_id;
                p->destination_id = destination_id;
                p->packet_id = packet_id;
                p->op   = op;
                p->len  = len;
                for (uint16_t i = 0; i < len; i++) {
                    p->data[i] = p->data[i+8];
                }
                ring_buffer_save_point(&rx);
                return 1;
            } else {
                // LOG("CRC not match %08lx != %08lx", crc32, crc32_calc);
                // CRC not match. zap to first.
                ring_buffer_revert_save_point(&rx);
                ring_buffer_pop(&rx);
                ring_buffer_save_point(&rx); // eat 1
                state = PREAMBLE_1;
                continue;
            }
            break;
        }
        }
    }
    return 0;
}

uint32_t packet_serialize(packet_t const* p, uint8_t* dest, uint32_t size)
{
    if (size < p->len+12) return 0;
    // set preamble
    dest[0] = 0xAA;
    dest[1] = 0x55;
    // type
    dest[2] = (uint8_t)p->type;
    dest[3] = p->sender_id;
    dest[4] = p->destination_id;
    dest[5] = p->packet_id;
    dest[6] = p->op;
    dest[7] = p->len;
    memcpy(&(dest[8]), p->data, p->len);
    uint32_t crc32 = calc_crc32((uint32_t*)dest, p->len+8);
    dest[p->len+8]  = (uint8_t)(crc32         & 0xFF);
    dest[p->len+9]  = (uint8_t)((crc32 >> 8)  & 0xFF);
    dest[p->len+10] = (uint8_t)((crc32 >> 16) & 0xFF);
    dest[p->len+11] = (uint8_t)((crc32 >> 24) & 0xFF);
    return p->len+12;
}
