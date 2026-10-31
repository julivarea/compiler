#ifndef TAC_OPERAND_H
#define TAC_OPERAND_H

/**
 * @file tac_operand.h
 * @brief Representación y constructores de operandos del Código de Tres Direcciones (TAC).
 */

#include "types.h"
#include "symbol_table.h"

/**
 * @brief Longitud máxima del nombre de un temporal, variable, etiqueta o método.
 *
 * Las variables llevan el sufijo "@<uid>", por lo que requieren holgura sobre
 * el largo del identificador original.
 */
#define TAC_NAME_LENGTH 64

/**
 * @brief Naturaleza o categoría de un operando TAC.
 *
 * Explicitar el tipo de operando evita que las etapas posteriores del compilador
 * deban deducirlo a partir de cadenas de texto.
 */
typedef enum OperandKind {
    OPERAND_NONE,      /**< El operando no se usa en esta instrucción. */
    OPERAND_TEMP,      /**< Temporal generado por el compilador (t0, t1...). */
    OPERAND_VARIABLE,  /**< Variable o parámetro declarado en el programa. */
    OPERAND_CONSTANT,  /**< Literal entero, booleano o real. */
    OPERAND_LABEL,     /**< Etiqueta de salto (L0, L1...). */
    OPERAND_METHOD     /**< Nombre de un método. */
} OperandKind;

/**
 * @brief Un operando de una instrucción TAC.
 *
 * Representa cualquier entidad manipulable en una cuádrupla de tres direcciones,
 * encapsulando su categoría, tipo de dato, representación textual y valor asociado.
 */
typedef struct TACOperand {
    OperandKind kind;              /**< Naturaleza o rol del operando. */
    DataType type;                 /**< Tipo de dato del valor (TYPE_VOID si no aplica). */
    char name[TAC_NAME_LENGTH];    /**< Nombre del temporal, variable, etiqueta o método. */
    Symbol *symbol;                /**< Puntero al símbolo asociado (solo en OPERAND_VARIABLE). */
    union {
        int intValue;              /**< Valor numérico para enteros y booleanos. */
        float floatValue;          /**< Valor numérico para literales reales. */
    } value;                       /**< Contenedor de valor inmediato (solo en OPERAND_CONSTANT). */
} TACOperand;

/**
 * @brief Crea e inicializa un operando ausente o no utilizado.
 *
 * Utilizado para cuádruplas unarias o instrucciones que no requieren todos los
 * operandos disponibles.
 *
 * @return Operando configurado con categoría OPERAND_NONE y tipo TYPE_VOID.
 */
TACOperand tacNone(void);

/**
 * @brief Crea un operando correspondiente a un temporal generado por el compilador.
 *
 * Asocia un nombre de temporal y su tipo de dato correspondiente.
 *
 * @param name Nombre identificador del temporal (por ejemplo "t0").
 * @param type Tipo de dato asociado al temporal.
 * @return Operando TAC configurado como temporal.
 */
TACOperand tacTemp(const char *name, DataType type);

/**
 * @brief Crea un operando para una variable o parámetro formal declarado.
 *
 * El nombre que queda en el operando es el del programa calificado con el @p uid
 * del símbolo (@p x@7), evitando colisiones entre variables homónimas de distintos
 * ámbitos. Conserva además la referencia a @p symbol para resolver su ubicación.
 *
 * @param symbol Puntero al símbolo de la tabla de símbolos (no debe ser NULL).
 * @return Operando TAC configurado como variable declarada.
 */
TACOperand tacVariable(Symbol *symbol);

/**
 * @brief Crea un operando para un literal constante entero.
 *
 * Almacena el valor entero en el campo @p intValue y formatea su representación textual.
 *
 * @param value Valor numérico entero.
 * @return Operando TAC de tipo constante entera.
 */
TACOperand tacConstantInt(int value);

/**
 * @brief Crea un operando para un literal constante booleano.
 *
 * Almacena el valor booleano numéricamente (0 o 1) y asigna "true" o "false" a su nombre.
 *
 * @param value Valor booleano (0 para falso, distinto de 0 para verdadero).
 * @return Operando TAC de tipo constante booleana.
 */
TACOperand tacConstantBool(int value);

/**
 * @brief Crea un operando para un literal constante de punto flotante.
 *
 * Almacena el valor real en el campo @p floatValue y genera su representación textual.
 *
 * @param value Valor de punto flotante.
 * @return Operando TAC de tipo constante real.
 */
TACOperand tacConstantFloat(float value);

/**
 * @brief Crea un operando para una etiqueta de salto de control de flujo.
 *
 * @param name Nombre identificador de la etiqueta (por ejemplo "L0").
 * @return Operando TAC de tipo etiqueta con tipo TYPE_VOID.
 */
TACOperand tacLabel(const char *name);

/**
 * @brief Crea un operando para la invocación o definición de un método.
 *
 * @param name Identificador del método.
 * @param returnType Tipo de dato retornado por el método.
 * @return Operando TAC de tipo método.
 */
TACOperand tacMethod(const char *name, DataType returnType);

/**
 * @brief Obtiene la representación textual del nombre de un operando.
 *
 * Retorna una cadena vacía en caso de que el operando sea nulo.
 *
 * @param operand Puntero al operando a consultar (acepta NULL).
 * @return Cadena constante con el nombre del operando, o "" si es NULL.
 */
const char *getOperandName(const TACOperand *operand);

#endif // TAC_OPERAND_H
