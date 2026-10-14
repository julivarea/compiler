#ifndef TYPES_H
#define TYPES_H

/**
 * @file types.h
 * @brief Definiciones de tipos de datos y operadores para el compilador.
 */

/* Tipos semanticos */
typedef enum {
    TYPE_INT,
    TYPE_BOOL,
    TYPE_FLOAT,
    TYPE_VOID,
    TYPE_ERROR
} DataType;

/* Operadores de expresiones */
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

/**
 * @brief Obtiene el nombre legible de un DataType.
 */
const char* getDataTypeName(DataType type);

#endif // TYPES_H
