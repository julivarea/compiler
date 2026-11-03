#include "optimizer.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "live_variable_analysis.h"
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

static bool isOptimizerDebugEnabled = false;

static int propagateConstants(TACList *tacList);
static int foldConstants(TACList *tacList);
static int simplifyConstantBranches(TACList *tacList);
static int removeUnreachableCode(TACList *tacList);
static int removeDeadStores(TACList *tacList);
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
static void forgetGlobalVariables(KnownConstantTable *table, const TACList *tacList);
static bool replaceWithKnownConstant(const KnownConstantTable *table, TACOperand *operand);

static TACInstruction *unlinkInstruction(TACList *tacList, TACInstruction *previous, TACInstruction *current);
static bool endsControlFlow(TACOperation operation);
static bool isStructural(TACOperation operation);
static int removeUnreferencedLabels(TACList *tacList);
static bool isDeadStoreCandidate(const TACList *tacList, const TACInstruction *instruction);
static bool isGlobalVariable(const TACList *tacList, const char *variableName);
static void printSingleInstruction(FILE *output, const TACInstruction *instruction);
static void printTACWithDeletedInRed(const TACList *tacList, TACInstruction **deletedList, size_t deletedCount, const char *reason);

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
            changes += removeDeadStores(tacList);
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

void setOptimizerDebug(bool enabled) {
    isOptimizerDebugEnabled = enabled;
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
                forgetGlobalVariables(&table, tacList);
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

    TACInstruction **unreachableList = malloc(sizeof(TACInstruction *) * (size_t)tacList->count);
    if (unreachableList == NULL) {
        return changes;
    }
    size_t unreachableCount = 0;

    for (TACInstruction *current = tacList->head; current != NULL; current = current->next) {
        bool mustKeep = (isReachable || isStructural(current->operation));
        if (mustKeep) {
            bool resumesControlFlow = (current->operation == TAC_LABEL ||
                                       current->operation == TAC_BEGIN_FUNC);
            if (resumesControlFlow) {
                isReachable = true;
            } else if (endsControlFlow(current->operation)) {
                isReachable = false;
            }
        } else {
            unreachableList[unreachableCount++] = current;
        }
    }

    if (unreachableCount == 0) {
        free(unreachableList);
        return changes;
    }

    if (isOptimizerDebugEnabled) {
        printTACWithDeletedInRed(tacList, unreachableList, unreachableCount, "Codigo Inalcanzable (tras ret / goto)");
    }

    size_t unreachIndex = 0;
    TACInstruction *previous = NULL;
    TACInstruction *current = tacList->head;

    while (current != NULL && unreachIndex < unreachableCount) {
        bool isTarget = (current == unreachableList[unreachIndex]);
        if (isTarget) {
            current = unlinkInstruction(tacList, previous, current);
            unreachIndex++;
            changes++;
        } else {
            previous = current;
            current = current->next;
        }
    }

    free(unreachableList);
    return changes;
}

static int removeDeadStores(TACList *tacList) {
    if (tacList == NULL || tacList->head == NULL) {
        return 0;
    }

    // Resuelve el punto fijo de vivacidad (LIVE_IN / LIVE_OUT) sobre el CFG de TAC.
    LiveVariableAnalysis *analysis = createLiveVariableAnalysis(tacList);
    if (analysis == NULL) {
        return 0;
    }

    TACInstruction **deadInstructions = malloc(sizeof(TACInstruction *) * (size_t)tacList->count);
    if (deadInstructions == NULL) {
        freeLiveVariableAnalysis(analysis);
        return 0;
    }

    // Marca como muerta toda asignación cuyo destino no esté en LIVE_OUT ni se lea después en el bloque.
    size_t deadCount = 0;
    for (TACInstruction *current = tacList->head; current != NULL; current = current->next) {
        bool canBeDeadStore = isDeadStoreCandidate(tacList, current);
        bool isDead = (canBeDeadStore && isDeadDefinition(analysis, current));
        if (isDead) {
            deadInstructions[deadCount++] = current;
        }
    }

    freeLiveVariableAnalysis(analysis);

    bool hasNoDeadStores = (deadCount == 0);
    if (hasNoDeadStores) {
        free(deadInstructions);
        return 0;
    }

    if (isOptimizerDebugEnabled) {
        printTACWithDeletedInRed(tacList, deadInstructions, deadCount, "Variables y Asignaciones Muertas (Live Variables)");
    }

    // Remueve de la lista TAC las instrucciones identificadas como código muerto.
    int changes = 0;
    size_t deadIndex = 0;
    TACInstruction *previous = NULL;
    TACInstruction *current = tacList->head;

    while (current != NULL && deadIndex < deadCount) {
        bool isTarget = (current == deadInstructions[deadIndex]);
        if (isTarget) {
            current = unlinkInstruction(tacList, previous, current);
            deadIndex++;
            changes++;
        } else {
            previous = current;
            current = current->next;
        }
    }

    free(deadInstructions);
    return changes;
}

static int removeSelfAssignments(TACList *tacList) {
    TACInstruction **selfList = malloc(sizeof(TACInstruction *) * (size_t)tacList->count);
    if (selfList == NULL) {
        return 0;
    }
    size_t selfCount = 0;

    for (TACInstruction *curr = tacList->head; curr != NULL; curr = curr->next) {
        bool isAssignment = (curr->operation == TAC_ASSIGN);
        bool isVariableOrTemp = (curr->result.kind == OPERAND_VARIABLE ||
                                 curr->result.kind == OPERAND_TEMP);
        bool hasSameKind = (curr->firstOperand.kind == curr->result.kind);
        bool hasSameName = (strcmp(curr->result.name, curr->firstOperand.name) == 0);
        bool isSelfAssignment = (isAssignment && isVariableOrTemp && hasSameKind && hasSameName);
        if (isSelfAssignment) {
            selfList[selfCount++] = curr;
        }
    }

    if (selfCount == 0) {
        free(selfList);
        return 0;
    }

    if (isOptimizerDebugEnabled) {
        printTACWithDeletedInRed(tacList, selfList, selfCount, "Autoasignaciones Redundantes (x = x)");
    }

    int changes = 0;
    size_t selfIndex = 0;
    TACInstruction *previous = NULL;
    TACInstruction *current = tacList->head;

    while (current != NULL && selfIndex < selfCount) {
        bool isTarget = (current == selfList[selfIndex]);
        if (isTarget) {
            current = unlinkInstruction(tacList, previous, current);
            selfIndex++;
            changes++;
        } else {
            previous = current;
            current = current->next;
        }
    }

    free(selfList);
    return changes;
}

static int removeRedundantJumps(TACList *tacList) {
    TACInstruction **jumpList = malloc(sizeof(TACInstruction *) * (size_t)tacList->count);
    if (jumpList == NULL) {
        return 0;
    }
    size_t jumpCount = 0;

    for (TACInstruction *curr = tacList->head; curr != NULL; curr = curr->next) {
        bool isJump = (curr->operation == TAC_GOTO);
        bool jumpsToNextInstruction = false;

        if (isJump && curr->next != NULL) {
            const TACInstruction *next = curr->next;
            bool targetMatchesNextLabel = (next->operation == TAC_LABEL &&
                                           strcmp(next->result.name, curr->firstOperand.name) == 0);
            jumpsToNextInstruction = targetMatchesNextLabel;
        }

        if (jumpsToNextInstruction) {
            jumpList[jumpCount++] = curr;
        }
    }

    if (jumpCount == 0) {
        free(jumpList);
        return 0;
    }

    if (isOptimizerDebugEnabled) {
        printTACWithDeletedInRed(tacList, jumpList, jumpCount, "Saltos Redundantes a la Siguiente Etiqueta");
    }

    int changes = 0;
    size_t jumpIndex = 0;
    TACInstruction *previous = NULL;
    TACInstruction *current = tacList->head;

    while (current != NULL && jumpIndex < jumpCount) {
        bool isTarget = (current == jumpList[jumpIndex]);
        if (isTarget) {
            current = unlinkInstruction(tacList, previous, current);
            jumpIndex++;
            changes++;
        } else {
            previous = current;
            current = current->next;
        }
    }

    free(jumpList);
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

static void forgetGlobalVariables(KnownConstantTable *table, const TACList *tacList) {
    int keptEntriesCount = 0;
    for (int i = 0; i < table->count; i++) {
        bool isGlobal = (table->entries[i].destinationKind == OPERAND_VARIABLE &&
                         isGlobalVariable(tacList, table->entries[i].name));
        if (!isGlobal) {
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
                int leftValue = asInt(firstOperand);
                int rightValue = asInt(secondOperand);
                bool isZeroDivisor = (rightValue == 0);
                bool isSignedOverflow = (leftValue == INT_MIN && rightValue == -1);
                if (isZeroDivisor || isSignedOverflow) {
                    return false;
                }
                *folded = tacConstantInt(leftValue / rightValue);
            }
            return true;

        case TAC_MOD: {
            int leftValue = asInt(firstOperand);
            int rightValue = asInt(secondOperand);
            bool isZeroDivisor = (rightValue == 0);
            bool isSignedOverflow = (leftValue == INT_MIN && rightValue == -1);
            if (isZeroDivisor || isSignedOverflow) {
                return false;
            }
            *folded = tacConstantInt(leftValue % rightValue);
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

typedef struct {
    const char **names;
    size_t count;
    size_t capacity;
} LabelReferenceSet;

static bool isLabelInSet(const LabelReferenceSet *set, const char *name) {
    if (set == NULL || name == NULL) {
        return false;
    }

    for (size_t i = 0; i < set->count; i++) {
        if (strcmp(set->names[i], name) == 0) {
            return true;
        }
    }

    return false;
}

static bool addReferencedLabel(LabelReferenceSet *set, const char *name) {
    if (set == NULL || name == NULL) {
        return false;
    }

    if (isLabelInSet(set, name)) {
        return true;
    }

    if (set->count >= set->capacity) {
        size_t newCapacity = (set->capacity == 0) ? 64 : set->capacity * 2;
        const char **reallocatedNames = (const char **)realloc(set->names, sizeof(const char *) * newCapacity);
        if (reallocatedNames == NULL) {
            return false;
        }
        set->names = reallocatedNames;
        set->capacity = newCapacity;
    }

    set->names[set->count++] = name;
    return true;
}

static int removeUnreferencedLabels(TACList *tacList) {
    LabelReferenceSet referencedSet = { .names = NULL, .count = 0, .capacity = 0 };

    for (const TACInstruction *instruction = tacList->head; instruction != NULL; instruction = instruction->next) {
        const TACOperand *target = NULL;

        if (instruction->operation == TAC_GOTO) {
            target = &instruction->firstOperand;
        } else if (instruction->operation == TAC_IF_FALSE || instruction->operation == TAC_IF_TRUE) {
            target = &instruction->secondOperand;
        }

        if (target != NULL) {
            bool wasAdded = addReferencedLabel(&referencedSet, target->name);
            if (!wasAdded) {
                free(referencedSet.names);
                return 0;
            }
        }
    }

    TACInstruction **deadLabelList = malloc(sizeof(TACInstruction *) * (size_t)tacList->count);
    if (deadLabelList == NULL) {
        free(referencedSet.names);
        return 0;
    }
    size_t deadLabelCount = 0;

    for (TACInstruction *current = tacList->head; current != NULL; current = current->next) {
        bool isLabel = (current->operation == TAC_LABEL);
        bool isReferenced = (isLabel && isLabelInSet(&referencedSet, current->result.name));
        if (isLabel && !isReferenced) {
            deadLabelList[deadLabelCount++] = current;
        }
    }

    free(referencedSet.names);

    if (deadLabelCount == 0) {
        free(deadLabelList);
        return 0;
    }

    if (isOptimizerDebugEnabled) {
        printTACWithDeletedInRed(tacList, deadLabelList, deadLabelCount, "Etiquetas sin Referencias");
    }

    int changes = 0;
    size_t labelIndex = 0;
    TACInstruction *previous = NULL;
    TACInstruction *current = tacList->head;

    while (current != NULL && labelIndex < deadLabelCount) {
        bool isTarget = (current == deadLabelList[labelIndex]);
        if (isTarget) {
            current = unlinkInstruction(tacList, previous, current);
            labelIndex++;
            changes++;
        } else {
            previous = current;
            current = current->next;
        }
    }

    free(deadLabelList);
    return changes;
}

static bool isGlobalVariable(const TACList *tacList, const char *variableName) {
    for (const TACInstruction *current = tacList->head; current != NULL; current = current->next) {
        if (current->operation == TAC_GLOBAL_DECL) {
            bool matchesName = (strcmp(current->firstOperand.name, variableName) == 0);
            if (matchesName) {
                return true;
            }
        } else if (current->operation == TAC_BEGIN_FUNC) {
            break;
        }
    }
    return false;
}

static bool isDeadStoreCandidate(const TACList *tacList, const TACInstruction *instruction) {
    bool hasCandidateResult = (instruction->result.kind == OPERAND_VARIABLE ||
                               instruction->result.kind == OPERAND_TEMP);
    if (!hasCandidateResult) {
        return false;
    }

    bool hasSideEffects = (instruction->operation == TAC_CALL);
    if (hasSideEffects) {
        return false;
    }

    bool isDeclaration = (instruction->operation == TAC_GLOBAL_DECL ||
                          instruction->operation == TAC_LOCAL_DECL ||
                          instruction->operation == TAC_PARAM_DECL);
    if (isDeclaration) {
        return false;
    }

    bool isGlobal = (instruction->result.kind == OPERAND_VARIABLE &&
                     isGlobalVariable(tacList, instruction->result.name));
    if (isGlobal) {
        return false;
    }

    return true;
}

static void printSingleInstruction(FILE *output, const TACInstruction *instruction) {
    if (instruction == NULL) return;

    const char *result = getOperandName(&instruction->result);
    const char *first = getOperandName(&instruction->firstOperand);
    const char *second = getOperandName(&instruction->secondOperand);

    switch (instruction->operation) {
        case TAC_BEGIN_FUNC:
            fprintf(output, "begin_func %s, %s\n", result, first);
            break;
        case TAC_END_FUNC:
            fprintf(output, "end_func %s\n", result);
            break;
        case TAC_PARAM_DECL:
            fprintf(output, "param_decl %s\n", first);
            break;
        case TAC_GLOBAL_DECL:
        case TAC_LOCAL_DECL:
            fprintf(output, "%s %s, %s\n",
                    instruction->operation == TAC_GLOBAL_DECL ? "global" : "local",
                    first, getDataTypeName(instruction->firstOperand.type));
            break;
        case TAC_LABEL:
            fprintf(output, "%s:\n", result);
            break;
        case TAC_GOTO:
            fprintf(output, "goto %s\n", first);
            break;
        case TAC_IF_FALSE:
            fprintf(output, "if %s false goto %s\n", first, second);
            break;
        case TAC_IF_TRUE:
            fprintf(output, "if %s true goto %s\n", first, second);
            break;
        case TAC_PARAM:
            fprintf(output, "param %s\n", first);
            break;
        case TAC_CALL:
            if (instruction->result.kind != OPERAND_NONE) {
                fprintf(output, "%s = call %s, %s\n", result, first, second);
            } else {
                fprintf(output, "call %s, %s\n", first, second);
            }
            break;
        case TAC_RETURN:
            if (instruction->firstOperand.kind != OPERAND_NONE) {
                fprintf(output, "ret %s\n", first);
            } else {
                fprintf(output, "ret\n");
            }
            break;
        case TAC_ASSIGN:
            fprintf(output, "%s = %s\n", result, first);
            break;
        case TAC_ADD: fprintf(output, "%s = %s + %s\n", result, first, second); break;
        case TAC_SUB: fprintf(output, "%s = %s - %s\n", result, first, second); break;
        case TAC_MUL: fprintf(output, "%s = %s * %s\n", result, first, second); break;
        case TAC_DIV: fprintf(output, "%s = %s / %s\n", result, first, second); break;
        case TAC_MOD: fprintf(output, "%s = %s %% %s\n", result, first, second); break;
        case TAC_LT:  fprintf(output, "%s = %s < %s\n", result, first, second); break;
        case TAC_GT:  fprintf(output, "%s = %s > %s\n", result, first, second); break;
        case TAC_EQ:  fprintf(output, "%s = %s == %s\n", result, first, second); break;
        case TAC_UMINUS: fprintf(output, "%s = -%s\n", result, first); break;
        case TAC_NOT: fprintf(output, "%s = !%s\n", result, first); break;
        case TAC_INT_TO_FLOAT: fprintf(output, "%s = int_to_float %s\n", result, first); break;
        default:
            fprintf(output, "instruccion tac %d\n", instruction->operation);
            break;
    }
}

static void printTACWithDeletedInRed(const TACList *tacList,
                                    TACInstruction **deletedList,
                                    size_t deletedCount,
                                    const char *reason) {
    if (!isOptimizerDebugEnabled || tacList == NULL || deletedCount == 0) {
        return;
    }

    printf("\n\033[1;36m=== Codigo Intermedio TAC: %s (%zu eliminada%s) ===\033[0m\n",
           reason, deletedCount, deletedCount > 1 ? "s" : "");

    for (const TACInstruction *current = tacList->head; current != NULL; current = current->next) {
        bool isDeleted = false;
        for (size_t i = 0; i < deletedCount; i++) {
            if (current == deletedList[i]) {
                isDeleted = true;
                break;
            }
        }

        if (isDeleted) {
            printf("\033[1;31m  [ELIMINADA] ");
            printSingleInstruction(stdout, current);
            printf("\033[0m");
        } else {
            printf("              ");
            printSingleInstruction(stdout, current);
        }
    }
    printf("\033[1;36m======================================================================\033[0m\n\n");
}
