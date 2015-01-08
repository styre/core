#include "utf8.h"

#include <stdlib.h>

st_status next_codepoint(const uint8_t *in, size_t len,
        uint32_t *out, size_t *bytes)
{
    if (len == 0)
        return st_eof;

    const uint8_t first = in[0];

    // Get the next character
    if (first <= 0x7F) {

        // Single ASCII character
        *out = first;

        // Set number of bytes consumed
        *bytes = 1;

        return st_ok;

    } else if ((first & 0xE0) == 0xC0) {

        // Set number of bytes consumed
        *bytes = 2;

        // Load first byte into place
        *out = first & 0x1F;

    } else if ((first & 0xF0) == 0xE0) {

        // Set number of bytes consumed
        *bytes = 3;

        // Load first byte into place
        *out = first & 0x0F;

    } else if ((first & 0xF8) == 0xF0) {

        // Set number of bytes consumed
        *bytes = 4;

        // Load first byte into place
        *out = first & 0x07;

    }

    // Two byte character
    if (len < *bytes)
        return st_eof;

    // Load remaining bytes
    for (int i = 1; i < *bytes; i++) {
        // Check that this byte is a valid UTF-8 byte
        if ((in[i] & 0xC0) == 0x80) {
            // Append bytes
            *out = (*out << 6) | (in[i] & 0x3F);
        } else {
            // Invalid bytes reached
            return st_utf8_invalid;
        }
    }

    // Return 1 to indicate success
    return st_ok;
}

st_status encode_unicode(const uint32_t *in, size_t len,
        uint8_t **out, size_t *bytes)
{
    *bytes = 0;
    *out = malloc(len * 4);
    if (*out == NULL)
        return st_out_of_memory;

    for (int i = 0; i < len; i++) {
        if (in[i] < 0x80) {
            *out[*bytes++] = in[i];
        } else if (in[i] < 0x800) {
            *out[*bytes++] = 192 + in[i] / 64;
            *out[*bytes++] = 128 + in[i] % 64;
        } else if ((in[i] - 0xd800u) < 0x800) {
            goto error;
        } else if (in[i] < 0x10000) {
            *out[*bytes++] = 224 + in[i] / 4096;
            *out[*bytes++] = 128 + in[i] / 64 % 64;
            *out[*bytes++] = 128 + in[i] % 64;
        } else if (in[i] < 0x110000) {
            *out[*bytes++] = 240 + in[i] / 262144;
            *out[*bytes++] = 128 + in[i] / 4096 % 64;
            *out[*bytes++] = 128 + in[i] / 64 % 64;
            *out[*bytes++] = 128 + in[i] % 64;
        } else {
            goto error;
        }
    }

    return st_ok;

error:
    return st_invalid_unicode;
}
