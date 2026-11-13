#ifndef LIVE_VARIABLE_ANALYSIS_H
#define LIVE_VARIABLE_ANALYSIS_H

/**
 * @file live_variable_analysis.h
 * @brief Análisis algebraico de variables vivas (Live Variable Analysis) sobre el CFG de TAC.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "tac.h"

#define MAX_LIVENESS_VARIABLES 64

/**
 * @brief Conjunto de variables representado mediante máscara de bits (1ULL << index).
 */
typedef uint64_t VariableSet;

/**
 * @brief Bloque básico de instrucciones TAC en el CFG con sus conjuntos de vivacidad.
 */
typedef struct BasicBlock {
    int id;                               /**< Identificador del bloque básico. */
    TACInstruction *firstInstruction;     /**< Primera instrucción del bloque. */
    TACInstruction *lastInstruction;      /**< Última instrucción del bloque. */

    struct BasicBlock *successors[2];     /**< Sucesores en el CFG (a lo sumo 2: salto y caída). */
    size_t successorCount;

    VariableSet use;     /**< USE(B): Variables leídas antes de ser definidas en B. */
    VariableSet def;     /**< DEF(B): Variables definidas antes de ser leídas en B. */
    VariableSet liveIn;  /**< LIVE_IN(B): Variables vivas al entrar a B. */
    VariableSet liveOut; /**< LIVE_OUT(B): Variables vivas al salir de B. */
} BasicBlock;

/**
 * @brief Grafo de Flujo de Control (CFG) y estado convergido de vivacidad.
 */
typedef struct LiveVariableAnalysis {
    const TACList *tacList;
    BasicBlock **blocks;
    size_t blockCount;

    char variableNames[MAX_LIVENESS_VARIABLES][TAC_NAME_LENGTH];
    size_t variableCount;
} LiveVariableAnalysis;

/**
 * @brief Construye el CFG y resuelve las ecuaciones de vivacidad hasta el punto fijo.
 *
 * @param tacList Lista de instrucciones TAC a analizar.
 * @return Estructura LiveVariableAnalysis convergida, o NULL si falló.
 */
LiveVariableAnalysis *createLiveVariableAnalysis(const TACList *tacList);

/**
 * @brief Libera los recursos del análisis de variables vivas y sus bloques básicos.
 *
 * @param analysis Puntero al análisis a liberar (acepta NULL).
 */
void freeLiveVariableAnalysis(LiveVariableAnalysis *analysis);

/**
 * @brief Obtiene el bloque básico al que pertenece una instrucción dentro del análisis.
 */
BasicBlock *findInstructionBlock(const LiveVariableAnalysis *analysis, const TACInstruction *instruction);

/**
 * @brief Determina si la definición en una instrucción es código muerto dentro de su bloque básico.
 *
 * Versión optimizada O(1) de isDeadDefinition para recorridos donde el bloque básico
 * ya es conocido, evitando la búsqueda global en el CFG.
 *
 * @param analysis Estructura del análisis de variables vivas.
 * @param block Bloque básico al que pertenece la instrucción.
 * @param instruction Puntero a la instrucción a evaluar.
 * @return true si la definición es código muerto, false en caso contrario.
 */
bool isDeadDefinitionInBlock(const LiveVariableAnalysis *analysis, const BasicBlock *block, const TACInstruction *instruction);

/**
 * @brief Determina si la definición en una instrucción es código muerto considerando el flujo global.
 *
 * Una asignación es muerta si su operando destino no se lee en el resto del bloque
 * y tampoco está vivo a la salida del bloque en el CFG.
 */
bool isDeadDefinition(const LiveVariableAnalysis *analysis, const TACInstruction *instruction);

/**
 * @brief Imprime en un stream el CFG, los bloques básicos, instrucciones y conjuntos USE/DEF/IN/OUT.
 *
 * @param analysis Estructura del análisis de variables vivas.
 * @param output Stream de salida (acepta NULL, por defecto stdout).
 */
void printLiveVariableAnalysis(const LiveVariableAnalysis *analysis, FILE *output);

#endif // LIVE_VARIABLE_ANALYSIS_H
