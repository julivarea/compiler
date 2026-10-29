#ifndef AST_H
#define AST_H

/**
 * @file ast.h
 * @brief Definición del Árbol de Sintaxis Abstracta (AST), sus tipos y constructores.
 */

#include <stdio.h>
#include "types.h"
#include "symbol_table.h"

/**
 * @brief Categorías de nodos que conforman el Árbol de Sintaxis Abstracta (AST).
 */
typedef enum NodeType {
    TYPE_NODE,                  /**< Nodo de tipo de dato. */
    VARIABLE_DECLARATION_NODE,  /**< Declaración de variable. */
    ID_NODE,                    /**< Identificador de variable o método. */
    ASSIGNMENT_NODE,            /**< Sentencia de asignación. */
    CONSTANT_NODE,              /**< Literal constante numérico o booleano. */
    RETURN_NODE,                /**< Sentencia de retorno de método. */
    IF_ELSE_NODE,               /**< Estructura de control condicional if/else. */
    WHILE_NODE,                 /**< Estructura de control iterativa while. */
    BINARYOPERATOR_NODE,        /**< Operación binaria o unaria. */
    BLOCK_NODE,                 /**< Bloque de sentencias delimitado por llaves. */
    METHOD_CALL_NODE,           /**< Invocación a método o función. */
    PARAMETERS_NODE,            /**< Lista de parámetros formales. */
    METHOD_DECLARATION_NODE     /**< Declaración de método o subrutina. */
} NodeType;

/**
 * @brief Representa un nodo dentro del Árbol de Sintaxis Abstracta (AST).
 *
 * Encapsula la categoría del nodo, su tipo de dato semántico, el símbolo
 * asociado en la tabla de símbolos y su lista dinámica de nodos hijos.
 */
typedef struct NodeAST {
    NodeType nodeType;              /**< Categoría sintáctica del nodo. */
    DataType type;                  /**< Tipo de dato semántico deducido o asignado. */
    Symbol *symbol;                 /**< Puntero al símbolo asociado (NULL si no aplica). */
    OperationType operationType;    /**< Código de operación para nodos BINARYOPERATOR_NODE. */
    int line;                       /**< Número de línea en el archivo fuente. */
    struct NodeAST **children;      /**< Arreglo dinámico de subárboles hijos. */
    int childCount;                 /**< Cantidad de nodos hijos adjuntados. */
} NodeAST;

/**
 * @brief Macros utilitarias para acceso posicional a hijos comunes del AST.
 */
#define GET_EXPRESSION(node)   ((node)->childCount > 0 ? (node)->children[0] : NULL)
#define GET_LEFT(node)         ((node)->childCount > 0 ? (node)->children[0] : NULL)
#define GET_RIGHT(node)        ((node)->childCount > 1 ? (node)->children[1] : NULL)
#define GET_CONDITION(node)    ((node)->childCount > 0 ? (node)->children[0] : NULL)
#define GET_IF_BLOCK(node)     ((node)->childCount > 1 ? (node)->children[1] : NULL)
#define GET_ELSE_BLOCK(node)   ((node)->childCount > 2 ? (node)->children[2] : NULL)
#define GET_WHILE_BLOCK(node)  ((node)->childCount > 1 ? (node)->children[1] : NULL)

/**
 * @brief Lista enlazada temporal utilizada durante el análisis sintáctico.
 */
typedef struct NodeList {
    NodeAST *node;                  /**< Nodo AST contenido en la celda. */
    struct NodeList *next;          /**< Puntero al siguiente elemento de la lista. */
    struct NodeList *tail;          /**< Puntero al último elemento (válido en el nodo cabeza). */
} NodeList;

/**
 * @brief Crea un nuevo nodo del AST empaquetando hasta tres hijos iniciales.
 *
 * Reserva memoria para la estructura @p NodeAST e inicializa sus campos. Los nodos
 * no nulos pasados en @p left, @p mid y @p right se agregan secuencialmente en @p children.
 *
 * @param nodeType Categoría sintáctica del nodo a instanciar.
 * @param symbol Puntero al símbolo asociado (acepta NULL si no aplica).
 * @param left Primer nodo hijo (acepta NULL).
 * @param mid Segundo nodo hijo (acepta NULL).
 * @param right Tercer nodo hijo (acepta NULL).
 * @return Puntero al nuevo nodo creado, o NULL si falló la reserva de memoria.
 */
NodeAST *newNode(NodeType nodeType, Symbol *symbol, NodeAST *left, NodeAST *mid, NodeAST *right);

/**
 * @brief Crea un nodo representativo de una operación binaria en el AST.
 *
 * Inicializa un nodo de categoría BINARYOPERATOR_NODE y asocia los subárboles
 * izquierdo y derecho.
 *
 * @param operator Código de la operación binaria a ejecutar.
 * @param left Subárbol correspondiente al operando izquierdo.
 * @param right Subárbol correspondiente al operando derecho.
 * @return Puntero al nodo binario creado, o NULL en caso de error.
 */
NodeAST *newBinaryOperatorNode(OperationType operator, NodeAST *left, NodeAST *right);

/**
 * @brief Crea un nodo representativo de la declaración de un método.
 *
 * @param returnType Tipo de dato retornado por el método.
 * @param methodName Nombre identificador de la función.
 * @param parameters Subárbol con la lista de parámetros formales (acepta NULL).
 * @param body Subárbol con el bloque de sentencias del cuerpo del método.
 * @return Puntero al nodo de declaración de método creado, o NULL en caso de error.
 */
NodeAST *newMethodDeclaration(DataType returnType, char *methodName, NodeAST *parameters, NodeAST *body);

/**
 * @brief Crea un nodo constante que contiene un literal entero.
 *
 * @param value Valor numérico entero asignado a la constante.
 * @return Puntero al nuevo nodo constante creado, o NULL en caso de error.
 */
NodeAST *newIntLiteralNode(int value);

/**
 * @brief Crea un nodo constante que contiene un literal en punto flotante.
 *
 * @param value Valor numérico flotante asignado a la constante.
 * @return Puntero al nuevo nodo constante creado, o NULL en caso de error.
 */
NodeAST *newFloatLiteralNode(float value);

/**
 * @brief Crea un nodo constante que contiene un literal booleano.
 *
 * @param value Valor booleano (0 para falso, distinto de 0 para verdadero).
 * @return Puntero al nuevo nodo constante creado, o NULL en caso de error.
 */
NodeAST *newBoolLiteralNode(int value);

/**
 * @brief Inicializa una lista enlazada transitoria para el parsing en bison.y.
 *
 * @param node Nodo inicial a incorporar en la lista.
 * @param next Siguiente elemento en la lista enlazada (acepta NULL).
 * @return Puntero a la nueva celda de lista creada, o NULL si falló la reserva de memoria.
 */
NodeList *initializeTemporaryList(NodeAST *node, NodeList *next);

/**
 * @brief Añade un nodo al final de una lista enlazada iterándola.
 *
 * Mantiene el orden de inserción secuencial para producciones recursivas por izquierda.
 *
 * @param list Lista receptora (si es NULL, inicializa una nueva).
 * @param node Nodo a agregar al final de la colección.
 * @return Puntero a la lista actualizada, o NULL en caso de error.
 */
NodeList *appendToTemporaryList(NodeList *list, NodeAST *node);

/**
 * @brief Vuelca los nodos de una NodeList al arreglo children de parent y libera la lista.
 *
 * @param parent Nodo padre donde se adjuntarán los elementos como hijos.
 * @param list Lista temporal de nodos que será volcada y liberada.
 */
void resolveTemporaryList(NodeAST *parent, NodeList *list);

/**
 * @brief Concatena dos listas temporales de nodos reutilizando las celdas existentes.
 *
 * @param list1 Primera lista de nodos.
 * @param list2 Segunda lista de nodos a enlazar a continuación.
 * @return Puntero a la cabeza de la lista combinada resultante.
 */
NodeList *mergeNodeLists(NodeList *list1, NodeList *list2);

/**
 * @brief Descompone declaraciones múltiples de variables en declaraciones unitarias.
 *
 * Transforma construcciones del estilo "int x, y;" en declaraciones individuales
 * "int x; int y;".
 *
 * @param type Tipo de dato asignado a las variables declaradas.
 * @param identifiers Lista de nodos identificadores a transformar.
 * @return Lista de declaraciones unitarias resultantes.
 */
NodeList *resolveVariableDefinition(DataType type, NodeList *identifiers);

/**
 * @brief Imprime la representación jerárquica del AST en el flujo de salida especificado.
 *
 * @param out Flujo de salida donde se emitirá la estructura (ej. stdout o archivo).
 * @param node Puntero a la raíz del árbol a imprimir (acepta NULL).
 * @param level Nivel de indentación inicial.
 */
void fprintAST(FILE *out, NodeAST *node, int level);

/**
 * @brief Imprime la representación jerárquica del AST por la salida estándar (stdout).
 *
 * @param node Puntero a la raíz del árbol a imprimir (acepta NULL).
 * @param level Nivel de indentación inicial.
 */
void printAST(NodeAST *node, int level);

#endif // AST_H
