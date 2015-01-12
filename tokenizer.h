#ifndef tokenizer_h
#define tokenizer_h

// TODO
//#ifdef __cplusplus
//extern "C" {
//#endif

#include <stdlib.h>
#include <stdint.h>

#include "styre.h"
#include "token.h"

// typedefs
typedef enum st_tokenizer_state st_tokenizer_state_t;

typedef struct st_tokenizer_callbacks st_tokenizer_callbacks_t;
typedef struct st_tokenizer st_tokenizer_t;

//
// The various states of the tokenizer. All references are to
// http://www.w3.org/TR/html5/syntax.html#tokenization
//
enum st_tokenizer_state {
    st_tokenizer_data_state = 0,                               // Ref. 8.2.4.1
    st_tokenizer_character_reference_in_data_state,            // Ref. 8.2.4.2
    st_tokenizer_rcdata_state,                                 // Ref. 8.2.4.3
    st_tokenizer_character_reference_in_rcdata_state,          // Ref. 8.2.4.4
    st_tokenizer_rawtext_state,                                // Ref. 8.2.4.5
    st_tokenizer_script_data_state,                            // Ref. 8.2.4.6
    st_tokenizer_plaintext_state,                              // Ref. 8.2.4.7
    st_tokenizer_tag_open_state,                               // Ref. 8.2.4.8
    st_tokenizer_end_tag_open_state,                           // Ref. 8.2.4.9
    st_tokenizer_tag_name_state,                               // Ref. 8.2.4.10
    st_tokenizer_rcdata_less_than_sign_state,                  // Ref. 8.2.4.11
    st_tokenizer_rcdata_end_tag_open_state,                    // Ref. 8.2.4.12
    st_tokenizer_rcdata_end_tag_name_state,                    // Ref. 8.2.4.13
    st_tokenizer_rawtext_less_than_sign_state,                 // Ref. 8.2.4.14
    st_tokenizer_rawtext_end_tag_open_state,                   // Ref. 8.2.4.15
    st_tokenizer_rawtext_end_tag_name_state,                   // Ref. 8.2.4.16
    st_tokenizer_script_data_less_than_sign_state,             // Ref. 8.2.4.17
    st_tokenizer_script_data_end_tag_open_state,               // Ref. 8.2.4.18
    st_tokenizer_script_data_end_tag_name_state,               // Ref. 8.2.4.19
    st_tokenizer_script_data_escape_start_state,               // Ref. 8.2.4.20
    st_tokenizer_script_data_escape_start_dash_state,          // Ref. 8.2.4.21
    st_tokenizer_script_data_escaped_state,                    // Ref. 8.2.4.22
    st_tokenizer_script_data_escaped_dash_state,               // Ref. 8.2.4.23
    st_tokenizer_script_data_escaped_dash_dash_state,          // Ref. 8.2.4.24
    st_tokenizer_script_data_escaped_less_than_sign_state,     // Ref. 8.2.4.25

    // TODO

    st_tokenizer_before_attribute_name_state,                  // Ref. 8.2.4.34
    st_tokenizer_attribute_name_state,                         // Ref. 8.2.4.35
    st_tokenizer_after_attribute_name_state,                   // Ref. 8.2.4.36
    st_tokenizer_before_attribute_value_state,                 // Ref. 8.2.4.37
    st_tokenizer_attribute_value_double_quoted_state,          // Ref. 8.2.4.38
    st_tokenizer_attribute_value_single_quoted_state,          // Ref. 8.2.4.39
    st_tokenizer_attribute_value_unquoted_state,               // Ref. 8.2.4.40
    st_tokenizer_character_reference_in_attribute_value_state, // Ref. 8.2.4.41
    st_tokenizer_after_attribute_value_state,                  // Ref. 8.2.4.42
    st_tokenizer_self_closing_start_tag_state,                 // Ref. 8.2.4.43
    st_tokenizer_bogus_comment_state,                          // Ref. 8.2.4.44
    st_tokenizer_markup_declaration_open_state,                // Ref. 8.2.4.45

    // TODO
};

//
// Input types
//
typedef enum {
    st_tokenizer_uninitialized_input = 0,
    st_tokenizer_string_input,
    st_tokenizer_file_input,
    st_tokenizer_stream_input,
} st_tokenizer_input_type_t;

//
// Input encodings
//
typedef enum {
    st_tokenizer_uninitialized_encoding = 0,
    st_tokenizer_encoding_utf8,
} st_tokenizer_input_encoding_t;

//
// Callbacks
//

// Start of document callback
typedef void (*st_tokenizer_cb_document_start)(st_tokenizer_t *tokenizer,
        void *ctx);

// End of document callback
typedef void (*st_tokenizer_cb_document_end)(st_tokenizer_t *tokenizer,
        void *ctx);

typedef void (*st_tokenizer_cd_token)(st_tokenizer_t *tokenizer,
        st_token_t *token, void *ctx);

// Error callback
typedef void (*st_tokenizer_cb_error)(st_tokenizer_t *tokenizer,
        const char *message, void *ctx);

// Structure for registring callbacks
struct st_tokenizer_callbacks {
    st_tokenizer_cb_document_start document_start;
    st_tokenizer_cb_document_end document_end;

    st_tokenizer_cd_token token;

    st_tokenizer_cb_error error;
};

// Initialize a new tokenizer
int st_tokenizer_init(st_tokenizer_t **tokenizer,
        st_tokenizer_callbacks_t *callbacks, void *ctx);

int st_tokenizer_utf8_string(st_tokenizer_t *tokenizer,
        const uint8_t *input, size_t len);

// TODO
//#ifdef __cplusplus
//}
//#endif

#endif
