#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "symbol_table.h"

Symbol *newSymbol(const char *id, SymbolType symbolType, DataType type, struct NodeAST *parameters) {
    Symbol *symbol = malloc(sizeof(Symbol));
    if (symbol == NULL) return NULL;
    symbol->symbolType = symbolType;
    symbol->type = type;
    symbol->parameters = parameters;
    symbol->id = (id != NULL) ? strdup(id) : NULL;
    symbol->uid = 0; // lo asigna appendSymbol al declararse; 0 = no declarado

    return symbol;
}

void freeSymbol(Symbol *symbol) {
    if (symbol == NULL) return;
    free(symbol->id);
    free(symbol);
}

Scope* createScope(int level, Scope *parent, DataType returnType) {
    Scope *scope = malloc(sizeof(Scope));
    if (!scope) return NULL;
    
    scope->level = level;
    scope->parent = parent; 
    scope->returnType = returnType;
    
    scope->symbol_capacity = 8;
    scope->symbol_count = 0;
    scope->symbols = malloc(sizeof(Symbol*) * scope->symbol_capacity);
    
    scope->children_capacity = 4;
    scope->children_count = 0;
    scope->childrens = malloc(sizeof(Scope*) * scope->children_capacity);
    
    return scope;
}

// Fuente de los uid: declarar un simbolo es lo que lo convierte en una entidad
// con identidad propia, y appendSymbol es el unico lugar por donde eso pasa
// (variables, parametros y metodos). Arranca en 1 para que 0 siga significando
// "simbolo no declarado".
static int next_symbol_uid = 1;

void appendSymbol(Scope *scope, Symbol *symbol) {
    if (!scope || !symbol) return;

    if (symbol->uid == 0) {
        symbol->uid = next_symbol_uid++;
    }

    if (scope->symbol_count >= scope->symbol_capacity) {
        scope->symbol_capacity *= 2;
        scope->symbols = realloc(scope->symbols, sizeof(Symbol*) * scope->symbol_capacity);
    }
    scope->symbols[scope->symbol_count++] = symbol;
}

void appendChildScope(Scope *parent, Scope *child) {
    if (!parent || !child) return;
    
    if (parent->children_count >= parent->children_capacity) {
        parent->children_capacity *= 2;
        parent->childrens = realloc(parent->childrens, sizeof(Scope*) * parent->children_capacity);
    }
    parent->childrens[parent->children_count++] = child;
}

// Verifica si un identificador fue definido en el ambito actual (sin buscar en padres)
Symbol* lookupVariableCurrentScope(Scope *scope, const char *id) {
    if (scope == NULL || id == NULL) return NULL;

    for (int i = 0; i < scope->symbol_count; i++) {
        Symbol *current_symbol = scope->symbols[i];
        
        if (current_symbol->id != NULL && strcmp(current_symbol->id, id) == 0) {
            return current_symbol;
        }
    }
    return NULL;
}

Symbol* lookupVariable(Scope *scope, const char *id) {
    if (scope == NULL || id == NULL) {
        return NULL;
    }

    for (int i = 0; i < scope->symbol_count; i++) {
        Symbol *current_symbol = scope->symbols[i];
        
        if (current_symbol->id != NULL && strcmp(current_symbol->id, id) == 0) {
            return current_symbol;
        }
    }
    return lookupVariable(scope->parent, id);
}
