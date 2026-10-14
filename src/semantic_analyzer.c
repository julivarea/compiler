#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "semantic_analyzer.h"
#include "type_checker.h"
#include "error_handler.h"
#include "string_builder.h"

static Symbol *current_function = NULL;

static void analyzeNodeSemantics(NodeAST *node, Scope *current_scope);
static void analyzeMethodNode(NodeAST *node, Scope *current_scope, DataType type);
static void analyzeBlockNode(NodeAST *node, Scope *current_scope);
static void analyzeVariableDeclarationNode(NodeAST *node, Scope *current_scope);
static void analyzeAssignmentNode(NodeAST *node, Scope *current_scope);
static void analyzeReturnNode(NodeAST *node, Scope *current_scope);
static void analyzeIfElseNode(NodeAST *node, Scope *current_scope);
static void analyzeWhileNode(NodeAST *node, Scope *current_scope);
static void analyzeIdNode(NodeAST *node, Scope *current_scope);

static void analyzeMethodNode(NodeAST *node, Scope *current_scope, DataType type) {
    if (node->symbol) {
        Symbol *existing = lookupVariableCurrentScope(current_scope, node->symbol->id);
        if (existing != NULL) {
            semanticError(node->line, "El metodo '%s' ya fue declarado previamente en este ambito.", node->symbol->id);
        } else {
            appendSymbol(current_scope, node->symbol);
        }
    }

    Symbol *prev_function = current_function;
    current_function = node->symbol;

    Scope *child_scope = analyzeSemantics(node, current_scope, type);
    appendChildScope(current_scope, child_scope);

    current_function = prev_function;
}

static void analyzeBlockNode(NodeAST *node, Scope *current_scope) {
    Scope *child_scope = analyzeSemantics(node, current_scope, current_scope->returnType);
    appendChildScope(current_scope, child_scope);
}

static void analyzeVariableDeclarationNode(NodeAST *node, Scope *current_scope) {
    if (node->symbol) {
        Symbol *existing = lookupVariableCurrentScope(current_scope, node->symbol->id);
        if (existing != NULL) {
            semanticError(node->line, "La variable '%s' ya fue declarada previamente en este ambito.", node->symbol->id);
        } else {
            appendSymbol(current_scope, node->symbol);
        }
    }
}

static void analyzeAssignmentNode(NodeAST *node, Scope *current_scope) {
    DataType varType = TYPE_ERROR;

    if (node->symbol) {
        Symbol *resolvedSymbol = lookupVariable(current_scope, node->symbol->id);
        if (!resolvedSymbol) {
            semanticError(node->line, "Asignacion a variable no declarada '%s'.", node->symbol->id);
        } else {
            varType = resolvedSymbol->type;

            // La gramatica crea dos simbolos nuevos para el destino de la
            // asignacion (uno para este nodo y otro para su ID_NODE hijo), sin
            // tipo y sin relacion con la tabla. Los reapuntamos al simbolo
            // declarado para que el generador de codigo sepa a que variable se
            // esta asignando -- con su uid y su tipo -- sin volver a resolver
            // nombres. Los duplicados quedan huerfanos a proposito: liberarlos
            // haria que un segundo recorrido del mismo AST leyera memoria ya
            // devuelta.
            node->symbol = resolvedSymbol;
            node->type = varType;

            NodeAST *target = GET_LEFT(node);
            if (target != NULL && target->nodeType == ID_NODE) {
                target->symbol = resolvedSymbol;
                target->type = varType;
            }
        }
    }

    // Verifica la expresion y que los tipos coincidan
    if (node->childCount > 1 && node->children[1] != NULL) {
        DataType exprType = analyzeExpression(node->children[1], current_scope);
        
        if (varType != TYPE_ERROR && exprType != TYPE_ERROR && !isAssignable(varType, exprType)) {
             semanticError(node->line, "El tipo de la expresion no coincide con la variable '%s'.", node->symbol->id);
        }
    }
}

static void analyzeReturnNode(NodeAST *node, Scope *current_scope) {
    if (current_function == NULL) {
        semanticError(node->line, "Sentencia 'return' fuera del cuerpo de una funcion.");
        return;
    }

    NodeAST *expression = GET_EXPRESSION(node);
    DataType expectedType = current_scope->returnType;
    const char *funcName = current_function->id ? current_function->id : "anonima";

    if (expression == NULL) {
        if (expectedType != TYPE_VOID) {
            semanticError(node->line, "El metodo '%s' espera un valor de retorno de tipo %s, pero no se especifico ninguno.", funcName, getDataTypeName(expectedType));
        }
        return;
    }

    if (expectedType == TYPE_VOID) {
        semanticError(node->line, "El metodo '%s' es void y no deberia retornar un valor.", funcName);
        return;
    }

    DataType actualType = analyzeExpression(expression, current_scope);

    if (actualType != TYPE_ERROR && !isAssignable(expectedType, actualType)) {
        semanticError(node->line, "El tipo de retorno no coincide en el metodo '%s': se esperaba %s pero se obtuvo %s.", funcName, getDataTypeName(expectedType), getDataTypeName(actualType));
    }
}

static void analyzeIdNode(NodeAST *node, Scope *current_scope) {
    if (node->symbol) {
        Symbol *resolvedSymbol = lookupVariable(current_scope, node->symbol->id);
        if (!resolvedSymbol) {
            semanticError(node->line, "Uso de variable no declarada '%s'.", node->symbol->id);
        }
    }
}

static void analyzeWhileNode(NodeAST *node, Scope *current_scope) {
    NodeAST *condition = GET_CONDITION(node);
    DataType conditionType = analyzeExpression(condition, current_scope);

    if (conditionType != TYPE_ERROR && conditionType != TYPE_BOOL) {
        semanticError(node->line, "La condicion del while debe ser de tipo BOOL, pero es %s.", getDataTypeName(conditionType));
    }

    NodeAST *body = GET_IF_BLOCK(node);
    analyzeNodeSemantics(body, current_scope);
}

static void analyzeIfElseNode(NodeAST *node, Scope *current_scope) {
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

static void analyzeNodeSemantics(NodeAST *node, Scope *current_scope) {
    if (!node) return;
    
    switch (node->nodeType) {
        case BLOCK_NODE:
            analyzeBlockNode(node, current_scope);
            break;

        case METHOD_DECLARATION_NODE:
            analyzeMethodNode(node, current_scope, node->type);
            break;
            
        case METHOD_CALL_NODE:
            analyzeMethodCall(node, current_scope, 0); // 0 porque viene de Statement
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
            break;
 
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

Scope* analyzeSemantics(NodeAST *root, Scope *parent, DataType returnType) {
    if (!root) return NULL;
    
    int level = parent ? parent->level + 1 : 0;
    Scope *current_scope = createScope(level, parent, returnType);
    
    for (int i = 0; i < root->childCount; i++) {
        analyzeNodeSemantics(root->children[i], current_scope);
    }
    
    return current_scope;
}

void checkMainMethodExists(Scope *globalScope) {
    Symbol *mainSymbol = lookupVariableCurrentScope(globalScope, "main");

    if (mainSymbol == NULL) {
        semanticError(0, "El programa no contiene la definicion del metodo 'main'.");
        return;
    }

    if (mainSymbol->symbolType != METHOD_SYMBOL) {
        semanticError(0, "'main' debe ser un metodo, no una variable.");
        return;
    }

    NodeAST *parametersNode = mainSymbol->parameters;
    int paramCount = parametersNode ? parametersNode->childCount : 0;

    if (paramCount != 0) {
        semanticError(0, "El metodo 'main' no debe tener parametros.");
    }
}
