#ifndef token_h
#define token_h

#include "styre.h"

#include <stdlib.h>
#include <stdint.h>

// Definitions
typedef struct st_token st_token_t;

// Token types
typedef enum {
    st_token_type_uninitialized = 0,
    st_token_type_error,
    st_token_type_character,
    st_token_type_eof,
    st_token_type_start_tag,
    st_token_type_end_tag,
} st_token_type_t;

//
// Create and modity
//

// Create a new token
st_status st_token_init(st_token_t **token);
st_status st_token_reset(st_token_t *token);

// Set token types
st_status st_token_set_error(st_token_t *token, uint32_t codepoint,
        const char *message, uint32_t line, uint32_t column);
st_status st_token_set_character(st_token_t *token, uint32_t codepoint);
st_status st_token_set_start_tag(st_token_t *token, uint32_t codepoint);
st_status st_token_set_end_tag(st_token_t *token, uint32_t codepoint);


// Append to the name of a tag token
st_status st_token_tag_append_name(st_token_t *token, uint32_t codepoint);

// Add attributes and attribute values
st_status st_token_attr_add(st_token_t *token);
st_status st_token_attr_append_name(st_token_t *token, uint32_t codepoint);
st_status st_token_attr_append_value(st_token_t *token, uint32_t codepoint);

//
// Get token information
//

st_token_type_t st_token_type(st_token_t *token);
uint32_t st_token_codepoint(st_token_t *token);

st_status st_token_tag_name(st_token_t *token, uint8_t **buffer, size_t *bytes);

size_t st_token_attr_num(st_token_t *token);
st_status st_token_attr_name(st_token_t *token,
        size_t attr_num, uint8_t **buffer, size_t *bytes);
st_status st_token_attr_value(st_token_t *token,
        size_t attr_num, uint8_t **buffer, size_t *bytes);

#endif
