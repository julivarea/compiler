#ifndef TAC_OPERATION_H
#define TAC_OPERATION_H

/**
 * @file tac_operation.h
 * @brief Operaciones y códigos de instrucción del Código de Tres Direcciones (TAC).
 */

typedef enum {
    // Métodos / Funciones
    TAC_BEGIN_FUNC, /**< result: método que empieza; arg1: nº de parámetros  */
    TAC_END_FUNC,   /**< result: método que termina                          */
    TAC_PARAM_DECL, /**< arg1: parámetro formal que el método recibe         */
    TAC_RETURN,     /**< arg1 opcional: valor devuelto                       */
    TAC_CALL,       /**< result (opcional) = call arg1, arg2 argumentos      */
    TAC_PARAM,      /**< arg1: argumento a pasar en el lugar de la llamada   */

    // Declaraciones
    TAC_GLOBAL_DECL, /**< variable global: va al segmento de datos            */
    TAC_LOCAL_DECL,  /**< variable local: ocupa un lugar en el frame          */

    // Control de flujo
    TAC_LABEL,      /**< result: etiqueta que se define aquí                 */
    TAC_GOTO,       /**< arg1: etiqueta destino                              */
    TAC_IF_FALSE,   /**< si arg1 es falso, salta a arg2                      */
    TAC_IF_TRUE,    /**< si arg1 es verdadero, salta a arg2                  */

    // Asignación
    TAC_ASSIGN,     /**< result = arg1                                       */

    // Aritméticas
    TAC_ADD,
    TAC_SUB,
    TAC_MUL,
    TAC_DIV,
    TAC_MOD,

    // Relacionales y lógicas
    TAC_LT,
    TAC_GT,
    TAC_EQ,
    TAC_AND,
    TAC_OR,

    // Unarias y conversiones
    TAC_NEG,
    TAC_NOT,
    TAC_INT_TO_FLOAT
} TACOperation;

#endif /* TAC_OPERATION_H */
