#include <stddef.h>
#include "ast.h"

Symbol *newSymbol(const char *id, const char *value) {
    (void)id;
    (void)value;
    /* TODO hernan jara*/
    return NULL;
}

NodeAST *newNode(NodeType nodeType, Symbol *symbol, NodeAST *left, NodeAST *right) {
    (void)nodeType;
    (void)symbol;
    (void)left;
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

void attachChildren(NodeAST *parent, NodeList *list) {
    (void)parent;
    (void)list;
    /* TODO hernan jara */
}
