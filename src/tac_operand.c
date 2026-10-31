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
        snprintf(operand.name, sizeof(operand.name), "%s", name);
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

    /* El uid identifica a la entidad declarada, no al nombre: dos variables que
     * se llaman igual en scopes distintos tienen uid distinto, así que el
     * nombre calificado las separa.
     *
     * El separador es '@' porque no puede aparecer en un identificador de
     * C-TDS: al leer el TAC se ve de inmediato que el sufijo lo puso el
     * compilador y no es parte del nombre que escribió el programador. Con '_'
     * no habría ambigüedad real (el uid se agrega siempre, así que un `x` y un
     * `x_3` del programa nunca producen la misma cadena), pero `x_3` se lee
     * como si fuera un identificador del código fuente.
     *
     * uid == 0 significa que el símbolo no quedó declarado en ningún scope, lo
     * que no debería ocurrir si el análisis semántico terminó sin errores. */
    if (symbol->uid != 0) {
        snprintf(qualifiedName, sizeof(qualifiedName), "%s@%d", id, symbol->uid);
    } else {
        snprintf(qualifiedName, sizeof(qualifiedName), "%s", id);
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

TACOperand tacConstantFloat(float value) {
    char str[TAC_NAME_LENGTH];
    snprintf(str, sizeof(str), "%f", value);
    TACOperand operand = makeOperand(OPERAND_CONSTANT, TYPE_FLOAT, str);
    operand.value.floatValue = value;

    return operand;
}

TACOperand tacLabel(const char *name) {
    return makeOperand(OPERAND_LABEL, TYPE_VOID, name);
}

TACOperand tacMethod(const char *name, DataType returnType) {
    return makeOperand(OPERAND_METHOD, returnType, name);
}

const char *getOperandName(const TACOperand *operand) {
    return (operand != NULL) ? operand->name : "";
}
