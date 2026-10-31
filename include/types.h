#ifndef TYPES_H
#define TYPES_H

/**
 * @file types.h
 * @brief Definiciones de tipos de datos semánticos y operadores de expresiones.
 */

/**
 * @brief Tipos de datos primitivos y de control semántico del lenguaje.
 *
 * Representa los tipos deducidos o declarados durante las fases de análisis
 * semántico y verificación de tipos en el AST y en la tabla de símbolos.
 */
typedef enum DataType {
    TYPE_INT,
    TYPE_BOOL,
    TYPE_FLOAT,
    TYPE_VOID,
    TYPE_ERROR
} DataType;

/**
 * @brief Obtiene la representación textual legible de un tipo de datos semántico.
 *
 * Convierte el enumerador @p type a su cadena constante representativa
 * ("INT", "BOOL", "FLOAT", "VOID" o "ERROR"). Si el tipo no es reconocido,
 * retorna "UNKNOWN".
 *
 * @param type Tipo de dato semántico a convertir en texto.
 * @return Cadena constante con el nombre legible del tipo de dato.
 */
const char *getDataTypeName(DataType type);

/**
 * @brief Operadores admitidos en nodos de expresiones del AST.
 *
 * Clasifica los distintos tipos de operadores aritméticos, relacionales,
 * lógicos y unarios que pueden conformar una expresión binaria o unaria.
 */
typedef enum OperationType {
    OP_ADD,
    OP_UMINUS,
    OP_NOT,
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

#endif // TYPES_H
