#include "function_frame.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

FunctionFrame *createFrame(const char *functionName) {
    FunctionFrame *frame = (FunctionFrame *)malloc(sizeof(FunctionFrame));
    if (frame == NULL) {
        return NULL;
    }

    if (functionName != NULL) {
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
    frame->pendingParamCapacity = INITIAL_PENDING_PARAMS_CAPACITY;
    frame->pendingParamSlots = (int *)malloc(sizeof(int) * frame->pendingParamCapacity);
    frame->pendingParamCount = 0;
    frame->maxPendingParams = 0;
    frame->integerParamCount = 0;
    frame->stackParamCount = 0;

    if (frame->slots == NULL || frame->pendingParamSlots == NULL) {
        freeFrame(frame);
        return NULL;
    }

    return frame;
}

void freeFrame(FunctionFrame *frame) {
    if (frame == NULL) return;

    free(frame->slots);
    free(frame->pendingParamSlots);
    free(frame);
}

int addSlot(FunctionFrame *frame, const TACOperand *operand) {
    bool isInvalidRequest = (frame == NULL || operand == NULL || operand->name[0] == '\0');
    if (isInvalidRequest) {
        return 0;
    }

    int existingOffset = 0;
    if (getSlotOffset(frame, operand->name, &existingOffset)) {
        return existingOffset;
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

bool getSlotOffset(const FunctionFrame *frame, const char *name, int *outOffset) {
    if (frame == NULL || name == NULL) return false;

    for (int i = 0; i < frame->slotCount; i++) {
        bool matchesSlot = (strcmp(frame->slots[i].name, name) == 0);
        if (matchesSlot) {
            if (outOffset != NULL) {
                *outOffset = frame->slots[i].offset;
            }
            return true;
        }
    }

    return false;
}

bool ensurePendingParamCapacity(FunctionFrame *frame, int neededCapacity) {
    if (frame == NULL || neededCapacity <= frame->pendingParamCapacity) {
        return true;
    }

    int newCapacity = (frame->pendingParamCapacity > 0) ? frame->pendingParamCapacity : INITIAL_PENDING_PARAMS_CAPACITY;
    while (newCapacity < neededCapacity) {
        newCapacity *= 2;
    }

    int *newSlots = (int *)realloc(frame->pendingParamSlots, sizeof(int) * newCapacity);
    if (newSlots == NULL) {
        return false;
    }
    frame->pendingParamSlots = newSlots;
    frame->pendingParamCapacity = newCapacity;
    return true;
}
