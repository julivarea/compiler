#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "intermediate_code.h"
#include "tac.h"

/* Lista que se está construyendo. Es estado de módulo para que el recorrido
 * del AST no tenga que arrastrarla por cada llamada recursiva. */
static TACList *current_code = NULL;

static int temp_count = 0;
static int label_count = 0;

/* Los temporales y las etiquetas se devuelven ya envueltos en un TACOperand,
 * que copia el nombre a un buffer propio. Así no hay punteros a buffers
 * estáticos compartidos ni cadenas reservadas con strdup que haya que liberar. */
static TACOperand newTemp(DataType type) {
    char name[TAC_NAME_LENGTH];

    snprintf(name, sizeof(name), "t%d", temp_count++);
    return tacTemp(name, type);
}

static TACOperand newLabel(void) {
    char name[TAC_NAME_LENGTH];

    snprintf(name, sizeof(name), "L%d", label_count++);
    return tacLabel(name);
}

static void emit(TACOp op, TACOperand result, TACOperand arg1, TACOperand arg2) {
    appendInstruction(current_code, op, result, arg1, arg2);
}

static TACOp getTACOpForOperation(OperationType operation) {
    switch (operation) {
        case OP_ADD:      return TAC_ADD;
        case OP_SUB:      return TAC_SUB;
        case OP_MUL:      return TAC_MUL;
        case OP_DIV:      return TAC_DIV;
        case OP_MOD:      return TAC_MOD;
        case OP_LT:       return TAC_LT;
        case OP_GT:       return TAC_GT;
        case OP_EQ:       return TAC_EQ;
        case OP_AND:      return TAC_AND;
        case OP_OR:       return TAC_OR;
        case OP_NEGATIVE: return TAC_NEG;
        case OP_NEGATION: return TAC_NOT;
        default:          return TAC_ASSIGN;
    }
}

/*
 * Emite los argumentos de una llamada como instrucciones 'param', de izquierda
 * a derecha segun exige la letra. Lo comparten la llamada como expresion y la
 * llamada como sentencia, que solo se diferencian en si el resultado se guarda.
 */
static void generateCallArguments(NodeAST *node) {
    for (int i = 0; i < node->childCount; i++) {
        TACOperand argument = generateExpressionCode(node->children[i]);
        emit(TAC_PARAM, tacNone(), argument, tacNone());
    }
}

/*
 * && y || no se traducen a una instruccion binaria sino a control de flujo,
 * porque la letra exige evaluacion de corto circuito: el operando derecho no
 * se evalua si el izquierdo ya determina el resultado de toda la expresion.
 *
 *   result = a && b              result = a || b
 *   --------------------         --------------------
 *   result = a                   result = a
 *   if result false goto L       if result true goto L
 *   result = b                   result = b
 *   L:                           L:
 *
 * La idea en los dos casos es la misma: si el operando izquierdo ya decide,
 * el valor que corresponde devolver es el suyo, que ya quedo en result, y el
 * salto saltea la evaluacion del derecho. Si no decide, el resultado de la
 * expresion completa es exactamente el del operando derecho.
 *
 * El resultado va a un temporal nuevo y no se reutiliza el operando izquierdo
 * porque este puede ser una variable del programa, y sobreescribirla cambiaria
 * el valor de la variable. La copia extra que eso introduce es candidata a
 * eliminarse en la etapa de optimizacion.
 */
static TACOperand generateShortCircuitCode(NodeAST *node) {
    TACOperand left = generateExpressionCode(GET_LEFT(node));
    TACOperand result = newTemp(node->type);
    TACOperand endLabel = newLabel();

    emit(TAC_ASSIGN, result, left, tacNone());

    TACOp shortCircuitJump = (node->operationType == OP_AND) ? TAC_IF_FALSE : TAC_IF_TRUE;
    emit(shortCircuitJump, tacNone(), result, endLabel);

    TACOperand right = generateExpressionCode(GET_RIGHT(node));
    emit(TAC_ASSIGN, result, right, tacNone());

    emit(TAC_LABEL, endLabel, tacNone(), tacNone());

    return result;
}

static TACOperand generateConstantOperand(NodeAST *node) {
    if (node->symbol == NULL) return tacNone();

    switch (node->symbol->type) {
        case TYPE_INT:
            return tacConstantInt(node->symbol->value.int_val);
        case TYPE_BOOL:
            return tacConstantBool(node->symbol->value.int_val);
        case TYPE_FLOAT:
            return tacConstantFloat(node->symbol->value.float_val);
        default:
            return tacNone();
    }
}

TACOperand generateExpressionCode(NodeAST *node) {
    if (node == NULL) {
        return tacNone();
    }

    switch (node->nodeType) {
        case ID_NODE:
            return tacVariable(node->symbol);

        case CONSTANT_NODE:
            return generateConstantOperand(node);

        case BINARYOPERATOR_NODE: {
            if (node->operationType == OP_AND || node->operationType == OP_OR) {
                return generateShortCircuitCode(node);
            }

            TACOperand left = generateExpressionCode(GET_LEFT(node));
            TACOperand result = newTemp(node->type);

            if (node->operationType == OP_NEGATIVE || node->operationType == OP_NEGATION) {
                emit(getTACOpForOperation(node->operationType), result, left, tacNone());
            } else {
                TACOperand right = generateExpressionCode(GET_RIGHT(node));
                emit(getTACOpForOperation(node->operationType), result, left, right);
            }

            return result;
        }

        case METHOD_CALL_NODE: {
            generateCallArguments(node);
            if (node->symbol == NULL || node->symbol->id == NULL) {
                return tacNone();
            }

            // El temporal se pide despues de los argumentos, asi su numero
            // queda despues de los temporales que estos hayan consumido.
            TACOperand result = newTemp(node->type);
            emit(TAC_CALL, result, tacMethod(node->symbol->id, node->type), tacConstantInt(node->childCount));

            return result;
        }

        default:
            return tacNone();
    }
}

void generateStatementCode(NodeAST *node) {
    if (node == NULL) {
        return;
    }

    switch (node->nodeType) {
        case METHOD_DECLARATION_NODE:
            if (node->symbol && node->symbol->id) {
                emit(TAC_BEGIN_FUNC, tacMethod(node->symbol->id, node->type), tacNone(), tacNone());
            }
            if (node->childCount > 1 && node->children[1] != NULL) {
                generateStatementCode(node->children[1]);
            }
            break;

        case ASSIGNMENT_NODE:
            if (node->symbol && node->symbol->id) {
                TACOperand value = generateExpressionCode(GET_RIGHT(node));
                emit(TAC_ASSIGN, tacVariable(node->symbol), value, tacNone());
            }
            break;

        // Una llamada como sentencia genera lo mismo que como expresion, pero
        // sin temporal destino: el resultado, si lo hay, se descarta.
        case METHOD_CALL_NODE:
            generateCallArguments(node);
            if (node->symbol && node->symbol->id) {
                emit(TAC_CALL, tacNone(), tacMethod(node->symbol->id, node->type), tacConstantInt(node->childCount));
            }
            break;

        case RETURN_NODE: {
            NodeAST *expression = GET_EXPRESSION(node);
            TACOperand value = (expression == NULL) ? tacNone() : generateExpressionCode(expression);

            emit(TAC_RETURN, tacNone(), value, tacNone());
            break;
        }

        case IF_ELSE_NODE: {
            TACOperand condition = generateExpressionCode(GET_CONDITION(node));
            NodeAST *elseBlock = GET_ELSE_BLOCK(node);

            if (elseBlock == NULL) {
                // Sin rama else, la condicion falsa saltea el bloque entero.
                TACOperand endIf = newLabel();

                emit(TAC_IF_FALSE, tacNone(), condition, endIf);
                generateStatementCode(GET_IF_BLOCK(node));
                emit(TAC_LABEL, endIf, tacNone(), tacNone());
            } else {
                TACOperand elseIf = newLabel();
                TACOperand endIf = newLabel();

                emit(TAC_IF_FALSE, tacNone(), condition, elseIf);
                generateStatementCode(GET_IF_BLOCK(node));
                emit(TAC_GOTO, tacNone(), endIf, tacNone());
                emit(TAC_LABEL, elseIf, tacNone(), tacNone());
                generateStatementCode(elseBlock);
                emit(TAC_LABEL, endIf, tacNone(), tacNone());
            }
            break;
        }

        case WHILE_NODE: {
            TACOperand startOfWhile = newLabel();
            TACOperand endOfWhile = newLabel();

            emit(TAC_LABEL, startOfWhile, tacNone(), tacNone());

            TACOperand condition = generateExpressionCode(GET_CONDITION(node));
            emit(TAC_IF_FALSE, tacNone(), condition, endOfWhile);

            generateStatementCode(GET_WHILE_BLOCK(node));

            emit(TAC_GOTO, tacNone(), startOfWhile, tacNone());
            emit(TAC_LABEL, endOfWhile, tacNone(), tacNone());
            break;
        }

        case BLOCK_NODE:
        default:
            for (int i = 0; i < node->childCount; i++) {
                generateStatementCode(node->children[i]);
            }
            break;
    }
}

void generateIntermediateCode(NodeAST *root) {
    if (root == NULL) {
        return;
    }

    freeTACList(current_code);
    current_code = createTACList();
    temp_count = 0;
    label_count = 0;

    for (int i = 0; i < root->childCount; i++) {
        generateStatementCode(root->children[i]);
    }
}

const TACList *getIntermediateCode(void) {
    return current_code;
}
