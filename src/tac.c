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
    TACOperand operand = makeOperand(OPERAND_CONSTANT, TYPE_INT, NULL);
    operand.value.int_val = value;

    return operand;
}

TACOperand tacConstantBool(int value) {
    TACOperand operand = makeOperand(OPERAND_CONSTANT, TYPE_BOOL, NULL);
    operand.value.int_val = value;

    return operand;
}

TACOperand tacConstantFloat(float value) {
    TACOperand operand = makeOperand(OPERAND_CONSTANT, TYPE_FLOAT, NULL);
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

static void printOperand(const TACOperand *operand) {
    if (operand->kind != OPERAND_CONSTANT) {
        buildStringf("%s", operand->name);
        return;
    }

    switch (operand->type) {
        case TYPE_INT:
            buildStringf("%d", operand->value.int_val);
            break;
        case TYPE_BOOL:
            buildStringf("%s", operand->value.int_val ? "true" : "false");
            break;
        case TYPE_FLOAT:
            buildStringf("%f", operand->value.float_val);
            break;
        default:
            break;
    }
}

static void printInstruction(const TACInstruction *instruction) {
    switch (instruction->op) {
        case TAC_LABEL:
            printOperand(&instruction->result);
            buildStringf(":\n");
            break;

        case TAC_BEGIN_FUNC:
            buildStringf("begin_func ");
            printOperand(&instruction->result);
            buildStringf(", ");
            printOperand(&instruction->arg1);
            buildStringf("\n");
            break;

        case TAC_END_FUNC:
            buildStringf("end_func ");
            printOperand(&instruction->result);
            buildStringf("\n");
            break;

        case TAC_PARAM_DECL:
            buildStringf("param_decl ");
            printOperand(&instruction->arg1);
            buildStringf("\n");
            break;

        case TAC_GLOBAL_DECL:
        case TAC_LOCAL_DECL:
            buildStringf(instruction->op == TAC_GLOBAL_DECL ? "global " : "local ");
            printOperand(&instruction->arg1);
            buildStringf(", %s\n", getDataTypeName(instruction->arg1.type));
            break;

        case TAC_ASSIGN:
            printOperand(&instruction->result);
            buildStringf(" = ");
            printOperand(&instruction->arg1);
            buildStringf("\n");
            break;

        case TAC_NEG:
        case TAC_NOT:
            printOperand(&instruction->result);
            buildStringf(" = %s", getOperatorSymbol(instruction->op));
            printOperand(&instruction->arg1);
            buildStringf("\n");
            break;

        case TAC_INT_TO_FLOAT:
            printOperand(&instruction->result);
            buildStringf(" = int_to_float ");
            printOperand(&instruction->arg1);
            buildStringf("\n");
            break;

        case TAC_GOTO:
            buildStringf("goto ");
            printOperand(&instruction->arg1);
            buildStringf("\n");
            break;

        case TAC_IF_FALSE:
        case TAC_IF_TRUE:
            buildStringf("if ");
            printOperand(&instruction->arg1);
            buildStringf(instruction->op == TAC_IF_FALSE ? " false goto " : " true goto ");
            printOperand(&instruction->arg2);
            buildStringf("\n");
            break;

        case TAC_PARAM:
            buildStringf("param ");
            printOperand(&instruction->arg1);
            buildStringf("\n");
            break;

        case TAC_CALL:
            // El resultado es opcional: una llamada como sentencia lo descarta.
            if (instruction->result.kind != OPERAND_NONE) {
                printOperand(&instruction->result);
                buildStringf(" = ");
            }
            buildStringf("call ");
            printOperand(&instruction->arg1);
            buildStringf(", ");
            printOperand(&instruction->arg2);
            buildStringf("\n");
            break;

        case TAC_RETURN:
            buildStringf("ret");
            if (instruction->arg1.kind != OPERAND_NONE) {
                buildStringf(" ");
                printOperand(&instruction->arg1);
            }
            buildStringf("\n");
            break;

        default:
            /* Binarias: result = arg1 <op> arg2 */
            printOperand(&instruction->result);
            buildStringf(" = ");
            printOperand(&instruction->arg1);
            buildStringf(" %s ", getOperatorSymbol(instruction->op));
            printOperand(&instruction->arg2);
            buildStringf("\n");
            break;
    }
}

void printTACList(const TACList *list) {
    if (list == NULL) return;

    for (const TACInstruction *current = list->head; current != NULL; current = current->next) {
        printInstruction(current);
    }
}
