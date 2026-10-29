#ifndef TAC_INSTRUCTION_H
#define TAC_INSTRUCTION_H

/**
 * @file tac_instruction.h
 * @brief Representación estructurada de una instrucción de Código de Tres Direcciones (cuádrupla).
 */

#include "tac_operand.h"

/**
 * @brief Operaciones del código de tres direcciones.
 */
typedef enum {
    /* Aritméticas: result = arg1 <op> arg2 */
    TAC_ADD,
    TAC_SUB,
    TAC_MUL,
    TAC_DIV,
    TAC_MOD,

    /* Unarias: result = <op> arg1 */
    TAC_NEG,
    TAC_NOT,
    TAC_INT_TO_FLOAT,

    /* Relacionales y lógicas: result = arg1 <op> arg2 */
    TAC_LT,
    TAC_GT,
    TAC_EQ,
    TAC_AND,
    TAC_OR,

    /* Movimiento de datos: result = arg1 */
    TAC_ASSIGN,

    /* Declaraciones: arg1 es la variable, y su tipo viaja dentro del operando */
    TAC_GLOBAL_DECL, /**< variable global: va al segmento de datos     */
    TAC_LOCAL_DECL,  /**< variable local: ocupa un lugar en el frame   */

    /* Control de flujo */
    TAC_LABEL,     /**< result: etiqueta que se define aquí     */
    TAC_GOTO,      /**< arg1: etiqueta destino                  */
    TAC_IF_FALSE,  /**< si arg1 es falso, salta a arg2          */
    TAC_IF_TRUE,   /**< si arg1 es verdadero, salta a arg2      */

    /* Métodos */
    TAC_PARAM,      /**< arg1: argumento a pasar en el lugar de la llamada   */
    TAC_PARAM_DECL, /**< arg1: parámetro formal que el método recibe         */
    TAC_CALL,       /**< result (opcional) = call arg1, arg2 argumentos      */
    TAC_RETURN,     /**< arg1 opcional: valor devuelto                       */
    TAC_BEGIN_FUNC, /**< result: método que empieza; arg1: nº de parámetros  */
    TAC_END_FUNC    /**< result: método que termina                          */
} TACOperation;

/**
 * @brief Estructura de datos que agrupa el operador y los operandos de una instrucción TAC (cuádrupla).
 */
typedef struct TACInstruction {
    TACOperation op;
    TACOperand result;
    TACOperand arg1;
    TACOperand arg2;
    struct TACInstruction *next;
} TACInstruction;

/**
 * @brief Crea una nueva instrucción TAC.
 */
TACInstruction *createTACInstruction(TACOperation op, TACOperand result, TACOperand arg1, TACOperand arg2);

/**
 * @brief Libera una instrucción TAC.
 */
void freeTACInstruction(TACInstruction *instruction);

/**
 * @brief Imprime una única instrucción TAC formateada al string_builder.
 */
void printTACInstruction(const TACInstruction *instruction);

#endif /* TAC_INSTRUCTION_H */
