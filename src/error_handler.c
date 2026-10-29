#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>
#include "error_handler.h"

#define MAX_MESSAGE_LENGTH 256

static void reportError(const char *stage, int *counter, int line, const char *format, va_list args);
static void printSourceLine(int line);

int lexical_errors = 0;
int semantic_errors = 0;

static const char *sourcePath = NULL;
static int lastErrorLine = 0;
static char lastErrorMessage[MAX_MESSAGE_LENGTH] = "";

/**
 * @brief Reporta un error léxico y suma uno al contador de errores léxicos.
 *
 * @param line Número de línea del archivo fuente donde ocurrió el error.
 * @param format Cadena de formato estilo printf con la descripción del error.
 * @param ... Argumentos de la cadena de formato.
 */
void lexicalError(int line, const char *format, ...) {
    va_list args;
    va_start(args, format);
    reportError("lexico", &lexical_errors, line, format, args);
    va_end(args);
}

/**
 * @brief Reporta un error sintáctico sin contarlo.
 *
 * @param line Número de línea del archivo fuente donde el parser detectó el error.
 * @param format Cadena de formato estilo printf con la descripción del error.
 * @param ... Argumentos de la cadena de formato.
 */
void syntaxError(int line, const char *format, ...) {
    va_list args;
    va_start(args, format);
    reportError("sintactico", NULL, line, format, args);
    va_end(args);
}

/**
 * @brief Reporta un error semántico y suma uno al contador de errores semánticos.
 *
 * @param line Número de línea del archivo fuente donde se detectó el error.
 * @param format Cadena de formato estilo printf con la descripción del error.
 * @param ... Argumentos de la cadena de formato.
 */
void semanticError(int line, const char *format, ...) {
    va_list args;
    va_start(args, format);
    reportError("semantico", &semantic_errors, line, format, args);
    va_end(args);
}

/**
 * @brief Indica el archivo fuente del que se muestra la línea de cada error.
 *
 * @param path Ruta del archivo fuente (NULL para no mostrar la línea).
 */
void setErrorSource(const char *path) {
    sourcePath = path;
}

/**
 * @brief Devuelve la línea del último error informado.
 *
 * @return Número de línea, o 0 si todavía no hubo ningún error.
 */
int getLastErrorLine(void) {
    return lastErrorLine;
}

/**
 * @brief Devuelve el mensaje del último error informado, sin el prefijo de etapa y línea.
 *
 * @return Texto del mensaje, o cadena vacía si todavía no hubo ningún error.
 */
const char *getLastErrorMessage(void) {
    return lastErrorMessage;
}

/**
 * @brief Pone en cero los contadores de errores y olvida el último error.
 */
void resetErrorHandler(void) {
    lexical_errors = 0;
    semantic_errors = 0;
    lastErrorLine = 0;
    lastErrorMessage[0] = '\0';
}

/**
 * @brief Imprime un error por stderr con el formato común y lo registra como último error.
 *
 * El mensaje se trunca a MAX_MESSAGE_LENGTH caracteres. El formato de salida es
 * "Error <etapa> en la linea N: mensaje", con el prefijo en rojo, seguido de la
 * línea del código fuente.
 *
 * @param stage Nombre de la etapa que informa el error ("lexico", "sintactico" o "semantico").
 * @param counter Contador de errores de la etapa que se incrementa (acepta NULL).
 * @param line Número de línea del archivo fuente donde ocurrió el error.
 * @param format Cadena de formato estilo printf con la descripción del error.
 * @param args Argumentos de la cadena de formato.
 */
static void reportError(const char *stage, int *counter, int line, const char *format, va_list args) {
    vsnprintf(lastErrorMessage, sizeof(lastErrorMessage), format, args);
    lastErrorLine = line;

    fprintf(stderr, "\033[1;31mError %s en la linea %d:\033[0m %s\n", stage, line, lastErrorMessage);
    printSourceLine(line);
    if (counter != NULL) (*counter)++;
}

/**
 * @brief Imprime por stderr la línea del archivo fuente donde ocurrió un error.
 *
 * Relee el archivo hasta la línea pedida, ya que los errores son pocos. No imprime
 * nada si no hay archivo fuente, si @p line es menor que 1 o si el archivo tiene
 * menos líneas.
 *
 * @param line Número de línea (empieza en 1).
 */
static void printSourceLine(int line) {
    if (sourcePath == NULL || line < 1) return;

    FILE *source = fopen(sourcePath, "r");
    if (source == NULL) return;

    char *text = NULL;
    size_t capacity = 0;
    ssize_t length = 0;
    for (int current = 0; current < line && length >= 0; current++) {
        length = getline(&text, &capacity, source);
    }

    if (length >= 0) {
        while (length > 0 && (text[length - 1] == '\n' || text[length - 1] == '\r')) text[--length] = '\0';
        fprintf(stderr, "%5d | %s\n", line, text);
    }

    free(text);
    fclose(source);
}
