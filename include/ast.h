#ifndef AST_H
#define AST_H

/**
 * @file ast.h
 * @brief Estructura y operaciones del Árbol de Sintaxis Abstracta (AST).
 */

#include <stdio.h>
#include "types.h"
#include "symbol_table.h"

/**
 * @brief Categorías de nodos que conforman el Árbol de Sintaxis Abstracta.
 */
typedef enum NodeType {
    VARIABLE_DECLARATION_NODE, /**< Declaración de variable global, local o parámetro formal. */
    ID_NODE,                   /**< Referencia a un identificador en una expresión. */
    ASSIGNMENT_NODE,           /**< Sentencia de asignación a una variable. */
    CONSTANT_NODE,             /**< Literal constante entero, flotante o booleano. */
    RETURN_NODE,               /**< Sentencia de retorno de método. */
    IF_ELSE_NODE,              /**< Sentencia condicional if o if-else. */
    WHILE_NODE,                /**< Sentencia de iteración while. */
    OPERATOR_NODE,             /**< Expresión de operación unaria o binaria. */
    BLOCK_NODE,                /**< Bloque léxico de declaraciones y sentencias. */
    METHOD_CALL_NODE,          /**< Invocación a un método como sentencia o expresión. */
    PARAMETERS_NODE,           /**< Contenedor de parámetros formales de un método. */
    METHOD_DECLARATION_NODE    /**< Declaración completa de un método con firma y cuerpo. */
} NodeType;

/**
 * @brief Nodo genérico del Árbol de Sintaxis Abstracta (AST).
 */
typedef struct NodeAST {
    NodeType nodeType;           /**< Categoría sintáctica del nodo. */
    DataType type;               /**< Tipo de dato semántico inferido o declarado. */
    Symbol *symbol;              /**< Símbolo asociado al nodo (NULL si no aplica). */
    OperationType operationType; /**< Operador aplicado (solo válido en OPERATOR_NODE). */
    int line;                    /**< Línea del código fuente donde inicia la construcción. */
    struct NodeAST **children;   /**< Arreglo dinámico de punteros a nodos hijos. */
    int childCount;              /**< Cantidad de nodos hijos almacenados. */
} NodeAST;

#define GET_CHILD(node, index) ((node)->childCount > (index) ? (node)->children[(index)] : NULL)
#define GET_EXPRESSION(node)   GET_CHILD(node, 0)
#define GET_LEFT(node)         GET_CHILD(node, 0)
#define GET_RIGHT(node)        GET_CHILD(node, 1)
#define GET_CONDITION(node)    GET_CHILD(node, 0)
#define GET_IF_BLOCK(node)     GET_CHILD(node, 1)
#define GET_ELSE_BLOCK(node)   GET_CHILD(node, 2)
#define GET_WHILE_BLOCK(node)  GET_CHILD(node, 1)

/**
 * @brief Crea un nodo AST inicializando hasta tres hijos directos no nulos.
 *
 * Compacta los punteros @p left, @p mid y @p right no nulos en el arreglo
 * dinámico de hijos y ajusta la línea de inicio para estructuras condicionales.
 *
 * @param symbol Símbolo asociado al nodo (acepta NULL).
 * @param left Primer subárbol hijo (acepta NULL).
 * @param mid Segundo subárbol hijo (acepta NULL).
 * @param right Tercer subárbol hijo (acepta NULL).
 * @return Puntero al nodo creado, o NULL si falló la reserva de memoria.
 */
NodeAST *newNode(NodeType nodeType, Symbol *symbol, NodeAST *left, NodeAST *mid, NodeAST *right);

/**
 * @brief Crea un nodo AST para una operación unaria o binaria.
 *
 * @param left Primer operando, o único operando en operaciones unarias.
 * @param right Segundo operando (NULL en operaciones unarias).
 * @return Puntero al nodo de operación creado, o NULL si falló la reserva de memoria.
 */
NodeAST *newOperatorNode(OperationType operator, NodeAST *left, NodeAST *right);

/**
 * @brief Crea un nodo AST de declaración de método con sus parámetros y bloque de cuerpo.
 *
 * @param methodName Nombre del método cedido en propiedad al nuevo símbolo.
 * @return Puntero al nodo de declaración creado, o NULL si falló la reserva de memoria.
 */
NodeAST *newMethodDeclaration(DataType returnType, char *methodName, NodeAST *parameters, NodeAST *body);

/**
 * @brief Crea un nodo AST constante de tipo entero.
 */
NodeAST *newIntLiteralNode(int value);

/**
 * @brief Crea un nodo AST constante de tipo punto flotante.
 */
NodeAST *newFloatLiteralNode(float value);

/**
 * @brief Crea un nodo AST constante de tipo booleano.
 *
 * @param value Valor de verdad (0 para false, distinto de 0 para true).
 */
NodeAST *newBoolLiteralNode(int value);

/**
 * @brief Libera recursivamente en postorden un árbol o subárbol AST y sus símbolos propios.
 *
 * @param node Raíz del subárbol a liberar (acepta NULL).
 */
void freeAST(NodeAST *node);

/**
 * @brief Imprime la representación jerárquica del AST en el flujo de salida indicado.
 *
 * @param out Flujo de archivo destino (no NULL).
 * @param node Raíz del subárbol a imprimir (acepta NULL).
 * @param level Nivel de indentación inicial (0 para la raíz).
 */
void fprintAST(FILE *out, NodeAST *node, int level);

/**
 * @brief Imprime la representación jerárquica del AST por la salida estándar.
 *
 * @param node Raíz del subárbol a imprimir (acepta NULL).
 * @param level Nivel de indentación inicial (0 para la raíz).
 */
void printAST(NodeAST *node, int level);

#endif // AST_H
