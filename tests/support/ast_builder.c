#include "ast_builder.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "parser_helpers.h"
#include "symbol_table.h"

static NodeArray *collectNodes(int count, va_list nodes);
static void appendChildren(NodeAST *parent, int count, va_list nodes);
static NodeAST *registerDeclaration(NodeAST *declaration, int uid, bool isGlobal);
static char *copyName(const char *name);

/* ------------------------------------------------------------------------
 * AST sin anotar
 * ------------------------------------------------------------------------ */

NodeAST *astProgram(int count, ...) {
    va_list nodes;
    va_start(nodes, count);
    NodeAST *program = newBlockNode(collectNodes(count, nodes));
    va_end(nodes);
    return program;
}

NodeAST *astBlock(int count, ...) {
    va_list nodes;
    va_start(nodes, count);
    NodeAST *block = newBlockNode(collectNodes(count, nodes));
    va_end(nodes);
    return block;
}

NodeAST *astMethod(DataType returnType, const char *name, NodeAST *parameters, NodeAST *body) {
    return newMethodDeclaration(returnType, copyName(name), parameters, body);
}

NodeAST *astParameters(int count, ...) {
    va_list nodes;
    va_start(nodes, count);
    NodeAST *parameters = newParametersNode(collectNodes(count, nodes));
    va_end(nodes);
    return parameters;
}

NodeAST *astDeclaration(DataType type, const char *name) {
    return newParameterDeclaration(type, copyName(name));
}

NodeAST *astIdentifier(const char *name) {
    return newIdentifierNode(copyName(name));
}

NodeAST *astAssignment(const char *name, NodeAST *value) {
    return newAssignmentNode(copyName(name), value);
}

NodeAST *astCall(const char *name, int count, ...) {
    va_list nodes;
    va_start(nodes, count);
    NodeAST *call = newMethodCallNode(copyName(name), collectNodes(count, nodes));
    va_end(nodes);
    return call;
}

NodeAST *astInt(int value) {
    return newIntLiteralNode(value);
}

NodeAST *astBool(int value) {
    return newBoolLiteralNode(value);
}

NodeAST *astFloat(float value) {
    return newFloatLiteralNode(value);
}

NodeAST *astBinary(OperationType operation, NodeAST *left, NodeAST *right) {
    NodeAST *node = newOperatorNode(operation, left, right);
    bool producesBoolean = (operation == OP_LT || operation == OP_GT || operation == OP_EQ ||
                            operation == OP_AND || operation == OP_OR || operation == OP_NOT);
    node->type = producesBoolean ? TYPE_BOOL : left->type;
    return node;
}

NodeAST *astUnary(OperationType operation, NodeAST *operand) {
    return astBinary(operation, operand, NULL);
}

NodeAST *astReturn(NodeAST *value) {
    return newReturnNode(value);
}

NodeAST *astIf(NodeAST *condition, NodeAST *ifBlock, NodeAST *elseBlock) {
    return newIfElseNode(condition, ifBlock, elseBlock);
}

NodeAST *astWhile(NodeAST *condition, NodeAST *body) {
    return newWhileNode(condition, body);
}

/* ------------------------------------------------------------------------
 * AST anotado
 * ------------------------------------------------------------------------ */

NodeAST *astGlobal(DataType type, const char *name, int uid) {
    return registerDeclaration(astDeclaration(type, name), uid, true);
}

NodeAST *astLocal(DataType type, const char *name, int uid) {
    return registerDeclaration(astDeclaration(type, name), uid, false);
}

NodeAST *astMethodDeclaration(DataType returnType, const char *name, int uid, NodeAST *parameters) {
    NodeAST *method = astMethod(returnType, name, parameters, newBlockNode(NULL));
    return registerDeclaration(method, uid, true);
}

void astAppend(NodeAST *methodOrBlock, int count, ...) {
    NodeAST *target = (methodOrBlock->nodeType == METHOD_DECLARATION_NODE) ? GET_RIGHT(methodOrBlock)
                                                                          : methodOrBlock;
    va_list nodes;
    va_start(nodes, count);
    appendChildren(target, count, nodes);
    va_end(nodes);
}

NodeAST *astUse(NodeAST *declaration) {
    NodeAST *node = astIdentifier(declaration->symbol->id);
    resolveNodeSymbol(node, declaration->symbol);
    node->type = declaration->symbol->type;
    return node;
}

NodeAST *astStore(NodeAST *declaration, NodeAST *value) {
    NodeAST *node = astAssignment(declaration->symbol->id, value);
    resolveNodeSymbol(node, declaration->symbol);
    node->type = declaration->symbol->type;
    return node;
}

NodeAST *astInvoke(NodeAST *methodDeclaration, int count, ...) {
    va_list nodes;
    va_start(nodes, count);
    NodeAST *call = newMethodCallNode(copyName(methodDeclaration->symbol->id), collectNodes(count, nodes));
    va_end(nodes);
    resolveNodeSymbol(call, methodDeclaration->symbol);
    call->type = methodDeclaration->symbol->type;
    return call;
}

/* ------------------------------------------------------------------------
 * Auxiliares
 * ------------------------------------------------------------------------ */

static NodeArray *collectNodes(int count, va_list nodes) {
    NodeArray *array = NULL;
    for (int i = 0; i < count; i++) {
        array = appendNode(array, va_arg(nodes, NodeAST *));
    }
    return array;
}

static void appendChildren(NodeAST *parent, int count, va_list nodes) {
    parent->children = realloc(parent->children, sizeof(NodeAST *) * (size_t)(parent->childCount + count));
    for (int i = 0; i < count; i++) {
        NodeAST *child = va_arg(nodes, NodeAST *);
        if (child != NULL) {
            parent->children[parent->childCount++] = child;
        }
    }
}

static NodeAST *registerDeclaration(NodeAST *declaration, int uid, bool isGlobal) {
    declaration->symbol->uid = uid;
    declaration->symbol->isGlobal = isGlobal;
    return declaration;
}

static char *copyName(const char *name) {
    return strdup(name);
}
