#include "tac_instruction.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "string_builder.h"

void formatTACInstruction(char *buffer, size_t size, const TACInstruction *instruction);
static const char *getOperatorSymbol(TACOperation operation);

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

void freeTACInstruction(TACInstruction *instruction) {
    if (instruction != NULL) {
        free(instruction);
    }
}

void printTACInstruction(const TACInstruction *instruction) {
    char line[TAC_INSTRUCTION_TEXT_LENGTH];
    formatTACInstruction(line, sizeof(line), instruction);
    buildString(line);
}

void formatTACInstruction(char *buffer, size_t size, const TACInstruction *instruction) {
    buffer[0] = '\0';
    if (instruction == NULL) return;

    /* getOperandName apunta al nombre interno del operando, estable mientras dura el llamado */
    switch (instruction->operation) {
        case TAC_BEGIN_FUNC:
            snprintf(buffer, size, "begin_func %s, %s\n",
                     getOperandName(&instruction->result), getOperandName(&instruction->firstOperand));
            break;

        case TAC_END_FUNC:
            snprintf(buffer, size, "end_func %s\n", getOperandName(&instruction->result));
            break;

        case TAC_PARAM_DECL:
            snprintf(buffer, size, "param_decl %s\n", getOperandName(&instruction->firstOperand));
            break;

        case TAC_GLOBAL_DECL:
        case TAC_LOCAL_DECL:
            snprintf(buffer, size, "%s %s, %s\n",
                     instruction->operation == TAC_GLOBAL_DECL ? "global" : "local",
                     getOperandName(&instruction->firstOperand),
                     getDataTypeName(instruction->firstOperand.type));
            break;

        case TAC_LABEL:
            snprintf(buffer, size, "%s:\n", getOperandName(&instruction->result));
            break;

        case TAC_GOTO:
            snprintf(buffer, size, "goto %s\n", getOperandName(&instruction->firstOperand));
            break;

        case TAC_IF_FALSE:
        case TAC_IF_TRUE:
            snprintf(buffer, size, "if %s %s %s\n",
                     getOperandName(&instruction->firstOperand),
                     instruction->operation == TAC_IF_FALSE ? "false goto" : "true goto",
                     getOperandName(&instruction->secondOperand));
            break;

        case TAC_PARAM:
            snprintf(buffer, size, "param %s\n", getOperandName(&instruction->firstOperand));
            break;

        case TAC_CALL:
            if (instruction->result.kind != OPERAND_NONE) {
                snprintf(buffer, size, "%s = call %s, %s\n",
                         getOperandName(&instruction->result),
                         getOperandName(&instruction->firstOperand),
                         getOperandName(&instruction->secondOperand));
            } else {
                snprintf(buffer, size, "call %s, %s\n",
                         getOperandName(&instruction->firstOperand),
                         getOperandName(&instruction->secondOperand));
            }
            break;

        case TAC_RETURN:
            if (instruction->firstOperand.kind != OPERAND_NONE) {
                snprintf(buffer, size, "ret %s\n", getOperandName(&instruction->firstOperand));
            } else {
                snprintf(buffer, size, "ret\n");
            }
            break;

        case TAC_ASSIGN:
            snprintf(buffer, size, "%s = %s\n",
                     getOperandName(&instruction->result), getOperandName(&instruction->firstOperand));
            break;

        case TAC_UMINUS:
        case TAC_NOT:
            snprintf(buffer, size, "%s = %s%s\n",
                     getOperandName(&instruction->result),
                     getOperatorSymbol(instruction->operation),
                     getOperandName(&instruction->firstOperand));
            break;

        case TAC_INT_TO_FLOAT:
            snprintf(buffer, size, "%s = int_to_float %s\n",
                     getOperandName(&instruction->result),
                     getOperandName(&instruction->firstOperand));
            break;

        default: /* operador binario: result = A sym B */
            snprintf(buffer, size, "%s = %s %s %s\n",
                     getOperandName(&instruction->result),
                     getOperandName(&instruction->firstOperand),
                     getOperatorSymbol(instruction->operation),
                     getOperandName(&instruction->secondOperand));
            break;
    }
}

static const char *getOperatorSymbol(TACOperation operation) {
    switch (operation) {
        case TAC_ADD:    return "+";
        case TAC_SUB:    return "-";
        case TAC_MUL:    return "*";
        case TAC_DIV:    return "/";
        case TAC_MOD:    return "%";
        case TAC_LT:     return "<";
        case TAC_GT:     return ">";
        case TAC_EQ:     return "==";
        case TAC_UMINUS: return "-";
        case TAC_NOT:    return "!";
        default:         return "?";
    }
}
