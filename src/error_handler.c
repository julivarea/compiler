#include "error_handler.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_MESSAGE_LENGTH 256

static void reportError(const char *stage, int *counter, int line, const char *format, va_list formatArguments);
static void printSourceLine(int line);

int lexical_errors = 0;
int semantic_errors = 0;

static const char *sourcePath = NULL;
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

void setErrorSource(const char *path) {
    sourcePath = path;
}

int getLastErrorLine(void) {
    return lastErrorLine;
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
    printSourceLine(line);

    bool hasCounter = (counter != NULL);
    if (hasCounter) {
        (*counter)++;
    }
}

static void printSourceLine(int line) {
    bool hasNoSourcePath = (sourcePath == NULL);
    bool isInvalidLine = (line < 1);
    if (hasNoSourcePath || isInvalidLine) return;

    FILE *sourceFile = fopen(sourcePath, "r");
    bool failedToOpenFile = (sourceFile == NULL);
    if (failedToOpenFile) return;

    char *lineBuffer = NULL;
    size_t bufferCapacity = 0;
    ssize_t bytesRead = 0;
    for (int currentLine = 0; currentLine < line && bytesRead >= 0; currentLine++) {
        bytesRead = getline(&lineBuffer, &bufferCapacity, sourceFile);
    }

    bool hasValidLine = (bytesRead >= 0);
    if (hasValidLine) {
        while (bytesRead > 0 && (lineBuffer[bytesRead - 1] == '\n' || lineBuffer[bytesRead - 1] == '\r')) {
            lineBuffer[--bytesRead] = '\0';
        }
        fprintf(stderr, "%5d | %s\n", line, lineBuffer);
    }

    free(lineBuffer);
    fclose(sourceFile);
}
