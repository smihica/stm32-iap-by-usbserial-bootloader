#ifndef __RING_BUFFER_H
#define __RING_BUFFER_H

#include <string.h>
#include "init.h"

typedef struct ring_buffer {
    uint8_t* buffer;
    uint32_t buffer_len_mask;
    uint32_t lead_ptr;
    uint32_t follow_ptr;
    uint32_t save_ptr;
} ring_buffer_t;

typedef enum ring_buffer_result {
    RING_BUFFER_NOT_FOUND    = -1,
    RING_BUFFER_OK           = 0,
    RING_BUFFER_NO_SPACE     = 1,
    RING_BUFFER_OUT_OF_RANGE = 2,
} ring_buffer_result_t;

void ring_buffer_reset(ring_buffer_t* r);
ring_buffer_result_t ring_buffer_push(ring_buffer_t* r, uint8_t data);
int32_t ring_buffer_pop(ring_buffer_t* r);
void ring_buffer_save_point(ring_buffer_t* r);
void ring_buffer_revert_save_point(ring_buffer_t* r);
uint32_t ring_buffer_length(ring_buffer_t const* r);
uint8_t ring_buffer_index(ring_buffer_t const* r, uint32_t idx);
uint8_t ring_buffer_index_with_offset(ring_buffer_t const* r, uint32_t idx, uint32_t offset);

#endif
