#include "error_handler.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>

#define MAX_MESSAGE_LENGTH 256

static void reportError(const char *stage, int *counter, int line, const char *format, va_list formatArguments);

int lexical_errors = 0;
int semantic_errors = 0;

static int lastErrorLine = 0;
static char lastErrorMessage[MAX_MESSAGE_LENGTH] = "";

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

int getLastErrorLine(void) {
    return lastErrorLine;
}

void compileAborted(const char *stage, int errorCount) {
    fprintf(stderr, "\n\033[1;31mCompilacion abortada: se encontraron %d errores %s.\033[0m\n",
            errorCount, stage);
}

const char *getLastErrorMessage(void) {
    return lastErrorMessage;
}

void resetErrorHandler(void) {
    lexical_errors = 0;
    semantic_errors = 0;
    lastErrorLine = 0;
    lastErrorMessage[0] = '\0';
}

static void reportError(const char *stage, int *counter, int line, const char *format, va_list formatArguments) {
    vsnprintf(lastErrorMessage, sizeof(lastErrorMessage), format, formatArguments);
    lastErrorLine = line;

    fprintf(stderr, "\033[1;31mError %s en la linea %d:\033[0m %s\n", stage, line, lastErrorMessage);

    if (counter) {
        (*counter)++;
    }
}
