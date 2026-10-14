#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include "string_builder.h"

static char *code_buffer = NULL;
static size_t code_len = 0;
static size_t code_cap = 0;

static void sb_ensure_capacity(size_t needed) {
    if (code_len + needed + 1 <= code_cap) return;
    size_t new_cap = code_cap ? code_cap * 2 : 64;
    while (new_cap < code_len + needed + 1) new_cap *= 2;
    char *new_buf = realloc(code_buffer, new_cap);
    if (!new_buf) { perror("string_builder realloc failed"); return; }
    code_buffer = new_buf;
    code_cap = new_cap;
}

void buildString(const char *s) {
    if (!s) return;
    size_t add_len = strlen(s);
    sb_ensure_capacity(add_len);
    memcpy(code_buffer + code_len, s, add_len);
    code_len += add_len;
    code_buffer[code_len] = '\0';
}

void buildStringf(const char *fmt, ...) {
    if (!fmt) return;

    va_list ap;
    va_start(ap, fmt);
    va_list ap_copy;
    va_copy(ap_copy, ap);
    int len = vsnprintf(NULL, 0, fmt, ap_copy);
    va_end(ap_copy);
    if (len < 0) { va_end(ap); return; }

    sb_ensure_capacity((size_t)len);

    vsnprintf(code_buffer + code_len, code_cap - code_len, fmt, ap);
    code_len += (size_t)len;
    code_buffer[code_len] = '\0';
    va_end(ap);
}

const char *getBuiltString(void) {
    return code_buffer ? code_buffer : "";
}

void resetBuildString(void) {
    free(code_buffer);
    code_buffer = NULL;
    code_len = 0;
    code_cap = 0;
}
