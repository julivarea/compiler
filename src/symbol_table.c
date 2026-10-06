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

Scope* analyzeSemantics(NodeAST *root, Scope *parent);

#include "error_handler.h"

// verifica que una variable no sea definida dos veces en un mismo scope
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
// en caso de que el bloque tenga otro bloque dentro, lo agrega en la lista de hijos
// en caso de que sea una sentencia como declaracion, asignacion, valida que sea correcto semanticamente
void analyzeNodeSemantics(NodeAST *node, Scope *current_scope) {
    if (!node) return;
    
    if (node->nodeType == BLOCK_NODE || node->nodeType == METHOD_DECLARATION_NODE) {
        Scope *child_scope = analyzeSemantics(node, current_scope);
        appendChildScope(current_scope, child_scope);
    } 
    else if (node->nodeType == VARIABLE_DECLARATION_NODE) {
        if (node->symbol) {
            Symbol *existing = lookupVariableCurrentScope(current_scope, node->symbol->id);
            if (existing != NULL) {
                semanticError(node->line, "La variable '%s' ya fue declarada previamente en este ambito.", node->symbol->id);
            } else {
                appendSymbol(current_scope, node->symbol);
            }
        }
    } 
    else if (node->nodeType == ASSIGNMENT_NODE) {
        if (node->symbol) {
            Symbol *resolvedSymbol = lookupVariable(current_scope, node->symbol->id);
            if (!resolvedSymbol) {
                semanticError(node->line, "Asignacion a variable no declarada '%s'.", node->symbol->id);
            }
        }
            if (node->childCount > 1 && node->children[1] != NULL) {
            analyzeNodeSemantics(node->children[1], current_scope);
        }
    } 
    else if (node->nodeType == ID_NODE) {
        if (node->symbol) {
            Symbol *resolvedSymbol = lookupVariable(current_scope, node->symbol->id);
            if (!resolvedSymbol) {
                semanticError(node->line, "Uso de variable no declarada '%s'.", node->symbol->id);
            }
        }
    } 
    else {
        for (int i = 0; i < node->childCount; i++) {
            analyzeNodeSemantics(node->children[i], current_scope);
        }
    }
}
// se encarga de por cada bloque que aparece, crea un nuevo scope / tabla de simbolos para ese scope
Scope* analyzeSemantics(NodeAST *root, Scope *parent) {
    if (!root) return NULL;
    
    int level = parent ? parent->level + 1 : 0;
    
    Scope *current_scope = createScope(level, parent);
    
    for (int i = 0; i < root->childCount; i++) {
        analyzeNodeSemantics(root->children[i], current_scope);
    }
    
    return current_scope;
}
// busca las variables en un scope determinado
Symbol* lookupVariable(Scope *scope, const char *id) {
    if (scope == NULL || id == NULL) {
        return NULL; // exception for not defined variable
    }

    for (int i = 0; i < scope->symbol_count; i++) {
        Symbol *current_symbol = scope->symbols[i];
        
        if (current_symbol->id != NULL && strcmp(current_symbol->id, id) == 0) {
            return current_symbol;
        }
    }
    return lookupVariable(scope->parent, id);
}