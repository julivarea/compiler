#ifndef TAC_INSTRUCTION_H
#define TAC_INSTRUCTION_H

/**
 * @file tac_instruction.h
 * @brief Representación estructurada de una instrucción de código de tres direcciones.
 */

#include "tac_operand.h"
#include "tac_operation.h"

#include <stddef.h>

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
 * Reserva espacio para una estructura TACInstruction, configura su código de
 * operación y sus tres operandos (resultado, primer operando y segundo operando),
 * e inicializa el puntero de enlace siguiente en NULL.
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
 * Verifica que el puntero no sea nulo antes de invocar la liberación
 * de la estructura. No libera las instrucciones encadenadas en 'next'.
 *
 * @param instruction Puntero a la instrucción TAC a liberar (acepta NULL).
 */
void freeTACInstruction(TACInstruction *instruction);

/**
 * @brief Vuelca la representación textual de una instrucción TAC al acumulador de cadenas.
 *
 * Extrae los nombres calificados de los operandos y formatea la instrucción
 * según su categoría sintáctica (etiquetas, declaraciones, asignaciones,
 * saltos condicionales, llamadas o expresiones binarias/unarias), escribiendo
 * el resultado mediante buildStringf.
 *
 * @param instruction Puntero a la instrucción TAC a imprimir (si es NULL no realiza acción).
 */
void printTACInstruction(const TACInstruction *instruction);

/**
 * @brief Longitud máxima segura para la línea de texto de una instrucción TAC.
 *
 * Considera hasta tres operandos de nombre TAC_NAME_LENGTH más el texto fijo del formato.
 */
#define TAC_INSTRUCTION_TEXT_LENGTH 1024

/**
 * @brief Renderiza una instrucción TAC como línea de texto sin imprimir.
 *
 * Única fuente de verdad del formato textual: la usan el dump por stdout, el debug
 * del optimizador y el análisis de variables vivas. Escribe en el buffer del llamador
 * truncando si no alcanza.
 *
 * @param buffer Buffer de salida escrito como cadena terminada en nulo.
 * @param size Tamaño del buffer de salida.
 * @param instruction Puntero a la instrucción TAC (si es NULL escribe cadena vacía).
 */
void formatTACInstruction(char *buffer, size_t size, const TACInstruction *instruction);

#endif // TAC_INSTRUCTION_H
