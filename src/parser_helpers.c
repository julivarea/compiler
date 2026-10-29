#include <stdlib.h>
#include "parser_helpers.h"
#include "ast.h"
#include "symbol_table.h"

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
