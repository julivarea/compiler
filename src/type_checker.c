#include <stdio.h>
#include <string.h>
#include "type_checker.h"
#include "error_handler.h"

int isAssignable(DataType expected, DataType actual) {
    if (expected == actual) return 1;
    if (expected == TYPE_FLOAT && actual == TYPE_INT) return 1;
    return 0;
}
static int isNumeric(DataType type) {
    return type == TYPE_INT || type == TYPE_FLOAT;
}

static DataType resolveNumericOperation(DataType left, DataType right) {
    if (!isNumeric(left) || !isNumeric(right))
        return TYPE_ERROR;

    return (left == TYPE_FLOAT || right == TYPE_FLOAT)
        ? TYPE_FLOAT
        : TYPE_INT;
}

DataType resolveBinaryOperation(
    OperationType operator,
    DataType left,
    DataType right
) {
    switch (operator) {
        case OP_ADD:
        case OP_SUB:
        case OP_MUL:
        case OP_DIV:
            return resolveNumericOperation(left, right);

        case OP_MOD:
            if (isNumeric(left) && isNumeric(right))
                return TYPE_INT;
            return TYPE_ERROR;

        case OP_LT:
        case OP_GT:
            if (isNumeric(left) && isNumeric(right))
                return TYPE_BOOL;
            return TYPE_ERROR;

        case OP_EQ:
            if (left == right)
                return TYPE_BOOL;

            if (isNumeric(left) && isNumeric(right))
                return TYPE_BOOL;

            return TYPE_ERROR;

        case OP_AND:
        case OP_OR:
            if (left == TYPE_BOOL && right == TYPE_BOOL)
                return TYPE_BOOL;
            return TYPE_ERROR;

        case OP_NEGATIVE:
            if (left == TYPE_INT || left == TYPE_FLOAT)
                return left;
            return TYPE_ERROR;

        case OP_NEGATION:
            if (left == TYPE_BOOL)
                return TYPE_BOOL;
            return TYPE_ERROR;

        default:
            return TYPE_ERROR;
    }
}

DataType analyzeIdentifier(NodeAST *node, Scope *scope) {
    Symbol *symbol = lookupVariable(scope, node->symbol->id);

    if (symbol == NULL) {
        semanticError(node->line, "Variable no declarada: %s", node->symbol->id);
        node->type = TYPE_ERROR;
        return TYPE_ERROR;
    }

    node->symbol = symbol;
    node->type = symbol->type;

    return symbol->type;
}

DataType analyzeMethodCall(NodeAST *node, Scope *scope, int isExpression) {
    Symbol *method = lookupVariable(scope, node->symbol->id);

    if (method == NULL) {
        semanticError(node->line, "Método no declarado: %s", node->symbol->id);
        return TYPE_ERROR;
    }

    if (isExpression && method->type == TYPE_VOID) {
        semanticError(node->line, "El metodo '%s' es void y no puede usarse como expresion.", node->symbol->id);
        return TYPE_ERROR;
    }
    
    if(method->symbolType =! METHOD_SYMBOL){
        semanticError(node->line, "El simbolo encontrado pertenece a una variable no a una llamada de un metodo");
        return TYPE_ERROR;
    }

    NodeAST *parametersNode = method->parameters;
    int expectedArgCount = parametersNode ? parametersNode->childCount : 0;

    if (node->childCount != expectedArgCount) {
        semanticError(node->line, "Cantidad de argumentos incorrecta. Se esperaban %d pero se pasaron %d.", expectedArgCount, node->childCount);
        return TYPE_ERROR;
    }

    for (int i = 0; i < node->childCount; i++) {
        DataType actual = analyzeExpression(node->children[i], scope);
        DataType expected = parametersNode->children[i]->type;

        if (!isAssignable(expected, actual)) {
            semanticError(node->line, "Parámetro incompatible en la posición %d", i + 1);
        }
    }

    node->type = method->type;
    return method->type;
}

DataType analyzeBinaryOperator(NodeAST *node, Scope *scope) {
    DataType leftType = analyzeExpression(GET_LEFT(node), scope); //
    DataType rightType = TYPE_VOID; // default
    
    if (GET_RIGHT(node) != NULL) {
        rightType = analyzeExpression(GET_RIGHT(node), scope);
    }

    DataType resultType = resolveBinaryOperation(
        node->operationType,
        leftType,
        rightType
    );

    if (resultType == TYPE_ERROR) {
        semanticError(node->line, "Tipos incompatibles en la operacion binaria.");
    }

    node->type = resultType;
    return resultType;
}

DataType analyzeExpression(NodeAST *node, Scope *scope) {
    if (node == NULL)
        return TYPE_ERROR;

    switch (node->nodeType) {
        case ID_NODE:
            return analyzeIdentifier(node, scope);

        case CONSTANT_NODE:
            node->type = node->symbol->type;
            return node->type;

        case METHOD_CALL_NODE:
            return analyzeMethodCall(node, scope, 1); // 1: expresion

        case BINARYOPERATOR_NODE:
            return analyzeBinaryOperator(node, scope);

        default:
            return TYPE_ERROR;
    }
}
