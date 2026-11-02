#include "tac_instruction.h"

#include <stdio.h>
#include <stdlib.h>
#include "string_builder.h"

static void formatDeclarationInstruction(const TACInstruction *instruction, const char *result, const char *firstOperand);
static void formatControlFlowInstruction(const TACInstruction *instruction, const char *result, const char *firstOperand, const char *secondOperand);
static void formatCallOrReturnInstruction(const TACInstruction *instruction, const char *result, const char *firstOperand, const char *secondOperand);
static void formatExpressionInstruction(const TACInstruction *instruction, const char *result, const char *firstOperand, const char *secondOperand);
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
    if (instruction == NULL) return;

    const char *result         = getOperandName(&instruction->result);
    const char *firstOperand   = getOperandName(&instruction->firstOperand);
    const char *secondOperand  = getOperandName(&instruction->secondOperand);

    switch (instruction->operation) {
        case TAC_BEGIN_FUNC:
        case TAC_END_FUNC:
        case TAC_PARAM_DECL:
        case TAC_GLOBAL_DECL:
        case TAC_LOCAL_DECL:
            formatDeclarationInstruction(instruction, result, firstOperand);
            break;

        case TAC_LABEL:
        case TAC_GOTO:
        case TAC_IF_FALSE:
        case TAC_IF_TRUE:
            formatControlFlowInstruction(instruction, result, firstOperand, secondOperand);
            break;

        case TAC_PARAM:
        case TAC_CALL:
        case TAC_RETURN:
            formatCallOrReturnInstruction(instruction, result, firstOperand, secondOperand);
            break;

        default:
            formatExpressionInstruction(instruction, result, firstOperand, secondOperand);
            break;
    }
}

static void formatDeclarationInstruction(const TACInstruction *instruction, const char *result, const char *firstOperand) {
    switch (instruction->operation) {
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

        default:
            break;
    }
}

static void formatControlFlowInstruction(const TACInstruction *instruction, const char *result, const char *firstOperand, const char *secondOperand) {
    switch (instruction->operation) {
        case TAC_LABEL:
            buildStringf("%s:\n", result);
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

        default:
            break;
    }
}

static void formatCallOrReturnInstruction(const TACInstruction *instruction, const char *result, const char *firstOperand, const char *secondOperand) {
    switch (instruction->operation) {
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

        default:
            break;
    }
}

static void formatExpressionInstruction(const TACInstruction *instruction, const char *result, const char *firstOperand, const char *secondOperand) {
    switch (instruction->operation) {
        case TAC_ASSIGN:
            buildStringf("%s = %s\n", result, firstOperand);
            break;

        case TAC_UMINUS:
        case TAC_NOT:
            buildStringf("%s = %s%s\n", result, getOperatorSymbol(instruction->operation), firstOperand);
            break;

        case TAC_INT_TO_FLOAT:
            buildStringf("%s = int_to_float %s\n", result, firstOperand);
            break;

        default:
            buildStringf("%s = %s %s %s\n", result, firstOperand, getOperatorSymbol(instruction->operation), secondOperand);
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
