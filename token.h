#ifndef token_h
#define token_h

#include <stdlib.h>
#include <stdint.h>

// Token types
typedef enum {
    st_token_type_character,
    st_token_type_eof,
    st_token_type_start_tag,
    st_token_type_end_tag,
} st_token_type_t;

// Charachter token type
typedef struct {
    uint32_t codepoint;
} st_token_character_t;

// Tag token
typedef struct {
    uint32_t name[50]; // TODO: Fix to allow longer names
    size_t len;
} st_token_tag_t;

// Structure for tokens
typedef struct {
    st_token_type_t type;
    union {
        st_token_character_t character;
        st_token_tag_t tag;
    };
} st_token_t;

#endif
