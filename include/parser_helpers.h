#ifndef PARSER_HELPERS_H
#define PARSER_HELPERS_H

#include "ast.h"
#include "types.h"

/**
 * @brief Arreglo dinámico temporal utilizado durante el análisis sintáctico.
 */
typedef struct NodeArray {
    NodeAST **items;    /**< Arreglo dinámico de punteros a nodos AST. */
    int count;          /**< Cantidad actual de nodos en el arreglo. */
    int capacity;       /**< Capacidad reservada en memoria. */
} NodeArray;

/**
 * @brief Crea una nueva instancia de NodeArray con la capacidad inicial especificada.
 */
NodeArray *createNodeArray(int initialCapacity);

/**
 * @brief Añade un nodo al final del arreglo dinámico (redimensiona si es necesario).
 */
NodeArray *appendNode(NodeArray *array, NodeAST *node);

/**
 * @brief Inserta un nodo al inicio del arreglo dinámico.
 */
NodeArray *prependNode(NodeArray *array, NodeAST *node);

/**
 * @brief Transfiere la propiedad del arreglo de nodos al nodo parent en O(1) y libera la estructura.
 */
void resolveNodeArray(NodeAST *parent, NodeArray *array);

/**
 * @brief Concatena dos arreglos dinámicos en uno solo.
 */
NodeArray *mergeNodeArrays(NodeArray *first, NodeArray *second);

/**
 * @brief Crea un nodo de declaración de parámetro formal para un método.
 */
NodeAST *newParameterDeclaration(DataType type, char *id);

/**
 * @brief Descompone declaraciones múltiples de variables en declaraciones unitarias.
 */
NodeArray *resolveVariableDefinition(DataType type, NodeArray *identifiers);

#endif // PARSER_HELPERS_H
