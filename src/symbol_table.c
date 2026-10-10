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

Scope* analyzeSemantics(NodeAST *root, Scope *parent, DataType returnType);
void analyzeNodeSemantics(NodeAST *node, Scope *current_scope);
DataType analyzeExpression(NodeAST *node, Scope *scope);
int isAssignable(DataType expected, DataType actual);

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
    Scope *child_scope = analyzeSemantics(node, current_scope, TYPE_VOID);
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
    DataType varType = TYPE_ERROR;

    if (node->symbol) {
        Symbol *resolvedSymbol = lookupVariable(current_scope, node->symbol->id);
        if (!resolvedSymbol) {
            semanticError(node->line, "Asignacion a variable no declarada '%s'.", node->symbol->id);
        } else {
            varType = resolvedSymbol->type;
        }
    }
    
    // verifica la expresion y que los tipos coincidan
    if (node->childCount > 1 && node->children[1] != NULL) {
        DataType exprType = analyzeExpression(node->children[1], current_scope);
        
        if (varType != TYPE_ERROR && exprType != TYPE_ERROR && !isAssignable(varType, exprType)) {
             semanticError(node->line, "El tipo de la expresion no coincide con la variable '%s'.", node->symbol->id);
        }
    }
}

void analyzeReturnNode(NodeAST *node, Scope *current_scope){
    NodeAST *expression = GET_EXPRESSION(node);
    DataType expectedType = current_scope->returnType;

    if (expression == NULL) {
        // return; sin expresion: solo es valido si el metodo es void
        if (expectedType != TYPE_VOID) {
            semanticError(node->line, "El metodo espera un valor de retorno de tipo %s, pero no se especifico ninguno.", getDataTypeName(expectedType));
        }
        return;
    }

    // return <expr>; con expresion: el metodo tiene que ser no-void
    if (expectedType == TYPE_VOID) {
        semanticError(node->line, "El metodo es void y no deberia retornar un valor.");
        return;
    }

    DataType actualType = analyzeExpression(expression, current_scope);

    if (actualType != TYPE_ERROR && !isAssignable(expectedType, actualType)) {
        semanticError(node->line, "El tipo de retorno no coincide: se esperaba %s pero se obtuvo %s.", getDataTypeName(expectedType), getDataTypeName(actualType));
    }
}

int isAssignable(DataType expected, DataType actual) {
    if (expected == actual) return 1;
    if (expected == TYPE_FLOAT && actual == TYPE_INT) return 1;
    return 0;
}

DataType analyzeIdentifier(NodeAST *node, Scope *scope) {
    Symbol *symbol = lookupVariable(scope, node->symbol->id);

    if (symbol == NULL) {
        semanticError(node->line, "Variable no declarada: %s", node->symbol->id);
        node->type = TYPE_ERROR;
        return TYPE_ERROR;
    }

    node->symbol = symbol;
    node->type = symbol->type;

    return symbol->type;
}

DataType analyzeExpression(NodeAST *node, Scope *scope);

DataType analyzeMethodCall(NodeAST *node, Scope *scope) {

    Symbol *method = lookupVariable(
        scope,
        node->symbol->id
    );

    if (method == NULL) {
        semanticError(node->line, "Método no declarado: %s", node->symbol->id);
        return TYPE_ERROR;
    }

    NodeAST *parametersNode = method->parameters;
    int expectedArgCount = parametersNode ? parametersNode->childCount : 0;

    if (node->childCount != expectedArgCount) {
        semanticError(node->line, "Cantidad de argumentos incorrecta. Se esperaban %d pero se pasaron %d.", expectedArgCount, node->childCount);
        return TYPE_ERROR;
    }

    for (int i = 0; i < node->childCount; i++) {

        DataType actual = analyzeExpression(
            node->children[i],
            scope
        );

        DataType expected = parametersNode->children[i]->type;

        if (!isAssignable(expected, actual)) {
            semanticError(node->line, "Parámetro incompatible en la posición %d", i + 1);
        }
    }

    node->type = method->type;

    return method->type;
}

DataType resolveBinaryOperation(
    OperationType operator,
    DataType left,
    DataType right
) {
    switch (operator) {

        case OP_ADD:
        case OP_SUB:
        case OP_MUL:
        case OP_DIV:
        case OP_MOD:

            if (left == TYPE_INT && right == TYPE_INT)
                return TYPE_INT;

            if (left == TYPE_FLOAT && right == TYPE_FLOAT)
                return TYPE_FLOAT;

            return TYPE_ERROR;


        case OP_LT:
        case OP_GT:
        case OP_EQ:

            if (left == right)
                return TYPE_BOOL;

            return TYPE_ERROR;


        case OP_AND:
        case OP_OR:

            if (left == TYPE_BOOL && right == TYPE_BOOL)
                return TYPE_BOOL;

            return TYPE_ERROR;


        case OP_NEGATIVE:

            if (left == TYPE_INT)
                return TYPE_INT;

            if (left == TYPE_FLOAT)
                return TYPE_FLOAT;

            return TYPE_ERROR;


        case OP_NEGATION:

            if (left == TYPE_BOOL)
                return TYPE_BOOL;

            return TYPE_ERROR;


        default:
            return TYPE_ERROR;
    }
}

DataType analyzeBinaryOperator(NodeAST *node, Scope *scope) {

    DataType leftType = analyzeExpression(
        GET_LEFT(node),
        scope
    );

    DataType rightType = TYPE_VOID;

    if (GET_RIGHT(node) != NULL) {
        rightType = analyzeExpression(
            GET_RIGHT(node),
            scope
        );
    }

    DataType resultType = resolveBinaryOperation(
        node->operationType,
        leftType,
        rightType
    );

    if (resultType == TYPE_ERROR) {
        semanticError(node->line, "Tipos incompatibles en la operacion binaria.");
    }

    node->type = resultType;

    return resultType;
}

DataType analyzeExpression(NodeAST *node, Scope *scope) {
    if (node == NULL)
        return TYPE_ERROR;

    switch (node->nodeType) {

        case ID_NODE:
            return analyzeIdentifier(node, scope);

        case CONSTANT_NODE:
            node->type = node->symbol->type;
            return node->type;

        case METHOD_CALL_NODE:
            return analyzeMethodCall(node, scope);

        case BINARYOPERATOR_NODE:
            return analyzeBinaryOperator(node, scope);

        default:
            return TYPE_ERROR;
    }
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
            analyzeMethodNode(node, current_scope, node->type);
            break;
            
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
 
        case IF_ELSE_NODE:
            analyzeIfElseNode(node, current_scope);
            break;

        case WHILE_NODE:
            analyzeWhileNode(node, current_scope);
            break;
            
        default:
            for (int i = 0; i < node->childCount; i++) {
                analyzeNodeSemantics(node->children[i], current_scope);
            }
            break;
    }
}
// se encarga de por cada bloque que aparece, crea un nuevo scope / tabla de simbolos para ese scope
Scope* analyzeSemantics(NodeAST *root, Scope *parent, DataType returnType) {
    if (!root) return NULL;
    
    int level = parent ? parent->level + 1 : 0;
    
    Scope *current_scope = createScope(level, parent, returnType);
    
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

void analyzeWhileNode(NodeAST *node, Scope *current_scope) {
    NodeAST *condition = GET_CONDITION(node);
    DataType conditionType = analyzeExpression(condition, current_scope);

    if (conditionType != TYPE_ERROR && conditionType != TYPE_BOOL) {
        semanticError(node->line, "La condicion del while debe ser de tipo BOOL, pero es %s.", getDataTypeName(conditionType));
    }

    NodeAST *body = GET_IF_BLOCK(node);
    analyzeNodeSemantics(body, current_scope);
}

void analyzeIfElseNode(NodeAST *node, Scope *current_scope) {
    NodeAST *condition = GET_CONDITION(node);
    DataType conditionType = analyzeExpression(condition, current_scope);

    if (conditionType != TYPE_ERROR && conditionType != TYPE_BOOL) {
        semanticError(node->line, "La condicion del if debe ser de tipo BOOL, pero es %s.", getDataTypeName(conditionType));
    }

    NodeAST *ifBlock = GET_IF_BLOCK(node);
    analyzeNodeSemantics(ifBlock, current_scope);

    NodeAST *elseBlock = GET_ELSE_BLOCK(node);
    if (elseBlock != NULL) {
        analyzeNodeSemantics(elseBlock, current_scope);
    }
}