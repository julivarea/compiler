#include "error_handler.h"

#include <stdarg.h>
#include <stdio.h>

static void reportError(const char *stage, int *counter, int line, const char *format, va_list formatArguments);

int lexical_errors = 0;
int semantic_errors = 0;

void lexicalError(int line, const char *format, ...) {
    va_list formatArguments;
    va_start(formatArguments, format);
    reportError("lexico", &lexical_errors, line, format, formatArguments);
    va_end(formatArguments);
}

void syntaxError(int line, const char *format, ...) {
    va_list formatArguments;
    va_start(formatArguments, format);
    reportError("sintactico", NULL, line, format, formatArguments);
    va_end(formatArguments);
}

void semanticError(int line, const char *format, ...) {
    va_list formatArguments;
    va_start(formatArguments, format);
    reportError("semantico", &semantic_errors, line, format, formatArguments);
    va_end(formatArguments);
}

void compileAborted(const char *stage, int errorCount) {
    fprintf(stderr, "\n\033[1;31mCompilacion abortada: se encontraron %d errores %s.\033[0m\n",
            errorCount, stage);
}

void resetErrorHandler(void) {
    lexical_errors = 0;
    semantic_errors = 0;
}

static void reportError(const char *stage, int *counter, int line, const char *format, va_list formatArguments) {
    fprintf(stderr, "\033[1;31mError %s en la linea %d:\033[0m ", stage, line);
    vfprintf(stderr, format, formatArguments);
    fprintf(stderr, "\n");

    if (counter) {
        (*counter)++;
    }
}
