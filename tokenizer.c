#include "tokenizer.h"

#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "utf8.h"

//
// Start adapted from Chromium (Blink)
//

#define BEGIN_STATE(state_name) case st_tokenizer_ ## state_name: state_name:
#define END_STATE() assert(0); break;

// We use this macro when the HTML5 spec says "reconsume the current input
// character in the <mumble> state."
#define RECONSUME_IN(state_name)                                               \
    t->state = st_tokenizer_ ## state_name;                                    \
    goto state_name;

// We use this macro when the HTML5 spec says "consume the next input
// character ... and switch to the <mumble> state."
#define SWITCH_TO(state_name)                                                  \
    t->state = st_tokenizer_ ## state_name;                                    \
    if ((rc = st_tokenizer_next_codepoint(t)) != st_ok) {                      \
        return rc;                                                             \
    }                                                                          \
    goto state_name;


//
// End adapted from Chromium
//

#define EMIT_TOKEN()                                                           \
    return st_ok;

#define EMIT_AND_RESUME_IN(state_name)                                         \
    t->state = st_tokenizer_ ## state_name;                                    \
    EMIT_TOKEN();

#define EMIT_AND_RECONSUME_IN(state_name)                                      \
    t->state = st_tokenizer_ ## state_name;                                    \
    t->reconsume = 1;                                                          \
    EMIT_TOKEN();

// Used when we reach an error. Takes an error message and calls the callback
// method.
#define EMIT_ERROR(message)                                                    \
    if ((rc = st_token_set_error(token, t->codepoint, message, 0, 0))          \
            != st_ok) {                                                        \
        return rc;                                                             \
    }                                                                          \
    return st_ok;

#define OPEN_CHARACTER_TOKEN(code)                                             \
    if ((rc = st_token_set_character(token, code)) == st_ok) {                 \
        return rc;                                                             \
    }

#define APPEND_TO_TAG_TOKEN(codepoint)                                         \
    if ((rc = st_token_tag_append_name(token, codepoint)) != st_ok) {          \
        return rc;                                                             \
    }

#define OPEN_START_TAG_TOKEN(codepoint)                                        \
    if ((rc = st_token_set_start_tag(token, codepoint)) != st_ok) {            \
        return rc;                                                             \
    }

#define OPEN_END_TAG_TOKEN(codepoint)                                          \
    if ((rc = st_token_set_end_tag(token, codepoint)) != st_ok) {              \
        return rc;                                                             \
    }

#define APPEND_TO_ATTR_NAME(codepoint)                                         \
    if ((rc = st_token_attr_append_name(token, codepoint)) != st_ok) {         \
        return rc;                                                             \
    }

#define APPEND_TO_ATTR_VALUE(codepoint)                                        \
    if ((rc = st_token_attr_append_value(token, codepoint)) != st_ok) {        \
        return rc;                                                             \
    }


#define OPEN_ATTR(codepoint)                                                   \
    if ((rc = st_token_attr_add(token)) != st_ok) {                            \
        return rc;                                                             \
    }                                                                          \
    APPEND_TO_ATTR_NAME(codepoint);


#define IS_WHITESPACE(c) (c== ' ' || c == 0x0A || c == 0x09 || c == 0x0C)

#define IS_ASCII_LOWER(codepoint) (codepoint >= 'a' && codepoint <= 'z')
#define IS_ASCII_UPPER(codepoint) (codepoint >= 'A' && codepoint <= 'Z')
#define TO_ASCII_LOWER(codepoint) (codepoint - 0x20)

#define REPLACEMENT_CHARACTER 0xFFFD

//
// The tokenizer state object
//
struct st_tokenizer {
    st_tokenizer_state_t state;             // Current state of the tokenizer

    st_tokenizer_callbacks_t callbacks;     // Callbacks method to call
    void *ctx;                              // User context

    st_tokenizer_next_codepoint_cb next_codepoint;  // Function to call to get
                                                    // the next codepoint.
    st_tokenizer_encode_string_cb encode_func;      // Function to encode a
                                                    // unicode string.

    st_tokenizer_input_cb input_func;       // Pointer to the input function
    void *input_ctx;                        // Input context
    uint32_t codepoint;                     // The current codepoint
    int reconsume;                          // If we should reconsume the last
                                            // codepoint

    const uint8_t *buf;                     // Input buffer
    size_t buf_s;                           // Size of the input buffer
    size_t buf_o;                           // Current offset into the buffer
};

typedef struct {
    FILE *file;                             // File hander
} st_tokenizer_file_input_t;


//
// Helper methods to read input from the input stream
//
static st_status st_tokenizer_next_token(st_tokenizer_t *t, st_token_t *token);
static st_status st_tokenizer_next_codepoint(st_tokenizer_t *t);

//
// Initialize a new tokenizer
//
int st_tokenizer_init(st_tokenizer_t **tokenizer,
        st_tokenizer_callbacks_t *callbacks, void *ctx)
{
    // Allocate memory
    *tokenizer = malloc(sizeof(**tokenizer));
    if (*tokenizer == NULL)
        return -1;

    // All memory to zero
    memset(*tokenizer, 0, sizeof(**tokenizer));

    // Set initial values
    (*tokenizer)->callbacks = *callbacks;
    (*tokenizer)->ctx = ctx;

    (*tokenizer)->next_codepoint = &utf8_next_codepoint;
    (*tokenizer)->encode_func = &utf8_encode_unicode;

    return 0;
}

st_status st_tokenizer_set_encoding_handler(st_tokenizer_t *t,
        st_tokenizer_next_codepoint_cb next_codepoint,
        st_tokenizer_encode_string_cb encode_func)
{
    t->next_codepoint = next_codepoint;
    t->encode_func = encode_func;

    return st_ok;
}

st_status st_tokenizer_set_input_handler(st_tokenizer_t *t,
        st_tokenizer_input_cb input_func, void *ctx)
{
    t->input_func = input_func;
    t->input_ctx = ctx;

    return st_ok;
}

// Input callback function for the default string handler
static st_status st_tokenizer_string_handler(const uint8_t **buffer,
        size_t *size, size_t *offset, void *ctx)
{
    return st_eof;
}

st_status st_tokenizer_set_string(st_tokenizer_t *t,
        const uint8_t *buf, size_t len)
{
    t->buf = buf;
    t->buf_s = len;
    t->buf_o = 0;

    t->input_func = &st_tokenizer_string_handler;
    t->input_ctx = NULL;

    return st_ok;
}

st_status st_tokenizer_run(st_tokenizer_t *t)
{
    if (t->buf == NULL)
        return st_invalid_config;

    if (t->buf_s == 0)
        return st_eof;

    if (t->input_func == NULL)
        return st_invalid_config;

    st_status rc;

    // Initialize the token
    st_token_t *token;
    if ((rc = st_token_init(&token)) != st_ok) {
        return rc;
    }

    assert(st_token_type(token) == st_token_type_uninitialized);

    // Call start of document callback
    t->callbacks.document_start(t, t->ctx);

    // Iterate through the input and emit tokens
    while ((rc = st_tokenizer_next_token(t, token)) == st_ok) {
        // Emit token
        t->callbacks.token(t, token, t->ctx);

        // Check if we got an error token
        if (st_token_type(token) == st_token_type_error) {
            break;
        }

        st_token_reset(token);

        assert(st_token_type(token) == st_token_type_uninitialized);
    }

    // If we did not reach the end of the file
    if (rc != st_eof) {
        return rc;
    }

    // Call end of document callback
    t->callbacks.document_end(t, t->ctx);

    return st_ok;
}

static st_status st_tokenizer_next_token(st_tokenizer_t *t, st_token_t *token) {
    // Status code from function calls
    st_status rc;

    // Ignore unused labels created by the macros
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-label"

    if (!t->reconsume && (rc = st_tokenizer_next_codepoint(t)) != st_ok) {
        return rc;
    }

    switch(t->state) {

        BEGIN_STATE(data_state) {
            switch(t->codepoint) {
                case '&':
                    SWITCH_TO(character_reference_in_data_state);
                case '<':
                    SWITCH_TO(tag_open_state);
                case 0:
                    EMIT_ERROR("Reached \\0-character.");
                default:
                    OPEN_CHARACTER_TOKEN(t->codepoint);
                    EMIT_TOKEN();
            }
        }
        END_STATE()


        BEGIN_STATE(character_reference_in_data_state) {
            EMIT_ERROR("Unsupported: Character reference in data state");
        }
        END_STATE();


        BEGIN_STATE(rcdata_state) {
            switch(t->codepoint) {
                case '&':
                    SWITCH_TO(character_reference_in_rcdata_state);
                case '<':
                    SWITCH_TO(rcdata_less_than_sign_state);
                case 0:
                    EMIT_ERROR("Reached \\0-character.");
                default:
                    OPEN_CHARACTER_TOKEN(t->codepoint);
                    EMIT_TOKEN();
            }
        }
        END_STATE();

        BEGIN_STATE(character_reference_in_rcdata_state) {
            EMIT_ERROR("Unsupported: Character reference in RCDATA state");
        }
        END_STATE();

        BEGIN_STATE(rawtext_state) {
            EMIT_ERROR("Unsupported: Rawtext state");
        }
        END_STATE();

        BEGIN_STATE(script_data_state) {
            EMIT_ERROR("Unsupported: Script data state");
        }
        END_STATE();

        BEGIN_STATE(plaintext_state) {
            EMIT_ERROR("Unsupported: Plain text state");
        }
        END_STATE();

        BEGIN_STATE(tag_open_state) {
            if (t->codepoint == '!') {
                SWITCH_TO(markup_declaration_open_state);
            } else if (t->codepoint == '/') {
                SWITCH_TO(end_tag_open_state);
            } else if (IS_ASCII_UPPER(t->codepoint)) {
                OPEN_START_TAG_TOKEN(TO_ASCII_LOWER(t->codepoint));
                SWITCH_TO(tag_name_state);
            } else if (IS_ASCII_LOWER(t->codepoint)) {
                OPEN_START_TAG_TOKEN(t->codepoint);
                SWITCH_TO(tag_name_state);
            } else if (t->codepoint == '?') {
                EMIT_ERROR("Parse error: invalid token ?");
            } else {
                EMIT_ERROR("Parse error: reached invalid token");
            }
        }
        END_STATE();

        BEGIN_STATE(end_tag_open_state) {
            if (IS_ASCII_UPPER(t->codepoint)) {
                OPEN_END_TAG_TOKEN(TO_ASCII_LOWER(t->codepoint));
                SWITCH_TO(tag_name_state);
            } else if (IS_ASCII_LOWER(t->codepoint)) {
                OPEN_END_TAG_TOKEN(t->codepoint);
                SWITCH_TO(tag_name_state);
            } else if (t->codepoint == '>') {
                EMIT_ERROR("Parse error: Empty end tag.");
            } else {
                EMIT_ERROR("Parse error: Invalid end tag.");
            }
        }
        END_STATE();

        BEGIN_STATE(tag_name_state) {
            if (IS_WHITESPACE(t->codepoint)) {
                SWITCH_TO(before_attribute_name_state);
            } else if (t->codepoint == '/') {
                SWITCH_TO(self_closing_start_tag_state);
            } else if (t->codepoint == '>') {
                EMIT_AND_RESUME_IN(data_state);
            } else if (IS_ASCII_UPPER(t->codepoint)) {
                APPEND_TO_TAG_TOKEN(TO_ASCII_LOWER(t->codepoint));
                SWITCH_TO(tag_name_state);
            } else if (t->codepoint == 0) {
                EMIT_ERROR("Reached \\0 charachter");
            } else {
                APPEND_TO_TAG_TOKEN(t->codepoint);
                SWITCH_TO(tag_name_state);
            }
        }
        END_STATE();

        BEGIN_STATE(rcdata_less_than_sign_state) {
            switch(t->codepoint) {
                case '/':
                    SWITCH_TO(rcdata_end_tag_open_state);
                default:
                    OPEN_CHARACTER_TOKEN('<');
                    EMIT_AND_RECONSUME_IN(rcdata_state);
            }
        }
        END_STATE();

        BEGIN_STATE(rcdata_end_tag_open_state) {
            EMIT_ERROR("Unsupported: RCDATA end tag open state");
        }
        END_STATE();

        // TODO

        BEGIN_STATE(before_attribute_name_state) {
            if (IS_WHITESPACE(t->codepoint)) {
                SWITCH_TO(before_attribute_name_state);
            } else if (t->codepoint == '/') {
                SWITCH_TO(self_closing_start_tag_state);
            } else if (t->codepoint == '>') {
                EMIT_AND_RESUME_IN(data_state);
            } else if (IS_ASCII_UPPER(t->codepoint)) {
                OPEN_ATTR(TO_ASCII_LOWER(t->codepoint));
                SWITCH_TO(attribute_name_state);
            } else if (t->codepoint == 0) {
                EMIT_ERROR("Reached \\0-charachter.");
            } else if (t->codepoint == '"' || t->codepoint == '<' ||
                    t->codepoint == '\'' || t->codepoint == '=') {
                EMIT_ERROR("Invalid start of token name");
            } else {
                OPEN_ATTR(t->codepoint);
                SWITCH_TO(attribute_name_state);
            }
        }
        END_STATE();

        BEGIN_STATE(attribute_name_state) {
            if (IS_WHITESPACE(t->codepoint)) {
                SWITCH_TO(after_attribute_name_state);
            } else if (t->codepoint == '/') {
                SWITCH_TO(self_closing_start_tag_state);
            } else if (t->codepoint == '=') {
                SWITCH_TO(before_attribute_value_state);
            } else if (t->codepoint == '>') {
                EMIT_AND_RESUME_IN(data_state);
            } else if (IS_ASCII_UPPER(t->codepoint)) {
                APPEND_TO_ATTR_NAME(TO_ASCII_LOWER(t->codepoint));
                SWITCH_TO(attribute_name_state);
            } else if (t->codepoint == 0) {
                EMIT_ERROR("Reached \\0-character");
            } else if (t->codepoint == '"' || t->codepoint == '\'' ||
                    t->codepoint == '>') {
                EMIT_ERROR("Invalid part of token name");
            } else {
                APPEND_TO_ATTR_NAME(t->codepoint);
                SWITCH_TO(attribute_name_state);
            }
        }
        END_STATE();

        BEGIN_STATE(after_attribute_name_state) {
            EMIT_ERROR("Unsupported: After attribute name state");
        }
        END_STATE();

        BEGIN_STATE(before_attribute_value_state) {
            if (IS_WHITESPACE(t->codepoint)) {
                SWITCH_TO(before_attribute_value_state);
            } else if (t->codepoint == '"') {
                SWITCH_TO(attribute_value_double_quoted_state);
            } else if (t->codepoint == '&') {
                RECONSUME_IN(attribute_value_unquoted_state);
            } else if (t->codepoint == '\'') {
                SWITCH_TO(attribute_value_single_quoted_state);
            } else if (t->codepoint == 0) {
                EMIT_ERROR("Reached \\0-character");
            } else if (t->codepoint == '>') {
                EMIT_ERROR("Premature closing bracked");
            } else if (t->codepoint == '<' || t->codepoint == '=' ||
                    t->codepoint == '`') {
                EMIT_ERROR("Invalid character");
            } else {
                APPEND_TO_ATTR_VALUE(t->codepoint);
                SWITCH_TO(attribute_value_unquoted_state);
            }
        }
        END_STATE();

        BEGIN_STATE(attribute_value_double_quoted_state) {
            if (t->codepoint == '"') {
                SWITCH_TO(after_attribute_value_quoted_state);
            } else if (t->codepoint == '&') {
                SWITCH_TO(character_reference_in_attribute_value_state);
            } else if (t->codepoint == '0') {
                EMIT_ERROR("Reached \\0-character");
            } else {
                APPEND_TO_ATTR_VALUE(t->codepoint);
                SWITCH_TO(attribute_value_double_quoted_state);
            }
        }
        END_STATE();

        BEGIN_STATE(attribute_value_single_quoted_state) {
            if (t->codepoint == '\'') {
                SWITCH_TO(after_attribute_value_quoted_state);
            } else if (t->codepoint == '&') {
                SWITCH_TO(character_reference_in_attribute_value_state);
            } else if (t->codepoint == 0) {
                EMIT_ERROR("Reached \\0-character");
            } else {
                APPEND_TO_ATTR_VALUE(t->codepoint);
                SWITCH_TO(attribute_value_single_quoted_state);
            }
        }
        END_STATE();

        BEGIN_STATE(attribute_value_unquoted_state) {
            EMIT_ERROR("Unsupported: Attribute value (unquoted) state");
        }
        END_STATE();

        BEGIN_STATE(character_reference_in_attribute_value_state) {
            EMIT_ERROR("Unsupported: Character reference in attribute value"
                    " state");
        }
        END_STATE();

        BEGIN_STATE(after_attribute_value_quoted_state) {
            if (IS_WHITESPACE(t->codepoint)) {
                SWITCH_TO(before_attribute_name_state);
            } else if (t->codepoint == '/') {
                SWITCH_TO(self_closing_start_tag_state);
            } else if (t->codepoint == '>') {
                EMIT_AND_RESUME_IN(data_state);
            } else {
                EMIT_ERROR("Invalid character");
            }
        }
        END_STATE();

        // TODO

        BEGIN_STATE(self_closing_start_tag_state) {
            EMIT_ERROR("Unsupported: Self closing start tag state");
        }
        END_STATE();

        // TODO

        BEGIN_STATE(markup_declaration_open_state) {
            EMIT_ERROR("Unsupported: Markup declaration open state");
        }
        END_STATE();


        default:
            EMIT_ERROR("Unsupported state.");

    }

    // Reenable unused label warnings
#pragma clang diagnostic pop

    return st_err;
}

static st_status st_tokenizer_next_codepoint(st_tokenizer_t *t)
{
    st_status rc;
    size_t bytes;

retry:
    // Try to decode the next codepoint
    if ((rc = t->next_codepoint(t->buf, t->buf_s, &t->codepoint, &bytes))
            == st_ok) {
        t->buf += bytes;
        t->buf_s -= bytes;
        t->buf_o += bytes;
    } else if (rc == st_eof) {
        // Decoding failed because we ran out of bytes to read, try to get more
        // bytes by calling the input function and then retrying to decode.
        if ((rc = t->input_func(&t->buf, &t->buf_s, &t->buf_o, t->input_ctx))
                == st_ok) {
            goto retry;
        }
    }

    return rc;
}

st_status st_tokenizer_encode_unicode(st_tokenizer_t *t,
        uint32_t *in, size_t size, uint8_t **out, size_t *bytes)
{
    return t->encode_func(in, size, out, bytes);
}
