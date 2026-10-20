#ifndef TAC_H
#define TAC_H

/**
 * @file tac.h
 * @brief Representación estructurada del Código de Tres Direcciones.
 *
 * El código intermedio vive como una lista enlazada de instrucciones, no como
 * texto. El texto es solo una de sus salidas posibles (printTACList); las
 * etapas que vienen después —intérprete, optimizador y generador x86-64—
 * recorren la lista directamente, sin volver a parsear nada.
 *
 * Este archivo define únicamente la estructura de datos y su impresión. Quien
 * la construye recorriendo el AST es `intermediate_code.c`.
 */

#include "types.h"
#include "symbol_table.h"

/** Longitud máxima del nombre de un temporal, variable, etiqueta o método. */
#define TAC_NAME_LENGTH 32

/**
 * @brief Operaciones del código de tres direcciones.
 */
typedef enum {
    /* Aritméticas: result = arg1 <op> arg2 */
    TAC_ADD,
    TAC_SUB,
    TAC_MUL,
    TAC_DIV,
    TAC_MOD,

    /* Unarias: result = <op> arg1 */
    TAC_NEG,
    TAC_NOT,

    /* Relacionales y lógicas: result = arg1 <op> arg2 */
    TAC_LT,
    TAC_GT,
    TAC_EQ,
    TAC_AND,
    TAC_OR,

    /* Movimiento de datos: result = arg1 */
    TAC_ASSIGN,

    /* Control de flujo */
    TAC_LABEL,     /**< result: etiqueta que se define aquí     */
    TAC_GOTO,      /**< arg1: etiqueta destino                  */
    TAC_IF_FALSE,  /**< si arg1 es falso, salta a arg2          */

    /* Métodos */
    TAC_PARAM,      /**< arg1: argumento a pasar                            */
    TAC_CALL,       /**< result (opcional) = call arg1, arg2 argumentos      */
    TAC_RETURN,     /**< arg1 opcional: valor devuelto                       */
    TAC_BEGIN_FUNC, /**< result: método que empieza                          */
    TAC_END_FUNC    /**< result: método que termina                          */
} TACOp;

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

/**
 * @brief Una instrucción de tres direcciones (cuádrupla).
 */
typedef struct TACInstruction {
    TACOp op;
    TACOperand result;
    TACOperand arg1;
    TACOperand arg2;
    struct TACInstruction *next;
} TACInstruction;

/**
 * @brief Lista enlazada de instrucciones, con puntero al último para que
 *        agregar al final sea O(1).
 */
typedef struct {
    TACInstruction *head;
    TACInstruction *tail;
    int count;
} TACList;

/* --- Constructores de operandos --- */

/** Operando ausente. */
TACOperand tacNone(void);
/** Temporal generado por el compilador. */
TACOperand tacTemp(const char *name, DataType type);
/** Variable o parámetro declarado; conserva el Symbol para las etapas siguientes. */
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

/* --- Lista de instrucciones --- */

/** Crea una lista vacía. */
TACList *createTACList(void);

/** Libera la lista y todas sus instrucciones. */
void freeTACList(TACList *list);

/**
 * Agrega una instrucción al final de la lista.
 * @return La instrucción creada, o NULL si falló la reserva de memoria.
 */
TACInstruction *appendInstruction(TACList *list, TACOp op, TACOperand result, TACOperand arg1, TACOperand arg2);

/* --- Salida --- */

/** Vuelca la lista como texto legible usando el string_builder. */
void printTACList(const TACList *list);

#endif /* TAC_H */
