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
static void checkMainMethodExists(Scope *globalScope);


/**
 * Determina si una sentencia garantiza que la ejecucion retorna antes de
 * alcanzar el fin del metodo. La letra exige esto de todo metodo no-void: no
 * alcanza con que exista un return en algun lado, tienen que retornar todos
 * los caminos de ejecucion.
 *
 * - Un return siempre garantiza.
 * - Un bloque garantiza si alguna de sus sentencias garantiza (lo que venga
 *   despues de ese punto es inalcanzable).
 * - Un if garantiza solo si tiene else y ambas ramas garantizan.
 * - Un while nunca garantiza: su condicion puede ser falsa en la primera
 *   evaluacion y el cuerpo no ejecutarse nunca.
 */
static int guaranteesReturn(NodeAST *node) {
    if (node == NULL) return 0;

    switch (node->nodeType) {
        case RETURN_NODE:
            return 1;

        case BLOCK_NODE:
            for (int i = 0; i < node->childCount; i++) {
                if (guaranteesReturn(node->children[i])) return 1;
            }
            return 0;

        case IF_ELSE_NODE: {
            NodeAST *elseBlock = GET_ELSE_BLOCK(node);
            if (elseBlock == NULL) return 0;

            return guaranteesReturn(GET_IF_BLOCK(node)) && guaranteesReturn(elseBlock);
        }

        default:
            return 0;
    }
}

static void analyzeMethodNode(NodeAST *node, Scope *current_scope, DataType type) {
    Symbol *existing = lookupVariableCurrentScope(current_scope, node->symbol->id);
    if (existing != NULL) {
        semanticError(node->line, "El metodo '%s' ya fue declarado previamente en este ambito.", node->symbol->id);
    } else {
        appendSymbol(current_scope, node->symbol);
    }

    Symbol *prev_function = current_function;
    current_function = node->symbol;

    int level = current_scope ? current_scope->level + 1 : 0;
    Scope *method_scope = createScope(level, current_scope, type);
    if (!method_scope) {
        semanticError(node->line, "Error: no se pudo reservar memoria para el ambito del metodo.");
        current_function = prev_function;
        return;
    }

    NodeAST *params = GET_LEFT(node);
    if (params) {
        for (int i = 0; i < params->childCount; i++) {
            analyzeNodeSemantics(params->children[i], method_scope);
        }
    }

    NodeAST *body = GET_RIGHT(node);
    if (body) {
        for (int i = 0; i < body->childCount; i++) {
            analyzeNodeSemantics(body->children[i], method_scope);
        }
    }

    if (node->type != TYPE_VOID && !guaranteesReturn(body)) {
        const char *methodName = node->symbol->id ? node->symbol->id : "anonimo";
        semanticError(node->line, "El metodo '%s' no es void: todos sus caminos de ejecucion deben terminar en un return.", methodName);
    }

    appendChildScope(current_scope, method_scope);

    current_function = prev_function;
}

static void analyzeBlockNode(NodeAST *node, Scope *current_scope) {
    Scope *child_scope = analyzeSemantics(node, current_scope, current_scope->returnType);
    appendChildScope(current_scope, child_scope);
}

static void analyzeVariableDeclarationNode(NodeAST *node, Scope *current_scope) {
    Symbol *existing = lookupVariableCurrentScope(current_scope, node->symbol->id);
    if (existing != NULL) {
        semanticError(node->line, "La variable '%s' ya fue declarada previamente en este ambito.", node->symbol->id);
    } else {
        appendSymbol(current_scope, node->symbol);
    }
}

static void analyzeAssignmentNode(NodeAST *node, Scope *current_scope) {
    DataType varType = TYPE_ERROR;

    Symbol *resolvedSymbol = lookupVariable(current_scope, node->symbol->id);
    if (!resolvedSymbol) {
        semanticError(node->line, "Asignacion a variable no declarada '%s'.", node->symbol->id);
    } else {
        if (resolvedSymbol->symbolType != ID_SYMBOL) {
            semanticError(node->line, "Ese nombre de simbolo corresponde al metodo '%s'.", node->symbol->id);
        }

        varType = resolvedSymbol->type;
        node->symbol = resolvedSymbol;
        node->type = varType;

        NodeAST *target = GET_LEFT(node);
        if (target != NULL && target->nodeType == ID_NODE) {
            target->symbol = resolvedSymbol;
            target->type = varType;
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
    if (!current_scope) {
        semanticError(root->line, "Error: no se pudo reservar memoria para el ambito.");
        return NULL;
    }
    
    for (int i = 0; i < root->childCount; i++) {
        analyzeNodeSemantics(root->children[i], current_scope);
    }

    // El scope global es el unico que puede declarar 'main', y se reconoce por
    // no tener padre. Validarlo aca hace que cualquier invocacion del analisis
    // -- el compilador o un test unitario -- verifique su presencia y su firma
    // sin tener que acordarse de pedirlo por separado.
    if (parent == NULL) {
        checkMainMethodExists(current_scope);
    }

    return current_scope;
}

static void checkMainMethodExists(Scope *globalScope) {
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
