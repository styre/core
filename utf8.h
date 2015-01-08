#ifndef utf8_h
#define utf8_h

#include <string.h>
#include <stdint.h>

typedef enum {
    st_ok,
    st_utf8_invalid,
    st_invalid_unicode,
    st_eof,
    st_out_of_memory,
} st_status;

st_status next_codepoint(const uint8_t *in, size_t len,
        uint32_t *out, size_t *bytes);

st_status encode_unicode(const uint32_t *in, size_t len,
        uint8_t **out, size_t *bytes);

#endif
