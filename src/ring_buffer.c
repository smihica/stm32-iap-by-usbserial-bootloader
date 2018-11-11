#include "init.h"
#include "ring_buffer.h"

void ring_buffer_reset(ring_buffer_t* r)
{
    r->lead_ptr = 0;
    r->follow_ptr = 0;
}

ring_buffer_result_t ring_buffer_push(ring_buffer_t* r, uint8_t data)
{
    if ((uint32_t)(r->follow_ptr - r->lead_ptr) == 1) {
        return RING_BUFFER_NO_SPACE;
    }
    r->buffer[r->lead_ptr] = data;
    r->lead_ptr = (r->lead_ptr + 1) & r->buffer_len_mask;
    return RING_BUFFER_OK;
}

int32_t ring_buffer_pop(ring_buffer_t* r)
{
    if (r->lead_ptr == r->follow_ptr) {
        return (int32_t)(RING_BUFFER_NOT_FOUND);
    }
    int32_t rt = (int32_t)(r->buffer[r->follow_ptr]);
    r->follow_ptr = (r->follow_ptr + 1) & r->buffer_len_mask;
    return rt;
}

void ring_buffer_save_point(ring_buffer_t* r)
{
    r->save_ptr = r->follow_ptr;
}

void ring_buffer_revert_save_point(ring_buffer_t* r)
{
    r->follow_ptr = r->save_ptr;
}

uint32_t ring_buffer_length(ring_buffer_t const* r)
{
    return (
        (r->lead_ptr >= r->follow_ptr) ?
        (r->lead_ptr - r->follow_ptr) :
        ((r->buffer_len_mask + 1) - r->follow_ptr + r->lead_ptr)
    );
}

uint8_t ring_buffer_index(ring_buffer_t const* r, uint32_t idx)
{
    return r->buffer[(r->follow_ptr+idx) & r->buffer_len_mask];
}

uint8_t ring_buffer_index_with_offset(ring_buffer_t const* r, uint32_t idx, uint32_t offset)
{
    return r->buffer[(offset+idx) & r->buffer_len_mask];
}
