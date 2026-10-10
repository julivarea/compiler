#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "ast.h"

extern int yylineno; 

NodeAST *newNode(NodeType nodeType, Symbol *symbol, NodeAST *left, NodeAST *mid, NodeAST *right) {
    NodeAST *node = malloc(sizeof(NodeAST));
    if (node == NULL) return NULL;

    node->nodeType = nodeType;
    node->symbol = symbol;
    node->type = TYPE_VOID;
    node->line = yylineno;

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

NodeList *initializeTemporaryList(NodeAST *node, NodeList *next) {
    NodeList *list = malloc(sizeof(NodeList));
    if (list == NULL) return NULL;

    list->node = node;
    list->next = next;

    list->tail = (next != NULL) ? next->tail : list;

    return list;
}

NodeList *appendToTemporaryList(NodeList *list, NodeAST *node) {
    NodeList *newNode = initializeTemporaryList(node, NULL);
    if (list == NULL) {
        return newNode;
    }
    
    // inserción en O(1) puro usando el puntero tail
    list->tail->next = newNode;
    
    // actualizamos el tail de la cabecera
    list->tail = newNode;
    
    return list;
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

/**
 * Crea un nuevo nodo AST. left, mid y right se empaquetan en children.
 *
 * @param operator tipo del operador
 * @param left Nodo hijo izquierdo 
 * @param right Nodo hijo derecho (opcional).
 * @return Nuevo nodo creado.
 */
NodeAST *newBinaryOperatorNode(OperationType operator, NodeAST *left, NodeAST *right){
    NodeAST *node = newNode(
        BINARYOPERATOR_NODE,
        NULL,
        left,
        right,
        NULL
    );
    node->operationType = operator;
    return node;
}

void resolveTemporaryList(NodeAST *parent, NodeList *list) {
    int count = 0;
    for (NodeList *current = list; current != NULL; current = current->next) {
        count++;
    }

    if (count == 0) {
        parent->children = NULL;
        parent->childCount = 0;
        return;
    }

    // reservamos el arreglo de punteros NodeAST de una vez
    NodeAST **children = malloc(count * sizeof(NodeAST *));
    if (children == NULL) return;

    // volcamos cada nodo al arreglo y liberamos la lista transitoria
    int i = 0;
    NodeList *current = list;
    while (current != NULL) {
        children[i] = current->node;
        i++;

        NodeList *toFree = current;
        current = current->next;
        free(toFree);
    }

    parent->children = children;
    parent->childCount = count;
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
    node->children = malloc(2 * sizeof(NodeAST*));
    node->childCount = 2;
    node->children[0] = parameters;
    node->children[1] = body;
    return node;
}

NodeList *mergeNodeLists(NodeList *list1, NodeList *list2) {
    if (list1 == NULL) return list2;
    if (list2 == NULL) return list1;

    list1->tail->next = list2;
    list1->tail = list2->tail;
    
    return list1;
}

// en este punto tenemos una lista de simbolos sin tipo, y un tipo, por lo tanto se itera la lista, poniendole el mismo tipo a todas las variables
NodeList *resolveVariableDefinition(DataType type, NodeList *identifiers) {
    NodeList *declarationList = NULL;
    NodeList **tail = &declarationList;
    NodeList *currentId = identifiers;
    
    while (currentId != NULL) {
        Symbol *varSymbol = currentId->node->symbol;
        varSymbol->type = type;
        
        NodeAST *individualDecl = newNode(VARIABLE_DECLARATION_NODE, varSymbol, NULL, NULL, NULL);        
        if (individualDecl == NULL) return declarationList;
        
        individualDecl->type = type; 

        free(currentId->node);

        NodeList *newCell = initializeTemporaryList(individualDecl, NULL);
        if (newCell == NULL) return declarationList;
        
        *tail = newCell;
        tail = &newCell->next;
        
        if (declarationList != NULL) {
            declarationList->tail = newCell;
        }
        
        currentId = currentId->next;
    }
    
    return declarationList;
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

static const char* getOpName(OperationType op) {
    switch(op) {
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
        case OP_NEGATIVE: return "- (unario)";
        case OP_NEGATION: return "!";
        default: return "?";
    }
}

void printAST(NodeAST *node, int level) {
    if (node == NULL) return;
    
    // Imprimir indentación basada en el nivel
    for (int i = 0; i < level; i++) {
        printf("  | ");
    }
    
    // Imprimir información del nodo actual
    switch(node->nodeType) {
        case METHOD_DECLARATION_NODE:
            printf("Metodo: %s (Retorna %s)\n", node->symbol->id, getDataTypeName(node->type));
            break;
        case VARIABLE_DECLARATION_NODE:
            printf("Declaracion Variable: %s (Tipo %s)\n", node->symbol->id, getDataTypeName(node->type));
            break;
        case ID_NODE:
            printf("ID: %s\n", node->symbol->id);
            break;
        case CONSTANT_NODE:
            if (node->symbol->type == TYPE_INT) 
                printf("Constante INT: %d\n", node->symbol->value.int_val);
            else if (node->symbol->type == TYPE_FLOAT) 
                printf("Constante FLOAT: %f\n", node->symbol->value.float_val);
            else if (node->symbol->type == TYPE_BOOL) 
                printf("Constante BOOL: %s\n", node->symbol->value.int_val ? "true" : "false");
            break;
        case BINARYOPERATOR_NODE:
            printf("Operacion Binaria: %s\n", getOpName(node->operationType));
            break;
        case ASSIGNMENT_NODE:
            printf("Asignacion\n");
            break;
        case IF_ELSE_NODE:
            printf("If-Else\n");
            break;
        case WHILE_NODE:
            printf("While\n");
            break;
        case RETURN_NODE:
            printf("Return\n");
            break;
        case METHOD_CALL_NODE:
            printf("Llamada a Metodo: %s\n", node->symbol->id);
            break;
        case BLOCK_NODE:
            printf("Bloque\n");
            break;
        case PARAMETERS_NODE:
            printf("Parametros\n");
            break;
        default:
            printf("Nodo Desconocido (Tipo %d)\n", node->nodeType);
    }
    
    // Llamada recursiva para los hijos
    for (int i = 0; i < node->childCount; i++) {
        printAST(node->children[i], level + 1);
    }
}