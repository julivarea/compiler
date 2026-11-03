#ifndef ERROR_HANDLER_H
#define ERROR_HANDLER_H

/**
 * @file error_handler.h
 * @brief Reporte y conteo de errores de las distintas fases del compilador.
 */

/**
 * @brief Reporta un error léxico e incrementa el contador de errores léxicos.
 */
void lexicalError(int line, const char *format, ...);

/**
 * @brief Reporta un error sintáctico detectado por el parser.
 */
void syntaxError(int line, const char *format, ...);

/**
 * @brief Reporta un error semántico e incrementa el contador de errores semánticos.
 *
 * @param line Número de línea del código fuente (0 para errores globales).
 */
void semanticError(int line, const char *format, ...);

/**
 * @brief Reporta el resumen de compilación abortada por acumulación de errores de una etapa.
 *
 * @param stage Descripción de la etapa o causa, en plural (ej. "lexicos").
 * @param errorCount Cantidad de errores que causaron el aborto.
 */
void compileAborted(const char *stage, int errorCount);

/**
 * @brief Devuelve la línea del último error informado.
 *
 * @return Número de línea, o 0 si no hubo ningún error.
 */
int getLastErrorLine(void);

/**
 * @brief Devuelve el mensaje del último error informado, sin prefijos de etapa ni línea.
 *
 * @return Texto del mensaje, o cadena vacía si no hubo ningún error.
 */
const char *getLastErrorMessage(void);

/**
 * @brief Reinicia los contadores de errores y olvida el último error registrado.
 */
void resetErrorHandler(void);

/**
 * @brief Cantidad acumulada de errores léxicos detectados durante el escaneo.
 */
extern int lexical_errors;

/**
 * @brief Cantidad acumulada de errores semánticos detectados durante la validación.
 */
extern int semantic_errors;

#endif // ERROR_HANDLER_H
