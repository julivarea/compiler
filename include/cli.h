#ifndef CLI_H
#define CLI_H

/**
 * @file cli.h
 * @brief Parseo y configuración de la línea de comandos del compilador.
 *
 * Procesa los argumentos recibidos por la invocación del compilador (ej. c-tds [opciones] archivo.ctds),
 * valida extensiones y banderas, y completa la estructura Options con la configuración resultante.
 * Esta unidad solo interpreta argumentos: no abre archivos ni ejecuta etapas.
 */

/**
 * @brief Etapas del pipeline de compilación hasta las que se puede procesar el código.
 *
 * El orden de los enumeradores respeta estrictamente el pipeline del compilador,
 * permitiendo comparar etapas con operadores relacionales para verificar disponibilidad.
 */
typedef enum Target {
    TARGET_SCAN,      /**< Análisis léxico (produce .lex). */
    TARGET_PARSE,     /**< Análisis sintáctico (produce .sint). */
    TARGET_SEM,       /**< Análisis semántico (produce .sem). */
    TARGET_CODINTER,  /**< Generación de código intermedio TAC (produce .ci). */
    TARGET_ASSEMBLY   /**< Generación de código objeto x86-64 (produce .ass). */
} Target;

/**
 * @brief Etapa más avanzada que el compilador implementa actualmente.
 *
 * Corresponde a la etapa por defecto cuando no se pasa la opción `-target`.
 */
#define CURRENT_TARGET TARGET_ASSEMBLY

/**
 * @brief Opciones de configuración resultantes del procesamiento de la línea de comandos.
 *
 * Agrupa las rutas de entrada y salida, la etapa del compilador objetivo
 * y las banderas de diagnóstico y optimización especificadas por el usuario.
 */
typedef struct Options {
    const char *input;   /**< Archivo .ctds de entrada (apunta a argv). */
    char *output;        /**< Archivo de salida (reservado dinámicamente con malloc). */
    Target target;       /**< Etapa objetivo hasta la que compila el pipeline. */
    int explicitTarget;  /**< 1 si se especificó explícitamente -target; 0 si es el default. */
    int debug;           /**< 1 si se activó la bandera -debug; 0 en caso contrario. */
    const char *opt;     /**< Nombre de optimización (-opt); "all", nombre o NULL si se omitió. */
} Options;

/**
 * @brief Indica si la corrida debe producir el ejecutable en vez del archivo de una etapa.
 *
 * El enunciado exige que sin `-target` se genere `nombreArchivo.out`. Si se pide explícitamente
 * `-target assembly`, se produce el `.ass` y se detiene ahí.
 *
 * @param options Puntero a la estructura de opciones evaluada.
 * @return 1 si debe generarse el ejecutable final, 0 si debe generarse el archivo de etapa.
 */
#define BUILDS_EXECUTABLE(options) (!(options)->explicitTarget)

/**
 * @brief Interpreta los argumentos de línea de comandos y completa la estructura de opciones.
 *
 * Procesa @p argv configurando @p options con los valores suministrados o sus valores por defecto.
 * Si ocurre un error de sintaxis o falta un argumento requerido, reporta el mensaje de error
 * y el modo de uso por stderr.
 *
 * @param argc Cantidad de argumentos pasados al programa.
 * @param argv Vector de cadenas con los argumentos de la línea de comandos.
 * @param options Puntero a la estructura de opciones que será completada.
 * @return 0 si los argumentos son válidos, 1 en caso de error.
 */
int parseArguments(int argc, char **argv, Options *options);

/**
 * @brief Libera la memoria reservada dinámicamente por la estructura de opciones.
 *
 * Libera el búfer de la cadena de salida reservado durante el parseo y restablece
 * el puntero a NULL.
 *
 * @param options Puntero a la estructura de opciones a liberar (acepta NULL).
 */
void freeOptions(Options *options);

/**
 * @brief Obtiene la representación textual de una etapa para mensajes informativos.
 *
 * Retorna el nombre literal correspondiente al identificador de la etapa @p target,
 * coincidente con el valor esperado por el parámetro `-target`.
 *
 * @param target Etapa del compilador a consultar.
 * @return Cadena constante con el nombre de la etapa, o "?" si no es reconocida.
 */
const char *getTargetName(Target target);

#endif // CLI_H
