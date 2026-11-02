struct NodeAST;
typedef struct NodeAST NodeAST;

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

static ExecResult executeStatement(NodeAST *node, Scope *currentScope);
static ExecResult executeBlock(NodeAST *node, Scope *currentScope);
static ExecResult executeAssignment(NodeAST *node, Scope *currentScope);
static ExecResult executeIfElse(NodeAST *node, Scope *currentScope);
static ExecResult executeWhile(NodeAST *node, Scope *currentScope);
static ExecResult executeReturn(NodeAST *node, Scope *currentScope);

static Symbol evaluateExpression(NodeAST *node, Scope *currentScope);
static Symbol evaluateIdentifier(NodeAST *node, Scope *currentScope);
static Symbol evaluateConstant(NodeAST *node);
static Symbol evaluateOperator(NodeAST *node, Scope *currentScope);
static Symbol evaluateUnaryOperator(OperationType operation, Symbol operand);
static Symbol evaluateBinaryOperator(OperationType operation, Symbol left, Symbol right);

static Symbol executeMethodCall(NodeAST *node, Scope *currentScope);
static Symbol executeFunctionBody(NodeAST *methodDeclarationNode, NodeAST *argumentsNode, Scope *callerScope, Scope *globalScope);

static Symbol makeIntSymbol(int value);
static Symbol makeFloatSymbol(float value);
static Symbol makeBoolSymbol(bool value);
static Symbol makeVoidSymbol(void);
static bool isTruthy(Symbol symbol);

Scope *interpreteSemantics(NodeAST *root, Scope *parent) {
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

    registerGlobalDeclarations(root, currentScope);

    if (isGlobalScope) {
        executeMainMethod(currentScope);
    }

    return currentScope;
}

static void registerGlobalDeclarations(NodeAST *root, Scope *globalScope) {
    // 1. Iterar sobre root->children:
    //    - Si es VARIABLE_DECLARATION_NODE -> registerVariableDeclaration(child, globalScope)
    //    - Si es METHOD_DECLARATION_NODE   -> registerMethodDeclaration(child, globalScope)
}

static void registerMethodDeclaration(NodeAST *node, Scope *globalScope) {
    // 1. Crear el simbolo con newSymbol(node->symbol->id, FUNCTION_ENTRY_SYMBOL, node->type, node->symbol->parameters).
    // 2. Guardar la referencia al AST del metodo: symbol->value.function = node.
    // 3. Registrar el simbolo en globalScope con appendSymbol(globalScope, symbol).
}

static void registerVariableDeclaration(NodeAST *node, Scope *currentScope) {
    // 1. Crear el simbolo con newSymbol(node->symbol->id, VARIABLE_SYMBOL, node->type, NULL).
    // 2. Inicializar su valor por defecto: 0 para TYPE_INT / TYPE_BOOL, 0.0 para TYPE_FLOAT.
    // 3. Registrar el simbolo en currentScope con appendSymbol(currentScope, symbol).
}

static void executeMainMethod(Scope *globalScope) {
    // 1. Buscar el metodo "main" en globalScope usando lookupSymbol(globalScope, "main").
    // 2. Si no existe, reportar error en tiempo de ejecucion.
    // 3. Si existe, ejecutar su cuerpo con executeFunctionBody o directamente executeBlock(mainDecl->children[...], globalScope).
}

static ExecResult executeStatement(NodeAST *node, Scope *currentScope) {
    ExecResult result = { .hasReturned = false, .value = makeVoidSymbol() };
    if (node == NULL) return result;

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

static ExecResult executeBlock(NodeAST *node, Scope *currentScope) {
    ExecResult result = { .hasReturned = false, .value = makeVoidSymbol() };

    // 1. Crear un nuevo Scope hijo: createScope(currentScope->level + 1, currentScope, currentScope->function).
    // 2. appendChildScope(currentScope, blockScope).
    // 3. Iterar por node->children ejecutando executeStatement(child, blockScope).
    // 4. Si alguna sentencia devuelve result.hasReturned == true:
    //    detener inmediatamente la ejecucion del bloque y retornar ese result.

    return result;
}

static ExecResult executeAssignment(NodeAST *node, Scope *currentScope) {
    ExecResult result = { .hasReturned = false, .value = makeVoidSymbol() };

    // 1. Evaluar la expresion derecha: Symbol rhs = evaluateExpression(GET_EXPRESSION(node), currentScope).
    // 2. Buscar la variable en currentScope: Symbol *target = lookupSymbol(currentScope, node->symbol->id).
    // 3. Asignar el valor evaluado al target (ej: target->value.intValue = rhs.value.intValue).

    return result;
}

static ExecResult executeIfElse(NodeAST *node, Scope *currentScope) {
    ExecResult result = { .hasReturned = false, .value = makeVoidSymbol() };

    // 1. Evaluar la condicion: Symbol condition = evaluateExpression(GET_CONDITION(node), currentScope).
    // 2. Si isTruthy(condition) es verdadero:
    //    return executeStatement(GET_IF_BLOCK(node), currentScope);
    // 3. Si no es verdadero y GET_ELSE_BLOCK(node) != NULL:
    //    return executeStatement(GET_ELSE_BLOCK(node), currentScope);

    return result;
}

static ExecResult executeWhile(NodeAST *node, Scope *currentScope) {
    ExecResult result = { .hasReturned = false, .value = makeVoidSymbol() };

    // 1. Bucle while (isTruthy(evaluateExpression(GET_CONDITION(node), currentScope))):
    //    a. result = executeStatement(GET_WHILE_BLOCK(node), currentScope);
    //    b. Si result.hasReturned == true:
    //       romper el bucle y retornar result.

    return result;
}

static ExecResult executeReturn(NodeAST *node, Scope *currentScope) {
    ExecResult result = { .hasReturned = true, .value = makeVoidSymbol() };

    // 1. Si node tiene expresion hija (GET_EXPRESSION(node) != NULL):
    //    result.value = evaluateExpression(GET_EXPRESSION(node), currentScope);
    // 2. result.hasReturned = true.

    return result;
}

static Symbol evaluateExpression(NodeAST *node, Scope *currentScope) {
    if (node == NULL) return makeVoidSymbol();

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
    // 1. Buscar el simbolo en el ambito actual y ancestros: lookupSymbol(currentScope, node->symbol->id).
    // 2. Si no existe, reportar error en tiempo de ejecucion.
    // 3. Retornar una copia del simbolo encontrado (*symbol).
    return makeVoidSymbol();
}

static Symbol evaluateConstant(NodeAST *node) {
    // Extraer y retornar el valor segun node->symbol->type:
    // - TYPE_INT:   makeIntSymbol(node->symbol->value.intValue)
    // - TYPE_FLOAT: makeFloatSymbol(node->symbol->value.floatValue)
    // - TYPE_BOOL:  makeBoolSymbol(node->symbol->value.intValue)
    return makeVoidSymbol();
}

static Symbol evaluateOperator(NodeAST *node, Scope *currentScope) {
    // 1. Si es unario (1 hijo, ej: OP_NOT, OP_UMINUS):
    //    Symbol operand = evaluateExpression(GET_LEFT(node), currentScope);
    //    return evaluateUnaryOperator(node->operationType, operand);
    // 2. Si es binario (2 hijos):
    //    Symbol left = evaluateExpression(GET_LEFT(node), currentScope);
    //    Symbol right = evaluateExpression(GET_RIGHT(node), currentScope);
    //    return evaluateBinaryOperator(node->operationType, left, right);
    return makeVoidSymbol();
}

static Symbol evaluateUnaryOperator(OperationType operation, Symbol operand) {
    // 1. Aplicar operador unario:
    //    - OP_NOT:    retornar makeBoolSymbol(!isTruthy(operand))
    //    - OP_UMINUS: invertir el signo (-operand.value.intValue o -operand.value.floatValue)
    return operand;
}

static Symbol evaluateBinaryOperator(OperationType operation, Symbol left, Symbol right) {
    // 1. Segun operation:
    //    - Aritmeticos (+, -, *, /, %): operar con enteros o floats segun el tipo y devolver makeIntSymbol/makeFloatSymbol.
    //    - Relacionales (<, <=, >, >=, ==, !=): comparar valores y devolver makeBoolSymbol(resultado).
    //    - Logicos (&&, ||): evaluar con isTruthy y devolver makeBoolSymbol(resultado).
    return makeVoidSymbol();
}

static Symbol executeMethodCall(NodeAST *node, Scope *currentScope) {
    // 1. Buscar la funcion: Symbol *func = lookupSymbol(currentScope, node->symbol->id).
    // 2. Obtener su nodo AST de declaracion: NodeAST *funcDecl = func->value.function.
    // 3. Invocar executeFunctionBody(funcDecl, node, currentScope, globalScope).
    return makeVoidSymbol();
}

static Symbol executeFunctionBody(NodeAST *methodDeclarationNode, NodeAST *argumentsNode, Scope *callerScope, Scope *globalScope) {
    // 1. Evaluar cada argumento en argumentsNode usando callerScope y guardarlos en un arreglo temporal.
    // 2. Crear un nuevo Scope para la funcion con parent = globalScope.
    // 3. Registrar los parametros formales (methodDeclarationNode->symbol->parameters) en el nuevo scope
    //    y asignarles los valores evaluados.
    // 4. Ejecutar el cuerpo del metodo (su bloque) con executeBlock.
    // 5. Retornar el result.value devuelto por executeBlock.
    return makeVoidSymbol();
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
    // Retornar true si symbol representa un valor verdadero:
    // - Para TYPE_BOOL / TYPE_INT: symbol.value.intValue != 0
    // - Para TYPE_FLOAT: symbol.value.floatValue != 0.0f
    return false;
}
