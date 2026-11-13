#ifndef OPTIMIZER_H
#define OPTIMIZER_H

/**
 * @file optimizer.h
 * @brief Transformaciones de optimización sobre el Código de Tres Direcciones (TAC).
 */

#include <stdbool.h>

#include "tac.h"

/**
 * @brief Optimizaciones soportadas como máscara de bits para su combinación.
 */
typedef enum {
    OPTIMIZATION_NONE         = 0,
    OPTIMIZATION_CONST_FOLD   = 1 << 0, /**< Plegado de constantes (`const-fold`). */
    OPTIMIZATION_CONST_PROP   = 1 << 1, /**< Propagación de constantes (`const-prop`). */
    OPTIMIZATION_DEAD_CODE    = 1 << 2, /**< Eliminación de código inalcanzable (`dead-code`). */
    OPTIMIZATION_CONST_BRANCH = 1 << 3, /**< Condición constante en `if` / `while` (`const-branch`). */
    OPTIMIZATION_SELF_ASSIGN  = 1 << 4, /**< Asignaciones de una variable a sí misma (`self-assign`). */
    OPTIMIZATION_PEEPHOLE     = 1 << 5, /**< Mirilla sobre instrucciones contiguas (`peephole`). */
    OPTIMIZATION_STRENGTH     = 1 << 6  /**< Reducción de fuerza en multiplicaciones (`strength-reduce`). */
} Optimization;

/**
 * @brief Máscara con todas las optimizaciones soportadas activas.
 */
#define OPTIMIZATION_ALL (OPTIMIZATION_CONST_FOLD | OPTIMIZATION_CONST_PROP | \
                          OPTIMIZATION_DEAD_CODE | OPTIMIZATION_CONST_BRANCH | \
                          OPTIMIZATION_SELF_ASSIGN | OPTIMIZATION_PEEPHOLE | \
                          OPTIMIZATION_STRENGTH)

/**
 * @brief Conjunto de optimizaciones activas en una corrida.
 */
typedef unsigned int OptimizationSet;

/**
 * @brief Aplica sobre la lista las optimizaciones activas hasta alcanzar punto fijo.
 *
 * Las pasadas de optimización se repiten iterativamente hasta que ninguna
 * introduce cambios adicionales o se alcanza el límite de rondas.
 *
 * @param tacList Lista de instrucciones TAC a transformar en el lugar (acepta NULL).
 * @param enabled Conjunto de optimizaciones habilitadas.
 * @return Cantidad total de instrucciones transformadas.
 */
int optimizeIntermediateCode(TACList *tacList, OptimizationSet enabled);

/**
 * @brief Habilita o deshabilita los mensajes informativos de depuración del optimizador.
 *
 * @param enabled true para imprimir cada instrucción transformada/removida por stdout.
 */
void setOptimizerDebug(bool enabled);

/**
 * @brief Indica si una optimización específica está activa en el conjunto.
 *
 * @param enabled Conjunto de optimizaciones activas.
 * @param optimization Bandera de la optimización a consultar.
 * @return true si la optimización está activa, false en caso contrario.
 */
static inline bool isOptimizationEnabled(OptimizationSet enabled, Optimization optimization) {
    return (enabled & (OptimizationSet)optimization) != 0;
}

#endif // OPTIMIZER_H
