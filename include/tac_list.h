#ifndef TAC_LIST_H
#define TAC_LIST_H

/**
 * @file tac_list.h
 * @brief Estructura y operaciones sobre listas enlazadas de instrucciones TAC.
 */

#include "tac_instruction.h"

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
 * Reserva memoria dinámica para la estructura contenedora e inicializa
 * los punteros de cabeza y cola en NULL y el contador de elementos en cero.
 *
 * @return Puntero a la lista TAC creada, o NULL si falló la reserva de memoria.
 */
TACList *createTACList(void);

/**
 * @brief Libera la memoria de la lista TAC y todas sus instrucciones.
 *
 * Recorre secuencialmente la lista liberando cada instrucción mediante
 * @p freeTACInstruction y finalmente libera la estructura contenedora.
 *
 * @param list Puntero a la lista TAC a liberar (acepta NULL).
 */
void freeTACList(TACList *list);

/**
 * @brief Vuelca toda la lista de instrucciones TAC en formato textual usando string_builder.
 *
 * Itera secuencialmente sobre todas las instrucciones enlazadas e invoca
 * @p printTACInstruction sobre cada una.
 *
 * @param list Puntero a la lista TAC a imprimir (si es NULL no realiza acción).
 */
void printTACList(const TACList *list);

/**
 * @brief Agrega una instrucción al final de la lista TAC.
 *
 * Construye una nueva instrucción TAC cuádrupla con los operandos suministrados
 * y la enlaza al final de @p list, actualizando el puntero de cola y el contador.
 *
 * @param list Puntero a la lista TAC receptora.
 * @param operation Código de operación que define la instrucción.
 * @param result Operando de destino o etiqueta asociada.
 * @param firstOperand Primer operando fuente o argumento principal.
 * @param secondOperand Segundo operando fuente o argumento secundario.
 * @return Puntero a la instrucción agregada, o NULL en caso de error o falta de memoria.
 */
TACInstruction *appendInstruction(TACList *list, TACOperation operation, TACOperand result, TACOperand firstOperand, TACOperand secondOperand);

#endif // TAC_LIST_H
