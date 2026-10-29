#ifndef ERROR_HANDLER_H
#define ERROR_HANDLER_H

/**
 * @file error_handler.h
 * @brief Reporte y conteo de errores de las distintas fases del compilador.
 *
 * Todos los errores se escriben por stderr, así que la salida de una etapa
 * (que va a stdout o a su archivo) nunca queda contaminada con mensajes.
 */

/**
 * @brief Reporta un error léxico ocurrido durante la fase de escaneo.
 *
 * Imprime el mensaje formateado en stderr destacando el número de línea
 * y el tipo de error en color rojo, e incrementa el contador de errores léxicos.
 *
 * @param line Número de línea del archivo fuente donde ocurrió el error.
 * @param format Cadena de control de formato estilo printf con la descripción del error.
 * @param ... Argumentos variables para la cadena de formato.
 */
void lexicalError(int line, const char *format, ...);

/**
 * @brief Reporta un error semántico durante el análisis semántico o verificación de tipos.
 *
 * Imprime el mensaje formateado en stderr indicando la línea y el detalle del
 * fallo semántico, e incrementa el contador de errores semánticos.
 *
 * @param line Número de línea del código fuente donde se detectó el error (0 para errores globales).
 * @param format Cadena de control de formato estilo printf con la descripción del error.
 * @param ... Argumentos variables para la cadena de formato.
 */
void semanticError(int line, const char *format, ...);

/**
 * @brief Cantidad acumulada de errores léxicos detectados durante el escaneo.
 */
extern int lexical_errors;

/**
 * @brief Cantidad acumulada de errores semánticos detectados durante la validación.
 */
extern int semantic_errors;

#endif // ERROR_HANDLER_H
