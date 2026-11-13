#include "ast.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser_helpers.h"
#include "symbol_table.h"

extern int yylineno;

static NodeAST *newConstantNode(DataType type);
static NodeAST *newSymbolNode(NodeType nodeType, SymbolType symbolType, DataType dataType, char *id, NodeAST *parameters, NodeAST *left, NodeAST *mid);
static void resolveNodeArray(NodeAST *parent, NodeArray *array);
static void describeNode(FILE *out, const NodeAST *node);
static const char *getOperationName(OperationType operation);

NodeAST *newNode(NodeType nodeType, Symbol *symbol, NodeAST *left, NodeAST *mid, NodeAST *right) {
    NodeAST *node = malloc(sizeof(NodeAST));
    if (node == NULL) return NULL;

    node->nodeType = nodeType;
    node->symbol = symbol;
    node->type = TYPE_VOID;
    node->line = yylineno;

    // if/while/method se reducen tras su bloque: usar la línea del encabezado, no la del '}'.
    if ((nodeType == IF_ELSE_NODE || nodeType == WHILE_NODE || nodeType == METHOD_DECLARATION_NODE) && left != NULL) {
        node->line = left->line;
    }

    int count = 0;
    NodeAST *temp[3];
    if (left) temp[count++] = left;
    if (mid) temp[count++] = mid;
    if (right) temp[count++] = right;

    if (count > 0) {
        node->children = malloc(count * sizeof(NodeAST *));
        // si no hay memoria para alguno de los hijos, se libera el nodo, y se devuelve null
        if (node->children == NULL) {
            free(node);
            return NULL;
        }

        for (int i = 0; i < count; i++) {
            node->children[i] = temp[i];
        }
        node->childCount = count;
    } else {
        node->children = NULL;
        node->childCount = 0;
    }

    return node;
}

NodeAST *newOperatorNode(OperationType operator, NodeAST *left, NodeAST *right) {
    NodeAST *node = newNode(OPERATOR_NODE, NULL, left, right, NULL);
    if (node == NULL) {
        freeAST(left);
        freeAST(right);
        return NULL;
    }
    node->operationType = operator;
    return node;
}

NodeAST *newMethodDeclaration(DataType returnType, char *methodName, NodeAST *parameters, NodeAST *body) {
    return newSymbolNode(METHOD_DECLARATION_NODE, METHOD_SYMBOL, returnType, methodName, parameters, parameters, body);
}

NodeAST *newParameterDeclaration(DataType type, char *id) {
    return newSymbolNode(VARIABLE_DECLARATION_NODE, VARIABLE_SYMBOL, type, id, NULL, NULL, NULL);
}

NodeAST *newIdentifierNode(char *id) {
    return newSymbolNode(ID_NODE, VARIABLE_SYMBOL, TYPE_VOID, id, NULL, NULL, NULL);
}

NodeAST *newAssignmentNode(char *id, NodeAST *expression) {
    return newSymbolNode(ASSIGNMENT_NODE, VARIABLE_SYMBOL, TYPE_VOID, id, NULL, expression, NULL);
}

NodeAST *newMethodCallNode(char *id, NodeArray *arguments) {
    NodeAST *node = newSymbolNode(METHOD_CALL_NODE, METHOD_SYMBOL, TYPE_VOID, id, NULL, NULL, NULL);
    resolveNodeArray(node, arguments);
    return node;
}

NodeAST *newBlockNode(NodeArray *items) {
    NodeAST *node = newNode(BLOCK_NODE, NULL, NULL, NULL, NULL);
    resolveNodeArray(node, items);
    return node;
}

NodeAST *newParametersNode(NodeArray *parameters) {
    NodeAST *node = newNode(PARAMETERS_NODE, NULL, NULL, NULL, NULL);
    resolveNodeArray(node, parameters);
    return node;
}

NodeAST *newReturnNode(NodeAST *expression) {
    return newNode(RETURN_NODE, NULL, expression, NULL, NULL);
}

NodeAST *newWhileNode(NodeAST *condition, NodeAST *body) {
    return newNode(WHILE_NODE, NULL, condition, body, NULL);
}

NodeAST *newIfElseNode(NodeAST *condition, NodeAST *ifBlock, NodeAST *elseBlock) {
    return newNode(IF_ELSE_NODE, NULL, condition, ifBlock, elseBlock);
}

NodeAST *newIntLiteralNode(int value) {
    NodeAST *node = newConstantNode(TYPE_INT);
    if (node != NULL) node->symbol->value.intValue = value;
    return node;
}

NodeAST *newFloatLiteralNode(float value) {
    NodeAST *node = newConstantNode(TYPE_FLOAT);
    if (node != NULL) node->symbol->value.floatValue = value;
    return node;
}

NodeAST *newBoolLiteralNode(int value) {
    NodeAST *node = newConstantNode(TYPE_BOOL);
    if (node != NULL) node->symbol->value.intValue = value;
    return node;
}

void freeAST(NodeAST *node) {
    if (node == NULL) return;

    for (int i = 0; i < node->childCount; i++) {
        freeAST(node->children[i]);
    }
    free(node->children);

    bool hasOwnedSymbol = (node->symbol != NULL &&
        (node->nodeType == VARIABLE_DECLARATION_NODE ||
         node->nodeType == METHOD_DECLARATION_NODE ||
         node->nodeType == CONSTANT_NODE ||
         node->symbol->uid == 0));
    if (hasOwnedSymbol) {
        freeSymbol(node->symbol);
    }

    free(node);
}

void printAST(NodeAST *node, int level) {
    fprintAST(stdout, node, level);
}

void fprintAST(FILE *out, NodeAST *node, int level) {
    if (node == NULL) return;

    for (int i = 0; i < level; i++) {
        fprintf(out, "  | ");
    }

    describeNode(out, node);

    for (int i = 0; i < node->childCount; i++) {
        fprintAST(out, node->children[i], level + 1);
    }
}

const char *getDataTypeName(DataType type) {
    switch (type) {
        case TYPE_INT:   return "INT";
        case TYPE_BOOL:  return "BOOL";
        case TYPE_FLOAT: return "FLOAT";
        case TYPE_VOID:  return "VOID";
        case TYPE_ERROR: return "ERROR";
        default:         return "UNKNOWN";
    }
}

static NodeAST *newConstantNode(DataType type) {
    return newSymbolNode(CONSTANT_NODE, CONSTANT_SYMBOL, type, NULL, NULL, NULL, NULL);
}

static NodeAST *newSymbolNode(NodeType nodeType, SymbolType symbolType, DataType dataType,
                              char *id, NodeAST *parameters, NodeAST *left, NodeAST *mid) {
    Symbol *symbol = newSymbol(id, symbolType, dataType, parameters);
    if (symbol == NULL) {
        free(id);
        freeAST(left);
        freeAST(mid);
        return NULL;
    }

    NodeAST *node = newNode(nodeType, symbol, left, mid, NULL);
    if (node == NULL) {
        freeSymbol(symbol);
        freeAST(left);
        freeAST(mid);
        return NULL;
    }

    node->type = dataType;
    return node;
}

static void resolveNodeArray(NodeAST *parent, NodeArray *array) {
    if (array == NULL) return;

    if (parent == NULL) {
        for (int i = 0; i < array->count; i++) {
            freeAST(array->items[i]);
        }
        free(array->items);
        free(array);
        return;
    }

    if (array->count > 0) {
        parent->children = array->items;
        parent->childCount = array->count;
    } else {
        free(array->items);
    }
    free(array);
}

static void describeNode(FILE *out, const NodeAST *node) {
    switch (node->nodeType) {
        case METHOD_DECLARATION_NODE:
            fprintf(out, "Metodo: %s (Retorna %s)\n", node->symbol->id, getDataTypeName(node->type));
            break;
        case VARIABLE_DECLARATION_NODE:
            fprintf(out, "Declaracion Variable: %s #%d (Tipo %s)\n", node->symbol->id, node->symbol->uid, getDataTypeName(node->type));
            break;
        case ID_NODE:
            fprintf(out, "ID: %s #%d (Tipo %s)\n", node->symbol->id, node->symbol->uid, getDataTypeName(node->type));
            break;
        case CONSTANT_NODE:
            if (node->symbol->type == TYPE_INT)
                fprintf(out, "Constante INT: %d\n", node->symbol->value.intValue);
            else if (node->symbol->type == TYPE_FLOAT)
                fprintf(out, "Constante FLOAT: %f\n", node->symbol->value.floatValue);
            else if (node->symbol->type == TYPE_BOOL)
                fprintf(out, "Constante BOOL: %s\n", node->symbol->value.intValue ? "true" : "false");
            break;
        case OPERATOR_NODE:
            fprintf(out, "Operacion: %s\n", getOperationName(node->operationType));
            break;
        case ASSIGNMENT_NODE:
            fprintf(out, "Asignacion a: %s #%d (Tipo %s)\n", node->symbol->id, node->symbol->uid, getDataTypeName(node->type));
            break;
        case IF_ELSE_NODE:
            fprintf(out, "If-Else\n");
            break;
        case WHILE_NODE:
            fprintf(out, "While\n");
            break;
        case RETURN_NODE:
            fprintf(out, "Return\n");
            break;
        case METHOD_CALL_NODE:
            fprintf(out, "Llamada a Metodo: %s\n", node->symbol->id);
            break;
        case BLOCK_NODE:
            fprintf(out, "Bloque\n");
            break;
        case PARAMETERS_NODE:
            fprintf(out, "Parametros\n");
            break;
        default:
            fprintf(out, "Nodo Desconocido (Tipo %d)\n", node->nodeType);
    }
}

static const char *getOperationName(OperationType operation) {
    switch (operation) {
        case OP_ADD:    return "+";
        case OP_SUB:    return "-";
        case OP_MUL:    return "*";
        case OP_DIV:    return "/";
        case OP_MOD:    return "%";
        case OP_LT:     return "<";
        case OP_GT:     return ">";
        case OP_EQ:     return "==";
        case OP_AND:    return "&&";
        case OP_OR:     return "||";
        case OP_UMINUS: return "- (unario)";
        case OP_NOT:    return "!";
        default:        return "?";
    }
}
