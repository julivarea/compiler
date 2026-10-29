#ifndef PARSER_HELPERS_H
#define PARSER_HELPERS_H

#include "ast.h"
#include "types.h"

// lista enlazada temporal utilizada durante el análisis sintáctico
typedef struct NodeList {
    NodeAST *node;                  // Nodo AST contenido en la celda
    struct NodeList *next;          // Puntero al siguiente elemento de la lista
    struct NodeList *tail;          // Puntero al último elemento (válido en el nodo cabeza)
} NodeList;

// inicializa una lista enlazada transitoria para el parsing en bison.y
NodeList *initializeTemporaryList(NodeAST *node, NodeList *next);

// añade un nodo al final de una lista enlazada iterándola
NodeList *appendToTemporaryList(NodeList *list, NodeAST *node);

// vuelca los nodos de una NodeList al arreglo children de parent y libera la lista
void resolveTemporaryList(NodeAST *parent, NodeList *list);

// concatena dos listas temporales de nodos reutilizando las celdas existentes
NodeList *mergeNodeLists(NodeList *list1, NodeList *list2);

// descompone declaraciones múltiples de variables en declaraciones unitarias
NodeList *resolveVariableDefinition(DataType type, NodeList *identifiers);

#endif // PARSER_HELPERS_H
