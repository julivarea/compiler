#ifndef AST_H
#define AST_H

/**
 * @file ast.h
 * @brief Estructura y operaciones del Árbol de Sintaxis Abstracta (AST).
 */

#include <stdbool.h>
#include <stdio.h>

#include "symbol_table.h"
#include "types.h"

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

/**
 * @brief Crea un nodo AST inicializando hasta tres hijos directos no nulos.
 *
 * Compacta los punteros @p left, @p middle y @p right no nulos en el arreglo
 * dinámico de hijos y ajusta la línea de inicio para estructuras condicionales.
 *
 * @param symbol Símbolo asociado al nodo (acepta NULL).
 * @param left Primer subárbol hijo (acepta NULL).
 * @param middle Segundo subárbol hijo (acepta NULL).
 * @param right Tercer subárbol hijo (acepta NULL).
 * @return Puntero al nodo creado, o NULL si falló la reserva de memoria.
 */
NodeAST *newNode(NodeType nodeType, Symbol *symbol, NodeAST *left, NodeAST *middle, NodeAST *right);

/**
 * @brief Libera recursivamente en postorden un árbol o subárbol AST y sus símbolos propios.
 *
 * @param node Raíz del subárbol a liberar (acepta NULL).
 */
void freeAST(NodeAST *node);

/**
 * @brief Imprime la representación jerárquica del AST por la salida estándar.
 *
 * @param node Raíz del subárbol a imprimir (acepta NULL).
 * @param level Nivel de indentación inicial (0 para la raíz).
 */
void printAST(NodeAST *node, int level);

/**
 * @brief Imprime la representación jerárquica del AST en el flujo de salida indicado.
 *
 * @param output Flujo de archivo destino (no NULL).
 * @param node Raíz del subárbol a imprimir (acepta NULL).
 * @param level Nivel de indentación inicial (0 para la raíz).
 */
void fprintAST(FILE *output, NodeAST *node, int level);

/**
 * @brief Crea un nodo AST para una operación unaria o binaria.
 *
 * @param operation Operador aplicado a la expresión.
 * @param left Primer operando, o único operando en operaciones unarias.
 * @param right Segundo operando (NULL en operaciones unarias).
 * @return Puntero al nodo de operación creado, o NULL si falló la reserva de memoria.
 */
NodeAST *newOperatorNode(OperationType operation, NodeAST *left, NodeAST *right);

/**
 * @brief Crea un nodo AST de declaración de método con sus parámetros y bloque de cuerpo.
 *
 * @param methodName Nombre del método cedido en propiedad al nuevo símbolo.
 * @param parameters Contenedor de parámetros formales (acepta NULL).
 * @param body Bloque con el cuerpo de sentencias del método (acepta NULL).
 * @return Puntero al nodo de declaración creado, o NULL si falló la reserva de memoria.
 */
NodeAST *newMethodDeclaration(DataType returnType, char *methodName, NodeAST *parameters, NodeAST *body);

/**
 * @brief Crea un nodo de declaración de parámetro formal para un método.
 *
 * @param id Nombre del parámetro cedido en propiedad al nuevo símbolo.
 * @return Puntero al nodo de declaración creado, o NULL si falló la reserva de memoria.
 */
NodeAST *newParameterDeclaration(DataType type, char *id);

/**
 * @brief Crea un nodo AST de referencia a identificador (ID_NODE).
 *
 * @param id Nombre del identificador cedido en propiedad al símbolo.
 * @return Puntero al nodo creado, o NULL si falló la reserva de memoria.
 */
NodeAST *newIdentifierNode(char *id);

/**
 * @brief Crea un nodo AST de sentencia de asignación (ASSIGNMENT_NODE).
 *
 * @param id Nombre de la variable destino cedido en propiedad al símbolo.
 * @param expression Subárbol de la expresión asignada.
 * @return Puntero al nodo creado, o NULL si falló la reserva de memoria.
 */
NodeAST *newAssignmentNode(char *id, NodeAST *expression);

/**
 * @brief Crea un nodo AST de invocación a método (METHOD_CALL_NODE) sin argumentos iniciales.
 *
 * @param id Nombre del método invocado cedido en propiedad al símbolo.
 * @return Puntero al nodo creado, o NULL si falló la reserva de memoria.
 */
NodeAST *newMethodCallNode(char *id);

/**
 * @brief Crea un nodo AST constante de tipo entero.
 *
 * @param value Valor numérico entero del literal.
 * @return Puntero al nodo creado, o NULL si falló la reserva de memoria.
 */
NodeAST *newIntLiteralNode(int value);

/**
 * @brief Crea un nodo AST constante de tipo punto flotante.
 *
 * @param value Valor de punto flotante del literal.
 * @return Puntero al nodo creado, o NULL si falló la reserva de memoria.
 */
NodeAST *newFloatLiteralNode(float value);

/**
 * @brief Crea un nodo AST constante de tipo booleano.
 *
 * @param value Valor de verdad (0 para false, distinto de 0 para true).
 * @return Puntero al nodo creado, o NULL si falló la reserva de memoria.
 */
NodeAST *newBoolLiteralNode(int value);

/**
 * @brief Macros de acceso a subárboles y nodos hijos del AST.
 */
#define GET_CHILD(node, index) ((node)->childCount > (index) ? (node)->children[(index)] : NULL)
#define GET_EXPRESSION(node)   GET_CHILD(node, 0)
#define GET_LEFT(node)         GET_CHILD(node, 0)
#define GET_RIGHT(node)        GET_CHILD(node, 1)
#define GET_CONDITION(node)    GET_CHILD(node, 0)
#define GET_IF_BLOCK(node)     GET_CHILD(node, 1)
#define GET_ELSE_BLOCK(node)   GET_CHILD(node, 2)
#define GET_WHILE_BLOCK(node)  GET_CHILD(node, 1)

#endif // AST_H
