#ifndef ERROR_HANDLER_H
#define ERROR_HANDLER_H

/**
 * @file error_handler.h
 * @brief Reporte y conteo de errores de las distintas fases del compilador.
 *
 * Todos los errores se escriben por stderr, así que la salida de una etapa
 * (que va a stdout o a su archivo) nunca queda contaminada con mensajes.
 * Cada error sale con el mismo formato: "Error <etapa> en la linea N: mensaje".
 * El módulo recuerda además el último error informado, para poder verificarlo
 * desde los tests.
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
 * @brief Reporta un error sintáctico detectado por el parser.
 *
 * Imprime el mensaje formateado en stderr indicando la línea y el detalle del
 * fallo. No lleva contador: el parser se detiene en el primer error, así que
 * alcanza con el resultado de yyparse().
 *
 * @param line Número de línea del código fuente donde el parser detectó el error.
 * @param format Cadena de control de formato estilo printf con la descripción del error.
 * @param ... Argumentos variables para la cadena de formato.
 */
void syntaxError(int line, const char *format, ...);

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
 * @brief Indica el archivo fuente del que se muestra la línea de cada error.
 *
 * Después de cada mensaje de error se imprime la línea del código fuente donde
 * ocurrió. Sin archivo fuente (por ejemplo, en los tests que parsean un string)
 * solo se imprime el mensaje.
 *
 * @param path Ruta del archivo fuente (NULL para no mostrar la línea). No se copia: debe seguir válida mientras se informen errores.
 */
void setErrorSource(const char *path);

/**
 * @brief Línea del último error informado, de cualquier etapa.
 *
 * @return Número de línea, o 0 si todavía no hubo ningún error.
 */
int getLastErrorLine(void);

/**
 * @brief Mensaje del último error informado, sin el prefijo de etapa y línea.
 *
 * @return Texto del mensaje, o cadena vacía si todavía no hubo ningún error.
 */
const char *getLastErrorMessage(void);

/**
 * @brief Pone en cero los contadores de errores y olvida el último error.
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
