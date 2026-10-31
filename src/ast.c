#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "ast.h"
#include "symbol_table.h"

extern int yylineno; 

NodeAST *newNode(NodeType nodeType, Symbol *symbol, NodeAST *left, NodeAST *mid, NodeAST *right) {
    NodeAST *node = malloc(sizeof(NodeAST));
    if (node == NULL) return NULL;

    node->nodeType = nodeType;
    node->symbol = symbol;
    node->type = TYPE_VOID;
    node->line = yylineno;

    bool hasConditionLine = ((nodeType == WHILE_NODE || nodeType == IF_ELSE_NODE) && left != NULL);
    if (hasConditionLine) {
        node->line = left->line;
    }

    int count = 0;
    NodeAST *temp[3];
    if (left) temp[count++] = left;
    if (mid) temp[count++] = mid;
    if (right) temp[count++] = right;

    if (count > 0) {
        node->children = malloc(count * sizeof(NodeAST *));
        if (node->children) {
            for (int i = 0; i < count; i++) {
                node->children[i] = temp[i];
            }
        }
        node->childCount = count;
    } else {
        node->children = NULL;
        node->childCount = 0;
    }

    return node;
}

NodeAST *newIntLiteralNode(int value){
    Symbol *symbol = newSymbol(NULL, CONSTANT_SYMBOL, TYPE_INT, NULL);
    symbol->value.int_val = value;
    return newNode(CONSTANT_NODE, symbol, NULL, NULL, NULL);
}

NodeAST *newFloatLiteralNode(float value){
    Symbol *symbol = newSymbol(NULL, CONSTANT_SYMBOL, TYPE_FLOAT, NULL);
    symbol->value.float_val = value;
    return newNode(CONSTANT_NODE, symbol, NULL, NULL, NULL);
}

NodeAST *newBoolLiteralNode(int value){
    Symbol *symbol = newSymbol(NULL, CONSTANT_SYMBOL, TYPE_BOOL, NULL);
    symbol->value.int_val = value;
    return newNode(CONSTANT_NODE, symbol, NULL, NULL, NULL);
}

// Crea un nuevo nodo AST para operaciones binarias o unarias
NodeAST *newOperatorNode(OperationType operator, NodeAST *left, NodeAST *right){
    NodeAST *node = newNode(
        OPERATOR_NODE,
        NULL,
        left,
        right,
        NULL
    );
    node->operationType = operator;
    return node;
}

NodeAST *newMethodDeclaration(DataType returnType, char *methodName, NodeAST *parameters, NodeAST *body) { 
    NodeAST *node = newNode(
        METHOD_DECLARATION_NODE,
        newSymbol(methodName, METHOD_SYMBOL, returnType, parameters),
        NULL,
        NULL,
        NULL
    );
    node->type = returnType;
    bool hasParameters = (parameters != NULL);
    if (hasParameters) {
        node->line = parameters->line;
    }
    node->children = malloc(2 * sizeof(NodeAST*));
    node->childCount = 2;
    node->children[0] = parameters;
    node->children[1] = body;
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

const char* getDataTypeName(DataType type) {
    switch(type) {
        case TYPE_INT: return "INT";
        case TYPE_BOOL: return "BOOL";
        case TYPE_FLOAT: return "FLOAT";
        case TYPE_VOID: return "VOID";
        case TYPE_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

static const char* getOperationName(OperationType operation) {
    switch(operation) {
        case OP_ADD: return "+";
        case OP_SUB: return "-";
        case OP_MUL: return "*";
        case OP_DIV: return "/";
        case OP_MOD: return "%";
        case OP_LT: return "<";
        case OP_GT: return ">";
        case OP_EQ: return "==";
        case OP_AND: return "&&";
        case OP_OR: return "||";
        case OP_UMINUS: return "- (unario)";
        case OP_NOT: return "!";
        default: return "?";
    }
}

void fprintAST(FILE *out, NodeAST *node, int level) {
    if (node == NULL) return;
    
    // Imprimir indentación basada en el nivel
    for (int i = 0; i < level; i++) {
        fprintf(out, "  | ");
    }
    
    // Imprimir información del nodo actual
    switch(node->nodeType) {
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
                fprintf(out, "Constante INT: %d\n", node->symbol->value.int_val);
            else if (node->symbol->type == TYPE_FLOAT) 
                fprintf(out, "Constante FLOAT: %f\n", node->symbol->value.float_val);
            else if (node->symbol->type == TYPE_BOOL) 
                fprintf(out, "Constante BOOL: %s\n", node->symbol->value.int_val ? "true" : "false");
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
    
    for (int i = 0; i < node->childCount; i++) {
        fprintAST(out, node->children[i], level + 1);
    }
}
void printAST(NodeAST *node, int level) {
    fprintAST(stdout, node, level);
}
