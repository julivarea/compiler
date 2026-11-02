#include "optimizer.h"

#include <stdio.h>
#include <string.h>

#include "tac_instruction.h"
#include "tac_operand.h"

#define MAX_KNOWN_CONSTANTS 256
#define MAX_OPTIMIZATION_ROUNDS 16

typedef struct {
    char name[TAC_NAME_LENGTH];
    OperandKind destinationKind;
    TACOperand value;
} KnownConstant;

typedef struct {
    KnownConstant entries[MAX_KNOWN_CONSTANTS];
    int count;
} KnownConstantTable;

static int propagateConstants(TACList *tacList);
static int foldConstants(TACList *tacList);
static int simplifyConstantBranches(TACList *tacList);
static int removeUnreachableCode(TACList *tacList);
static int removeSelfAssignments(TACList *tacList);
static int removeRedundantJumps(TACList *tacList);

static bool isConstant(const TACOperand *operand);
static float asFloat(const TACOperand *operand);
static int asInt(const TACOperand *operand);
static bool evaluateBinary(TACOperation operation, const TACOperand *firstOperand,
                           const TACOperand *secondOperand, DataType resultType, TACOperand *folded);
static bool evaluateUnary(TACOperation operation, const TACOperand *operand,
                          DataType resultType, TACOperand *folded);

static const TACOperand *lookupKnownConstant(const KnownConstantTable *table, const TACOperand *operand);
static void forgetConstant(KnownConstantTable *table, const TACOperand *operand);
static void rememberConstant(KnownConstantTable *table, const TACOperand *destination, const TACOperand *value);
static void forgetVariables(KnownConstantTable *table);
static bool replaceWithKnownConstant(const KnownConstantTable *table, TACOperand *operand);

static TACInstruction *unlinkInstruction(TACList *tacList, TACInstruction *previous, TACInstruction *current);
static bool endsControlFlow(TACOperation operation);
static bool isStructural(TACOperation operation);
static int removeUnreferencedLabels(TACList *tacList);

int optimizeIntermediateCode(TACList *tacList, OptimizationSet enabled) {
    if (tacList == NULL || enabled == OPTIMIZATION_NONE) {
        return 0;
    }

    int totalChanges = 0;

    for (int round = 0; round < MAX_OPTIMIZATION_ROUNDS; round++) {
        int changes = 0;

        if (isOptimizationEnabled(enabled, OPTIMIZATION_CONST_PROP)) {
            changes += propagateConstants(tacList);
        }
        if (isOptimizationEnabled(enabled, OPTIMIZATION_CONST_FOLD)) {
            changes += foldConstants(tacList);
        }
        if (isOptimizationEnabled(enabled, OPTIMIZATION_CONST_BRANCH)) {
            changes += simplifyConstantBranches(tacList);
        }
        if (isOptimizationEnabled(enabled, OPTIMIZATION_DEAD_CODE)) {
            changes += removeUnreachableCode(tacList);
        }
        if (isOptimizationEnabled(enabled, OPTIMIZATION_SELF_ASSIGN)) {
            changes += removeSelfAssignments(tacList);
        }
        if (isOptimizationEnabled(enabled, OPTIMIZATION_PEEPHOLE)) {
            changes += removeRedundantJumps(tacList);
        }

        bool hasConverged = (changes == 0);
        if (hasConverged) {
            break;
        }

        totalChanges += changes;
    }

    return totalChanges;
}

static int propagateConstants(TACList *tacList) {
    KnownConstantTable table = { .count = 0 };
    int changes = 0;

    for (TACInstruction *instruction = tacList->head; instruction != NULL; instruction = instruction->next) {
        switch (instruction->operation) {
            case TAC_LABEL:
            case TAC_GOTO:
            case TAC_IF_FALSE:
            case TAC_IF_TRUE:
            case TAC_BEGIN_FUNC:
            case TAC_END_FUNC: {
                bool isConditionalJump = (instruction->operation == TAC_IF_FALSE ||
                                          instruction->operation == TAC_IF_TRUE);
                if (isConditionalJump) {
                    if (replaceWithKnownConstant(&table, &instruction->firstOperand)) {
                        changes++;
                    }
                }
                table.count = 0;
                continue;
            }

            case TAC_CALL:
                forgetVariables(&table);
                forgetConstant(&table, &instruction->result);
                continue;

            case TAC_GLOBAL_DECL:
            case TAC_LOCAL_DECL:
            case TAC_PARAM_DECL:
                continue;

            default:
                break;
        }

        if (replaceWithKnownConstant(&table, &instruction->firstOperand)) {
            changes++;
        }
        if (replaceWithKnownConstant(&table, &instruction->secondOperand)) {
            changes++;
        }

        bool writesResult = (instruction->result.kind == OPERAND_TEMP ||
                             instruction->result.kind == OPERAND_VARIABLE);
        if (!writesResult) {
            continue;
        }

        bool assignsConstant = (instruction->operation == TAC_ASSIGN &&
                                isConstant(&instruction->firstOperand));
        if (assignsConstant) {
            rememberConstant(&table, &instruction->result, &instruction->firstOperand);
        } else {
            forgetConstant(&table, &instruction->result);
        }
    }

    return changes;
}

static int foldConstants(TACList *tacList) {
    int changes = 0;

    for (TACInstruction *instruction = tacList->head; instruction != NULL; instruction = instruction->next) {
        bool hasConstantFirst = isConstant(&instruction->firstOperand);
        bool hasConstantSecond = isConstant(&instruction->secondOperand);
        bool isBinaryCandidate = (hasConstantFirst && hasConstantSecond);
        bool isUnaryCandidate = (hasConstantFirst && instruction->secondOperand.kind == OPERAND_NONE);
        TACOperand folded;
        bool wasFolded = false;

        if (isBinaryCandidate) {
            wasFolded = evaluateBinary(instruction->operation, &instruction->firstOperand,
                                       &instruction->secondOperand, instruction->result.type, &folded);
        } else if (isUnaryCandidate) {
            wasFolded = evaluateUnary(instruction->operation, &instruction->firstOperand,
                                      instruction->result.type, &folded);
        }

        if (wasFolded) {
            instruction->operation = TAC_ASSIGN;
            instruction->firstOperand = folded;
            instruction->secondOperand = tacNone();
            changes++;
        }
    }

    return changes;
}

static int simplifyConstantBranches(TACList *tacList) {
    int changes = 0;
    TACInstruction *previous = NULL;
    TACInstruction *current = tacList->head;

    while (current != NULL) {
        bool isConditionalJump = (current->operation == TAC_IF_FALSE ||
                                  current->operation == TAC_IF_TRUE);
        bool hasConstantCondition = (isConditionalJump && isConstant(&current->firstOperand));

        if (!hasConstantCondition) {
            previous = current;
            current = current->next;
            continue;
        }

        bool conditionHolds = (asInt(&current->firstOperand) != 0);
        bool jumpIsTaken = (current->operation == TAC_IF_TRUE) ? conditionHolds : !conditionHolds;

        if (jumpIsTaken) {
            current->operation = TAC_GOTO;
            current->firstOperand = current->secondOperand;
            current->secondOperand = tacNone();

            previous = current;
            current = current->next;
        } else {
            current = unlinkInstruction(tacList, previous, current);
        }

        changes++;
    }

    return changes;
}

static int removeUnreachableCode(TACList *tacList) {
    int changes = removeUnreferencedLabels(tacList);
    bool isReachable = true;

    TACInstruction *previous = NULL;
    TACInstruction *current = tacList->head;

    while (current != NULL) {
        TACInstruction *next = current->next;
        bool mustKeep = (isReachable || isStructural(current->operation));

        if (mustKeep) {
            bool resumesControlFlow = (current->operation == TAC_LABEL ||
                                       current->operation == TAC_BEGIN_FUNC);
            if (resumesControlFlow) {
                isReachable = true;
            } else if (endsControlFlow(current->operation)) {
                isReachable = false;
            }

            previous = current;
        } else {
            unlinkInstruction(tacList, previous, current);
            changes++;
        }

        current = next;
    }

    return changes;
}

static int removeSelfAssignments(TACList *tacList) {
    int changes = 0;
    TACInstruction *previous = NULL;
    TACInstruction *current = tacList->head;

    while (current != NULL) {
        bool isAssignment = (current->operation == TAC_ASSIGN);
        bool isVariableOrTemp = (current->result.kind == OPERAND_VARIABLE ||
                                 current->result.kind == OPERAND_TEMP);
        bool hasSameKind = (current->firstOperand.kind == current->result.kind);
        bool hasSameName = (strcmp(current->result.name, current->firstOperand.name) == 0);
        bool isSelfAssignment = (isAssignment && isVariableOrTemp && hasSameKind && hasSameName);

        if (isSelfAssignment) {
            current = unlinkInstruction(tacList, previous, current);
            changes++;
        } else {
            previous = current;
            current = current->next;
        }
    }

    return changes;
}

static int removeRedundantJumps(TACList *tacList) {
    int changes = 0;
    TACInstruction *previous = NULL;
    TACInstruction *current = tacList->head;

    while (current != NULL) {
        bool isJump = (current->operation == TAC_GOTO);
        bool jumpsToNextInstruction = false;

        if (isJump && current->next != NULL) {
            const TACInstruction *next = current->next;
            bool targetMatchesNextLabel = (next->operation == TAC_LABEL &&
                                           strcmp(next->result.name, current->firstOperand.name) == 0);
            jumpsToNextInstruction = targetMatchesNextLabel;
        }

        if (jumpsToNextInstruction) {
            current = unlinkInstruction(tacList, previous, current);
            changes++;
        } else {
            previous = current;
            current = current->next;
        }
    }

    return changes;
}

static const TACOperand *lookupKnownConstant(const KnownConstantTable *table, const TACOperand *operand) {
    bool canHoldConstant = (operand->kind == OPERAND_TEMP || operand->kind == OPERAND_VARIABLE);
    if (!canHoldConstant) {
        return NULL;
    }

    for (int i = 0; i < table->count; i++) {
        if (strcmp(table->entries[i].name, operand->name) == 0) {
            return &table->entries[i].value;
        }
    }
    return NULL;
}

static void forgetConstant(KnownConstantTable *table, const TACOperand *operand) {
    for (int i = 0; i < table->count; i++) {
        if (strcmp(table->entries[i].name, operand->name) == 0) {
            table->entries[i] = table->entries[table->count - 1];
            table->count--;
            return;
        }
    }
}

static void rememberConstant(KnownConstantTable *table, const TACOperand *destination, const TACOperand *value) {
    forgetConstant(table, destination);

    bool isTableFull = (table->count >= MAX_KNOWN_CONSTANTS);
    if (isTableFull) {
        return;
    }

    KnownConstant *entry = &table->entries[table->count];
    snprintf(entry->name, sizeof(entry->name), "%s", destination->name);
    entry->destinationKind = destination->kind;
    entry->value = *value;
    table->count++;
}

static void forgetVariables(KnownConstantTable *table) {
    int keptEntriesCount = 0;
    for (int i = 0; i < table->count; i++) {
        bool isTemporary = (table->entries[i].destinationKind == OPERAND_TEMP);
        if (isTemporary) {
            table->entries[keptEntriesCount++] = table->entries[i];
        }
    }
    table->count = keptEntriesCount;
}

static bool replaceWithKnownConstant(const KnownConstantTable *table, TACOperand *operand) {
    const TACOperand *knownConstant = lookupKnownConstant(table, operand);
    if (knownConstant == NULL) {
        return false;
    }

    *operand = *knownConstant;
    return true;
}

static bool isConstant(const TACOperand *operand) {
    return operand->kind == OPERAND_CONSTANT;
}

static float asFloat(const TACOperand *operand) {
    return operand->value.floatValue;
}

static int asInt(const TACOperand *operand) {
    return operand->value.intValue;
}

static bool evaluateBinary(TACOperation operation, const TACOperand *firstOperand,
                           const TACOperand *secondOperand, DataType resultType, TACOperand *folded) {
    bool isFloatOperation = (firstOperand->type == TYPE_FLOAT);

    switch (operation) {
        case TAC_ADD:
        case TAC_SUB:
        case TAC_MUL:
            if (resultType == TYPE_FLOAT) {
                float leftValue = asFloat(firstOperand);
                float rightValue = asFloat(secondOperand);
                float resultValue = (operation == TAC_ADD) ? leftValue + rightValue
                                  : (operation == TAC_SUB) ? leftValue - rightValue
                                                           : leftValue * rightValue;
                *folded = tacConstantFloat(resultValue);
            } else {
                int leftValue = asInt(firstOperand);
                int rightValue = asInt(secondOperand);
                int resultValue = (operation == TAC_ADD) ? leftValue + rightValue
                                : (operation == TAC_SUB) ? leftValue - rightValue
                                                         : leftValue * rightValue;
                *folded = tacConstantInt(resultValue);
            }
            return true;

        case TAC_DIV:
            if (resultType == TYPE_FLOAT) {
                float rightValue = asFloat(secondOperand);
                bool isZeroDivisor = (rightValue == 0.0f);
                if (isZeroDivisor) {
                    return false;
                }
                *folded = tacConstantFloat(asFloat(firstOperand) / rightValue);
            } else {
                int rightValue = asInt(secondOperand);
                bool isZeroDivisor = (rightValue == 0);
                if (isZeroDivisor) {
                    return false;
                }
                *folded = tacConstantInt(asInt(firstOperand) / rightValue);
            }
            return true;

        case TAC_MOD: {
            int rightValue = asInt(secondOperand);
            bool isZeroDivisor = (rightValue == 0);
            if (isZeroDivisor) {
                return false;
            }
            *folded = tacConstantInt(asInt(firstOperand) % rightValue);
            return true;
        }

        case TAC_LT:
            *folded = tacConstantBool(isFloatOperation ? (asFloat(firstOperand) < asFloat(secondOperand))
                                                       : (asInt(firstOperand) < asInt(secondOperand)));
            return true;

        case TAC_GT:
            *folded = tacConstantBool(isFloatOperation ? (asFloat(firstOperand) > asFloat(secondOperand))
                                                       : (asInt(firstOperand) > asInt(secondOperand)));
            return true;

        case TAC_EQ:
            *folded = tacConstantBool(isFloatOperation ? (asFloat(firstOperand) == asFloat(secondOperand))
                                                       : (asInt(firstOperand) == asInt(secondOperand)));
            return true;

        default:
            return false;
    }
}

static bool evaluateUnary(TACOperation operation, const TACOperand *operand,
                          DataType resultType, TACOperand *folded) {
    switch (operation) {
        case TAC_UMINUS:
            if (resultType == TYPE_FLOAT) {
                *folded = tacConstantFloat(-asFloat(operand));
            } else {
                *folded = tacConstantInt(-asInt(operand));
            }
            return true;

        case TAC_NOT:
            *folded = tacConstantBool(!asInt(operand));
            return true;

        case TAC_INT_TO_FLOAT:
            *folded = tacConstantFloat((float)asInt(operand));
            return true;

        default:
            return false;
    }
}

static TACInstruction *unlinkInstruction(TACList *tacList, TACInstruction *previous, TACInstruction *current) {
    TACInstruction *next = current->next;

    if (previous == NULL) {
        tacList->head = next;
    } else {
        previous->next = next;
    }
    if (tacList->tail == current) {
        tacList->tail = previous;
    }

    freeTACInstruction(current);
    tacList->count--;

    return next;
}

static bool endsControlFlow(TACOperation operation) {
    return operation == TAC_RETURN || operation == TAC_GOTO;
}

static bool isStructural(TACOperation operation) {
    switch (operation) {
        case TAC_LABEL:
        case TAC_BEGIN_FUNC:
        case TAC_END_FUNC:
        case TAC_GLOBAL_DECL:
        case TAC_LOCAL_DECL:
        case TAC_PARAM_DECL:
            return true;
        default:
            return false;
    }
}

static int removeUnreferencedLabels(TACList *tacList) {
    char referencedLabels[MAX_KNOWN_CONSTANTS][TAC_NAME_LENGTH];
    int referencedCount = 0;

    for (const TACInstruction *instruction = tacList->head; instruction != NULL; instruction = instruction->next) {
        const TACOperand *target = NULL;

        if (instruction->operation == TAC_GOTO) {
            target = &instruction->firstOperand;
        } else if (instruction->operation == TAC_IF_FALSE || instruction->operation == TAC_IF_TRUE) {
            target = &instruction->secondOperand;
        }

        bool hasRoom = (referencedCount < MAX_KNOWN_CONSTANTS);
        if (target != NULL && hasRoom) {
            snprintf(referencedLabels[referencedCount], TAC_NAME_LENGTH, "%s", target->name);
            referencedCount++;
        }
    }

    int changes = 0;
    TACInstruction *previous = NULL;
    TACInstruction *current = tacList->head;

    while (current != NULL) {
        bool isLabel = (current->operation == TAC_LABEL);
        bool isReferenced = false;

        for (int i = 0; isLabel && i < referencedCount; i++) {
            if (strcmp(referencedLabels[i], current->result.name) == 0) {
                isReferenced = true;
            }
        }

        bool shouldRemoveLabel = (isLabel && !isReferenced);
        if (shouldRemoveLabel) {
            current = unlinkInstruction(tacList, previous, current);
            changes++;
        } else {
            previous = current;
            current = current->next;
        }
    }

    return changes;
}
