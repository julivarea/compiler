#ifndef OPTIMIZER_H
#define OPTIMIZER_H

/**
 * @file optimizer.h
 * @brief Transformaciones de optimización sobre el Código de Tres Direcciones.
 *
 * Las optimizaciones se activan individualmente desde la línea de comandos con
 * `-opt <nombre>`, o todas juntas con `-opt all`. Sin `-opt` no se aplica
 * ninguna y el código generado es idéntico al del compilador sin esta etapa:
 * esa equivalencia es lo que permite aislar una regresión introducida por una
 * optimización.
 *
 * El catálogo completo, con ejemplos y precondiciones, está en
 * `optimizaciones.md`.
 */

#include <stdbool.h>

#include "ast.h"
#include "tac_list.h"

/**
 * @brief Optimizaciones soportadas, como máscara de bits para poder combinarlas.
 */
typedef enum {
    OPTIMIZATION_NONE       = 0,
    OPTIMIZATION_CONST_FOLD = 1 << 0, /**< Plegado de constantes (`const-fold`). */
    OPTIMIZATION_CONST_PROP = 1 << 1, /**< Propagación de constantes (`const-prop`). */
    OPTIMIZATION_DEAD_CODE  = 1 << 2, /**< Eliminación de código inalcanzable (`dead-code`). */
    OPTIMIZATION_CONST_BRANCH = 1 << 3, /**< Condición constante en `if` / `while` (`const-branch`). */
    OPTIMIZATION_SELF_ASSIGN  = 1 << 4, /**< Asignaciones de una variable a sí misma (`self-assign`). */
    OPTIMIZATION_PEEPHOLE     = 1 << 5  /**< Mirilla sobre instrucciones contiguas (`peephole`). */
} Optimization;

/** @brief Todas las optimizaciones soportadas hasta el momento. */
#define OPTIMIZATION_ALL (OPTIMIZATION_CONST_FOLD | OPTIMIZATION_CONST_PROP | \
                          OPTIMIZATION_DEAD_CODE | OPTIMIZATION_CONST_BRANCH | \
                          OPTIMIZATION_SELF_ASSIGN | OPTIMIZATION_PEEPHOLE)

/** @brief Conjunto de optimizaciones activas en una corrida. */
typedef unsigned int OptimizationSet;

/**
 * @brief Traduce el valor recibido en `-opt` al conjunto de optimizaciones activas.
 *
 * @param option Nombre de la optimización, `"all"`, o NULL si no se pasó `-opt`.
 * @return El conjunto correspondiente, u `OPTIMIZATION_NONE` si @p option es NULL.
 *         Si el nombre no se reconoce, informa el error por stderr y devuelve
 *         `OPTIMIZATION_NONE`.
 */
OptimizationSet parseOptimizations(const char *option);

/**
 * @brief Indica si una optimización está activa en el conjunto.
 */
bool isOptimizationEnabled(OptimizationSet enabled, Optimization optimization);

/**
 * @brief Aplica sobre la lista las optimizaciones activas.
 *
 * Las pasadas se repiten hasta que ninguna introduce cambios, porque se
 * habilitan entre sí: propagar una constante crea una operación entre
 * constantes, que al plegarse crea una nueva constante que propagar.
 *
 * @param tacList Lista de instrucciones, modificada en el lugar.
 * @param enabled Conjunto de optimizaciones a aplicar.
 * @return Cantidad total de instrucciones transformadas.
 */
int optimizeIntermediateCode(TACList *tacList, OptimizationSet enabled);

/**
 * @brief Aplica sobre el AST las optimizaciones que trabajan a ese nivel.
 *
 * Se invoca antes de generar el código intermedio. En vez de desenganchar nodos
 * del árbol —con el riesgo de romper los punteros de sus padres— marca los que
 * no deben emitirse con `hasExecute = false`, que es lo que consulta
 * `generateStatementCode()`.
 *
 * @param root Nodo raíz del programa.
 * @param enabled Conjunto de optimizaciones a aplicar.
 * @return Cantidad de nodos marcados como no ejecutables.
 */
int optimizeSyntaxTree(NodeAST *root, OptimizationSet enabled);

#endif // OPTIMIZER_H
