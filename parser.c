#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "utf8.h"
#include "tokenizer.h"
#include "token.h"

const uint8_t xml[] =  "<div class=\"test\" id='hei'><header></header><h1>å</h1>@</div>";

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
    uint8_t *val;
    size_t bytes;

    if ((rc = st_token_tag_name(token, &name, &bytes)) != st_ok) {
        fprintf(stderr, "Unable to encode token name\n");
        if (rc != st_out_of_memory) {
            free(name);
        }
        return;
    }

    fprintf(stderr, "Got %s token %s:\n", type, name);

    free(name);

    size_t attributes = st_token_attr_num(token);
    if (attributes > 0) {
        for (int i = 0; i < attributes; i++) {
            if ((rc = st_token_attr_name(token, i, &name, &bytes)) != st_ok) {
                if (rc != st_out_of_memory) {
                    free(name);
                }
                fprintf(stderr, "Unable to get attribute name\n");
                return;
            }

            if ((rc = st_token_attr_value(token, i, &val, &bytes)) != st_ok) {
                if (rc != st_out_of_memory) {
                    free(val);
                }
                fprintf(stderr, "Unable to get attribute value\n");
                return;
            }

            fprintf(stderr, " %s=%s\n", name, val);

            // Clean up memory
            free(name);
            free(val);
        }
    } else {
        fprintf(stderr, " No attributes\n");
    }
}

void tokenizer_token(st_tokenizer_t *tokenizer, st_token_t *token, void *ctx)
{
    switch(st_token_type(token)) {
        case st_token_type_character:
            fprintf(stderr, "Got character token: U+%04X\n",
                    st_token_codepoint(token));
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
