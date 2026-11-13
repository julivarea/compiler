#ifndef TAC_H
#define TAC_H

/**
 * @file tac.h
 * @brief Representación intermedia de Código de Tres Direcciones (TAC):
 *        operaciones, operandos, cuádruplas e instrucciones enlazadas.
 */

#include "types.h"
#include "symbol_table.h"

#include <stddef.h>
#include <stdio.h>

/* ========================================================================= */
/* 1. Constantes y Límites                                                   */
/* ========================================================================= */

/**
 * @brief Longitud máxima del nombre de un temporal, variable, etiqueta o método.
 *
 * Las variables llevan el sufijo "@<uid>", por lo que requieren holgura sobre
 * el largo del identificador original.
 */
#define TAC_NAME_LENGTH 256

/**
 * @brief Longitud máxima segura para la línea de texto de una instrucción TAC.
 *
 * Considera hasta tres operandos de nombre TAC_NAME_LENGTH más el texto fijo del formato.
 */
#define TAC_INSTRUCTION_TEXT_LENGTH 1024

/* ========================================================================= */
/* 2. Operaciones TAC (Opcodes)                                              */
/* ========================================================================= */

/**
 * @brief Códigos de operación para instrucciones de Código de Tres Direcciones (TAC).
 *
 * Define el conjunto de mnemónicos u operaciones intermedias soportadas por el compilador,
 * incluyendo llamadas a funciones, declaraciones, control de flujo, asignaciones,
 * conversiones y operaciones aritmético-lógicas.
 */
typedef enum {
    /* Métodos y funciones */
    TAC_BEGIN_FUNC,  /**< Inicio de método (@p result: método; @p firstOperand: número de parámetros). */
    TAC_END_FUNC,    /**< Fin de método (@p result: método que termina). */
    TAC_PARAM_DECL,  /**< Declaración de parámetro formal (@p result: parámetro que se recibe). */
    TAC_RETURN,      /**< Retorno de método (@p firstOperand: valor opcional devuelto). */
    TAC_CALL,        /**< Llamada a método (@p result: destino opcional; @p firstOperand: método; @p secondOperand: número de argumentos). */
    TAC_PARAM,       /**< Paso de argumento en llamada (@p firstOperand: argumento a pasar). */

    /* Declaraciones */
    TAC_GLOBAL_DECL, /**< Declaración de variable global en segmento de datos (@p result: variable). */
    TAC_LOCAL_DECL,  /**< Declaración de variable local en registro de activación (@p result: variable). */

    /* Control de flujo */
    TAC_LABEL,       /**< Definición de etiqueta de salto (@p result: etiqueta que se define). */
    TAC_GOTO,        /**< Salto incondicional (@p firstOperand: etiqueta destino). */
    TAC_IF_FALSE,    /**< Salto condicional si falso (@p firstOperand: condición; @p secondOperand: etiqueta destino). */
    TAC_IF_TRUE,     /**< Salto condicional si verdadero (@p firstOperand: condición; @p secondOperand: etiqueta destino). */

    /* Asignación */
    TAC_ASSIGN,      /**< Asignación simple (@p result = @p firstOperand). */

    /* Operaciones aritméticas */
    TAC_ADD,         /**< Suma binaria (@p result = @p firstOperand + @p secondOperand). */
    TAC_SUB,         /**< Resta binaria (@p result = @p firstOperand - @p secondOperand). */
    TAC_MUL,         /**< Multiplicación binaria (@p result = @p firstOperand * @p secondOperand). */
    TAC_DIV,         /**< División binaria (@p result = @p firstOperand / @p secondOperand). */
    TAC_MOD,         /**< Módulo binario (@p result = @p firstOperand % @p secondOperand). */

    /* Operaciones relacionales */
    TAC_LT,          /**< Comparación menor que (@p result = @p firstOperand < @p secondOperand). */
    TAC_GT,          /**< Comparación mayor que (@p result = @p firstOperand > @p secondOperand). */
    TAC_EQ,          /**< Comparación de igualdad (@p result = @p firstOperand == @p secondOperand). */

    /* Operaciones unarias */
    TAC_UMINUS,      /**< Negación aritmética (@p result = -@p firstOperand). */
    TAC_NOT          /**< Negación lógica (@p result = !@p firstOperand). */
} TACOperation;

/* ========================================================================= */
/* 3. Operandos TAC                                                          */
/* ========================================================================= */

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
    } value;                       /**< Contenedor de valor inmediato (solo en OPERAND_CONSTANT). */
} TACOperand;

/**
 * @brief Crea e inicializa un operando ausente o no utilizado.
 *
 * @return Operando configurado con categoría OPERAND_NONE y tipo TYPE_VOID.
 */
TACOperand tacNone(void);

/**
 * @brief Crea un operando correspondiente a un temporal generado por el compilador.
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
 * @param value Valor numérico entero.
 * @return Operando TAC de tipo constante entera.
 */
TACOperand tacConstantInt(int value);

/**
 * @brief Crea un operando para un literal constante booleano.
 *
 * @param value Valor booleano (0 para falso, distinto de 0 para verdadero).
 * @return Operando TAC de tipo constante booleana.
 */
TACOperand tacConstantBool(int value);

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
 * Retorna una cadena vacía en caso de que el operando sea nulo o no utilizado (OPERAND_NONE).
 *
 * @param operand Puntero al operando a consultar (acepta NULL).
 * @return Cadena constante con el nombre del operando, o "" si es NULL o inactivo.
 */
const char *getOperandName(const TACOperand *operand);

/* ========================================================================= */
/* 4. Instrucciones TAC (Cuádruplas)                                         */
/* ========================================================================= */

/**
 * @brief Estructura de datos que agrupa el operador y los operandos de una instrucción de tres direcciones.
 */
typedef struct TACInstruction {
    TACOperation operation;
    TACOperand result;
    TACOperand firstOperand;
    TACOperand secondOperand;
    struct TACInstruction *next;
} TACInstruction;

/**
 * @brief Crea e inicializa una nueva instrucción TAC cuádrupla en memoria dinámica.
 *
 * @param operation Código de operación que define la instrucción.
 * @param result Operando de destino o etiqueta asociada.
 * @param firstOperand Primer operando fuente o argumento principal.
 * @param secondOperand Segundo operando fuente o argumento secundario.
 * @return Puntero a la instrucción creada, o NULL si falló la reserva de memoria.
 */
TACInstruction *createTACInstruction(TACOperation operation, TACOperand result, TACOperand firstOperand, TACOperand secondOperand);

/**
 * @brief Libera la memoria ocupada por una instrucción TAC individual.
 *
 * @param instruction Puntero a la instrucción TAC a liberar (acepta NULL).
 */
void freeTACInstruction(TACInstruction *instruction);

/**
 * @brief Vuelca la representación textual de una instrucción TAC al acumulador de cadenas.
 *
 * @param instruction Puntero a la instrucción TAC a imprimir (si es NULL no realiza acción).
 */
void printTACInstruction(const TACInstruction *instruction);

/**
 * @brief Escribe la representación textual de una instrucción TAC en el flujo de salida indicado.
 *
 * @param output Flujo de archivo destino (si es NULL no realiza acción).
 * @param instruction Puntero a la instrucción TAC a imprimir (si es NULL no realiza acción).
 */
void fprintTACInstruction(FILE *output, const TACInstruction *instruction);

/**
 * @brief Renderiza una instrucción TAC como línea de texto sin imprimir.
 *
 * @param buffer Buffer de salida escrito como cadena terminada en nulo.
 * @param size Tamaño del buffer de salida.
 * @param instruction Puntero a la instrucción TAC (si es NULL escribe cadena vacía).
 */
void formatTACInstruction(char *buffer, size_t size, const TACInstruction *instruction);

/* ========================================================================= */
/* 5. Lista de Instrucciones TAC                                             */
/* ========================================================================= */

/**
 * @brief Lista enlazada de instrucciones TAC.
 *
 * Mantiene punteros al primer y último elemento para inserciones en tiempo
 * constante O(1), además de la cantidad total de instrucciones almacenadas.
 */
typedef struct TACList {
    TACInstruction *head;
    TACInstruction *tail;
    int count;
} TACList;

/**
 * @brief Crea una nueva lista TAC vacía.
 *
 * @return Puntero a la lista TAC creada, o NULL si falló la reserva de memoria.
 */
TACList *createTACList(void);

/**
 * @brief Libera la memoria de la lista TAC y todas sus instrucciones.
 *
 * @param list Puntero a la lista TAC a liberar (acepta NULL).
 */
void freeTACList(TACList *list);

/**
 * @brief Vuelca toda la lista de instrucciones TAC en formato textual usando string_builder.
 *
 * @param list Puntero a la lista TAC a formatear (si es NULL no realiza acción).
 */
void formatTACList(const TACList *list);

/**
 * @brief Conecta una instrucción TAC ya creada al final de la lista.
 *
 * @param list Puntero a la lista TAC receptora.
 * @param instruction Puntero a la instrucción TAC a anexar.
 * @return Puntero a la instrucción agregada, o NULL si list o instruction son NULL.
 */
TACInstruction *appendTACInstruction(TACList *list, TACInstruction *instruction);

/**
 * @brief Agrega una instrucción al final de la lista TAC construyéndola a partir de sus componentes.
 *
 * @param list Puntero a la lista TAC receptora.
 * @param operation Código de operación que define la instrucción.
 * @param result Operando de destino o etiqueta asociada.
 * @param firstOperand Primer operando fuente o argumento principal.
 * @param secondOperand Segundo operando fuente o argumento secundario.
 * @return Puntero a la instrucción agregada, o NULL en caso de error o falta de memoria.
 */
TACInstruction *appendInstruction(TACList *list, TACOperation operation, TACOperand result, TACOperand firstOperand, TACOperand secondOperand);

#endif // TAC_H
