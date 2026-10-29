#ifndef TAC_OPERATION_H
#define TAC_OPERATION_H

/**
 * @file tac_operation.h
 * @brief Operaciones y códigos de instrucción del Código de Tres Direcciones (TAC).
 */

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
    TAC_PARAM_DECL,  /**< Declaración de parámetro formal (@p firstOperand: parámetro que se recibe). */
    TAC_RETURN,      /**< Retorno de método (@p firstOperand: valor opcional devuelto). */
    TAC_CALL,        /**< Llamada a método (@p result: destino opcional; @p firstOperand: método; @p secondOperand: número de argumentos). */
    TAC_PARAM,       /**< Paso de argumento en llamada (@p firstOperand: argumento a pasar). */

    /* Declaraciones */
    TAC_GLOBAL_DECL, /**< Declaración de variable global en segmento de datos (@p firstOperand: variable). */
    TAC_LOCAL_DECL,  /**< Declaración de variable local en registro de activación (@p firstOperand: variable). */

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

    /* Operaciones relacionales y lógicas */
    TAC_LT,          /**< Comparación menor que (@p result = @p firstOperand < @p secondOperand). */
    TAC_GT,          /**< Comparación mayor que (@p result = @p firstOperand > @p secondOperand). */
    TAC_EQ,          /**< Comparación de igualdad (@p result = @p firstOperand == @p secondOperand). */
    TAC_AND,         /**< Operación lógica Y (@p result = @p firstOperand && @p secondOperand). */
    TAC_OR,          /**< Operación lógica O (@p result = @p firstOperand || @p secondOperand). */

    /* Operaciones unarias y conversiones */
    TAC_NEG,         /**< Negación aritmética (@p result = -@p firstOperand). */
    TAC_NOT,         /**< Negación lógica (@p result = !@p firstOperand). */
    TAC_INT_TO_FLOAT /**< Conversión de tipo entero a flotante (@p result = int_to_float @p firstOperand). */
} TACOperation;

#endif // TAC_OPERATION_H
