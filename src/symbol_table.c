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
// se pasa el type en el scope, para el momento de retornar poder ver que es lo que espera el scope
void analyzeMethodNode(NodeAST *node, Scope *current_scope, DataType type) {
    Scope *child_scope = analyzeSemantics(node, current_scope, type);
    appendChildScope(current_scope, child_scope);
}
// un bloque no necesariamente necesita hacer un retorno por eso el null
void analyzeBlockNode(NodeAST *node, Scope *current_scope) {
    Scope *child_scope = analyzeSemantics(node, current_scope, NULL);
    appendChildScope(current_scope, child_scope);
}

void analyzeVariableDeclarationNode(NodeAST *node, Scope *current_scope) {
    if (node->symbol) {
        Symbol *existing = lookupVariableCurrentScope(current_scope, node->symbol->id);
        if (existing != NULL) {
            semanticError(node->line, "La variable '%s' ya fue declarada previamente en este ambito.", node->symbol->id);
        } else {
            appendSymbol(current_scope, node->symbol);
        }
    }
}

void analyzeAssignmentNode(NodeAST *node, Scope *current_scope) {
    if (node->symbol) {
        Symbol *resolvedSymbol = lookupVariable(current_scope, node->symbol->id);
        if (!resolvedSymbol) {
            semanticError(node->line, "Asignacion a variable no declarada '%s'.", node->symbol->id);
        }
    }
    // verifica la expresion, que las variables que use esten definidas, faltaria ver que la expresion tenga sentido
    if (node->childCount > 1 && node->children[1] != NULL) {
        analyzeNodeSemantics(node->children[1], current_scope);
    }
}

void analyzeReturnNode(NodeAST *node, Scope *current_scope){
    // if(!solveTypeOfExpression(node) == current_scope->returnType) semanticError(node->line, "Tipo de expresion incompatible con el metodo")
}

void analyzeIdNode(NodeAST *node, Scope *current_scope) {
    if (node->symbol) {
        Symbol *resolvedSymbol = lookupVariable(current_scope, node->symbol->id);
        if (!resolvedSymbol) {
            semanticError(node->line, "Uso de variable no declarada '%s'.", node->symbol->id);
        }
    }
}

void analyzeNodeSemantics(NodeAST *node, Scope *current_scope) {
    if (!node) return;
    
    switch (node->nodeType) {
        case BLOCK_NODE:
            analyzeBlockNode(node, current_scope);
            break;

        case METHOD_DECLARATION_NODE:
            analyzeMethodNode(node, current_scope, node-returnType);
            
        case VARIABLE_DECLARATION_NODE:
            analyzeVariableDeclarationNode(node, current_scope);
            break;
            
        case ASSIGNMENT_NODE:
            analyzeAssignmentNode(node, current_scope);
            break;
            
        case ID_NODE:
            analyzeIdNode(node, current_scope);
            break;

        case RETURN_NODE:
            analyzeReturnNode(node, current_scope);
            
        default:
            for (int i = 0; i < node->childCount; i++) {
                analyzeNodeSemantics(node->children[i], current_scope);
            }
            break;
    }
}
// se encarga de por cada bloque que aparece, crea un nuevo scope / tabla de simbolos para ese scope
Scope* analyzeSemantics(NodeAST *root, Scope *parent) {
    if (!root) return NULL;
    
    int level = parent ? parent->level + 1 : 0;
    
    Scope *current_scope = createScope(level, parent, NULL);
    
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