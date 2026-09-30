#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"

Symbol *newSymbol(const char *id, const char *value, SymbolType symbolType, DataType type, NodeAST *parameters) {
    Symbol *symbol = malloc(sizeof(Symbol));
    if (symbol == NULL) return NULL;
    symbol->symbolType = symbolType;
    symbol->type = type;
    symbol->parameters = parameters;
    symbol->id = (id != NULL) ? strdup(id) : NULL;
    symbol->value = (value != NULL) ? strdup(value) : NULL;
    return symbol;
}

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

NodeAST *newLiteralNode(DataType type, const char *value)
{
    Symbol *symbol = newSymbol(NULL, value, CONSTANT_SYMBOL, type, NULL);

    NodeAST *node = newNode(
        CONSTANT_NODE,
        symbol,
        NULL,
        NULL,
        NULL
    );

    if (node == NULL) {
        freeSymbol(symbol);
        return NULL;
    }

    node->type = type;

    return node;
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
        newSymbol(methodName, NULL, METHOD_SYMBOL, returnType, parameters),
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

NodeList *resolveVariableDefinition(DataType type, NodeList *identifiers) {
    NodeList *declarationList = NULL;
    NodeList **tail = &declarationList;
    NodeList *currentId = identifiers;
    
    while (currentId != NULL) {
        NodeAST *individualDecl = newNode(VARIABLE_DECLARATION_NODE, NULL, currentId->node, NULL, NULL);        
        if (individualDecl == NULL) return declarationList;
        
        individualDecl->type = type;

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

void freeSymbol(Symbol *symbol) {
    if (symbol == NULL) return;

    free(symbol->id);
    free(symbol->value);
    free(symbol);
}