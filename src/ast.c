#include <stddef.h>
#include "ast.h"

Symbol *newSymbol(const char *id, const char *value) {
    (void)id;
    (void)value;
    /* TODO hernan jara*/
    return NULL;
}

NodeAST *newNode(NodeType nodeType, Symbol *symbol, NodeAST *left, NodeAST *mid, NodeAST *right) {
    (void)nodeType;
    (void)symbol;
    (void)left;
    (void)mid;
    (void)right;
    /* TODO hernan jara */
    return NULL;
}

NodeList *newNodeList(NodeAST *node, NodeList *next) {
    (void)node;
    (void)next;
    /* TODO hernan jara */
    return NULL;
}

NodeAST *newLiteralNode(DataType type, const char *value)
{
    Symbol *symbol = newSymbol(NULL, value);

    NodeAST *node = newNode(
        CONSTANT_NODE,
        symbol,
        NULL,
        NULL,
        NULL
    );

    node->type = type;

    return node;
}

void attachChildren(NodeAST *parent, NodeList *list) {
    (void)parent;
    (void)list;
    /* TODO hernan jara */
}
