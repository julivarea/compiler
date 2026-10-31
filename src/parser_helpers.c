#include "parser_helpers.h"

#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "symbol_table.h"

NodeArray *createNodeArray(int initialCapacity) {
    NodeArray *array = malloc(sizeof(NodeArray));
    if (array == NULL) return NULL;

    int capacity = (initialCapacity > 0) ? initialCapacity : 4;
    array->items = malloc(capacity * sizeof(NodeAST *));
    if (array->items == NULL) {
        free(array);
        return NULL;
    }
    array->count = 0;
    array->capacity = capacity;
    return array;
}

NodeArray *appendNode(NodeArray *array, NodeAST *node) {
    if (node == NULL) return array;

    if (array == NULL) {
        array = createNodeArray(4);
        if (array == NULL) return NULL;
    }

    if (array->count >= array->capacity) {
        int newCapacity = array->capacity * 2;
        NodeAST **newItems = realloc(array->items, newCapacity * sizeof(NodeAST *));
        if (newItems == NULL) return array;
        array->items = newItems;
        array->capacity = newCapacity;
    }

    array->items[array->count++] = node;
    return array;
}

NodeArray *prependNode(NodeArray *array, NodeAST *node) {
    if (node == NULL) return array;

    if (array == NULL) {
        return appendNode(NULL, node);
    }

    if (array->count >= array->capacity) {
        int newCapacity = array->capacity * 2;
        NodeAST **newItems = realloc(array->items, newCapacity * sizeof(NodeAST *));
        if (newItems == NULL) return array;
        array->items = newItems;
        array->capacity = newCapacity;
    }

    memmove(array->items + 1, array->items, array->count * sizeof(NodeAST *));
    array->items[0] = node;
    array->count++;
    return array;
}

void resolveNodeArray(NodeAST *parent, NodeArray *array) {
    if (parent == NULL) {
        if (array != NULL) {
            free(array->items);
            free(array);
        }
        return;
    }

    if (array == NULL || array->count == 0) {
        parent->children = NULL;
        parent->childCount = 0;
        if (array != NULL) {
            free(array->items);
            free(array);
        }
        return;
    }

    parent->children = array->items;
    parent->childCount = array->count;
    free(array);
}

NodeArray *mergeNodeArrays(NodeArray *first, NodeArray *second) {
    if (first == NULL) return second;
    if (second == NULL) return first;

    int total = first->count + second->count;
    if (total > first->capacity) {
        NodeAST **newItems = realloc(first->items, total * sizeof(NodeAST *));
        if (newItems == NULL) return first;
        first->items = newItems;
        first->capacity = total;
    }

    memcpy(first->items + first->count, second->items, second->count * sizeof(NodeAST *));
    first->count = total;

    free(second->items);
    free(second);
    return first;
}

NodeAST *newParameterDeclaration(DataType type, char *id) {
    NodeAST *declaration = newNode(VARIABLE_DECLARATION_NODE, newSymbol(id, ID_SYMBOL, type, NULL), NULL, NULL, NULL);
    if (declaration != NULL) {
        declaration->type = type;
    }
    return declaration;
}

NodeArray *resolveVariableDefinition(DataType type, NodeArray *identifiers) {
    if (identifiers == NULL) return NULL;

    NodeArray *declarations = createNodeArray(identifiers->count);
    if (declarations == NULL) {
        free(identifiers->items);
        free(identifiers);
        return NULL;
    }

    for (int i = 0; i < identifiers->count; i++) {
        NodeAST *idNode = identifiers->items[i];
        Symbol *varSymbol = idNode->symbol;
        varSymbol->type = type;

        NodeAST *individualDecl = newNode(VARIABLE_DECLARATION_NODE, varSymbol, NULL, NULL, NULL);
        if (individualDecl != NULL) {
            individualDecl->type = type;
            appendNode(declarations, individualDecl);
        }

        free(idNode);
    }

    free(identifiers->items);
    free(identifiers);
    return declarations;
}
