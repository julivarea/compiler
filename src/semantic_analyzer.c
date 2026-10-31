#include "semantic_analyzer.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error_handler.h"
#include "type_checker.h"

static void analyzeNodeSemantics(NodeAST *node, Scope *currentScope);
static void analyzeMethodNode(NodeAST *node, Scope *currentScope);
static void analyzeVariableDeclarationNode(NodeAST *node, Scope *currentScope);
static void analyzeAssignmentNode(NodeAST *node, Scope *currentScope);
static void analyzeReturnNode(NodeAST *node, Scope *currentScope);
static void analyzeIfElseNode(NodeAST *node, Scope *currentScope);
static void analyzeWhileNode(NodeAST *node, Scope *currentScope);

static bool guaranteesReturn(NodeAST *node);
static void checkMainMethodExists(Scope *globalScope);

Scope *analyzeSemantics(NodeAST *root, Scope *parent) {
    if (root == NULL) return NULL;

    bool isGlobalScope = (parent == NULL);
    int level = !isGlobalScope ? parent->level + 1 : 0;
    Symbol *function = !isGlobalScope ? parent->function : NULL;

    Scope *currentScope = createScope(level, parent, function);
    if (currentScope == NULL) {
        semanticError(root->line, "Error: no se pudo reservar memoria para el ambito.");
        return NULL;
    }

    if (!isGlobalScope) {
        appendChildScope(parent, currentScope);
    }

    for (int i = 0; i < root->childCount; i++) {
        analyzeNodeSemantics(root->children[i], currentScope);
    }

    if (isGlobalScope) {
        checkMainMethodExists(currentScope);
    }

    return currentScope;
}

static void analyzeNodeSemantics(NodeAST *node, Scope *currentScope) {
    if (node == NULL) return;

    switch (node->nodeType) {
        case BLOCK_NODE:
            analyzeSemantics(node, currentScope);
            break;

        case METHOD_DECLARATION_NODE:
            analyzeMethodNode(node, currentScope);
            break;

        case METHOD_CALL_NODE:
            analyzeMethodCall(node, currentScope, 0); // isExpression = 0 (llamada como sentencia)
            break;

        case VARIABLE_DECLARATION_NODE:
            analyzeVariableDeclarationNode(node, currentScope);
            break;

        case ASSIGNMENT_NODE:
            analyzeAssignmentNode(node, currentScope);
            break;

        case RETURN_NODE:
            analyzeReturnNode(node, currentScope);
            break;

        case IF_ELSE_NODE:
            analyzeIfElseNode(node, currentScope);
            break;

        case WHILE_NODE:
            analyzeWhileNode(node, currentScope);
            break;

        default:
            for (int i = 0; i < node->childCount; i++) {
                analyzeNodeSemantics(node->children[i], currentScope);
            }
            break;
    }
}

static void analyzeMethodNode(NodeAST *node, Scope *currentScope) {
    bool isAlreadyDeclared = (lookupSymbolInScope(currentScope, node->symbol->id) != NULL);
    if (isAlreadyDeclared) {
        semanticError(node->line, "El metodo '%s' ya fue declarado previamente en este ambito.", node->symbol->id);
        return;
    }

    appendSymbol(currentScope, node->symbol);

    int level = currentScope->level + 1;
    Scope *methodScope = createScope(level, currentScope, node->symbol);
    if (methodScope == NULL) {
        semanticError(node->line, "Error: no se pudo reservar memoria para el ambito del metodo.");
        return;
    }

    NodeAST *parameters = GET_LEFT(node);
    bool hasParameters = (parameters != NULL);
    if (hasParameters) {
        for (int i = 0; i < parameters->childCount; i++) {
            analyzeNodeSemantics(parameters->children[i], methodScope);
        }
    }

    NodeAST *body = GET_RIGHT(node);
    bool hasBody = (body != NULL);
    if (hasBody) {
        for (int i = 0; i < body->childCount; i++) {
            analyzeNodeSemantics(body->children[i], methodScope);
        }
    }

    bool requiresReturn = (node->type != TYPE_VOID);
    bool hasGuaranteedReturn = guaranteesReturn(body);
    if (requiresReturn && !hasGuaranteedReturn) {
        const char *methodName = node->symbol->id ? node->symbol->id : "anonimo";
        semanticError(node->line, "El metodo '%s' no es void: todos sus caminos de ejecucion deben terminar en un return.", methodName);
    }

    appendChildScope(currentScope, methodScope);
}

static void analyzeVariableDeclarationNode(NodeAST *node, Scope *currentScope) {
    bool isAlreadyDeclared = (lookupSymbolInScope(currentScope, node->symbol->id) != NULL);
    if (isAlreadyDeclared) {
        semanticError(node->line, "La variable '%s' ya fue declarada previamente en este ambito.", node->symbol->id);
        return;
    }

    appendSymbol(currentScope, node->symbol);
}

static void analyzeAssignmentNode(NodeAST *node, Scope *currentScope) {
    node->type = TYPE_ERROR;

    Symbol *resolvedSymbol = lookupSymbol(currentScope, node->symbol->id);
    bool isVariableDeclared = (resolvedSymbol != NULL);
    if (!isVariableDeclared) {
        semanticError(node->line, "Asignacion a variable no declarada '%s'.", node->symbol->id);
        return;
    }

    bool isVariable = (resolvedSymbol->symbolType == VARIABLE_SYMBOL);
    if (!isVariable) {
        semanticError(node->line, "Ese nombre de simbolo corresponde al metodo '%s'.", node->symbol->id);
        return;
    }

    node->symbol = resolvedSymbol;

    NodeAST *expression = GET_EXPRESSION(node);
    bool hasExpression = (expression != NULL);
    if (hasExpression) {
        DataType expressionType = analyzeExpression(expression, currentScope);
        bool isExpressionValid = (expressionType != TYPE_ERROR);

        if (isExpressionValid) {
            bool areTypesCompatible = isAssignable(resolvedSymbol->type, expressionType);
            if (!areTypesCompatible) {
                semanticError(node->line, "El tipo de la expresion no coincide con la variable '%s'.", node->symbol->id);
            } else {
                node->type = resolvedSymbol->type;
            }
        }
    }
}

static void analyzeReturnNode(NodeAST *node, Scope *currentScope) {
    bool isInsideFunction = (currentScope != NULL && currentScope->function != NULL);
    if (!isInsideFunction) {
        semanticError(node->line, "Sentencia 'return' fuera del cuerpo de una funcion.");
        return;
    }

    Symbol *function = currentScope->function;
    DataType expectedType = function->type;
    const char *functionName = function->id ? function->id : "anonima";

    NodeAST *expression = GET_EXPRESSION(node);
    bool hasReturnValue = (expression != NULL);
    bool isVoidMethod = (expectedType == TYPE_VOID);

    if (!hasReturnValue) {
        if (!isVoidMethod) {
            semanticError(node->line, "El metodo '%s' espera un valor de retorno de tipo %s, pero no se especifico ninguno.", functionName, getDataTypeName(expectedType));
        }
        return;
    }

    if (isVoidMethod) {
        semanticError(node->line, "El metodo '%s' es void y no deberia retornar un valor.", functionName);
        return;
    }

    DataType actualType = analyzeExpression(expression, currentScope);
    bool isExpressionValid = (actualType != TYPE_ERROR);
    bool areTypesCompatible = isAssignable(expectedType, actualType);

    if (isExpressionValid && !areTypesCompatible) {
        semanticError(node->line, "El tipo de retorno no coincide en el metodo '%s': se esperaba %s pero se obtuvo %s.", functionName, getDataTypeName(expectedType), getDataTypeName(actualType));
    }
}

static void analyzeIfElseNode(NodeAST *node, Scope *currentScope) {
    NodeAST *condition = GET_CONDITION(node);
    DataType conditionType = analyzeExpression(condition, currentScope);

    bool isInvalidConditionType = (conditionType != TYPE_ERROR && conditionType != TYPE_BOOL);
    if (isInvalidConditionType) {
        semanticError(node->line, "La condicion del if debe ser de tipo BOOL, pero es %s.", getDataTypeName(conditionType));
    }

    NodeAST *ifBlock = GET_IF_BLOCK(node);
    analyzeNodeSemantics(ifBlock, currentScope);

    NodeAST *elseBlock = GET_ELSE_BLOCK(node);
    bool hasElseBlock = (elseBlock != NULL);
    if (hasElseBlock) {
        analyzeNodeSemantics(elseBlock, currentScope);
    }
}

static void analyzeWhileNode(NodeAST *node, Scope *currentScope) {
    NodeAST *condition = GET_CONDITION(node);
    DataType conditionType = analyzeExpression(condition, currentScope);

    bool isInvalidConditionType = (conditionType != TYPE_ERROR && conditionType != TYPE_BOOL);
    if (isInvalidConditionType) {
        semanticError(node->line, "La condicion del while debe ser de tipo BOOL, pero es %s.", getDataTypeName(conditionType));
    }

    NodeAST *body = GET_IF_BLOCK(node);
    analyzeNodeSemantics(body, currentScope);
}

static bool guaranteesReturn(NodeAST *node) {
    if (node == NULL) return false;

    switch (node->nodeType) {
        case RETURN_NODE:
            return true;

        case BLOCK_NODE:
            for (int i = 0; i < node->childCount; i++) {
                if (guaranteesReturn(node->children[i])) return true;
            }
            return false;

        case IF_ELSE_NODE: {
            NodeAST *elseBlock = GET_ELSE_BLOCK(node);
            bool hasElseBlock = (elseBlock != NULL);
            if (!hasElseBlock) return false;

            return guaranteesReturn(GET_IF_BLOCK(node)) && guaranteesReturn(elseBlock);
        }

        default:
            return false;
    }
}

static void checkMainMethodExists(Scope *globalScope) {
    Symbol *mainSymbol = lookupSymbolInScope(globalScope, "main");

    bool hasMainMethod = (mainSymbol != NULL);
    if (!hasMainMethod) {
        semanticError(0, "El programa no contiene la definicion del metodo 'main'.");
        return;
    }

    bool isMethod = (mainSymbol->symbolType == METHOD_SYMBOL);
    if (!isMethod) {
        semanticError(0, "'main' debe ser un metodo, no una variable.");
        return;
    }

    NodeAST *parameters = mainSymbol->parameters;
    int parameterCount = (parameters != NULL) ? parameters->childCount : 0;
    bool hasParameters = (parameterCount != 0);

    if (hasParameters) {
        semanticError(0, "El metodo 'main' no debe tener parametros.");
    }
}
