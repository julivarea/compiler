#ifndef CLI_H
#define CLI_H

/**
 * @file cli.h
 * @brief Parseo de la línea de comandos exigida por el enunciado del proyecto:
 *
 *     c-tds [opcion] nombreArchivo.ctds
 *
 * Esta unidad solo interpreta argumentos: no abre archivos ni ejecuta etapas.
 * Decidir qué fases correr y dónde escribir es responsabilidad del driver
 * (la función `main()` de `src/bison.y`), de modo que el parseo se pueda probar
 * sin compilar nada.
 */

/**
 * @brief Etapas hasta las que se puede compilar (`-target`), en el orden del
 *        pipeline. El orden importa: el driver compara con `>` para saber si la
 *        etapa pedida todavía no está implementada.
 */
typedef enum {
    TARGET_SCAN,      /**< análisis léxico           -> .lex   */
    TARGET_PARSE,     /**< análisis sintáctico       -> .sint  */
    TARGET_SEM,       /**< análisis semántico        -> .sem   */
    TARGET_CODINTER,  /**< código intermedio (TAC)   -> .ci    */
    TARGET_ASSEMBLY   /**< código objeto x86-64      -> .ass   */
} Target;

/**
 * @brief Etapa más avanzada que el compilador implementa hoy.
 *
 * Es también la etapa por defecto cuando no se pasa `-target`. Al terminar cada
 * etapa nueva, mover esta constante es lo único necesario para que el default y
 * el chequeo de "etapa no implementada" acompañen.
 */
#define CURRENT_TARGET TARGET_ASSEMBLY

typedef struct {
    const char *input;   /**< archivo .ctds de entrada (apunta a argv) */
    char *output;        /**< archivo de salida; reservado con malloc  */
    Target target;       /**< etapa hasta la que se compila            */
    int explicitTarget;  /**< 1 si se pasó -target; 0 si es el default */
    int debug;           /**< -debug                                   */
    const char *opt;     /**< -opt <nombre>; NULL si no se pasó        */
} Options;

/**
 * @brief Indica si la corrida debe producir el ejecutable en vez del archivo
 *        de una etapa.
 *
 * El enunciado pide que sin `-target` el compilador genere `nombreArchivo.out`.
 * Pedir explícitamente `-target assembly` produce el `.ass` y se detiene ahí,
 * aunque sea la misma etapa.
 */
#define BUILDS_EXECUTABLE(options) (!(options)->explicitTarget)

/**
 * @brief Interpreta argv y completa `options` con los valores por defecto de lo
 *        que no se haya pasado. Los errores se reportan por stderr junto con el
 *        modo de uso.
 *
 * @return 0 si los argumentos son válidos, 1 si no.
 */
int parseArguments(int argc, char **argv, Options *options);

/** @brief Libera la memoria reservada por parseArguments. */
void freeOptions(Options *options);

/** @brief Nombre de la etapa tal como se escribe en `-target`, para mensajes. */
const char *getTargetName(Target target);

#endif /* CLI_H */
