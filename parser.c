#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "utf8.h"
#include "tokenizer.h"

const uint8_t xml[] = "<div><h1>å</h1>@</div>";

void tokenizer_document_start(st_tokenizer_t *tokenizer, void *ctx)
{
    fprintf(stderr, "Document start\n");
}

void tokenizer_document_end(st_tokenizer_t *tokenizer, void *ctx)
{
    fprintf(stderr, "Document end\n");
}

void tokenizer_error(st_tokenizer_t *tokenizer, const char *msg, void *ctx)
{
    fprintf(stderr, "Tokenizer error: %s\n", msg);
}

void print_tag_token(st_tokenizer_t *t, st_token_t *token, char *type) {
    st_status rc;

    uint8_t *name;
    size_t bytes;

    if ((rc = st_tokenizer_encode_unicode(t, token->tag.name, token->tag.len,
                    &name, &bytes)) == st_ok) {
        fprintf(stderr, "Got %s tag token: %s\n", type, name);
    } else {
        fprintf(stderr, "Unable to encode token name\n");
    }

    if (rc != st_out_of_memory) {
        free(name);
    }
}

void tokenizer_token(st_tokenizer_t *tokenizer, st_token_t *token, void *ctx)
{
    switch(token->type) {
        case st_token_type_character:
            fprintf(stderr, "Got character token: U+%04X\n",
                    token->character.codepoint);
            break;
        case st_token_type_eof:
            fprintf(stderr, "Reached end of string!\n");
            break;
        case st_token_type_start_tag:
            print_tag_token(tokenizer, token, "start");
            break;
        case st_token_type_end_tag:
            print_tag_token(tokenizer, token, "end");
            break;
        case st_token_type_uninitialized:
            fprintf(stderr, "Received uninitialized token\n");
            break;
        default:
            fprintf(stderr, "Unknown token received\n");
    }
}

int main() {
    st_status rc;

    st_tokenizer_t *tokenizer;
    st_tokenizer_callbacks_t callbacks = {
        .document_start = &tokenizer_document_start,
        .document_end = &tokenizer_document_end,

        .token = &tokenizer_token,

        .error = &tokenizer_error,
    };

    st_tokenizer_init(&tokenizer, &callbacks, NULL);

    st_tokenizer_set_string(tokenizer, xml, sizeof(xml));
    if ((rc = st_tokenizer_run(tokenizer)) != st_ok) {
        fprintf(stderr, "Invalid state: %d\n", rc);
    }

    return 0;
}
