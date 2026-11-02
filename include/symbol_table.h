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
    VARIABLE_SYMBOL,
    METHOD_SYMBOL,
    CONSTANT_SYMBOL,
    FUNCTION_ENTRY_SYMBOL, // for interpret
} SymbolType;

/**
 * @brief Símbolo registrado en un ámbito léxico.
 */
typedef struct Symbol {
    char *id;
    int uid;                    /**< Identificador único (0 si aún no fue registrado en un ámbito). */
    DataType type;
    SymbolType symbolType;
    struct NodeAST *parameters; /**< Parámetros formales si symbolType es METHOD_SYMBOL. */
    union {
        int intValue;
        float floatValue;
        NodeAST* function;
    } value;
} Symbol;

/**
 * @brief Crea un nuevo símbolo en memoria dinámica.
 *
 * @param id Identificador textual (acepta NULL para constantes anónimas).
 * @param parameters Parámetros formales (NULL si no es método).
 * @return Puntero al símbolo creado, o NULL si falló la reserva de memoria.
 */
Symbol *newSymbol(char *id, SymbolType symbolType, DataType type, struct NodeAST *parameters);

/**
 * @brief Libera la memoria ocupada por un símbolo y su identificador.
 *
 * @param symbol Símbolo a liberar (acepta NULL).
 */
void freeSymbol(Symbol *symbol);

/**
 * @brief Ámbito léxico (scope) en la jerarquía de la tabla de símbolos.
 */
typedef struct Scope {
    int level;               /**< Nivel de anidamiento léxico (0 para el ámbito global). */
    Symbol **symbols;
    int symbolCount;
    int symbolCapacity;
    Symbol *function;        /**< Método contenedor (NULL si es global). */
    struct Scope *parent;    /**< Ámbito léxico padre (NULL para el ámbito raíz global). */
    struct Scope **children;
    int childrenCount;
    int childrenCapacity;
} Scope;

/**
 * @brief Crea un nuevo ámbito léxico en memoria dinámica.
 *
 * @param level Nivel de anidamiento léxico (0 para el nivel global).
 * @param parent Ámbito léxico padre (NULL para el ámbito global).
 * @param function Método contenedor (NULL si no pertenece a un método).
 * @return Ámbito creado, o NULL si falló la reserva de memoria.
 */
Scope *createScope(int level, Scope *parent, Symbol *function);

/**
 * @brief Agrega un símbolo al ámbito asignándole un uid si aún no poseía uno.
 *
 * @param scope Ámbito donde se registra el símbolo (acepta NULL).
 * @param symbol Símbolo a registrar (acepta NULL).
 */
void appendSymbol(Scope *scope, Symbol *symbol);

/**
 * @brief Agrega un sub-ámbito como hijo del ámbito padre.
 *
 * @param parent Ámbito léxico contenedor (acepta NULL).
 * @param child Sub-ámbito a incorporar (acepta NULL).
 */
void appendChildScope(Scope *parent, Scope *child);

/**
 * @brief Busca un identificador en el ámbito actual y en sus ancestros.
 *
 * @param scope Ámbito donde comienza la búsqueda (acepta NULL).
 * @param id Identificador del símbolo a localizar.
 * @return Símbolo hallado, o NULL si no está declarado.
 */
Symbol *lookupSymbol(Scope *scope, const char *id);

/**
 * @brief Busca un identificador exclusivamente en el ámbito léxico actual.
 *
 * @param scope Ámbito local a inspeccionar (acepta NULL).
 * @param id Identificador del símbolo a verificar.
 * @return Símbolo si existe localmente, o NULL en caso contrario.
 */
Symbol *lookupSymbolInScope(Scope *scope, const char *id);

/**
 * @brief Reinicia a 1 el contador de identificadores únicos (uid) de símbolos.
 */
void resetSymbolUids(void);

#endif // SYMBOL_TABLE_H
