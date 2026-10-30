#include "symbol_table.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

// Arranca en 1 para reservar uid = 0 como indicador de simbolo no declarado.
static int nextSymbolUid = 1;

Symbol *newSymbol(const char *id, SymbolType symbolType, DataType type, struct NodeAST *parameters) {
    Symbol *symbol = malloc(sizeof(Symbol));
    bool allocationFailed = (symbol == NULL);
    if (allocationFailed) {
        return NULL;
    }

    bool hasId = (id != NULL);

    symbol->symbolType = symbolType;
    symbol->type = type;
    symbol->parameters = parameters;
    symbol->id = hasId ? strdup(id) : NULL;
    symbol->uid = 0; // Asignado por appendSymbol al registrarse en un ambito

    return symbol;
}

void freeSymbol(Symbol *symbol) {
    if (symbol == NULL) return;

    bool hasId = (symbol->id != NULL);
    if (hasId) {
        free(symbol->id);
    }

    free(symbol);
}

Scope *createScope(int level, Scope *parent, Symbol *function) {
    Scope *scope = malloc(sizeof(Scope));
    bool allocationFailed = (scope == NULL);
    if (allocationFailed) {
        return NULL;
    }

    scope->level = level;
    scope->parent = parent;
    scope->function = function;

    scope->symbol_capacity = 8;
    scope->symbol_count = 0;
    scope->symbols = malloc(sizeof(Symbol *) * scope->symbol_capacity);

    scope->children_capacity = 4;
    scope->children_count = 0;
    scope->childrens = malloc(sizeof(Scope *) * scope->children_capacity);

    bool internalAllocationFailed = (scope->symbols == NULL || scope->childrens == NULL);
    if (internalAllocationFailed) {
        free(scope->symbols);
        free(scope->childrens);
        free(scope);
        return NULL;
    }

    return scope;
}

void appendSymbol(Scope *scope, Symbol *symbol) {
    bool isInvalidInput = (scope == NULL || symbol == NULL);
    if (isInvalidInput) return;

    bool isUndeclared = (symbol->uid == 0);
    if (isUndeclared) {
        symbol->uid = nextSymbolUid++;
    }

    bool needsExpansion = (scope->symbol_count >= scope->symbol_capacity);
    if (needsExpansion) {
        int newCapacity = scope->symbol_capacity * 2;
        Symbol **newSymbols = realloc(scope->symbols, sizeof(Symbol *) * newCapacity);
        bool allocationFailed = (newSymbols == NULL);
        if (allocationFailed) {
            return;
        }

        scope->symbols = newSymbols;
        scope->symbol_capacity = newCapacity;
    }

    scope->symbols[scope->symbol_count++] = symbol;
}

void appendChildScope(Scope *parent, Scope *child) {
    bool isInvalidInput = (parent == NULL || child == NULL);
    if (isInvalidInput) return;

    bool needsExpansion = (parent->children_count >= parent->children_capacity);
    if (needsExpansion) {
        int newCapacity = parent->children_capacity * 2;
        Scope **newChildren = realloc(parent->childrens, sizeof(Scope *) * newCapacity);
        bool allocationFailed = (newChildren == NULL);
        if (allocationFailed) {
            return;
        }

        parent->childrens = newChildren;
        parent->children_capacity = newCapacity;
    }

    parent->childrens[parent->children_count++] = child;
}

Symbol *lookupVariable(Scope *scope, const char *id) {
    bool isInvalidInput = (scope == NULL || id == NULL);
    if (isInvalidInput) {
        return NULL;
    }

    Symbol *symbol = lookupVariableCurrentScope(scope, id);
    bool isFoundInCurrentScope = (symbol != NULL);
    if (isFoundInCurrentScope) {
        return symbol;
    }

    return lookupVariable(scope->parent, id);
}

Symbol *lookupVariableCurrentScope(Scope *scope, const char *id) {
    bool isInvalidInput = (scope == NULL || id == NULL);
    if (isInvalidInput) {
        return NULL;
    }

    for (int i = 0; i < scope->symbol_count; i++) {
        Symbol *currentSymbol = scope->symbols[i];
        bool matchesId = (currentSymbol->id != NULL && strcmp(currentSymbol->id, id) == 0);
        if (matchesId) {
            return currentSymbol;
        }
    }

    return NULL;
}
