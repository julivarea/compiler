#ifndef DATA_FLOW_H
#define DATA_FLOW_H

/**
 * @file data_flow.h
 * @brief Análisis de flujo de datos y consultas de 'use' y 'def' sobre instrucciones TAC.
 */

#include <stddef.h>
#include <stdbool.h>

#include "tac_instruction.h"
#include "tac_operand.h"

#define MAX_TAC_DEFS 1
#define MAX_TAC_USES 2

/**
 * @brief Información de uso (use) y definición (def) de una instrucción individual TAC.
 */
typedef struct InstructionDataFlow {
    TACInstruction *instruction;          /**< Puntero a la instrucción analizada. */

    const TACOperand *defs[MAX_TAC_DEFS]; /**< Operandos definidos o escritos (variables o temporales). */
    size_t defCount;                      /**< Cantidad de operandos definidos (0 o 1). */

    const TACOperand *uses[MAX_TAC_USES]; /**< Operandos leídos o usados. */
    size_t useCount;                      /**< Cantidad de operandos leídos (0 a 2). */
} InstructionDataFlow;

/**
 * @brief Representación del flujo de datos de un bloque de sentencias o secuencia de TAC.
 */
typedef struct BlockDataFlow {
    TACInstruction *firstInstruction;     /**< Primera instrucción incluida en el bloque. */
    TACInstruction *lastInstruction;      /**< Última instrucción incluida en el bloque. */
    InstructionDataFlow *entries;         /**< Arreglo contiguo de análisis de cada instrucción. */
    size_t count;                         /**< Cantidad de instrucciones analizadas. */
} BlockDataFlow;

/**
 * @brief Construye el análisis de flujo de datos para un bloque delimitado entre start y end.
 *
 * @param start Primera instrucción del bloque.
 * @param end Última instrucción (inclusive), o NULL para analizar hasta el final de la lista.
 * @return Bloque de flujo de datos creado dinámicamente, o NULL si start es NULL o falló la reserva.
 */
BlockDataFlow *createBlockDataFlow(TACInstruction *start, TACInstruction *end);

/**
 * @brief Libera la memoria ocupada por la estructura de flujo de datos del bloque.
 *
 * No libera las instrucciones TAC subyacentes, únicamente los metadatos del análisis.
 *
 * @param block Bloque a liberar (acepta NULL).
 */
void freeBlockDataFlow(BlockDataFlow *block);

/**
 * @brief Obtiene la información de flujo de una instrucción específica dentro del bloque.
 *
 * @return Información de flujo asociada a la instrucción, o NULL si no pertenece al bloque.
 */
const InstructionDataFlow *blockGetFlow(const BlockDataFlow *block, const TACInstruction *instruction);

/**
 * @brief Busca la próxima instrucción en el bloque que lee la variable a partir de 'from'.
 *
 * @param from Instrucción a partir de la cual iniciar la búsqueda (no inclusiva).
 * @return Instrucción que lee la variable, o NULL si no existe un uso posterior.
 */
TACInstruction *blockFindNextUse(const BlockDataFlow *block, const TACInstruction *from, const char *variableName);

/**
 * @brief Busca la próxima instrucción en el bloque que define la variable a partir de 'from'.
 *
 * @param from Instrucción a partir de la cual iniciar la búsqueda (no inclusiva).
 * @return Instrucción que define la variable, o NULL si no hay una redefinición posterior.
 */
TACInstruction *blockFindNextDef(const BlockDataFlow *block, const TACInstruction *from, const char *variableName);

/**
 * @brief Determina si la variable es redefinida entre dos instrucciones (rango abierto/excluyente).
 *
 * @param from Instrucción de inicio (excluyente).
 * @param to Instrucción de fin (excluyente).
 */
bool blockIsRedefinedBetween(const BlockDataFlow *block, const TACInstruction *from, const TACInstruction *to, const char *variableName);

/**
 * @brief Determina si la definición en una instrucción es código muerto o redundante.
 *
 * Se considera muerta si dentro del bloque se vuelve a definir la misma variable sin que haya
 * existido ningún uso intermedio.
 */
bool blockIsDeadDefinition(const BlockDataFlow *block, const TACInstruction *instruction);

/**
 * @brief Indica si la instrucción escribe o define la variable o temporal especificado.
 */
bool instructionFlowDefines(const InstructionDataFlow *flow, const char *variableName);

/**
 * @brief Indica si la instrucción lee o usa la variable o temporal especificado.
 */
bool instructionFlowUses(const InstructionDataFlow *flow, const char *variableName);

#endif // DATA_FLOW_H
