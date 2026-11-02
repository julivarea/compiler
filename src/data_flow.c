#include "data_flow.h"

#include <stdlib.h>
#include <string.h>

static InstructionDataFlow analyzeInstruction(TACInstruction *instruction);
static long findInstructionIndex(const BlockDataFlow *block, const TACInstruction *instruction);
static bool operandEquals(const TACOperand *operand, const char *variableName);
static bool isCandidate(const TACOperand *operand);

BlockDataFlow *createBlockDataFlow(TACInstruction *start, TACInstruction *end) {
    if (start == NULL) return NULL;

    size_t count = 0;
    for (TACInstruction *current = start; current != NULL; current = current->next) {
        count++;
        if (current == end) break;
    }

    BlockDataFlow *block = malloc(sizeof(BlockDataFlow));
    if (block == NULL) return NULL;

    block->firstInstruction = start;
    block->lastInstruction = end;
    block->count = count;
    block->entries = malloc(sizeof(InstructionDataFlow) * count);
    if (block->entries == NULL) {
        free(block);
        return NULL;
    }

    size_t index = 0;
    for (TACInstruction *current = start; current != NULL; current = current->next) {
        block->entries[index++] = analyzeInstruction(current);
        if (current == end) break;
    }

    return block;
}

void freeBlockDataFlow(BlockDataFlow *block) {
    if (block == NULL) return;

    free(block->entries);
    free(block);
}

const InstructionDataFlow *blockGetFlow(const BlockDataFlow *block, const TACInstruction *instruction) {
    if (block == NULL || instruction == NULL) return NULL;

    long index = findInstructionIndex(block, instruction);
    if (index < 0) return NULL;

    return &block->entries[index];
}

TACInstruction *blockFindNextUse(const BlockDataFlow *block, const TACInstruction *from, const char *variableName) {
    if (block == NULL || from == NULL || variableName == NULL) return NULL;

    long startIndex = findInstructionIndex(block, from);
    if (startIndex < 0) return NULL;

    for (size_t i = (size_t)(startIndex + 1); i < block->count; i++) {
        if (instructionFlowUses(&block->entries[i], variableName)) {
            return block->entries[i].instruction;
        }
    }

    return NULL;
}

TACInstruction *blockFindNextDef(const BlockDataFlow *block, const TACInstruction *from, const char *variableName) {
    if (block == NULL || from == NULL || variableName == NULL) return NULL;

    long startIndex = findInstructionIndex(block, from);
    if (startIndex < 0) return NULL;

    for (size_t i = (size_t)(startIndex + 1); i < block->count; i++) {
        if (instructionFlowDefines(&block->entries[i], variableName)) {
            return block->entries[i].instruction;
        }
    }

    return NULL;
}

bool blockIsRedefinedBetween(const BlockDataFlow *block, const TACInstruction *from, const TACInstruction *to, const char *variableName) {
    if (block == NULL || from == NULL || to == NULL || variableName == NULL) return false;

    long fromIndex = findInstructionIndex(block, from);
    long toIndex = findInstructionIndex(block, to);
    bool isInvalidRange = (fromIndex < 0 || toIndex < 0 || fromIndex >= toIndex);
    if (isInvalidRange) return false;

    for (size_t i = (size_t)(fromIndex + 1); i < (size_t)toIndex; i++) {
        if (instructionFlowDefines(&block->entries[i], variableName)) {
            return true;
        }
    }

    return false;
}

bool blockIsDeadDefinition(const BlockDataFlow *block, const TACInstruction *instruction) {
    if (block == NULL || instruction == NULL) return false;

    const InstructionDataFlow *flow = blockGetFlow(block, instruction);
    if (flow == NULL || flow->defCount == 0) return false;

    const char *variableName = flow->defs[0]->name;

    TACInstruction *nextUse = blockFindNextUse(block, instruction, variableName);
    TACInstruction *nextDef = blockFindNextDef(block, instruction, variableName);

    bool hasLaterDefinition = (nextDef != NULL);
    if (hasLaterDefinition) {
        bool hasNoLaterUse = (nextUse == NULL);
        if (hasNoLaterUse) {
            return true;
        }

        long defIndex = findInstructionIndex(block, nextDef);
        long useIndex = findInstructionIndex(block, nextUse);
        bool isRedefinedBeforeUse = (defIndex < useIndex);
        if (isRedefinedBeforeUse) {
            return true;
        }
    }

    return false;
}

bool instructionFlowDefines(const InstructionDataFlow *flow, const char *variableName) {
    if (flow == NULL || variableName == NULL) return false;

    for (size_t i = 0; i < flow->defCount; i++) {
        if (operandEquals(flow->defs[i], variableName)) return true;
    }

    return false;
}

bool instructionFlowUses(const InstructionDataFlow *flow, const char *variableName) {
    if (flow == NULL || variableName == NULL) return false;

    for (size_t i = 0; i < flow->useCount; i++) {
        if (operandEquals(flow->uses[i], variableName)) return true;
    }

    return false;
}

static InstructionDataFlow analyzeInstruction(TACInstruction *instruction) {
    InstructionDataFlow flow = {
        .instruction = instruction,
        .defCount = 0,
        .useCount = 0
    };

    switch (instruction->operation) {
        case TAC_ADD:
        case TAC_SUB:
        case TAC_MUL:
        case TAC_DIV:
        case TAC_MOD:
        case TAC_LT:
        case TAC_GT:
        case TAC_EQ:
            if (isCandidate(&instruction->result))        flow.defs[flow.defCount++] = &instruction->result;
            if (isCandidate(&instruction->firstOperand))  flow.uses[flow.useCount++] = &instruction->firstOperand;
            if (isCandidate(&instruction->secondOperand)) flow.uses[flow.useCount++] = &instruction->secondOperand;
            break;

        case TAC_ASSIGN:
        case TAC_NEG:
        case TAC_NOT:
        case TAC_INT_TO_FLOAT:
            if (isCandidate(&instruction->result))        flow.defs[flow.defCount++] = &instruction->result;
            if (isCandidate(&instruction->firstOperand))  flow.uses[flow.useCount++] = &instruction->firstOperand;
            break;

        case TAC_CALL:
            if (isCandidate(&instruction->result))        flow.defs[flow.defCount++] = &instruction->result;
            break;

        case TAC_PARAM:
        case TAC_RETURN:
        case TAC_IF_TRUE:
        case TAC_IF_FALSE:
            if (isCandidate(&instruction->firstOperand))  flow.uses[flow.useCount++] = &instruction->firstOperand;
            break;

        default:
            break;
    }

    return flow;
}

static long findInstructionIndex(const BlockDataFlow *block, const TACInstruction *instruction) {
    for (size_t i = 0; i < block->count; i++) {
        if (block->entries[i].instruction == instruction) {
            return (long)i;
        }
    }

    return -1;
}

static bool operandEquals(const TACOperand *operand, const char *variableName) {
    return isCandidate(operand) && (strcmp(operand->name, variableName) == 0);
}

static bool isCandidate(const TACOperand *operand) {
    return (operand != NULL) && (operand->kind == OPERAND_VARIABLE || operand->kind == OPERAND_TEMP);
}
