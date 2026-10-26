#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tac.h"
#include "string_builder.h"

/* Constructores de operandos */

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
    operand.value.int_val = value;

    return operand;
}

TACOperand tacConstantBool(int value) {
    TACOperand operand = makeOperand(OPERAND_CONSTANT, TYPE_BOOL, value ? "true" : "false");
    operand.value.int_val = value;

    return operand;
}

TACOperand tacConstantFloat(float value) {
    char str[TAC_NAME_LENGTH];
    snprintf(str, sizeof(str), "%f", value);
    TACOperand operand = makeOperand(OPERAND_CONSTANT, TYPE_FLOAT, str);
    operand.value.float_val = value;

    return operand;
}

TACOperand tacLabel(const char *name) {
    return makeOperand(OPERAND_LABEL, TYPE_VOID, name);
}

TACOperand tacMethod(const char *name, DataType returnType) {
    return makeOperand(OPERAND_METHOD, returnType, name);
}

/* Lista de instrucciones */

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
        free(toFree);
    }

    free(list);
}

TACInstruction *appendInstruction(TACList *list, TACOp op, TACOperand result, TACOperand arg1, TACOperand arg2) {
    if (list == NULL) return NULL;

    TACInstruction *instruction = malloc(sizeof(TACInstruction));
    if (instruction == NULL) return NULL;

    instruction->op = op;
    instruction->result = result;
    instruction->arg1 = arg1;
    instruction->arg2 = arg2;
    instruction->next = NULL;

    // El puntero tail evita recorrer la lista entera en cada emisión.
    if (list->tail == NULL) {
        list->head = instruction;
    } else {
        list->tail->next = instruction;
    }
    list->tail = instruction;
    list->count++;

    return instruction;
}

/* Salida como texto */

/* Símbolo textual de las operaciones que se imprimen de forma infija. */
static const char *getOperatorSymbol(TACOp op) {
    switch (op) {
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

static const char *getOperand(const TACOperand *operand) {
    return (operand != NULL) ? operand->name : "";
}

static void printInstruction(const TACInstruction *instruction) {
    const char *res  = getOperand(&instruction->result);
    const char *arg1 = getOperand(&instruction->arg1);
    const char *arg2 = getOperand(&instruction->arg2);

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

        default:
            /* Binarias: result = arg1 <op> arg2 */
            buildStringf("%s = %s %s %s\n", res, arg1, getOperatorSymbol(instruction->op), arg2);
            break;
    }
}

void printTACList(const TACList *list) {
    if (list == NULL) return;

    for (const TACInstruction *current = list->head; current != NULL; current = current->next) {
        printInstruction(current);
    }
}
