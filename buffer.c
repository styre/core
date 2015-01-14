#include "buffer.h"

#include <string.h>

struct st_buffer {
    uint8_t *ptr;
    size_t used;
    size_t allocated;
};

// Initialize a new buffer
st_status st_buffer_init(st_buffer_t **buffer, size_t initial_size)
{
    *buffer = malloc(sizeof(**buffer) + initial_size);
    if (*buffer == NULL)
        return st_out_of_memory;

    (*buffer)->ptr = ((uint8_t *)*buffer) + sizeof(**buffer);
    (*buffer)->used = 0;
    (*buffer)->allocated = initial_size;

    return st_ok;
}

// Append some bytes to the buffer
st_status st_buffer_append(st_buffer_t **buffer,
        const void *val, size_t len)
{
    // Expand the buffer if needed
    if ((*buffer)->used + len > (*buffer)->allocated) {
        st_buffer_t *tmp = realloc(*buffer, (*buffer)->allocated * 2);

        if (tmp == NULL)
            return st_out_of_memory;

        // Update pointer to memory
        tmp->ptr = ((uint8_t *)tmp) + sizeof(**buffer);
        tmp->allocated *= 2;

        // Update the buffer pointer
        *buffer = tmp;
    }

    // Copy data into the buffer
    memcpy((*buffer)->ptr + (*buffer)->used, val, len);

    // Update number of bytes used
    (*buffer)->used += len;

    return st_ok;
}

void *st_buffer_offset_pointer(st_buffer_t *buffer, size_t offset)
{
    return buffer->ptr + offset;
}
