#ifndef TAC_INSTRUCTION_H
#define TAC_INSTRUCTION_H

/**
 * @file tac_instruction.h
 * @brief Representación estructurada de una instrucción de código de tres direcciones 
 */

#include "tac_operand.h"
#include "tac_operation.h"

/**
 * @brief Estructura de datos que agrupa el operador y los operandos de una instrucción de tres direcciones.
 */
typedef struct TACInstruction {
    TACOperation operation;
    TACOperand result;
    TACOperand firstOperand;
    TACOperand secondOperand;
    struct TACInstruction *next;
} TACInstruction;

/**
 * @brief Crea una nueva instrucción TAC.
 */
TACInstruction *createTACInstruction(TACOperation operation, TACOperand result, TACOperand firstOperand, TACOperand secondOperand);

/**
 * @brief Libera una instrucción TAC.
 */
void freeTACInstruction(TACInstruction *instruction);

/**
 * @brief Imprime una única instrucción TAC formateada al string_builder.
 */
void printTACInstruction(const TACInstruction *instruction);

#endif /* TAC_INSTRUCTION_H */
