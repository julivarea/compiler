#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

/**
 * @file symbol_table.h
 * @brief Estructuras y operaciones sobre la tabla de símbolos y ámbitos léxicos (scopes).
 */

#include "types.h"

struct NodeAST;

/**
 * @brief Categorías de símbolos gestionados por el compilador.
 */
typedef enum SymbolType {
    ID_SYMBOL,          /**< Identificador de variable o parámetro. */
    METHOD_SYMBOL,      /**< Identificador de método o función. */
    CONSTANT_SYMBOL,    /**< Literal o valor constante primitivo. */
    DECLARATION_SYMBOL  /**< Símbolo asociado a una declaración de tipo. */
} SymbolType;

/**
 * @brief Representa un símbolo (variable, método o constante) registrado en un ámbito.
 */
typedef struct Symbol {
    char *id;                   /**< Nombre textual del identificador en el código fuente. */
    int uid;                    /**< Identificador único asignado al entrar a un ámbito (0 si no está declarado). */
    DataType type;              /**< Tipo de dato semántico asociado al símbolo. */
    SymbolType symbolType;      /**< Categoría sintáctico-semántica del símbolo. */
    struct NodeAST *parameters; /**< Lista de parámetros formales (solo si symbolType es METHOD_SYMBOL). */
    union {
        int int_val;            /**< Valor numérico para constantes de tipo entero o booleano. */
        float float_val;        /**< Valor numérico para constantes de punto flotante. */
        char *str_val;          /**< Puntero a cadena para literales textuales. */
    } value;
} Symbol;

/**
 * @brief Crea e inicializa un nuevo símbolo en memoria dinámica.
 *
 * Reserva memoria para la estructura Symbol, duplica la cadena @p id si
 * no es nula y configura los metadatos iniciales con @p uid en 0 hasta su registro.
 *
 * @param id Nombre textual del identificador (acepta NULL para literales anónimos).
 * @param symbolType Categoría semántica del símbolo a instanciar.
 * @param type Tipo de dato semántico asignado.
 * @param parameters Lista de nodos AST con parámetros formales (NULL si no es método).
 * @return Puntero al nuevo símbolo creado, o NULL si falló la reserva de memoria.
 */
Symbol *newSymbol(const char *id, SymbolType symbolType, DataType type, struct NodeAST *parameters);

/**
 * @brief Libera la memoria ocupada por un símbolo y su identificador.
 *
 * Comprueba que el puntero no sea nulo antes de liberar el texto del nombre
 * duplicado y la estructura del símbolo en memoria.
 *
 * @param symbol Puntero al símbolo a liberar (acepta NULL).
 */
void freeSymbol(Symbol *symbol);

/**
 * @brief Representa un ámbito léxico (scope) con su jerarquía y símbolos asociados.
 */
typedef struct Scope {
    int level;                  /**< Nivel de anidamiento léxico (0 para el ámbito global). */
    Symbol **symbols;           /**< Arreglo dinámico de símbolos declarados en este ámbito. */
    int symbol_count;           /**< Cantidad actual de símbolos registrados en el ámbito. */
    int symbol_capacity;        /**< Capacidad total del arreglo de símbolos antes de requerir realocación. */
    Symbol *function;           /**< Símbolo del método contenedor asociado a este ámbito (NULL si es global). */
    struct Scope *parent;       /**< Ámbito léxico padre inmediato (NULL para el ámbito raíz global). */
    struct Scope **childrens;   /**< Arreglo dinámico de sub-ámbitos léxicos anidados. */
    int children_count;         /**< Cantidad de sub-ámbitos anidados registrados. */
    int children_capacity;      /**< Capacidad total del arreglo de sub-ámbitos. */
} Scope;

/**
 * @brief Crea e inicializa un nuevo ámbito (scope) en memoria dinámica.
 *
 * Configura el nivel de anidamiento léxico, establece el enlace al ámbito @p parent
 * y al método contenedor @p function, e inicializa los arreglos dinámicos internos.
 *
 * @param level Nivel de anidamiento léxico (0 para el nivel global).
 * @param parent Puntero al ámbito léxico padre contenedor (NULL para el ámbito global).
 * @param function Símbolo del método contenedor del ámbito (NULL si no pertenece a un método).
 * @return Puntero al nuevo ámbito creado, o NULL si falló la reserva de memoria.
 */
Scope *createScope(int level, Scope *parent, Symbol *function);

/**
 * @brief Agrega un símbolo al ámbito actual asignándole un identificador único si no lo poseía.
 *
 * Inserta el @p symbol en el arreglo dinámico de @p scope redimensionándolo si es
 * necesario, y asigna un valor positivo a su campo @p uid si aún no poseía uno.
 *
 * @param scope Ámbito donde se registrará la entidad declarada (si es NULL no realiza acción).
 * @param symbol Símbolo a registrar en el entorno (si es NULL no realiza acción).
 */
void appendSymbol(Scope *scope, Symbol *symbol);

/**
 * @brief Agrega un ámbito hijo a la colección de sub-ámbitos del ámbito padre.
 *
 * Redimensiona el arreglo dinámico de sub-ámbitos de @p parent de ser necesario
 * e inserta el puntero al nuevo @p child.
 *
 * @param parent Ámbito léxico contenedor que recibirá al nuevo hijo (si es NULL no realiza acción).
 * @param child Sub-ámbito léxico a incorporar (si es NULL no realiza acción).
 */
void appendChildScope(Scope *parent, Scope *child);

/**
 * @brief Busca un identificador recursivamente en el ámbito actual y en sus ancestros.
 *
 * Recorre linealmente los símbolos de @p scope. Si no lo encuentra, continúa la búsqueda
 * de manera ascendente a través de la cadena de punteros hacia los ámbitos padre.
 *
 * @param scope Ámbito léxico donde comienza la resolución (NULL retorna NULL).
 * @param id Nombre textual del símbolo que se desea localizar (no debe ser NULL).
 * @return Puntero al símbolo hallado en la jerarquía, o NULL si no está declarado.
 */
Symbol *lookupVariable(Scope *scope, const char *id);

/**
 * @brief Busca un identificador exclusivamente en el ámbito léxico actual.
 *
 * Realiza una búsqueda no recursiva sobre los símbolos locales de @p scope,
 * utilizada principalmente para validar colisiones de declaraciones duplicadas.
 *
 * @param scope Ámbito léxico local a inspeccionar (NULL retorna NULL).
 * @param id Nombre textual del símbolo a verificar (no debe ser NULL).
 * @return Puntero al símbolo si ya existe localmente, o NULL en caso contrario.
 */
Symbol *lookupVariableCurrentScope(Scope *scope, const char *id);

#endif // SYMBOL_TABLE_H
