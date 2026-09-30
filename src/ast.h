#ifndef AST_H
#define AST_H

/* Tipos semanticos */
typedef enum {
    TYPE_INT,
    TYPE_BOOL,
    TYPE_FLOAT,
    TYPE_VOID
} DataType;

typedef enum {
    OP_ADD,
    OP_NEGATIVE,
    OP_NEGATION,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_MOD,
    OP_LT,
    OP_GT,
    OP_EQ,
    OP_AND,
    OP_OR
} OperationType;

typedef enum {
    TYPE_NODE, 
    VARIABLE_DECLARATION_NODE,
    ID_NODE, // simbolo que guarda el valor del id
    ASSIGNMENT_NODE, // solo sirve a la hora de parsear
    CONSTANT_NODE, // tiene simbolo pero no tiene un id 
    RETURN_NODE, 
    IF_ELSE_NODE,
    WHILE_NODE,
    BINARYOPERATOR_NODE,
    BLOCK_NODE, // simbolo que guarda lista de sentenciasq
    METHOD_CALL_NODE, 
    PARAMETERS_NODE, // no hace falta simbolo
    METHOD_DECLARATION_NODE // simbolo guarda lista de parametros y bloque
} NodeType;

typedef enum {
    ID_SYMBOL,
    METHOD_SYMBOL,
    CONSTANT_SYMBOL,

} SymbolType;

typedef struct Symbol {
    char *id;
    char *value;
    DataType type;
    struct NodeAST **children;
    int childCount;
    SymbolType symbolType;
} Symbol;


typedef struct NodeAST {
    NodeType nodeType;
    DataType type;
    Symbol *symbol;
    OperationType operationType; // si es binOp le guardamos un operador, y si no va NULL
    int line;
    struct NodeAST **children;
    int childCount;
} NodeAST;


#define GET_LEFT(node)  ((node)->childCount > 0 ? (node)->children[0] : NULL)
#define GET_RIGHT(node) ((node)->childCount > 1 ? (node)->children[1] : NULL)

#define GET_CONDITION(node)  ((node)->childCount > 0 ? (node)->children[0] : NULL)
#define GET_IF_BLOCK(node)   ((node)->childCount > 1 ? (node)->children[1] : NULL)
#define GET_ELSE_BLOCK(node) ((node)->childCount > 2 ? (node)->children[2] : NULL)

typedef struct NodeList {
    NodeAST *node;
    struct NodeList *next;
    struct NodeList *tail; // puntero al último elemento (solo válido en el nodo cabecera)
} NodeList;

/**
 * Crea un Symbol reservado dinámicamente copiando id y value.
 *
 * @param id Identificador del símbolo.
 * @param value Valor del símbolo.
 *  @param symbol Tipo de simbolo
 * @param children Lista para el simbolo de tipo metodo
 * @param dataType Tipo del simbolo
 * @return Nuevo símbolo creado.
 */
Symbol *newSymbol(const char *id, const char *value, SymbolType symbolType, NodeAST* children, DataType type);

/**
 * Crea un nuevo nodo AST. left, mid y right se empaquetan en children.
 *
 * @param nodeType Tipo de nodo.
 * @param symbol Símbolo asociado al nodo.
 * @param left Nodo hijo izquierdo (opcional).
 * @param mid Nodo hijo medio (opcional).
 * @param right Nodo hijo derecho (opcional).
 * @return Nuevo nodo creado.
 */
NodeAST *newNode(NodeType nodeType, Symbol *symbol, NodeAST *left, NodeAST *mid, NodeAST *right);

/**
 * Crea un nuevo nodo AST. left, mid y right se empaquetan en children.
 *
 * @param operator tipo del operador
 * @param left Nodo hijo izquierdo 
 * @param right Nodo hijo derecho (opcional).
 * @return Nuevo nodo creado.
 */
NodeAST *newBinaryOperatorNode(OperationType operator, NodeAST *left, NodeAST *right);

/**
 * Crea una lista enlazada transitoria para el parsing en bison.y.
 *
 * @param node Nodo a agregar a la lista.
 * @param next Siguiente elemento en la lista.
 * @return Nueva lista de nodos.
 */
NodeList *initializeTemporaryList(NodeAST *node, NodeList *next);

/**
 * Añade un nodo al final de una lista enlazada iterándola.
 * Es útil para mantener el orden correcto en recursión por la izquierda.
 *
 * @param list Lista original.
 * @param node Nodo a agregar al final.
 * @return La lista actualizada (o la nueva lista si list era NULL).
 */
NodeList *appendToTemporaryList(NodeList *list, NodeAST *node);

/**
 * Crea un nodo para la declaracion de metodo
 *
 * @param returnType tipo del metodo
 * @param methodName nombre del metodo
 * @param parameters lista de parametros que recibe el metodo 
 * @param body lista de sentencias que tiene el metodo
 * @return  nodo de metodo
 */
NodeAST *newMethodDeclaration(DataType returnType, char *methodName, NodeAST *parameters, NodeAST *body);


/**
 * Vuelca los nodos de NodeList al arreglo children[] de parent y libera la lista.
 *
 * @param parent Nodo padre donde se adjuntarán los hijos.
 * @param list Lista de nodos a volcar.
 */
void resolveTemporaryList(NodeAST *parent, NodeList *list);

/**
 * Concatena dos NodeList sin reservar nueva memoria.
 *
 * @param list1 Primera lista de nodos.
 * @param list2 Segunda lista de nodos.
 * @return Lista de nodos concatenada.
 */
NodeList *mergeNodeLists(NodeList *list1, NodeList *list2);

/**
 * Desarma declaraciones múltiples en unitarias (ej: int x, y; -> int x; int y;).
 *
 * @param type Tipo de dato de las variables declaradas.
 * @param identifiers Lista de identificadores.
 * @return Lista con declaraciones unitarias.
 */
NodeList *resolveVariableDefinition(DataType type, NodeList *identifiers);

/**
 * Crea un CONSTANT_NODE para un literal numérico/booleano.
 *
 * @param type Tipo de dato de la constante.
 * @param value Valor de la constante como cadena de texto.
 * @return Nuevo nodo constante creado.
 */
NodeAST *newLiteralNode(DataType type, const char *value);

/**
 * Libera un Symbol y sus cadenas internas.
 *
 * @param symbol Símbolo a liberar.
 */
void freeSymbol(Symbol *symbol);

#endif /* AST_H */
