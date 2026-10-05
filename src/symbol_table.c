#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "symbol_table.h"

Symbol *newSymbol(const char *id, SymbolType symbolType, DataType type, NodeAST *parameters) {
    Symbol *symbol = malloc(sizeof(Symbol));
    if (symbol == NULL) return NULL;
    symbol->symbolType = symbolType;
    symbol->type = type;
    symbol->parameters = parameters;
    symbol->id = (id != NULL) ? strdup(id) : NULL;
    
    return symbol;
}

void freeSymbol(Symbol *symbol) {
    if (symbol == NULL) return;

    free(symbol->id);
    
    free(symbol);
}

Scope* createScope(int level, Scope *parent) {
    Scope *scope = malloc(sizeof(Scope));
    if (!scope) return NULL;
    
    scope->level = level;
    scope->parent = parent;
    
    scope->symbol_capacity = 8;
    scope->symbol_count = 0;
    scope->symbols = malloc(sizeof(Symbol*) * scope->symbol_capacity);
    
    scope->children_capacity = 4;
    scope->children_count = 0;
    scope->childrens = malloc(sizeof(Scope*) * scope->children_capacity);
    
    return scope;
}

void appendSymbol(Scope *scope, Symbol *symbol) {
    if (!scope || !symbol) return;
    
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

Scope* solveAST(NodeAST *root, Scope *parent);

void populateScope(NodeAST *node, Scope *current_scope) {
    if (!node) return;
    
    if (node->nodeType == BLOCK_NODE || node->nodeType == METHOD_DECLARATION_NODE) {
        Scope *child_scope = solveAST(node, current_scope);
        appendChildScope(current_scope, child_scope);
    } 
    else if (node->nodeType == VARIABLE_DECLARATION_NODE) {
        if (node->symbol) {
            appendSymbol(current_scope, node->symbol);
        }
    } 
    else {
        for (int i = 0; i < node->childCount; i++) {
            populateScope(node->children[i], current_scope);
        }
    }
}

Scope* solveAST(NodeAST *root, Scope *parent) {
    if (!root) return NULL;
    
    int level = parent ? parent->level + 1 : 0;
    
    Scope *current_scope = createScope(level, parent);
    
    for (int i = 0; i < root->childCount; i++) {
        populateScope(root->children[i], current_scope);
    }
    
    return current_scope;
}

Symbol* solveVariable(Scope *scope, const char *id) {
    if (scope == NULL || id == NULL) {
        return NULL; 
    }

    for (int i = 0; i < scope->symbol_count; i++) {
        Symbol *current_symbol = scope->symbols[i];
        
        if (current_symbol->id != NULL && strcmp(current_symbol->id, id) == 0) {
            return current_symbol;
        }
    }
    return solveVariable(scope->parent, id);
}