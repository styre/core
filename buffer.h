#ifndef buffer_h
#define buffer_h

#include "styre.h"

#include <stdint.h>
#include <stdlib.h>

typedef struct st_buffer st_buffer_t;

// Initialize a new buffer
st_status st_buffer_init(st_buffer_t **buffer, size_t initial_size);

// Append some bytes to the buffer
st_status st_buffer_append(st_buffer_t **buffer, const void *val, size_t len);

// Get a pointer to an offset into the buffer
void *st_buffer_offset_pointer(st_buffer_t *buffer, size_t offset);

#endif
