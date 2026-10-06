#include <stdio.h>
#include <stdarg.h>
#include "error_handler.h"

int semantic_errors = 0;

void semanticError(int line, const char *format, ...) {
    va_list args;
    va_start(args, format);
    
    fprintf(stderr, "\033[1;31mError semantico en la linea %d:\033[0m ", line);
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    
    va_end(args);
    semantic_errors++;
}
