#ifndef _PACKET_H
#define _PACKET_H

#include "init.h"
#include "ring_buffer.h"

typedef enum packet_type {
    Command  = 0x00,
    Response = 0x01,
} packet_type_t;

typedef struct packet {
    packet_type_t type;
    uint8_t sender_id;
    uint8_t destination_id;
    uint8_t packet_id;
    uint8_t op;
    uint8_t len;
    uint8_t data[0x100];
} packet_t;

void packet_parser_init();
void packet_parser_reset();
void packet_parser_bulk_push(uint8_t const* src, uint32_t size);

uint8_t packet_parse(packet_t* p);
uint32_t packet_serialize(packet_t const* p, uint8_t* dest, uint32_t size);

#endif // _PACKET_HPP
