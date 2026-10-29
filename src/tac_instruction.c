#include <stdio.h>
#include <stdlib.h>
#include "tac_instruction.h"
#include "string_builder.h"

static const char *getOperatorSymbol(TACOperation operation);

/**
 * @brief Crea e inicializa una nueva instrucción TAC cuádrupla en memoria dinámica.
 * @param op Código de operación que define la instrucción.
 * @param result Operando de destino o etiqueta asociada.
 * @param arg1 Primer operando fuente o argumento principal.
 * @param arg2 Segundo operando fuente o argumento secundario.
 * @return Puntero a la instrucción creada, o NULL si falló la reserva de memoria.
 */
TACInstruction *createTACInstruction(TACOperation op, TACOperand result, TACOperand arg1, TACOperand arg2) {
    TACInstruction *instruction = malloc(sizeof(TACInstruction));
    if (instruction == NULL) return NULL;

    instruction->op = op;
    instruction->result = result;
    instruction->arg1 = arg1;
    instruction->arg2 = arg2;
    instruction->next = NULL;

    return instruction;
}

/**
 * @brief Libera la memoria ocupada por una instrucción TAC individual.
 * @param instruction Puntero a la instrucción TAC a liberar
 */
void freeTACInstruction(TACInstruction *instruction) {
    if (instruction != NULL) {
        free(instruction);
    }
}

/**
 * @brief Obtiene la representación textual del operador para instrucciones binarias o unarias.
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

/**
 * @brief Imprime la representación textual de una instrucción TAC al acumulador de cadenas.
 *
 * Extrae la representacion de los operandos, y formatea segun la operacion de la instruccion
 *
 * @param instruction Puntero a la instrucción TAC a imprimir
 */
void printTACInstruction(const TACInstruction *instruction) {
    if (instruction == NULL) return;

    const char *res  = getOperandName(&instruction->result);
    const char *arg1 = getOperandName(&instruction->arg1);
    const char *arg2 = getOperandName(&instruction->arg2);

    switch (instruction->op) {
        case TAC_LABEL:
            buildStringf("%s:\n", res);
            break;

        case TAC_BEGIN_FUNC:
            buildStringf("begin_func %s, %s\n", res, arg1);
            break;

        case TAC_END_FUNC:
            buildStringf("end_func %s\n", res);
            break;

        case TAC_PARAM_DECL:
            buildStringf("param_decl %s\n", arg1);
            break;

        case TAC_GLOBAL_DECL:
        case TAC_LOCAL_DECL:
            buildStringf("%s %s, %s\n",
                         instruction->op == TAC_GLOBAL_DECL ? "global" : "local",
                         arg1, getDataTypeName(instruction->arg1.type));
            break;

        case TAC_ASSIGN:
            buildStringf("%s = %s\n", res, arg1);
            break;

        case TAC_NEG:
        case TAC_NOT:
            buildStringf("%s = %s%s\n", res, getOperatorSymbol(instruction->op), arg1);
            break;

        case TAC_INT_TO_FLOAT:
            buildStringf("%s = int_to_float %s\n", res, arg1);
            break;

        case TAC_GOTO:
            buildStringf("goto %s\n", arg1);
            break;

        case TAC_IF_FALSE:
        case TAC_IF_TRUE:
            buildStringf("if %s %s %s\n",
                         arg1,
                         instruction->op == TAC_IF_FALSE ? "false goto" : "true goto",
                         arg2);
            break;

        case TAC_PARAM:
            buildStringf("param %s\n", arg1);
            break;

        case TAC_CALL:
            if (instruction->result.kind != OPERAND_NONE) {
                buildStringf("%s = call %s, %s\n", res, arg1, arg2);
            } else {
                buildStringf("call %s, %s\n", arg1, arg2);
            }
            break;

        case TAC_RETURN:
            if (instruction->arg1.kind != OPERAND_NONE) {
                buildStringf("ret %s\n", arg1);
            } else {
                buildStringf("ret\n");
            }
            break;

        default: // binarias
            buildStringf("%s = %s %s %s\n", res, arg1, getOperatorSymbol(instruction->op), arg2);
            break;
    }
}
