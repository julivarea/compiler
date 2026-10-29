#ifndef ERROR_HANDLER_H
#define ERROR_HANDLER_H

/**
 * @file error_handler.h
 * @brief Reporte y conteo de errores de las distintas fases.
 *
 * Todos los errores se escriben por stderr, asi que la salida de una etapa
 * (que va a stdout o a su archivo) nunca queda contaminada con mensajes.
 */

extern int semantic_errors;
extern int lexical_errors;

void semanticError(int line, const char *format, ...);
void lexicalError(int line, const char *format, ...);

#endif
