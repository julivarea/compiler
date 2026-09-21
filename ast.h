#ifndef AST_H
#define AST_H

/* Tipos semanticos  */
typedef enum {
    TYPE_INT,
    TYPE_BOOL,
    TYPE_FLOAT,
    TYPE_VOID
} DataType;

/* Tipos de nodo del AST */
typedef enum {
    TYPE_NODE,  /* Type: int | boolean | float */
    VARIABLE_DECLARATION_NODE,    /* VarDecl: <type> <id>+, ; */
    ID_NODE,          /* uso/mencion de un identificador */
    ASSIGNMENT_NODE,  /* Statement: <id> = <expr> ; */
    CONSTANT_NODE     /* Expr: literal (por ahora, NUMBER) */
} NodeType;

/* Symbol 
 * id y value solamente. El tipo NO va aca (va en NodeAST.type) 
 * para no duplicar informacion ni tener dos fuentes de verdad
 * distintas para el tipo. */
typedef struct Symbol {
    char *id;
    char *value;
} Symbol;

/* NodeAST
 * left/right para nodos binarios (ASSIGNMENT_NODE). children/childCount
 * para nodos con una cantidad variable de hijos (VARIABLE_DECLARATION_NODE, ver
 * IdList en bison.y). */
typedef struct NodeAST {
    Symbol *symbol;
    struct NodeAST *left;
    struct NodeAST *right;
    DataType type;
    NodeType nodeType;
    struct NodeAST **children;
    int childCount;
} NodeAST;

/* NodeList 
 * Lista enlazada auxiliar, usada solo durante la construccion del
 * AST (dentro de bison.y) para acumular nodos hermanos antes de volcarlos al
 * arreglo children[] del nodo padre. No forma parte del AST final. */
typedef struct NodeList {
    NodeAST *node;
    struct NodeList *next;
} NodeList;

/* Prototipos 
 * Implementados en ast.c, cuerpos vacios por ahora (a proposito). */

/*
 * newSymbol
 * ---------
 * Crea un Symbol reservado dinamicamente con los valores dados.
 *
 * Parametros:
 *   id    - identificador o lexema asociado (por ejemplo el nombre de
 *           una variable). Puede ser NULL si el nodo no necesita id.
 *   value - valor asociado (por ejemplo el valor literal de una
 *           constante). Puede ser NULL si todavia no se conoce.
 *
 * Devuelve:
 *   Puntero a un Symbol nuevo. La implementacion es responsable de
 *   copiar las cadenas recibidas (no quedarse con el puntero original)
 *   para que el Symbol sea independiente del buffer de yytext, que
 *   flex reutiliza en cada token.
 *
 * Dueño de la memoria: el llamador es dueño del Symbol devuelto.
 */
Symbol *newSymbol(const char *id, const char *value);

/*
 * newNode
 * -------
 * Crea un NodeAST reservado dinamicamente.
 *
 * Parametros:
 *   nodeType - tipo de nodo (ver enum NodeType).
 *   symbol   - symbol asociado al nodo, o NULL si no aplica.
 *   left     - hijo izquierdo, o NULL.
 *   right    - hijo derecho, o NULL.
 *
 * Devuelve:
 *   Puntero a un NodeAST nuevo, con children == NULL y childCount == 0
 *   (se completan aparte con attachChildren). El campo type se espera
 *   inicializado a un valor por defecto, ya que el tipo real de la
 *   mayoria de los nodos se resuelve recien en el analisis semantico.
 *
 * Dueño de la memoria:
 *   El llamador es dueño del NodeAST devuelto. left, right y symbol
 *   quedan referenciados (no copiados): el nodo padre pasa a ser
 *   responsable de esos punteros tambien.
 */
NodeAST *newNode(NodeType nodeType, Symbol *symbol, NodeAST *left, NodeAST *right);

/*
 * newNodeList
 * -----------
 * Crea (o extiende) una lista enlazada de nodos hermanos, usada
 * unicamente durante la construccion del AST en bison.y.
 *
 * Parametros:
 *   node - nodo a envolver en esta celda de la lista.
 *   next - resto de la lista (NULL si este es el ultimo elemento).
 *
 * Devuelve:
 *   Puntero a la nueva celda NodeList, con node y next asignados
 *   tal cual se recibieron.
 *
 * Dueño de la memoria:
 *   El llamador es dueño de la celda devuelta. Es una estructura
 *   transitoria: no queda colgada del AST final (ver attachChildren).
 */
NodeList *newNodeList(NodeAST *node, NodeList *next);

/*
 * attachChildren
 * --------------
 * Vuelca una NodeList (acumulada durante el parsing) al arreglo
 * children[] de un NodeAST ya existente, fijando tambien childCount.
 *
 * Parametros:
 *   parent - nodo al que se le asignan los children. Se espera que
 *            parent->children sea NULL antes de llamar a esta funcion.
 *   list   - lista de nodos hijos, en el orden en que deben quedar en
 *            children[]. Puede ser NULL (cero hijos).
 *
 * Efecto:
 *   Modifica parent->children y parent->childCount in-place.
 *
 * Dueño de la memoria:
 *   Las celdas de list dejan de ser necesarias una vez volcado su
 *   contenido a children[]; la implementacion decide si las libera
 *   aca o si eso queda a cargo del llamador.
 */
void attachChildren(NodeAST *parent, NodeList *list);

#endif /* AST_H */
