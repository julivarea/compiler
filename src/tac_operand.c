#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tac_operand.h"

static TACOperand makeOperand(OperandKind kind, DataType type, const char *name) {
    TACOperand operand;
    memset(&operand, 0, sizeof(operand));

    operand.kind = kind;
    operand.type = type;
    if (name != NULL) {
        int written = snprintf(operand.name, sizeof(operand.name), "%s", name);
        // Si written >= sizeof, la cadena no cupo en el buffer y fue truncada por snprintf.
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
