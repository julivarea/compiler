#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

/**
 * @file symbol_table.h
 * @brief Estructuras y operaciones sobre la tabla de símbolos y ámbitos (Scopes).
 *
 * Módulo responsable exclusivamente de la gestión del entorno (almacenamiento
 * y resolución de identificadores y sus tipos por ámbito léxico).
 */

#include "types.h"

struct NodeAST;

/**
 * @brief Categorías de símbolos en el compilador.
 */
typedef enum {
    ID_SYMBOL,       
    METHOD_SYMBOL,  
    CONSTANT_SYMBOL,
    DECLARATION_SYMBOL 
} SymbolType;

typedef struct Symbol Symbol;

/**
 * @brief Representa un ámbito (scope) léxico con su lista de símbolos y scopes anidados.
 */
typedef struct Scope {
    int level;              
    struct Symbol **symbols; 
    int symbol_count;
    int symbol_capacity;
    DataType returnType;
    struct Scope *parent; 
    struct Scope **childrens;  
    int children_count;
    int children_capacity;
} Scope;

/**
 * @brief Representa un símbolo (variable, método o constante) registrado en un scope.
 */
struct Symbol {
    char *id;
    /**
     * Identificador unico de la entidad declarada, asignado por appendSymbol
     * cuando el simbolo entra a un scope. Dos variables con el mismo nombre en
     * scopes distintos (ocultamiento) tienen uid distinto.
     * uid == 0 significa "no es una entidad declarada" (p. ej. una constante).
     */
    int uid;
    DataType type;
    SymbolType symbolType;
    struct NodeAST *parameters;
    union {
        int int_val;
        float float_val;
        char *str_val;
    } value;
};

/**
 * @brief Crea e inicializa un nuevo símbolo.
 */
Symbol *newSymbol(const char *id, SymbolType symbolType, DataType type, struct NodeAST *parameters);

/**
 * @brief Libera la memoria ocupada por un símbolo.
 */
void freeSymbol(Symbol *symbol);

/**
 * @brief Crea un nuevo ámbito (scope) hijo de parent con un tipo de retorno esperado.
 */
Scope* createScope(int level, Scope *parent, DataType returnType);

/**
 * @brief Agrega un símbolo al scope actual asignándole un UID único si no lo tenía.
 */
void appendSymbol(Scope *scope, Symbol *symbol);

/**
 * @brief Agrega un scope hijo a la lista de scopes anidados del padre.
 */
void appendChildScope(Scope *parent, Scope *child);

/**
 * @brief Busca un identificador sólo en el ámbito actual (sin subir a los padres).
 */
Symbol* lookupVariableCurrentScope(Scope *scope, const char *id);

/**
 * @brief Busca un identificador en el scope actual y, recursivamente, en los padres.
 */
Symbol* lookupVariable(Scope *scope, const char *id);

#endif // SYMBOL_TABLE_H
