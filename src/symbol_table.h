#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

/**
 * @file symbol_table.h
 * @brief Estructuras y funciones para la tabla de símbolos del compilador.
 */

/**
 * @brief Declaración adelantada de la estructura Symbol.
 */
typedef struct Symbol Symbol;
struct NodeAST;

/**
 * @brief Define los tipos de símbolos que pueden existir en la tabla.
 */
typedef enum {
    ID_SYMBOL,       
    METHOD_SYMBOL,  
    CONSTANT_SYMBOL,
    DECLARATION_SYMBOL 
} SymbolType;

#include "ast.h"

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

Scope* createScope(int level, Scope *parent, DataType returnType);
void appendSymbol(Scope *scope, struct Symbol *symbol);
void appendChildScope(Scope *parent, Scope *child);
Scope* analyzeSemantics(struct NodeAST *root, Scope *parent, DataType returnType);
struct Symbol* lookupVariable(Scope *scope, const char *id);
void checkMainMethodExists(Scope *globalScope);


/**
 * Representa un símbolo en la tabla de símbolos.
 * 
 * Contiene toda la información semántica necesaria de un identificador, 
 * método o constante recopilada durante el análisis del programa.
 */
struct Symbol {
    char *id;
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
 * Crea e inicializa un nuevo símbolo.
 * 
 * @param id Nombre del identificador (se realizará una copia interna de la cadena).
 * @param symbolType Categoría del símbolo (variable, método, constante).
 * @param type Tipo de dato devuelto o almacenado por el símbolo.
 * @param parameters Parámetros asociados (si aplica).
 * @return Puntero a la nueva instancia de Symbol creada.
 */
Symbol *newSymbol(const char *id, SymbolType symbolType, DataType type, struct NodeAST *parameters);

/**
 *  Libera la memoria ocupada por un símbolo y sus componentes.
 * 
 * @param symbol Puntero al símbolo que se desea destruir.
 */
void freeSymbol(Symbol *symbol);

#endif // SYMBOL_TABLE_H
