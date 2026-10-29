#ifndef AST_H
#define AST_H

// Definición del Árbol de Sintaxis Abstracta (AST), sus tipos y constructores.

#include <stdio.h>
#include "types.h"
#include "symbol_table.h"

// Categorías de nodos que conforman el Árbol de Sintaxis Abstracta (AST).
typedef enum NodeType {
    TYPE_NODE,                  // Nodo de tipo de dato
    VARIABLE_DECLARATION_NODE,  // Declaración de variable
    ID_NODE,                    // Identificador de variable o método
    ASSIGNMENT_NODE,            // Sentencia de asignación
    CONSTANT_NODE,              // Literal constante numérico o booleano
    RETURN_NODE,                // Sentencia de retorno de método
    IF_ELSE_NODE,               // Estructura de control condicional if/else
    WHILE_NODE,                 // Estructura de control iterativa while
    BINARYOPERATOR_NODE,        // Operación binaria o unaria
    BLOCK_NODE,                 // Bloque de sentencias delimitado por llaves
    METHOD_CALL_NODE,           // Invocación a método o función
    PARAMETERS_NODE,            // Lista de parámetros formales
    METHOD_DECLARATION_NODE     // Declaración de método o subrutina
} NodeType;

// Representa un nodo dentro del Árbol de Sintaxis Abstracta (AST).
// Encapsula la categoría del nodo, su tipo de dato semántico, el símbolo
// asociado en la tabla de símbolos y su lista dinámica de nodos hijos.
typedef struct NodeAST {
    NodeType nodeType;              // Categoría sintáctica del nodo
    DataType type;                  // Tipo de dato semántico deducido o asignado
    Symbol *symbol;                 // Puntero al símbolo asociado (NULL si no aplica)
    OperationType operationType;    // Código de operación para nodos BINARYOPERATOR_NODE
    int line;                       // Número de línea en el archivo fuente
    struct NodeAST **children;      // Arreglo dinámico de subárboles hijos
    int childCount;                 // Cantidad de nodos hijos adjuntados
} NodeAST;

// Macros utilitarias para acceso posicional a hijos comunes del AST.
#define GET_EXPRESSION(node)   ((node)->childCount > 0 ? (node)->children[0] : NULL)
#define GET_LEFT(node)         ((node)->childCount > 0 ? (node)->children[0] : NULL)
#define GET_RIGHT(node)        ((node)->childCount > 1 ? (node)->children[1] : NULL)
#define GET_CONDITION(node)    ((node)->childCount > 0 ? (node)->children[0] : NULL)
#define GET_IF_BLOCK(node)     ((node)->childCount > 1 ? (node)->children[1] : NULL)
#define GET_ELSE_BLOCK(node)   ((node)->childCount > 2 ? (node)->children[2] : NULL)
#define GET_WHILE_BLOCK(node)  ((node)->childCount > 1 ? (node)->children[1] : NULL)

// Crea un nuevo nodo del AST empaquetando hasta tres hijos iniciales.
NodeAST *newNode(NodeType nodeType, Symbol *symbol, NodeAST *left, NodeAST *mid, NodeAST *right);

// Crea un nodo representativo de una operación binaria en el AST.
NodeAST *newBinaryOperatorNode(OperationType operator, NodeAST *left, NodeAST *right);

// Crea un nodo representativo de la declaración de un método.
NodeAST *newMethodDeclaration(DataType returnType, char *methodName, NodeAST *parameters, NodeAST *body);

// Crea un nodo constante que contiene un literal entero.
NodeAST *newIntLiteralNode(int value);

// Crea un nodo constante que contiene un literal en punto flotante.
NodeAST *newFloatLiteralNode(float value);

// Crea un nodo constante que contiene un literal booleano.
NodeAST *newBoolLiteralNode(int value);

// Imprime la representación jerárquica del AST en el flujo de salida especificado.
void fprintAST(FILE *out, NodeAST *node, int level);

// Imprime la representación jerárquica del AST por la salida estándar (stdout).
void printAST(NodeAST *node, int level);

#endif // AST_H
