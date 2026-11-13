#include <stdio.h>
#include <stdlib.h>
#include "tac_list.h"

TACList *createTACList(void) {
    TACList *list = malloc(sizeof(TACList));
    if (list == NULL) return NULL;

    list->head = NULL;
    list->tail = NULL;
    list->count = 0;

    return list;
}

void freeTACList(TACList *list) {
    if (list == NULL) return;

    TACInstruction *current = list->head;
    while (current != NULL) {
        TACInstruction *toFree = current;
        current = current->next;
        freeTACInstruction(toFree);
    }

    free(list);
}

void formatTACList(const TACList *list) {
    if (list == NULL) return;

    for (const TACInstruction *current = list->head; current != NULL; current = current->next) {
        printTACInstruction(current);
    }
}

TACInstruction *appendTACInstruction(TACList *list, TACInstruction *instruction) {
    if (list == NULL || instruction == NULL) return NULL;

    instruction->next = NULL;
    if (list->tail == NULL) {
        list->head = instruction;
    } else {
        list->tail->next = instruction;
    }
    list->tail = instruction;
    list->count++;

    return instruction;
}

TACInstruction *appendInstruction(TACList *list, TACOperation operation, TACOperand result, TACOperand firstOperand, TACOperand secondOperand) {
    if (list == NULL) return NULL;

    TACInstruction *instruction = createTACInstruction(operation, result, firstOperand, secondOperand);
    if (instruction == NULL) return NULL;

    return appendTACInstruction(list, instruction);
}
