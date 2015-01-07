#ifndef tokenizer_h
#define tokenizer_h

// TODO
//#ifdef __cplusplus
//extern "C" {
//#endif

#include <stdlib.h>
#include <stdint.h>

// typedefs
typedef enum st_tokenizer_state st_tokenizer_state_t;
typedef enum st_token_type st_token_type_t;

typedef struct st_tokenizer_callbacks st_tokenizer_callbacks_t;
typedef struct st_tokenizer st_tokenizer_t;
typedef struct st_token st_token_t;

//
// The various states of the tokenizer. All references are to
// http://www.w3.org/TR/html5/syntax.html#tokenization
//
enum st_tokenizer_state {
    st_tokenizer_data_state,                                // Ref. 8.2.4.1
    st_tokenizer_character_reference_in_data_state,         // Ref. 8.2.4.2
    st_tokenizer_rcdata_state,                              // Ref. 8.2.4.3
    st_tokenizer_character_reference_in_rcdata_state,       // Ref. 8.2.4.4
    st_tokenizer_rawtext_state,                             // Ref. 8.2.4.5
    st_tokenizer_script_data_state,                         // Ref. 8.2.4.6
    st_tokenizer_plaintext_state,                           // Ref. 8.2.4.7
    st_tokenizer_tag_open_state,                            // Ref. 8.2.4.8
    st_tokenizer_end_tag_open_state,                        // Ref. 8.2.4.9
    st_tokenizer_tag_name_state,                            // Ref. 8.2.4.10
    st_tokenizer_rcdata_less_than_sign_state,               // Ref. 8.2.4.11
    st_tokenizer_rcdata_end_tag_open_state,                 // Ref. 8.2.4.12
    st_tokenizer_rcdata_end_tag_name_state,                 // Ref. 8.2.4.13
    st_tokenizer_rawtext_less_than_sign_state,              // Ref. 8.2.4.14
};

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

//
// Tokens
//

// Token types
enum st_token_type {
    st_token_type_character,
    st_token_type_eof,
};

// Charachter token type
typedef struct {
    uint32_t codepoint;
} st_token_character_t;

// Structure for tokens
struct st_token {
    st_token_type_t type;
    union {
        st_token_character_t character;
    };
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
