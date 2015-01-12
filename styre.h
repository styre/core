#ifndef styre_h
#define styre_h

// Status enum
typedef enum {
    st_ok,                  // OK
    st_err,                 // Generic error
    st_invalid_config,      // The tokenizer is not correctly configured
    st_utf8_invalid,        // An invalid UTF-8 byte sequence was encountered
    st_invalid_unicode,     // An invalid unicode codepoint was reached
    st_eof,                 // The end of the input was reached
    st_out_of_memory,       // Unable to allocate memory
} st_status;

#endif
