#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

#include "string_builder.h"

static char *code_buffer = NULL;
static size_t code_len = 0;
static size_t code_cap = 0;
static bool builderFailed = false;

static bool sb_ensure_capacity(size_t needed);

void buildString(const char *string) {
    if (!string) return;
    size_t add_len = strlen(string);
    if (!sb_ensure_capacity(add_len)) return;
    memcpy(code_buffer + code_len, string, add_len);
    code_len += add_len;
    code_buffer[code_len] = '\0';
}

void buildStringf(const char *format, ...) {
    if (!format) return;

    va_list ap;
    va_start(ap, format);
    va_list ap_copy;
    va_copy(ap_copy, ap);
    int len = vsnprintf(NULL, 0, format, ap_copy);
    va_end(ap_copy);
    if (len < 0) {
        builderFailed = true;
        va_end(ap);
        return;
    }

    if (!sb_ensure_capacity((size_t)len)) {
        va_end(ap);
        return;
    }

    vsnprintf(code_buffer + code_len, code_cap - code_len, format, ap);
    code_len += (size_t)len;
    code_buffer[code_len] = '\0';
    va_end(ap);
}

const char *getBuiltString(void) {
    return code_buffer ? code_buffer : "";
}

bool hasStringBuilderError(void) {
    return builderFailed;
}

void resetBuildString(void) {
    free(code_buffer);
    code_buffer = NULL;
    code_len = 0;
    code_cap = 0;
    builderFailed = false;
}

static bool sb_ensure_capacity(size_t needed) {
    if (needed > SIZE_MAX - code_len - 1) {
        builderFailed = true;
        return false;
    }
    size_t required = code_len + needed + 1;
    if (required <= code_cap) return true;

    size_t new_cap = code_cap ? code_cap : 64;
    while (new_cap < required) {
        if (new_cap > SIZE_MAX / 2) { new_cap = required; break; }
        new_cap *= 2;
    }

    char *new_buf = realloc(code_buffer, new_cap);
    if (!new_buf) {
        perror("string_builder realloc failed");
        builderFailed = true;
        return false;
    }
    code_buffer = new_buf;
    code_cap = new_cap;
    return true;
}
