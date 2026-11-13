#include "type_checker.h"

#include <stdbool.h>
#include <stddef.h>

#include "error_handler.h"

static DataType analyzeIdentifier(NodeAST *node, Scope *scope);
static DataType analyzeOperator(NodeAST *node, Scope *scope);
static DataType resolveOperation(OperationType operator, DataType left, DataType right);
static DataType resolveNumericOperation(DataType left, DataType right);
static bool isNumeric(DataType type);
static bool isBasicType(DataType type);

DataType analyzeExpression(NodeAST *node, Scope *scope) {
    if (node == NULL) {
        return TYPE_ERROR;
    }

    switch (node->nodeType) {
        case ID_NODE:
            return analyzeIdentifier(node, scope);

        case CONSTANT_NODE:
            node->type = node->symbol->type;
            return node->type;

        case METHOD_CALL_NODE:
            return analyzeMethodCall(node, scope, true);

        case OPERATOR_NODE:
            return analyzeOperator(node, scope);

        default:
            return TYPE_ERROR;
    }
}

DataType analyzeMethodCall(NodeAST *node, Scope *scope, bool isExpression) {
    Symbol *method = lookupSymbol(scope, node->symbol->id);

    if (method == NULL) {
        semanticError(node->line, "Método no declarado: %s", node->symbol->id);
        return TYPE_ERROR;
    }

    if (method->symbolType != METHOD_SYMBOL) {
        semanticError(node->line, "El simbolo encontrado pertenece a una variable no a una llamada de un metodo");
        return TYPE_ERROR;
    }


    if (isExpression && method->type == TYPE_VOID) {
        semanticError(node->line, "El metodo '%s' es void y no puede usarse como expresion.", node->symbol->id);
        return TYPE_ERROR;
    }
    

    NodeAST *parametersNode = method->parameters;
    int expectedArgumentCount = parametersNode->childCount;

    if (node->childCount != expectedArgumentCount) {
        semanticError(node->line, "Cantidad de argumentos incorrecta. Se esperaban %d pero se pasaron %d.", expectedArgumentCount, node->childCount);
        return TYPE_ERROR;
    }

    for (int i = 0; i < node->childCount; i++) {
        DataType actual = analyzeExpression(node->children[i], scope);
        DataType expected = parametersNode->children[i]->type;

        // Evita reportar error en cascada si el argumento ya fallo previamente.
        if (actual == TYPE_ERROR) continue;

        if (!isAssignable(expected, actual)) {
            semanticError(node->line, "Parámetro incompatible en la posición %d", i + 1);
        }
    }

    node->symbol = method;
    node->type = method->type;

    return method->type;
}

bool isAssignable(DataType expected, DataType actual) {
    if (expected == actual) return true;
    if (expected == TYPE_FLOAT && actual == TYPE_INT) return true;
    return false;
}

static DataType analyzeIdentifier(NodeAST *node, Scope *scope) {
    Symbol *symbol = lookupSymbol(scope, node->symbol->id);

    if (symbol == NULL) {
        semanticError(node->line, "Variable no declarada: %s", node->symbol->id);
        node->type = TYPE_ERROR;
        return TYPE_ERROR;
    }

    if (symbol->symbolType != VARIABLE_SYMBOL) {
        semanticError(node->line, "El identificador '%s' no es una variable.", node->symbol->id);
        node->type = TYPE_ERROR;
        return TYPE_ERROR;
    }

    node->symbol = symbol;
    node->type = symbol->type;

    return symbol->type;
}

static DataType analyzeOperator(NodeAST *node, Scope *scope) {
    DataType leftType = analyzeExpression(GET_LEFT(node), scope);
    NodeAST *rightNode = GET_RIGHT(node);
    DataType rightType = (rightNode != NULL) ? analyzeExpression(rightNode, scope) : TYPE_VOID;

    // Evita reportar error en cascada si algun operando ya fallo previamente.
    if (leftType == TYPE_ERROR || rightType == TYPE_ERROR) {
        node->type = TYPE_ERROR;
        return TYPE_ERROR;
    }

    DataType resultType = resolveOperation(node->operationType, leftType, rightType);

    if (resultType == TYPE_ERROR) {
        semanticError(node->line, "Tipos incompatibles en la operacion %s.",
                      (rightNode != NULL) ? "binaria" : "unaria");
    }

    node->type = resultType;
    return resultType;
}

static DataType resolveOperation(OperationType operator, DataType left, DataType right) {
    switch (operator) {
        case OP_ADD:
        case OP_SUB:
        case OP_MUL:
        case OP_DIV:
            return resolveNumericOperation(left, right);

        // division solo aplica a enteros, cualquier otra combinacion provoca un fallo
        case OP_MOD:
            if (left == TYPE_INT && right == TYPE_INT)
                return TYPE_INT;
            return TYPE_ERROR;

        case OP_LT:
        case OP_GT:
            if (isNumeric(left) && isNumeric(right))
                return TYPE_BOOL;
            return TYPE_ERROR;
                    
        case OP_EQ:
            if (left == right && isBasicType(left))
                return TYPE_BOOL;

            return TYPE_ERROR;

        case OP_AND:
        case OP_OR:
            if (left == TYPE_BOOL && right == TYPE_BOOL)
                return TYPE_BOOL;
            return TYPE_ERROR;

        case OP_UMINUS:
            if (isNumeric(left))
                return left;
            return TYPE_ERROR;

        case OP_NOT:
            if (left == TYPE_BOOL)
                return TYPE_BOOL;
            return TYPE_ERROR;

        default:
            return TYPE_ERROR;
    }
}

static DataType resolveNumericOperation(DataType left, DataType right) {
    if (!isNumeric(left) || !isNumeric(right))
        return TYPE_ERROR;

    bool coalesceToFloat = (left == TYPE_FLOAT || right == TYPE_FLOAT);
    if (coalesceToFloat) {
        return TYPE_FLOAT;
    }
    return TYPE_INT;
}

static bool isNumeric(DataType type) {
    return type == TYPE_INT || type == TYPE_FLOAT;
}

static bool isBasicType(DataType type) {
    return type == TYPE_INT || type == TYPE_FLOAT || type == TYPE_BOOL;
}
