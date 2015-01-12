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
#define EMIT_ERROR(message, code)                                              \
    t->callbacks.error(t, message, t->ctx);                                    \
    return code;

#define OPEN_CHARACTER_TOKEN(code)                                             \
    token->type = st_token_type_character;                                     \
    token->character.codepoint = code;

#define APPEND_TO_TAG_TOKEN(codepoint)                                         \
    token->tag.name[token->tag.len] = codepoint;                               \
    token->tag.len += 1;

#define OPEN_START_TAG_TOKEN(codepoint)                                        \
    token->type = st_token_type_start_tag;                                     \
    token->tag.len = 0;                                                        \
    token->tag.attr_num = 0;                                                   \
    APPEND_TO_TAG_TOKEN(codepoint)

#define OPEN_END_TAG_TOKEN(codepoint)                                          \
    token->type = st_token_type_end_tag;                                       \
    token->tag.len = 0;                                                        \
    token->tag.attr_num = 0;                                                   \
    APPEND_TO_TAG_TOKEN(codepoint)

#define APPEND_TO_ATTR_NAME(codepoint)                                         \
    {                                                                       \
        st_token_attribute_t *attr = &token->tag.attrs[token->tag.attr_num];   \
        attr->name[attr->name_len] = codepoint;                                \
        attr->name_len += 1;                                                   \
    }


#define OPEN_ATTR(codepoint)                                                   \
    token->tag.attrs[token->tag.attr_num].name_len = 0;                        \
    token->tag.attrs[token->tag.attr_num].value_len = 0;                       \
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
    st_token_t token;
    token.type = st_token_type_uninitialized;

    // Call start of document callback
    t->callbacks.document_start(t, t->ctx);

    // Iterate through the input and emit tokens
    while ((rc = st_tokenizer_next_token(t, &token)) == st_ok) {
        // Emit token
        t->callbacks.token(t, &token, t->ctx);

        token.type = st_token_type_uninitialized;
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
                    EMIT_ERROR("Reached \\0-character.", st_err);
                default:
                    OPEN_CHARACTER_TOKEN(t->codepoint);
                    EMIT_TOKEN();
            }
        }
        END_STATE()


        BEGIN_STATE(character_reference_in_data_state) {
            EMIT_ERROR("Unsupported: Character reference in data state",
                    -1);
        }
        END_STATE();


        BEGIN_STATE(rcdata_state) {
            switch(t->codepoint) {
                case '&':
                    SWITCH_TO(character_reference_in_rcdata_state);
                case '<':
                    SWITCH_TO(rcdata_less_than_sign_state);
                case 0:
                    EMIT_ERROR("Reached \\0-character.", st_err);
                default:
                    OPEN_CHARACTER_TOKEN(t->codepoint);
                    EMIT_TOKEN();
            }
        }
        END_STATE();

        BEGIN_STATE(character_reference_in_rcdata_state) {
            EMIT_ERROR("Unsupported: Character reference in RCDATA state",
                    -1);
        }
        END_STATE();

        BEGIN_STATE(rawtext_state) {
            EMIT_ERROR("Unsupported: Rawtext state", st_err);
        }
        END_STATE();

        BEGIN_STATE(script_data_state) {
            EMIT_ERROR("Unsupported: Script data state", st_err);
        }
        END_STATE();

        BEGIN_STATE(plaintext_state) {
            EMIT_ERROR("Unsupported: Plain text state", st_err);
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
                EMIT_ERROR("Parse error: invalid token ?", st_err);
            } else {
                EMIT_ERROR("Parse error: reached invalid token", st_err);
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
                EMIT_ERROR("Parse error: Empty end tag.", st_err);
            } else {
                EMIT_ERROR("Parse error: Invalid end tag.", st_err);
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
                EMIT_ERROR("Reached \\0 charachter", st_err);
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
            EMIT_ERROR("Unsupported: RCDATA end tag open state", st_err);
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
                EMIT_ERROR("Reached \\0-charachter.", st_err);
            } else if (t->codepoint == '"' || t->codepoint == '<' ||
                    t->codepoint == '\'' || t->codepoint == '=') {
                EMIT_ERROR("Invalid start of token name", st_err);
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
                EMIT_ERROR("Reached \\0-character", st_err);
            } else if (t->codepoint == '"' || t->codepoint == '\'' ||
                    t->codepoint == '>') {
                EMIT_ERROR("Invalid part of token name", st_err);
            } else {
                APPEND_TO_ATTR_NAME(t->codepoint);
                SWITCH_TO(attribute_name_state);
            }
        }
        END_STATE();

        BEGIN_STATE(after_attribute_name_state) {
            EMIT_ERROR("Unsupported: After attribute name state", st_err);
        }
        END_STATE();

        BEGIN_STATE(before_attribute_value_state) {
            EMIT_ERROR("Unsupported: Before attribute value state", st_err);
        }
        END_STATE();

        // TODO

        BEGIN_STATE(self_closing_start_tag_state) {
            EMIT_ERROR("Unsupported: Self closing start tag state", st_err);
        }
        END_STATE();

        // TODO

        BEGIN_STATE(markup_declaration_open_state) {
            EMIT_ERROR("Unsupported: Markup declaration open state", st_err);
        }
        END_STATE();


        default:
            EMIT_ERROR("Unsupported state.", -1);

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
