#ifndef PARSER_HELPERS_H
#define PARSER_HELPERS_H

/**
 * @file parser_helpers.h
 * @brief Estructuras y funciones auxiliares para la construcción del AST en el parser.
 */

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
 *
 * @param initialCapacity Capacidad inicial deseada (usa 4 por defecto si es menor o igual a 0).
 * @return Puntero al arreglo creado, o NULL si falló la reserva de memoria.
 */
NodeArray *createNodeArray(int initialCapacity);

/**
 * @brief Añade un nodo al final del arreglo dinámico, creándolo o redimensionándolo si es necesario.
 *
 * @param array Arreglo destino (si es NULL y @p node no es NULL, se crea uno nuevo).
 * @param node Nodo AST a insertar (si es NULL no realiza acción).
 * @return Puntero al arreglo actualizado.
 */
NodeArray *appendNode(NodeArray *array, NodeAST *node);

/**
 * @brief Concatena el segundo arreglo al final del primero y libera la estructura del segundo.
 *
 * @param first Primer arreglo dinámico (acepta NULL).
 * @param second Segundo arreglo dinámico a anexar y liberar (acepta NULL).
 * @return Puntero al arreglo resultante con los elementos combinados.
 */
NodeArray *mergeNodeArrays(NodeArray *first, NodeArray *second);

/**
 * @brief Convierte in-place una lista de identificadores en nodos de declaración de variable con el tipo dado.
 *
 * @param identifiers Arreglo de nodos ID_NODE a transformar en VARIABLE_DECLARATION_NODE (acepta NULL).
 * @return El mismo arreglo @p identifiers con sus nodos y símbolos tipados.
 */
NodeArray *resolveVariableDefinition(DataType type, NodeArray *identifiers);

#endif // PARSER_HELPERS_H
