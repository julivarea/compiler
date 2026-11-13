#include "tac.h"
#include "string_builder.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static TACOperand makeOperand(OperandKind kind, DataType type, const char *name) {
    TACOperand operand;
    memset(&operand, 0, sizeof(operand));

    operand.kind = kind;
    operand.type = type;
    if (name != NULL) {
        int written = snprintf(operand.name, sizeof(operand.name), "%s", name);
        if (written < 0 || (size_t)written >= sizeof(operand.name)) {
            fprintf(stderr,
                    "\033[1;31mError interno del TAC:\033[0m el nombre del operando '%s' excede el limite de %zu caracteres.\n",
                    name, sizeof(operand.name) - 1);
        }
    }

    return operand;
}

TACOperand tacNone(void) {
    return makeOperand(OPERAND_NONE, TYPE_VOID, NULL);
}

TACOperand tacTemp(const char *name, DataType type) {
    return makeOperand(OPERAND_TEMP, type, name);
}

TACOperand tacVariable(Symbol *symbol) {
    if (symbol == NULL) {
        return makeOperand(OPERAND_VARIABLE, TYPE_ERROR, NULL);
    }

    const char *id = (symbol->id != NULL) ? symbol->id : "";
    char qualifiedName[TAC_NAME_LENGTH];
    int written;
    if (symbol->uid != 0) {
        written = snprintf(qualifiedName, sizeof(qualifiedName), "%s@%d", id, symbol->uid);
    } else {
        written = snprintf(qualifiedName, sizeof(qualifiedName), "%s", id);
    }

    // snprintf retorna la longitud necesaria: si written >= sizeof, el nombre fue truncado y perdio su sufijo @uid.
    if (written < 0 || (size_t)written >= sizeof(qualifiedName)) {
        fprintf(stderr,
                "\033[1;31mError interno del TAC:\033[0m el identificador calificado para '%s' excede el limite de %zu caracteres.\n",
                id, sizeof(qualifiedName) - 1);
    }

    TACOperand operand = makeOperand(OPERAND_VARIABLE, symbol->type, qualifiedName);
    operand.symbol = symbol;

    return operand;
}

TACOperand tacConstantInt(int value) {
    char str[TAC_NAME_LENGTH];
    snprintf(str, sizeof(str), "%d", value);
    TACOperand operand = makeOperand(OPERAND_CONSTANT, TYPE_INT, str);
    operand.value.intValue = value;

    return operand;
}

TACOperand tacConstantBool(int value) {
    TACOperand operand = makeOperand(OPERAND_CONSTANT, TYPE_BOOL, value ? "true" : "false");
    operand.value.intValue = value;

    return operand;
}

TACOperand tacLabel(const char *name) {
    return makeOperand(OPERAND_LABEL, TYPE_VOID, name);
}

TACOperand tacMethod(const char *name, DataType returnType) {
    return makeOperand(OPERAND_METHOD, returnType, name);
}

const char *getOperandName(const TACOperand *operand) {
    if (operand == NULL || operand->kind == OPERAND_NONE) {
        return "";
    }
    return operand->name;
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
    if (buffer == NULL || size == 0) return;

    buffer[0] = '\0';
    if (instruction == NULL) return;

    switch (instruction->operation) {
        case TAC_BEGIN_FUNC:
            snprintf(buffer, size, "begin_func %s, %s\n",
                     getOperandName(&instruction->result), getOperandName(&instruction->firstOperand));
            break;

        case TAC_END_FUNC:
            snprintf(buffer, size, "end_func %s\n", getOperandName(&instruction->result));
            break;

        case TAC_PARAM_DECL:
            snprintf(buffer, size, "param_decl %s, %s\n",
                     getOperandName(&instruction->result),
                     getDataTypeName(instruction->result.type));
            break;

        case TAC_GLOBAL_DECL:
        case TAC_LOCAL_DECL:
            snprintf(buffer, size, "%s %s, %s\n",
                     instruction->operation == TAC_GLOBAL_DECL ? "global" : "local",
                     getOperandName(&instruction->result),
                     getDataTypeName(instruction->result.type));
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

        default: /* operador binario: result = A sym B */
            snprintf(buffer, size, "%s = %s %s %s\n",
                     getOperandName(&instruction->result),
                     getOperandName(&instruction->firstOperand),
                     getOperatorSymbol(instruction->operation),
                     getOperandName(&instruction->secondOperand));
            break;
    }
}

TACList *createTACList(void) {
    TACList *list = malloc(sizeof(TACList));
    if (list == NULL) return NULL;

    list->head = NULL;
    list->tail = NULL;
    list->count = 0;

    return list;
}

void freeTACList(TACList *list) {
    if (list == NULL) return;

    TACInstruction *current = list->head;
    while (current != NULL) {
        TACInstruction *toFree = current;
        current = current->next;
        freeTACInstruction(toFree);
    }

    free(list);
}

void formatTACList(const TACList *list) {
    if (list == NULL) return;

    for (const TACInstruction *current = list->head; current != NULL; current = current->next) {
        printTACInstruction(current);
    }
}

TACInstruction *appendTACInstruction(TACList *list, TACInstruction *instruction) {
    if (list == NULL || instruction == NULL) return NULL;

    instruction->next = NULL;
    if (list->tail == NULL) {
        list->head = instruction;
    } else {
        list->tail->next = instruction;
    }
    list->tail = instruction;
    list->count++;

    return instruction;
}

TACInstruction *appendInstruction(TACList *list, TACOperation operation, TACOperand result, TACOperand firstOperand, TACOperand secondOperand) {
    if (list == NULL) return NULL;

    TACInstruction *instruction = createTACInstruction(operation, result, firstOperand, secondOperand);
    if (instruction == NULL) return NULL;

    return appendTACInstruction(list, instruction);
}
