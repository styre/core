#include "tokenizer.h"

#include <string.h>

#include "utf8.h"

//
// Start adapted from Chromium (Blink)
//

#define BEGIN_STATE(state_name) case st_tokenizer_ ## state_name: state_name:
#define END_STATE() break;

// We use this macro when the HTML5 spec says "reconsume the current input
// character in the <mumble> state."
#define RECONSUME_IN(state_name)                                               \
    tokenizer->state = st_tokenizer_ ## state_name;                            \
    goto state_name     ;                                                      \

// We use this macro when the HTML5 spec says "consume the next input
// character ... and switch to the <mumble> state."
#define SWITCH_TO(state_name)                                                  \
    tokenizer->state = st_tokenizer_ ## state_name;                            \
    break;                                                                     \

//
// End adapted from Chromium
//

#define EMIT_TOKEN()                                                           \
    tokenizer->callbacks.token(tokenizer, &token, tokenizer->ctx);

// Used when we reach an error. Takes an error message and calls the callback
// method.
#define EMIT_ERROR(message, code)                                              \
    tokenizer->callbacks.error(tokenizer, message, tokenizer->ctx);            \
    return code;                                                               \

#define EMIT_CHARACTER_TOKEN(code)                                             \
    token.type = st_token_type_character;                                      \
    token.character.codepoint = code;                                          \
    EMIT_TOKEN()

#define APPEND_TO_TAG_TOKEN(codepoint)                                         \
    token.tag.name[token.tag.len] = codepoint;                                 \
    token.tag.len += 1;                                                        \

#define OPEN_START_TAG_TOKEN(codepoint)                                        \
    token.type = st_token_type_start_tag;                                      \
    token.tag.len = 0;                                                         \
    APPEND_TO_TAG_TOKEN(codepoint)

#define OPEN_END_TAG_TOKEN(codepoint)                                          \
    token.type = st_token_type_end_tag;                                        \
    token.tag.len = 0;                                                         \
    APPEND_TO_TAG_TOKEN(codepoint)

#define IS_WHITESPACE(c) (c== ' ' || c == 0x0A || c == 0x09 || c == 0x0C)

#define IS_ASCII_LOWER(codepoint) (codepoint >= 'a' && codepoint <= 'z')
#define IS_ASCII_UPPER(codepoint) (codepoint >= 'A' && codepoint <= 'Z')
#define TO_ASCII_LOWER(codepoint) (codepoint - 0x20)

// Character shortcuts
#define AMPERSAND               0x0026
#define LESS_THAN               0x003C
#define GREATER_THAN            0x003D
#define NULL_CHARACTER          0x0000
#define EXCLAMATION_POINT       0x0021
#define FORWARD_SLASH           0x002F
#define REPLACEMENT_CHARACTER   0xFFFD


//
// The tokenizer state object
//
struct st_tokenizer {
    st_tokenizer_state_t state;             // Current state of the tokenizer

    st_tokenizer_callbacks_t callbacks;     // Callbacks method to call
    void *ctx;                              // User context

    st_tokenizer_input_type_t input_type;   // The input type
    void *input_func;                       // Pointer to the input function
    void *input_ctx;                        // Input context

    uint8_t *buffer;                        // Input buffer
    size_t buffer_size;                     // Size of the input buffer
    size_t buffer_offset;                   // Current offset into the buffer
};


//
// Helper methods to read input from the input stream
//
int st_tokenizer_next_character(st_tokenizer_t *tokenizer,
        uint32_t *codepoint, size_t *bytes);

int st_tokenizer_peek_character(st_tokenizer_t *tokenizer,
        uint32_t *codepoint, size_t *bytes);

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

    return 0;
}

int st_tokenizer_utf8_string(st_tokenizer_t *tokenizer,
        const uint8_t *buf, size_t len)
{
    // Status code from function calls
    st_status rc;

    // State variables
    uint32_t codepoint = 0;
    size_t bytes = 0;

    // Call start of document callback
    tokenizer->callbacks.document_start(tokenizer, tokenizer->ctx);

    // Ignore unused labels created by the macros
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-label"

    // Loop through the entire string
    while (len > 0) {
        if ((rc = next_codepoint(buf, len, &codepoint, &bytes)) != st_ok) {
            switch (rc) {
                case st_utf8_invalid:
                    EMIT_ERROR("Invalid UTF-8 character.", -1);
                case st_eof:
                    EMIT_ERROR("Reached end of input", -1);
                default:
                    EMIT_ERROR("Unknown error occured", -1);
            }
        }

        st_token_t token;

        switch(tokenizer->state) {

            BEGIN_STATE(data_state) {
                switch(codepoint) {
                    case AMPERSAND:
                        SWITCH_TO(character_reference_in_data_state);
                    case LESS_THAN:
                        SWITCH_TO(tag_open_state);
                    case NULL_CHARACTER:
                        EMIT_ERROR("Reached \\0-character.", -1);
                    default:
                        EMIT_CHARACTER_TOKEN(codepoint);
                }
            }
            END_STATE()


            BEGIN_STATE(character_reference_in_data_state) {
                EMIT_ERROR("Unsupported: Character reference in data state",
                        -1);
            }
            END_STATE();


            BEGIN_STATE(rcdata_state) {
                switch(codepoint) {
                    case AMPERSAND:
                        SWITCH_TO(character_reference_in_rcdata_state);
                    case LESS_THAN:
                        SWITCH_TO(rcdata_less_than_sign_state);
                    case NULL_CHARACTER:
                        EMIT_CHARACTER_TOKEN(REPLACEMENT_CHARACTER);
                        EMIT_ERROR("Reached \\0-character.", -1);
                    default:
                        EMIT_CHARACTER_TOKEN(codepoint);
                }
            }
            END_STATE();

            BEGIN_STATE(character_reference_in_rcdata_state) {
                EMIT_ERROR("Unsupported: Character reference in RCDATA state",
                        -1);
            }
            END_STATE();

            BEGIN_STATE(rawtext_state) {
                EMIT_ERROR("Unsupported: Rawtext state", -1);
            }
            END_STATE();

            BEGIN_STATE(script_data_state) {
                EMIT_ERROR("Unsupported: Script data state", -1);
            }
            END_STATE();

            BEGIN_STATE(plaintext_state) {
                EMIT_ERROR("Unsupported: Plain text state", -1);
            }
            END_STATE();

            BEGIN_STATE(tag_open_state) {
                if (codepoint == '!') {
                    SWITCH_TO(markup_declaration_open_state);
                } else if (codepoint == '/') {
                    SWITCH_TO(end_tag_open_state);
                } else if (IS_ASCII_UPPER(codepoint)) {
                    OPEN_START_TAG_TOKEN(TO_ASCII_LOWER(codepoint));
                    SWITCH_TO(tag_name_state);
                } else if (IS_ASCII_LOWER(codepoint)) {
                    OPEN_START_TAG_TOKEN(codepoint);
                    SWITCH_TO(tag_name_state);
                } else if (codepoint == '?') {
                    EMIT_ERROR("Parse error: invalid token ?", -1);
                } else {
                    EMIT_ERROR("Parse error: reached invalid token", -1);
                }
            }
            END_STATE();

            BEGIN_STATE(end_tag_open_state) {
                if (IS_ASCII_UPPER(codepoint)) {
                    OPEN_END_TAG_TOKEN(TO_ASCII_LOWER(codepoint));
                    SWITCH_TO(tag_name_state);
                } else if (IS_ASCII_LOWER(codepoint)) {
                    OPEN_END_TAG_TOKEN(codepoint);
                    SWITCH_TO(tag_name_state);
                } else if (codepoint == '>') {
                    EMIT_ERROR("Parse error: Empty end tag.", -1);
                } else {
                    EMIT_ERROR("Parse error: Invalid end tag.", -1);
                }
            }
            END_STATE();

            BEGIN_STATE(tag_name_state) {
                if (IS_WHITESPACE(codepoint)) {
                    SWITCH_TO(before_attribute_name_state);
                } else if (codepoint == '/') {
                    SWITCH_TO(self_closing_start_tag_state);
                } else if (codepoint == '>') {
                    EMIT_TOKEN();
                    SWITCH_TO(data_state);
                } else {
                }
            }
            END_STATE();

            BEGIN_STATE(rcdata_less_than_sign_state) {
                switch(codepoint) {
                    case FORWARD_SLASH: // /
                        SWITCH_TO(rcdata_end_tag_open_state);
                    default:
                        EMIT_CHARACTER_TOKEN(LESS_THAN);
                        RECONSUME_IN(rcdata_state);
                }
            }
            END_STATE();

            BEGIN_STATE(rcdata_end_tag_open_state) {
                EMIT_ERROR("Unsupported: RCDATA end tag open state", -1);
            }
            END_STATE();


            default:
                EMIT_ERROR("Unsupported state.", -1);

        }

        // Advance to the next character
        buf = buf + bytes;
        len = len - bytes;
    }

    // Reenable unused label warnings
#pragma clang diagnostic pop

    // Call end of document callback
    tokenizer->callbacks.document_end(tokenizer, tokenizer->ctx);

    return 0;
}

int st_tokenizer_next_character_string(st_tokenizer_t *tokenizer,
        uint32_t *cp, size_t *bytes)
{
    return 0;
}

int st_tokenizer_next_character_file(st_tokenizer_t *tokenizer,
        uint32_t *cp, size_t *bytes)
{
    return 0;
}

int st_tokenizer_next_character_stream(st_tokenizer_t *tokenizer,
        uint32_t *cp, size_t *bytes)
{
    return 0;
}

int st_tokenizer_next_character(st_tokenizer_t *tokenizer,
        uint32_t *cp, size_t *bytes)
{
    switch(tokenizer->input_type) {
        case st_tokenizer_string_input:
            return st_tokenizer_next_character_string(tokenizer, cp, bytes);
        case st_tokenizer_file_input:
            return st_tokenizer_next_character_file(tokenizer, cp, bytes);
        case st_tokenizer_stream_input:
            return st_tokenizer_next_character_stream(tokenizer, cp, bytes);
        default:
            return -1;
    };

    return 0;
}
