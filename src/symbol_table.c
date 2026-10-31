#include "symbol_table.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

// Arranca en 1 para reservar uid = 0 como indicador de simbolo no declarado.
static int nextSymbolUid = 1;

Symbol *newSymbol(char *id, SymbolType symbolType, DataType type, struct NodeAST *parameters) {
    Symbol *symbol = malloc(sizeof(Symbol));
    bool allocationFailed = (symbol == NULL);
    if (allocationFailed) {
        return NULL;
    }

    symbol->symbolType = symbolType;
    symbol->type = type;
    symbol->parameters = parameters;
    symbol->id = id;
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

    scope->symbolCapacity = 8;
    scope->symbolCount = 0;
    scope->symbols = malloc(sizeof(Symbol *) * scope->symbolCapacity);

    scope->childrenCapacity = 4;
    scope->childrenCount = 0;
    scope->children = malloc(sizeof(Scope *) * scope->childrenCapacity);

    bool internalAllocationFailed = (scope->symbols == NULL || scope->children == NULL);
    if (internalAllocationFailed) {
        free(scope->symbols);
        free(scope->children);
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

    bool needsExpansion = (scope->symbolCount >= scope->symbolCapacity);
    if (needsExpansion) {
        int newCapacity = scope->symbolCapacity * 2;
        Symbol **newSymbols = realloc(scope->symbols, sizeof(Symbol *) * newCapacity);
        bool allocationFailed = (newSymbols == NULL);
        if (allocationFailed) {
            return;
        }

        scope->symbols = newSymbols;
        scope->symbolCapacity = newCapacity;
    }

    scope->symbols[scope->symbolCount++] = symbol;
}

void appendChildScope(Scope *parent, Scope *child) {
    bool isInvalidInput = (parent == NULL || child == NULL);
    if (isInvalidInput) return;

    bool needsExpansion = (parent->childrenCount >= parent->childrenCapacity);
    if (needsExpansion) {
        int newCapacity = parent->childrenCapacity * 2;
        Scope **newChildren = realloc(parent->children, sizeof(Scope *) * newCapacity);
        bool allocationFailed = (newChildren == NULL);
        if (allocationFailed) {
            return;
        }

        parent->children = newChildren;
        parent->childrenCapacity = newCapacity;
    }

    parent->children[parent->childrenCount++] = child;
}

Symbol *lookupSymbol(Scope *scope, const char *id) {
    bool isInvalidInput = (scope == NULL || id == NULL);
    if (isInvalidInput) {
        return NULL;
    }

    Symbol *symbol = lookupSymbolInScope(scope, id);
    bool isFoundInCurrentScope = (symbol != NULL);
    if (isFoundInCurrentScope) {
        return symbol;
    }

    return lookupSymbol(scope->parent, id);
}

Symbol *lookupSymbolInScope(Scope *scope, const char *id) {
    bool isInvalidInput = (scope == NULL || id == NULL);
    if (isInvalidInput) {
        return NULL;
    }

    for (int i = 0; i < scope->symbolCount; i++) {
        Symbol *currentSymbol = scope->symbols[i];
        bool matchesId = (currentSymbol->id != NULL && strcmp(currentSymbol->id, id) == 0);
        if (matchesId) {
            return currentSymbol;
        }
    }

    return NULL;
}
