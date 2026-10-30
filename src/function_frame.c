#include "function_frame.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

FunctionFrame *createFrame(const char *functionName) {
    FunctionFrame *frame = (FunctionFrame *)malloc(sizeof(FunctionFrame));
    if (frame == NULL) {
        return NULL;
    }

    bool hasFunctionName = (functionName != NULL);
    if (hasFunctionName) {
        strncpy(frame->functionName, functionName, TAC_NAME_LENGTH - 1);
        frame->functionName[TAC_NAME_LENGTH - 1] = '\0';
    } else {
        frame->functionName[0] = '\0';
    }

    frame->returnType = TYPE_VOID;
    frame->slotCapacity = 16;
    frame->slotCount = 0;
    frame->slots = (StackSlot *)malloc(sizeof(StackSlot) * frame->slotCapacity);
    frame->nextOffset = 0;
    frame->frameSize = 0;
    frame->paramCount = 0;
    frame->pendingParamCount = 0;
    frame->maxPendingParams = 0;
    frame->integerParamCount = 0;
    frame->floatParamCount = 0;
    frame->stackParamCount = 0;

    return frame;
}

void freeFrame(FunctionFrame *frame) {
    if (frame == NULL) return;

    bool hasSlots = (frame->slots != NULL);
    if (hasSlots) {
        free(frame->slots);
        frame->slots = NULL;
    }
    free(frame);
}

int addSlot(FunctionFrame *frame, const TACOperand *operand) {
    bool isInvalidRequest = (frame == NULL || operand == NULL || operand->name[0] == '\0');
    if (isInvalidRequest) {
        return 0;
    }

    for (int i = 0; i < frame->slotCount; i++) {
        bool matchesExistingSlot = (strcmp(frame->slots[i].name, operand->name) == 0);
        if (matchesExistingSlot) {
            return frame->slots[i].offset;
        }
    }

    bool needsExpansion = (frame->slotCount >= frame->slotCapacity);
    if (needsExpansion) {
        int newCapacity = frame->slotCapacity * 2;
        StackSlot *newSlots = (StackSlot *)realloc(frame->slots, sizeof(StackSlot) * newCapacity);
        if (newSlots == NULL) {
            return 0;
        }
        frame->slots = newSlots;
        frame->slotCapacity = newCapacity;
    }

    frame->nextOffset += 4;

    StackSlot *slot = &frame->slots[frame->slotCount];
    strncpy(slot->name, operand->name, TAC_NAME_LENGTH - 1);
    slot->name[TAC_NAME_LENGTH - 1] = '\0';
    slot->offset = -frame->nextOffset;

    frame->slotCount++;

    return slot->offset;
}

int addAnonymousSlot(FunctionFrame *frame) {
    if (frame == NULL) return 0;

    frame->nextOffset += 4;
    return -frame->nextOffset;
}

int getSlotOffset(const FunctionFrame *frame, const char *name) {
    bool isInvalidQuery = (frame == NULL || name == NULL);
    if (isInvalidQuery) return 0;

    for (int i = 0; i < frame->slotCount; i++) {
        bool matchesSlot = (strcmp(frame->slots[i].name, name) == 0);
        if (matchesSlot) {
            return frame->slots[i].offset;
        }
    }
    return 0;
}
