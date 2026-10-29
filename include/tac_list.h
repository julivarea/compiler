#ifndef TAC_LIST_H
#define TAC_LIST_H

/**
 * @file tac_list.h
 * @brief Estructura y operaciones sobre listas enlazadas de instrucciones TAC.
 */

#include "tac_instruction.h"

/**
 * @brief Lista enlazada de instrucciones TAC.
 */
typedef struct TACList {
    TACInstruction *head;
    TACInstruction *tail;
    int count;
} TACList;

/**
 * @brief Crea una nueva lista TAC vacía.
 */
TACList *createTACList(void);

/**
 * @brief Libera la memoria de la lista TAC y todas sus instrucciones.
 */
void freeTACList(TACList *list);

/**
 * @brief Agrega una instrucción al final de la lista TAC.
 * @return La instrucción agregada, o NULL en caso de error.
 */
TACInstruction *appendInstruction(TACList *list, TACOperation op, TACOperand result, TACOperand arg1, TACOperand arg2);

/**
 * @brief Vuelca toda la lista de instrucciones TAC en formato textual usando string_builder.
 */
void printTACList(const TACList *list);

#endif /* TAC_LIST_H */
