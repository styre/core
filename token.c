#include "token.h"

#include <string.h>
#include <assert.h>

#include "buffer.h"

#define STRLEN 10

//
// Token types
//

// Error token
typedef struct {
    uint32_t codepoint;
    const char *message;
    uint32_t line;
    uint32_t column;
} st_token_error_t;

// Charachter token type
typedef struct {
    uint32_t codepoint;
} st_token_character_t;

// Tag token attribute
typedef struct {
    st_buffer_t *name;
    size_t name_len;
    st_buffer_t *value;
    size_t value_len;
} st_token_attribute_t;

// Tag token
typedef struct {
    st_buffer_t *name;              // The tag name
    size_t len;                     // Current length of the tag name
    size_t allocated_bytes;         // Number of bytes allocated to the name

    st_buffer_t *attrs;
    size_t num_attrs;
} st_token_tag_t;

//
// The token
//

// Structure for tokens
struct st_token {
    st_token_type_t type;
    union {
        st_token_error_t error;
        st_token_character_t character;
        st_token_tag_t tag;
    };
};

st_status st_token_init(st_token_t **token)
{
    // Allocate memory
    *token = malloc(sizeof(**token));
    if (token == NULL)
        return st_out_of_memory;

    // Set entire struct to zero
    memset(*token, 0, sizeof(**token));

    return st_ok;
}

st_status st_token_reset(st_token_t *token)
{
    // TODO: Clean up buffers, this is currently a memory leak
    memset(token, 0, sizeof(*token));

    return st_ok;
}

//
// Error token
//
st_status st_token_set_error(st_token_t *token, uint32_t codepoint,
        const char *message, uint32_t line, uint32_t column)
{
    assert(token->type == st_token_type_uninitialized);

    token->type = st_token_type_error;
    token->error.codepoint = codepoint;
    token->error.message = message;
    token->error.line = line;
    token->error.column = column;

    return st_ok;
}

st_status st_token_set_character(st_token_t *token, uint32_t codepoint)
{
    assert(token->type == st_token_type_uninitialized);

    token->type = st_token_type_character;
    token->character.codepoint = codepoint;

    return st_ok;
}

//
// Tag token
//

st_status st_token_set_tag(st_token_t *token, uint32_t codepoint)
{
    st_status rc;

    // Initialize the tag name buffer
    rc = st_buffer_init(&token->tag.name, 10 * sizeof(codepoint));
    if (rc != st_ok)
        return rc;

    // Initialize the aattribute buffer
    rc = st_buffer_init(&token->tag.attrs, 3 * sizeof(st_token_attribute_t));
    if (rc != st_ok)
        return rc;

    // Add the first character to the buffer
    return st_buffer_append(&token->tag.name, &codepoint, sizeof(codepoint));
}

st_status st_token_set_start_tag(st_token_t *token, uint32_t codepoint)
{
    assert(token->type == st_token_type_uninitialized);

    token->type = st_token_type_start_tag;

    return st_token_set_tag(token, codepoint);
}

st_status st_token_set_end_tag(st_token_t *token, uint32_t codepoint)
{
    assert(token->type == st_token_type_uninitialized);

    token->type = st_token_type_end_tag;

    return st_token_set_tag(token, codepoint);
}

st_status st_token_tag_append_name(st_token_t *token, uint32_t codepoint)
{
    token->tag.len += 1;
    return st_buffer_append(&token->tag.name, &codepoint, sizeof(codepoint));
}

//
// Tag token attributed
//

st_status st_token_attr_add(st_token_t *token)
{
    st_status rc;

    // Set up a new attribute
    st_token_attribute_t attr;

    // Initialize the tag name buffer
    rc = st_buffer_init(&attr.name, 10 * sizeof(uint32_t));
    if (rc != st_ok)
        return rc;

    attr.name_len = 0;

    // Initialize the aattribute buffer
    rc = st_buffer_init(&attr.value, 10 * sizeof(uint32_t));
    if (rc != st_ok)
        return rc;

    attr.value_len = 0;

    // Write the token to the buffer
    st_buffer_append(&token->tag.attrs, &attr, sizeof(attr));
    token->tag.num_attrs += 1;

    return st_ok;
}

st_status st_token_attr_append_name(st_token_t *token, uint32_t codepoint)
{
    int attr_offset = (token->tag.num_attrs - 1) * sizeof(st_token_attribute_t);
    st_token_attribute_t *attr =
        st_buffer_offset_pointer(token->tag.attrs, attr_offset);

    attr->name_len += 1;

    return st_buffer_append(&attr->name, &codepoint, sizeof(codepoint));
}

st_status st_token_attr_append_value(st_token_t *token, uint32_t codepoint)
{
    int attr_offset = (token->tag.num_attrs - 1) * sizeof(st_token_attribute_t);
    st_token_attribute_t *attr =
        st_buffer_offset_pointer(token->tag.attrs, attr_offset);

    attr->value_len += 1;

    return st_buffer_append(&attr->value, &codepoint, sizeof(codepoint));
}

//
// Get information about tokens
//

st_token_type_t st_token_type(st_token_t *token)
{
    return token->type;
}

uint32_t st_token_codepoint(st_token_t *token)
{
    assert(token->type == st_token_type_character);

    return token->character.codepoint;
}

uint32_t *st_token_tag_name(st_token_t *token)
{
    assert(token->type == st_token_type_start_tag ||
            token->type == st_token_type_end_tag);

    return st_buffer_offset_pointer(token->tag.name, 0);
}

size_t st_token_tag_name_length(st_token_t *token)
{
    assert(token->type == st_token_type_start_tag ||
            token->type == st_token_type_end_tag);

    return token->tag.len;
}
