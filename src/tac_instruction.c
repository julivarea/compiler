#include <stdio.h>
#include <stdlib.h>
#include "tac_instruction.h"
#include "string_builder.h"

static const char *getOperatorSymbol(TACOperation operation);

/**
 * @brief Crea e inicializa una nueva instrucción TAC
 *
 * @param operation Código de operación que define la instrucción.
 * @param result Operando de destino o etiqueta asociada.
 * @param firstOperand Primer operando fuente o argumento principal.
 * @param secondOperand Segundo operando fuente o argumento secundario.
 * @return Puntero a la instrucción creada, o NULL si falló la reserva de memoria.
 */
TACInstruction *createTACInstruction(TACOperation operation, TACOperand result, TACOperand firstOperand, TACOperand secondOperand) {
    TACInstruction *instruction = malloc(sizeof(TACInstruction));
    if (instruction == NULL) return NULL;

    instruction->operation = operation;
    instruction->result = result;
    instruction->firstOperand = firstOperand;
    instruction->secondOperand = secondOperand;
    instruction->next = NULL;

    return instruction;
}

/**
 * @brief Libera la memoria ocupada por una instrucción TAC individual.
 *
 * @param instruction Puntero a la instrucción TAC a liberar
 */
void freeTACInstruction(TACInstruction *instruction) {
    if (instruction != NULL) {
        free(instruction);
    }
}

/**
 * @brief Imprime la representación textual de una instrucción TAC al acumulador de cadenas.
 *
 * Extrae la representación de los operandos y formatea la instrucción según su operacion
 *
 * @param instruction Puntero a la instrucción TAC a imprimir
 */
void printTACInstruction(const TACInstruction *instruction) {
    if (instruction == NULL) return;

    const char *result         = getOperandName(&instruction->result);
    const char *firstOperand   = getOperandName(&instruction->firstOperand);
    const char *secondOperand  = getOperandName(&instruction->secondOperand);

    switch (instruction->operation) {
        case TAC_LABEL:
            buildStringf("%s:\n", result);
            break;

        case TAC_BEGIN_FUNC:
            buildStringf("begin_func %s, %s\n", result, firstOperand);
            break;

        case TAC_END_FUNC:
            buildStringf("end_func %s\n", result);
            break;

        case TAC_PARAM_DECL:
            buildStringf("param_decl %s\n", firstOperand);
            break;

        case TAC_GLOBAL_DECL:
        case TAC_LOCAL_DECL:
            buildStringf("%s %s, %s\n",
                         instruction->operation == TAC_GLOBAL_DECL ? "global" : "local",
                         firstOperand, getDataTypeName(instruction->firstOperand.type));
            break;

        case TAC_ASSIGN:
            buildStringf("%s = %s\n", result, firstOperand);
            break;

        case TAC_NEG:
        case TAC_NOT:
            buildStringf("%s = %s%s\n", result, getOperatorSymbol(instruction->operation), firstOperand);
            break;

        case TAC_INT_TO_FLOAT:
            buildStringf("%s = int_to_float %s\n", result, firstOperand);
            break;

        case TAC_GOTO:
            buildStringf("goto %s\n", firstOperand);
            break;

        case TAC_IF_FALSE:
        case TAC_IF_TRUE:
            buildStringf("if %s %s %s\n",
                         firstOperand,
                         instruction->operation == TAC_IF_FALSE ? "false goto" : "true goto",
                         secondOperand);
            break;

        case TAC_PARAM:
            buildStringf("param %s\n", firstOperand);
            break;

        case TAC_CALL:
            if (instruction->result.kind != OPERAND_NONE) {
                buildStringf("%s = call %s, %s\n", result, firstOperand, secondOperand);
            } else {
                buildStringf("call %s, %s\n", firstOperand, secondOperand);
            }
            break;

        case TAC_RETURN:
            if (instruction->firstOperand.kind != OPERAND_NONE) {
                buildStringf("ret %s\n", firstOperand);
            } else {
                buildStringf("ret\n");
            }
            break;

        default: // binarias
            buildStringf("%s = %s %s %s\n", result, firstOperand, getOperatorSymbol(instruction->operation), secondOperand);
            break;
    }
}

/**
 * @brief Obtiene la representación textual del operador para instrucciones binarias o unarias.
 *
 * Mapea códigos de operación a su simbolo correspondiente
 *
 * @param operation Código de operación del TAC.
 * @return Cadena constante con el símbolo del operador.
 */
static const char *getOperatorSymbol(TACOperation operation) {
    switch (operation) {
        case TAC_ADD: return "+";
        case TAC_SUB: return "-";
        case TAC_MUL: return "*";
        case TAC_DIV: return "/";
        case TAC_MOD: return "%";
        case TAC_LT:  return "<";
        case TAC_GT:  return ">";
        case TAC_EQ:  return "==";
        case TAC_AND: return "&&";
        case TAC_OR:  return "||";
        case TAC_NEG: return "-";
        case TAC_NOT: return "!";
        default:      return "?";
    }
}
