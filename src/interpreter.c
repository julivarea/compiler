#include "interpreter.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "error_handler.h"
#include "symbol_table.h"
#include "types.h"

typedef struct ExecResult {
    bool hasReturned;
    Symbol value;
} ExecResult;

static void registerGlobalDeclarations(NodeAST *root, Scope *globalScope);
static void registerMethodDeclaration(NodeAST *node, Scope *globalScope);
static void registerVariableDeclaration(NodeAST *node, Scope *currentScope);
static void executeMainMethod(Scope *globalScope);

static Symbol executeFunctionBody(NodeAST *methodDeclarationNode, NodeAST *argumentsNode, Scope *callerScope, Scope *globalScope);
static ExecResult executeBlock(NodeAST *node, Scope *currentScope);
static ExecResult executeStatement(NodeAST *node, Scope *currentScope);
static ExecResult executeAssignment(NodeAST *node, Scope *currentScope);
static ExecResult executeIfElse(NodeAST *node, Scope *currentScope);
static ExecResult executeWhile(NodeAST *node, Scope *currentScope);
static ExecResult executeReturn(NodeAST *node, Scope *currentScope);
static Symbol executeMethodCall(NodeAST *node, Scope *currentScope);

static Symbol evaluateExpression(NodeAST *node, Scope *currentScope);
static Symbol evaluateIdentifier(NodeAST *node, Scope *currentScope);
static Symbol evaluateConstant(NodeAST *node);
static Symbol evaluateOperator(NodeAST *node, Scope *currentScope);
static Symbol evaluateUnaryOperator(OperationType operation, Symbol operand);
static Symbol evaluateBinaryOperator(OperationType operation, Symbol left, Symbol right);

static Scope *getGlobalScope(Scope *scope);
static float getFloatValue(Symbol symbol);
static Symbol makeIntSymbol(int value);
static Symbol makeFloatSymbol(float value);
static Symbol makeBoolSymbol(bool value);
static Symbol makeVoidSymbol(void);
static bool isTruthy(Symbol symbol);

Scope *interpreteSemantics(NodeAST *root, Scope *parent) {
    if (root == NULL) {
        return NULL;
    }

    bool isGlobalScope = (parent == NULL);
    int level = !isGlobalScope ? parent->level + 1 : 0;

    Scope *currentScope = createScope(level, parent, NULL);
    if (currentScope == NULL) {
        semanticError(root->line, "Error: no se pudo reservar memoria para el ambito.");
        return NULL;
    }

    if (!isGlobalScope) {
        appendChildScope(parent, currentScope);
    }

    registerGlobalDeclarations(root, currentScope);

    if (isGlobalScope) {
        executeMainMethod(currentScope);
    }

    return currentScope;
}

static void registerGlobalDeclarations(NodeAST *root, Scope *globalScope) {
    if (root == NULL || globalScope == NULL) {
        return;
    }

    for (int i = 0; i < root->childCount; i++) {
        NodeAST *child = root->children[i];
        if (child == NULL) {
            continue;
        }

        if (child->nodeType == VARIABLE_DECLARATION_NODE) {
            registerVariableDeclaration(child, globalScope);
        } else if (child->nodeType == METHOD_DECLARATION_NODE) {
            registerMethodDeclaration(child, globalScope);
        }
    }
}

static void registerVariableDeclaration(NodeAST *node, Scope *currentScope) {
    if (node == NULL || node->symbol == NULL || currentScope == NULL) {
        return;
    }

    Symbol *symbol = newSymbol(node->symbol->id, VARIABLE_SYMBOL, node->type, NULL);
    if (symbol->type == TYPE_FLOAT) {
        symbol->value.floatValue = 0.0f;
    } else {
        symbol->value.intValue = 0;
    }
    appendSymbol(currentScope, symbol);
}

static void registerMethodDeclaration(NodeAST *node, Scope *globalScope) {
    if (node == NULL || node->symbol == NULL || globalScope == NULL) {
        return;
    }

    Symbol *symbol = newSymbol(node->symbol->id, FUNCTION_ENTRY_SYMBOL, node->type, node->symbol->parameters);
    symbol->value.function = node;
    appendSymbol(globalScope, symbol);
}

static void executeMainMethod(Scope *globalScope) {
    if (globalScope == NULL) {
        return;
    }

    Symbol *mainSymbol = lookupSymbol(globalScope, "main");
    if (mainSymbol == NULL || mainSymbol->symbolType != FUNCTION_ENTRY_SYMBOL) {
        semanticError(0, "Error en tiempo de ejecucion: no se encontro el metodo 'main'.");
        return;
    }

    NodeAST *mainDeclaration = mainSymbol->value.function;
    if (mainDeclaration == NULL) {
        return;
    }

    Symbol result = executeFunctionBody(mainDeclaration, NULL, globalScope, globalScope);
    if (result.type == TYPE_INT) {
        printf("[INTERPRETE] 'main' retorno el valor entero: %d\n", result.value.intValue);
    } else if (result.type == TYPE_FLOAT) {
        printf("[INTERPRETE] 'main' retorno el valor float: %f\n", result.value.floatValue);
    } else if (result.type == TYPE_BOOL) {
        printf("[INTERPRETE] 'main' retorno el valor booleano: %s\n", result.value.intValue ? "true" : "false");
    } else {
        printf("[INTERPRETE] 'main' finalizo correctamente (void)\n");
    }
}

static Symbol executeFunctionBody(NodeAST *methodDeclarationNode, NodeAST *argumentsNode, Scope *callerScope, Scope *globalScope) {
    if (methodDeclarationNode == NULL || globalScope == NULL) {
        return makeVoidSymbol();
    }

    int argumentCount = (argumentsNode != NULL) ? argumentsNode->childCount : 0;
    Symbol *argumentValues = NULL;
    if (argumentCount > 0) {
        argumentValues = malloc(sizeof(Symbol) * argumentCount);
        if (argumentValues != NULL) {
            for (int i = 0; i < argumentCount; i++) {
                argumentValues[i] = evaluateExpression(argumentsNode->children[i], callerScope);
            }
        }
    }

    Scope *functionScope = createScope(1, globalScope, NULL);
    if (functionScope == NULL) {
        free(argumentValues);
        return makeVoidSymbol();
    }
    appendChildScope(globalScope, functionScope);

    NodeAST *parametersNode = methodDeclarationNode->children[0];
    if (parametersNode != NULL) {
        for (int i = 0; i < parametersNode->childCount; i++) {
            NodeAST *parameterNode = parametersNode->children[i];
            if (parameterNode != NULL && parameterNode->symbol != NULL) {
                Symbol *parameterSymbol = newSymbol(parameterNode->symbol->id, VARIABLE_SYMBOL, parameterNode->type, NULL);
                if (argumentValues != NULL && i < argumentCount) {
                    bool needsFloatCoercion = (parameterSymbol->type == TYPE_FLOAT && argumentValues[i].type == TYPE_INT);
                    if (needsFloatCoercion) {
                        parameterSymbol->value.floatValue = (float)argumentValues[i].value.intValue;
                    } else if (parameterSymbol->type == TYPE_FLOAT) {
                        parameterSymbol->value.floatValue = argumentValues[i].value.floatValue;
                    } else {
                        parameterSymbol->value.intValue = argumentValues[i].value.intValue;
                    }
                }
                appendSymbol(functionScope, parameterSymbol);
            }
        }
    }

    if (argumentValues != NULL) {
        free(argumentValues);
    }

    NodeAST *bodyNode = methodDeclarationNode->children[1];
    ExecResult result = { .hasReturned = false, .value = makeVoidSymbol() };
    if (bodyNode != NULL) {
        result = executeBlock(bodyNode, functionScope);
    }

    bool needsFloatCoercion = (methodDeclarationNode->type == TYPE_FLOAT && result.value.type == TYPE_INT);
    if (needsFloatCoercion) {
        result.value = makeFloatSymbol((float)result.value.value.intValue);
    }

    return result.value;
}

static ExecResult executeBlock(NodeAST *node, Scope *currentScope) {
    ExecResult result = { .hasReturned = false, .value = makeVoidSymbol() };
    if (node == NULL || currentScope == NULL) {
        return result;
    }

    Scope *blockScope = createScope(currentScope->level + 1, currentScope, NULL);
    if (blockScope == NULL) {
        return result;
    }
    appendChildScope(currentScope, blockScope);

    for (int i = 0; i < node->childCount; i++) {
        result = executeStatement(node->children[i], blockScope);
        if (result.hasReturned) {
            return result;
        }
    }

    return result;
}

static ExecResult executeStatement(NodeAST *node, Scope *currentScope) {
    ExecResult result = { .hasReturned = false, .value = makeVoidSymbol() };
    if (node == NULL) {
        return result;
    }

    switch (node->nodeType) {
        case BLOCK_NODE:
            return executeBlock(node, currentScope);

        case VARIABLE_DECLARATION_NODE:
            registerVariableDeclaration(node, currentScope);
            return result;

        case ASSIGNMENT_NODE:
            return executeAssignment(node, currentScope);

        case IF_ELSE_NODE:
            return executeIfElse(node, currentScope);

        case WHILE_NODE:
            return executeWhile(node, currentScope);

        case RETURN_NODE:
            return executeReturn(node, currentScope);

        case METHOD_CALL_NODE:
            executeMethodCall(node, currentScope);
            return result;

        default:
            return result;
    }
}

static ExecResult executeAssignment(NodeAST *node, Scope *currentScope) {
    ExecResult result = { .hasReturned = false, .value = makeVoidSymbol() };
    if (node == NULL || node->symbol == NULL || node->symbol->id == NULL || currentScope == NULL) {
        return result;
    }

    Symbol assignedValue = evaluateExpression(GET_EXPRESSION(node), currentScope);
    Symbol *target = lookupSymbol(currentScope, node->symbol->id);
    if (target != NULL) {
        bool needsFloatCoercion = (target->type == TYPE_FLOAT && assignedValue.type == TYPE_INT);
        if (needsFloatCoercion) {
            target->value.floatValue = (float)assignedValue.value.intValue;
        } else if (target->type == TYPE_FLOAT) {
            target->value.floatValue = assignedValue.value.floatValue;
        } else {
            target->value.intValue = assignedValue.value.intValue;
        }
    }

    return result;
}

static ExecResult executeIfElse(NodeAST *node, Scope *currentScope) {
    ExecResult result = { .hasReturned = false, .value = makeVoidSymbol() };
    if (node == NULL) {
        return result;
    }

    Symbol condition = evaluateExpression(GET_CONDITION(node), currentScope);
    if (isTruthy(condition)) {
        if (GET_IF_BLOCK(node) != NULL) {
            return executeStatement(GET_IF_BLOCK(node), currentScope);
        }
    } else if (GET_ELSE_BLOCK(node) != NULL) {
        return executeStatement(GET_ELSE_BLOCK(node), currentScope);
    }

    return result;
}

static ExecResult executeWhile(NodeAST *node, Scope *currentScope) {
    ExecResult result = { .hasReturned = false, .value = makeVoidSymbol() };
    if (node == NULL) {
        return result;
    }

    while (isTruthy(evaluateExpression(GET_CONDITION(node), currentScope))) {
        result = executeStatement(GET_WHILE_BLOCK(node), currentScope);
        if (result.hasReturned) {
            break;
        }
    }

    return result;
}

static ExecResult executeReturn(NodeAST *node, Scope *currentScope) {
    ExecResult result = { .hasReturned = true, .value = makeVoidSymbol() };
    if (node == NULL) {
        return result;
    }

    if (GET_EXPRESSION(node) != NULL) {
        result.value = evaluateExpression(GET_EXPRESSION(node), currentScope);
    }

    return result;
}

static Symbol executeMethodCall(NodeAST *node, Scope *currentScope) {
    if (node == NULL || node->symbol == NULL || node->symbol->id == NULL || currentScope == NULL) {
        return makeVoidSymbol();
    }

    Symbol *functionSymbol = lookupSymbol(currentScope, node->symbol->id);
    if (functionSymbol == NULL || functionSymbol->symbolType != FUNCTION_ENTRY_SYMBOL) {
        semanticError(node->line, "Metodo '%s' no encontrado.", node->symbol->id);
        return makeVoidSymbol();
    }

    NodeAST *functionDeclaration = functionSymbol->value.function;
    if (functionDeclaration == NULL) {
        return makeVoidSymbol();
    }

    Scope *globalScope = getGlobalScope(currentScope);
    return executeFunctionBody(functionDeclaration, node, currentScope, globalScope);
}

static Symbol evaluateExpression(NodeAST *node, Scope *currentScope) {
    if (node == NULL) {
        return makeVoidSymbol();
    }

    switch (node->nodeType) {
        case CONSTANT_NODE:
            return evaluateConstant(node);

        case ID_NODE:
            return evaluateIdentifier(node, currentScope);

        case OPERATOR_NODE:
            return evaluateOperator(node, currentScope);

        case METHOD_CALL_NODE:
            return executeMethodCall(node, currentScope);

        default:
            return makeVoidSymbol();
    }
}

static Symbol evaluateIdentifier(NodeAST *node, Scope *currentScope) {
    if (node == NULL || node->symbol == NULL || node->symbol->id == NULL) {
        return makeVoidSymbol();
    }

    Symbol *symbol = lookupSymbol(currentScope, node->symbol->id);
    if (symbol == NULL) {
        semanticError(node->line, "Identificador '%s' no declarado.", node->symbol->id);
        return makeVoidSymbol();
    }

    return *symbol;
}

static Symbol evaluateConstant(NodeAST *node) {
    if (node == NULL || node->symbol == NULL) {
        return makeVoidSymbol();
    }

    switch (node->symbol->type) {
        case TYPE_INT:
            return makeIntSymbol(node->symbol->value.intValue);

        case TYPE_FLOAT:
            return makeFloatSymbol(node->symbol->value.floatValue);

        case TYPE_BOOL:
            return makeBoolSymbol(node->symbol->value.intValue != 0);

        default:
            return makeVoidSymbol();
    }
}

static Symbol evaluateOperator(NodeAST *node, Scope *currentScope) {
    if (node == NULL) {
        return makeVoidSymbol();
    }

    bool isUnary = (node->childCount == 1 || GET_RIGHT(node) == NULL);
    if (isUnary) {
        Symbol operand = evaluateExpression(GET_LEFT(node), currentScope);
        return evaluateUnaryOperator(node->operationType, operand);
    }

    if (node->operationType == OP_AND) {
        Symbol left = evaluateExpression(GET_LEFT(node), currentScope);
        if (!isTruthy(left)) {
            return makeBoolSymbol(false);
        }
        Symbol right = evaluateExpression(GET_RIGHT(node), currentScope);
        return makeBoolSymbol(isTruthy(right));
    }

    if (node->operationType == OP_OR) {
        Symbol left = evaluateExpression(GET_LEFT(node), currentScope);
        if (isTruthy(left)) {
            return makeBoolSymbol(true);
        }
        Symbol right = evaluateExpression(GET_RIGHT(node), currentScope);
        return makeBoolSymbol(isTruthy(right));
    }

    Symbol left = evaluateExpression(GET_LEFT(node), currentScope);
    Symbol right = evaluateExpression(GET_RIGHT(node), currentScope);
    return evaluateBinaryOperator(node->operationType, left, right);
}

static Symbol evaluateUnaryOperator(OperationType operation, Symbol operand) {
    switch (operation) {
        case OP_NOT:
            return makeBoolSymbol(!isTruthy(operand));

        case OP_UMINUS:
            if (operand.type == TYPE_FLOAT) {
                return makeFloatSymbol(-operand.value.floatValue);
            } else if (operand.type == TYPE_INT) {
                return makeIntSymbol(-operand.value.intValue);
            }
            break;

        default:
            break;
    }
    return operand;
}

static Symbol evaluateBinaryOperator(OperationType operation, Symbol left, Symbol right) {
    bool isFloat = (left.type == TYPE_FLOAT || right.type == TYPE_FLOAT);

    switch (operation) {
        case OP_ADD:
            if (isFloat) {
                float leftValue = getFloatValue(left);
                float rightValue = getFloatValue(right);
                return makeFloatSymbol(leftValue + rightValue);
            }
            return makeIntSymbol(left.value.intValue + right.value.intValue);

        case OP_SUB:
            if (isFloat) {
                float leftValue = getFloatValue(left);
                float rightValue = getFloatValue(right);
                return makeFloatSymbol(leftValue - rightValue);
            }
            return makeIntSymbol(left.value.intValue - right.value.intValue);

        case OP_MUL:
            if (isFloat) {
                float leftValue = getFloatValue(left);
                float rightValue = getFloatValue(right);
                return makeFloatSymbol(leftValue * rightValue);
            }
            return makeIntSymbol(left.value.intValue * right.value.intValue);

        case OP_DIV:
            if (isFloat) {
                float leftValue = getFloatValue(left);
                float rightValue = getFloatValue(right);
                if (rightValue == 0.0f) {
                    semanticError(0, "Division por cero.");
                    return makeFloatSymbol(0.0f);
                }
                return makeFloatSymbol(leftValue / rightValue);
            }
            if (right.value.intValue == 0) {
                semanticError(0, "Division por cero.");
                return makeIntSymbol(0);
            }
            return makeIntSymbol(left.value.intValue / right.value.intValue);

        case OP_MOD:
            if (right.value.intValue == 0) {
                semanticError(0, "Division por cero en operacion modulo.");
                return makeIntSymbol(0);
            }
            return makeIntSymbol(left.value.intValue % right.value.intValue);

        case OP_LT:
            if (isFloat) {
                float leftValue = getFloatValue(left);
                float rightValue = getFloatValue(right);
                return makeBoolSymbol(leftValue < rightValue);
            }
            return makeBoolSymbol(left.value.intValue < right.value.intValue);

        case OP_GT:
            if (isFloat) {
                float leftValue = getFloatValue(left);
                float rightValue = getFloatValue(right);
                return makeBoolSymbol(leftValue > rightValue);
            }
            return makeBoolSymbol(left.value.intValue > right.value.intValue);

        case OP_EQ:
            if (isFloat) {
                float leftValue = getFloatValue(left);
                float rightValue = getFloatValue(right);
                return makeBoolSymbol(leftValue == rightValue);
            }
            return makeBoolSymbol(left.value.intValue == right.value.intValue);

        case OP_AND:
            return makeBoolSymbol(isTruthy(left) && isTruthy(right));

        case OP_OR:
            return makeBoolSymbol(isTruthy(left) || isTruthy(right));

        default:
            return makeVoidSymbol();
    }
}

static Scope *getGlobalScope(Scope *scope) {
    Scope *current = scope;
    while (current != NULL && current->parent != NULL) {
        current = current->parent;
    }
    return current;
}

static float getFloatValue(Symbol symbol) {
    return (symbol.type == TYPE_FLOAT) ? symbol.value.floatValue : (float)symbol.value.intValue;
}

static Symbol makeIntSymbol(int value) {
    Symbol symbol;
    memset(&symbol, 0, sizeof(Symbol));
    symbol.symbolType = CONSTANT_SYMBOL;
    symbol.type = TYPE_INT;
    symbol.value.intValue = value;
    return symbol;
}

static Symbol makeFloatSymbol(float value) {
    Symbol symbol;
    memset(&symbol, 0, sizeof(Symbol));
    symbol.symbolType = CONSTANT_SYMBOL;
    symbol.type = TYPE_FLOAT;
    symbol.value.floatValue = value;
    return symbol;
}

static Symbol makeBoolSymbol(bool value) {
    Symbol symbol;
    memset(&symbol, 0, sizeof(Symbol));
    symbol.symbolType = CONSTANT_SYMBOL;
    symbol.type = TYPE_BOOL;
    symbol.value.intValue = value ? 1 : 0;
    return symbol;
}

static Symbol makeVoidSymbol(void) {
    Symbol symbol;
    memset(&symbol, 0, sizeof(Symbol));
    symbol.symbolType = CONSTANT_SYMBOL;
    symbol.type = TYPE_VOID;
    return symbol;
}

static bool isTruthy(Symbol symbol) {
    if (symbol.type == TYPE_BOOL || symbol.type == TYPE_INT) {
        return symbol.value.intValue != 0;
    } else if (symbol.type == TYPE_FLOAT) {
        return symbol.value.floatValue != 0.0f;
    }
    return false;
}
