#ifndef TAC_OPERAND_H
#define TAC_OPERAND_H

/**
 * @file tac_operand.h
 * @brief Representación y constructores de operandos del Código de Tres Direcciones (TAC).
 *
 * Define la estructura de datos TACOperand, las diferentes
 * naturalezas de operandos (OperandKind), y los constructores para instanciar
 * temporales, variables, constantes, etiquetas y métodos.
 */

#include "types.h"
#include "symbol_table.h"

/** Longitud máxima del nombre de un temporal, variable, etiqueta o método.
 *  Las variables llevan el sufijo "@<uid>", así que necesitan holgura sobre el
 *  largo del identificador original. */
#define TAC_NAME_LENGTH 64

/**
 * @brief Naturaleza de un operando.
 *
 * Tenerla explícita evita que las etapas siguientes tengan que deducirla
 * mirando la cadena (por ejemplo, para saber si `5` es una constante o una
 * variable llamada así, o si `x` es global o local).
 */
typedef enum {
    OPERAND_NONE,      /**< el operando no se usa en esta instrucción */
    OPERAND_TEMP,      /**< temporal generado por el compilador (t0, t1...) */
    OPERAND_VARIABLE,  /**< variable o parámetro declarado en el programa */
    OPERAND_CONSTANT,  /**< literal entero, booleano o real */
    OPERAND_LABEL,     /**< etiqueta de salto (L0, L1...) */
    OPERAND_METHOD     /**< nombre de un método */
} OperandKind;

/**
 * @brief Un operando de una instrucción TAC.
 */
typedef struct {
    OperandKind kind;
    DataType type;                 /**< tipo del valor; TYPE_VOID si no aplica */
    char name[TAC_NAME_LENGTH];    /**< temporal, variable, etiqueta o método */
    Symbol *symbol;                /**< solo OPERAND_VARIABLE: da acceso al uid y al scope */
    union {
        int int_val;               /**< enteros y booleanos */
        float float_val;
    } value;                       /**< solo OPERAND_CONSTANT */
} TACOperand;

/* --- Constructores de operandos --- */

/** Operando ausente. */
TACOperand tacNone(void);

/** Temporal generado por el compilador. */
TACOperand tacTemp(const char *name, DataType type);

/**
 * Variable o parámetro declarado.
 *
 * El nombre que queda en el operando es el del programa calificado con el `uid`
 * del símbolo (`x@7`), de modo que dos variables homónimas declaradas en scopes
 * distintos no se confunden en el código generado. Además conserva el puntero
 * al `Symbol`, que es lo que usan las etapas siguientes para resolver
 * almacenamiento.
 */
TACOperand tacVariable(Symbol *symbol);

/** Literal entero. */
TACOperand tacConstantInt(int value);

/** Literal booleano (se almacena como entero 0/1). */
TACOperand tacConstantBool(int value);

/** Literal real. */
TACOperand tacConstantFloat(float value);

/** Etiqueta de salto. */
TACOperand tacLabel(const char *name);

/** Nombre de método, con su tipo de retorno. */
TACOperand tacMethod(const char *name, DataType returnType);

/** Obtiene la representación textual / nombre del operando (o cadena vacía si es NULL). */
const char *getOperandName(const TACOperand *operand);

#endif /* TAC_OPERAND_H */
