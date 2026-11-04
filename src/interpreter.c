#include "interpreter.h"

#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "debug.h"
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
static void executeMainMethod(Scope *globalScope, bool debug);

static Symbol executeFunctionBody(NodeAST *methodDeclarationNode, NodeAST *argumentsNode, Scope *callerScope, Scope *globalScope);
static Symbol *evaluateArguments(NodeAST *argumentsNode, Scope *callerScope);
static Scope *createFunctionScope(Scope *globalScope);
static void bindParameters(Scope *functionScope, NodeAST *parametersNode, Symbol *argumentValues);
static void assignSymbolValue(Symbol *target, Symbol source);
static Symbol coerceReturnValue(DataType expectedType, Symbol result);
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
static bool isTruthy(Symbol symbol);

Scope *interpreteSemantics(NodeAST *root, bool debug) {
    if (root == NULL) {
        return NULL;
    }

    Scope *globalScope = createScope(0, NULL, NULL);
    if (globalScope == NULL) {
        semanticError(root->line, "Error: no se pudo reservar memoria para el ambito.");
        return NULL;
    }

    registerGlobalDeclarations(root, globalScope);
    executeMainMethod(globalScope, debug);

    return globalScope;
}

static void registerGlobalDeclarations(NodeAST *root, Scope *globalScope) {
    for (int i = 0; i < root->childCount; i++) {
        NodeAST *child = root->children[i];
        bool isVariableDeclaration = (child->nodeType == VARIABLE_DECLARATION_NODE);
        bool isMethodDeclaration = (child->nodeType == METHOD_DECLARATION_NODE);

        if (isVariableDeclaration) {
            registerVariableDeclaration(child, globalScope);
        } else if (isMethodDeclaration) {
            registerMethodDeclaration(child, globalScope);
        }
    }
}

static void registerVariableDeclaration(NodeAST *node, Scope *currentScope) {
    char *id = strdup(node->symbol->id);
    if (id == NULL) {
        semanticError(node->line, "Error: no se pudo reservar memoria para la variable '%s'.", node->symbol->id);
        return;
    }

    Symbol *symbol = newSymbol(id, VARIABLE_SYMBOL, node->type, NULL);
    if (symbol == NULL) {
        free(id);
        semanticError(node->line, "Error: no se pudo reservar memoria para la variable '%s'.", node->symbol->id);
        return;
    }

    bool isFloatType = (symbol->type == TYPE_FLOAT);
    if (isFloatType) {
        symbol->value.floatValue = 0.0f;
    } else {
        symbol->value.intValue = 0;
    }
    appendSymbol(currentScope, symbol);
}

static void registerMethodDeclaration(NodeAST *node, Scope *globalScope) {
    char *id = strdup(node->symbol->id);
    if (id == NULL) {
        semanticError(node->line, "Error: no se pudo reservar memoria para el metodo '%s'.", node->symbol->id);
        return;
    }

    Symbol *symbol = newSymbol(id, FUNCTION_ENTRY_SYMBOL, node->type, node->symbol->parameters);
    if (symbol == NULL) {
        free(id);
        semanticError(node->line, "Error: no se pudo reservar memoria para el metodo '%s'.", node->symbol->id);
        return;
    }

    symbol->value.function = node;
    appendSymbol(globalScope, symbol);
}

static void executeMainMethod(Scope *globalScope, bool debug) {
    Symbol *mainSymbol = lookupSymbol(globalScope, "main");
    if (mainSymbol == NULL || mainSymbol->value.function == NULL) {
        semanticError(0, "No se encontro la definicion del metodo 'main' para interpretar.");
        return;
    }

    NodeAST *mainDeclaration = mainSymbol->value.function;

    Symbol result = executeFunctionBody(mainDeclaration, NULL, globalScope, globalScope);
    if (result.type == TYPE_INT) {
        debugPrintf(debug, "[INTERPRETE] 'main' retorno el valor entero: %d\n", result.value.intValue);
    } else if (result.type == TYPE_FLOAT) {
        debugPrintf(debug, "[INTERPRETE] 'main' retorno el valor float: %f\n", result.value.floatValue);
    } else if (result.type == TYPE_BOOL) {
        debugPrintf(debug, "[INTERPRETE] 'main' retorno el valor booleano: %s\n", result.value.intValue ? "true" : "false");
    } else {
        debugPrintf(debug, "[INTERPRETE] 'main' finalizo correctamente (void)\n");
    }
}


static Symbol executeFunctionBody(NodeAST *methodDeclarationNode, NodeAST *argumentsNode, Scope *callerScope, Scope *globalScope) {
    Symbol *argumentValues = evaluateArguments(argumentsNode, callerScope);

    Scope *functionScope = createFunctionScope(globalScope);
    if (functionScope == NULL) {
        free(argumentValues);
        return makeVoidSymbol();
    }

    bindParameters(functionScope, methodDeclarationNode->children[0], argumentValues);
    free(argumentValues);

    NodeAST *bodyNode = methodDeclarationNode->children[1];
    ExecResult result = executeBlock(bodyNode, functionScope);

    freeScope(functionScope);

    return coerceReturnValue(methodDeclarationNode->type, result.value);
}


static Symbol *evaluateArguments(NodeAST *argumentsNode, Scope *callerScope) {
    int argumentCount = (argumentsNode != NULL) ? argumentsNode->childCount : 0;
    if (argumentCount == 0) {
        return NULL;
    }

    Symbol *argumentValues = malloc(sizeof(Symbol) * argumentCount);
    if (argumentValues == NULL) {
        semanticError(argumentsNode->line, "Error: no se pudo reservar memoria para los argumentos de la llamada.");
        return NULL;
    }

    for (int i = 0; i < argumentCount; i++) {
        argumentValues[i] = evaluateExpression(argumentsNode->children[i], callerScope);
    }
    return argumentValues;
}

static Scope *createFunctionScope(Scope *globalScope) {
    return createScope(1, globalScope, NULL);
}

static void assignSymbolValue(Symbol *target, Symbol source) {
    if (target == NULL) {
        return;
    }

    bool needsFloatCoercion = (target->type == TYPE_FLOAT && source.type == TYPE_INT);
    if (needsFloatCoercion) {
        target->value.floatValue = (float)source.value.intValue;
    } else if (target->type == TYPE_FLOAT) {
        target->value.floatValue = source.value.floatValue;
    } else {
        target->value.intValue = source.value.intValue;
    }
}

static void bindParameters(Scope *functionScope, NodeAST *parametersNode, Symbol *argumentValues) {
    for (int i = 0; i < parametersNode->childCount; i++) {
        NodeAST *parameterNode = parametersNode->children[i];
        char *id = strdup(parameterNode->symbol->id);
        if (id == NULL) {
            semanticError(parameterNode->line, "Error: no se pudo reservar memoria para el parametro '%s'.", parameterNode->symbol->id);
            continue;
        }

        Symbol *parameterSymbol = newSymbol(id, VARIABLE_SYMBOL, parameterNode->type, NULL);
        if (parameterSymbol == NULL) {
            free(id);
            semanticError(parameterNode->line, "Error: no se pudo reservar memoria para el parametro '%s'.", parameterNode->symbol->id);
            continue;
        }

        if (argumentValues != NULL) {
            assignSymbolValue(parameterSymbol, argumentValues[i]);
        }
        appendSymbol(functionScope, parameterSymbol);
    }
}

static Symbol coerceReturnValue(DataType expectedType, Symbol result) {
    bool needsFloatCoercion = (expectedType == TYPE_FLOAT && result.type == TYPE_INT);
    if (needsFloatCoercion) {
        return makeFloatSymbol((float)result.value.intValue);
    }
    return result;
}

static ExecResult executeBlock(NodeAST *node, Scope *currentScope) {
    ExecResult result = { .hasReturned = false, .value = makeVoidSymbol() };
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
    Symbol assignedValue = evaluateExpression(GET_EXPRESSION(node), currentScope);
    Symbol *target = lookupSymbol(currentScope, node->symbol->id);
    if (target == NULL) {
        semanticError(node->line, "Asignacion a variable no declarada '%s'.", node->symbol->id);
        return result;
    }
    assignSymbolValue(target, assignedValue);

    return result;
}

static ExecResult executeIfElse(NodeAST *node, Scope *currentScope) {
    Symbol condition = evaluateExpression(GET_CONDITION(node), currentScope);
    bool conditionHolds = isTruthy(condition);
    if (conditionHolds) {
        return executeStatement(GET_IF_BLOCK(node), currentScope);
    }

    bool hasElseBlock = (GET_ELSE_BLOCK(node) != NULL);
    if (hasElseBlock) {
        return executeStatement(GET_ELSE_BLOCK(node), currentScope);
    }

    ExecResult result = { .hasReturned = false, .value = makeVoidSymbol() };
    return result;
}

static ExecResult executeWhile(NodeAST *node, Scope *currentScope) {
    ExecResult result = { .hasReturned = false, .value = makeVoidSymbol() };
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
    bool hasReturnExpression = (GET_EXPRESSION(node) != NULL);
    if (hasReturnExpression) {
        result.value = evaluateExpression(GET_EXPRESSION(node), currentScope);
    }

    return result;
}

static Symbol executeMethodCall(NodeAST *node, Scope *currentScope) {
    Symbol *functionSymbol = lookupSymbol(currentScope, node->symbol->id);
    if (functionSymbol == NULL || functionSymbol->value.function == NULL) {
        semanticError(node->line, "Metodo no declarado: %s", node->symbol->id);
        return makeVoidSymbol();
    }

    NodeAST *functionDeclaration = functionSymbol->value.function;
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
    Symbol *symbol = lookupSymbol(currentScope, node->symbol->id);
    if (symbol == NULL) {
        semanticError(node->line, "Variable no declarada: %s", node->symbol->id);
        return makeVoidSymbol();
    }
    return *symbol;
}

static Symbol evaluateConstant(NodeAST *node) {
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
    bool isUnary = (node->childCount == 1 || GET_RIGHT(node) == NULL);
    if (isUnary) {
        Symbol operand = evaluateExpression(GET_LEFT(node), currentScope);
        return evaluateUnaryOperator(node->operationType, operand);
    }

    bool isLogicalAnd = (node->operationType == OP_AND);
    if (isLogicalAnd) {
        Symbol left = evaluateExpression(GET_LEFT(node), currentScope);
        if (!isTruthy(left)) {
            return makeBoolSymbol(false);
        }
        Symbol right = evaluateExpression(GET_RIGHT(node), currentScope);
        return makeBoolSymbol(isTruthy(right));
    }

    bool isLogicalOr = (node->operationType == OP_OR);
    if (isLogicalOr) {
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
            }
            return makeIntSymbol(-operand.value.intValue);

        default:
            return operand;
    }
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
                bool isDivisionByZero = (rightValue == 0.0f);
                if (isDivisionByZero) {
                    semanticError(0, "Division por cero.");
                    return makeFloatSymbol(0.0f);
                }
                return makeFloatSymbol(leftValue / rightValue);
            }
            {
                bool isDivisionByZero = (right.value.intValue == 0);
                if (isDivisionByZero) {
                    semanticError(0, "Division por cero.");
                    return makeIntSymbol(0);
                }
                // INT_MIN / -1 desborda: el cociente no es representable en int.
                // Es el mismo caso que el optimizador se niega a plegar, y en x86
                // 'idivl' trapea, asi que el intérprete tampoco puede dar un valor.
                bool isSignedOverflow = (left.value.intValue == INT_MIN && right.value.intValue == -1);
                if (isSignedOverflow) {
                    semanticError(0, "Desborde en la division de enteros: INT_MIN / -1 no es representable.");
                    return makeIntSymbol(0);
                }
                return makeIntSymbol(left.value.intValue / right.value.intValue);
            }

        case OP_MOD:
            {
                bool isModuloByZero = (right.value.intValue == 0);
                if (isModuloByZero) {
                    semanticError(0, "Division por cero en operacion modulo.");
                    return makeIntSymbol(0);
                }
                // INT_MIN % -1 vale 0 matematicamente, pero en C es indefinido y en
                // x86 trapea igual que la division, porque usa la misma 'idivl'.
                bool isSignedOverflow = (left.value.intValue == INT_MIN && right.value.intValue == -1);
                if (isSignedOverflow) {
                    semanticError(0, "Desborde en el modulo de enteros: INT_MIN %% -1 no es representable.");
                    return makeIntSymbol(0);
                }
                return makeIntSymbol(left.value.intValue % right.value.intValue);
            }

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

static bool isTruthy(Symbol symbol) {
    return symbol.value.intValue != 0;
}
