#include "optimizer.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "live_variable_analysis.h"
#include "tac.h"

#define MAX_KNOWN_CONSTANTS 256
#define MAX_OPTIMIZATION_ROUNDS 16

typedef struct {
    char name[TAC_NAME_LENGTH];
    OperandKind destinationKind;
    bool hasSymbol;
    bool isGlobal;
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
static int asInt(const TACOperand *operand);
static bool evaluateBinary(TACOperation operation, const TACOperand *firstOperand,
                           const TACOperand *secondOperand, TACOperand *folded);
static bool evaluateUnary(TACOperation operation, const TACOperand *operand, TACOperand *folded);

static const TACOperand *lookupKnownConstant(const KnownConstantTable *table, const TACOperand *operand);
static void forgetConstant(KnownConstantTable *table, const TACOperand *operand);
static void rememberConstant(KnownConstantTable *table, const TACOperand *destination, const TACOperand *value);
static void forgetGlobalVariables(KnownConstantTable *table, const TACList *tacList);
static bool replaceWithKnownConstant(const KnownConstantTable *table, TACOperand *operand);

static TACInstruction *unlinkInstruction(TACList *tacList, TACInstruction *previous, TACInstruction *current);
static int unlinkInstructionList(TACList *tacList, TACInstruction **instructions, size_t count);
static bool endsControlFlow(TACOperation operation);
static bool isStructural(TACOperation operation);
static int removeUnreferencedLabels(TACList *tacList);
static bool isLabelReferenced(const TACList *tacList, const char *labelName);
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
                                       &instruction->secondOperand, &folded);
        } else if (isUnaryCandidate) {
            wasFolded = evaluateUnary(instruction->operation, &instruction->firstOperand, &folded);
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

    if (isOptimizerDebugEnabled && unreachableCount > 0) {
        printTACWithDeletedInRed(tacList, unreachableList, unreachableCount, "Codigo Inalcanzable (tras ret / goto)");
    }

    changes += unlinkInstructionList(tacList, unreachableList, unreachableCount);
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
    for (size_t blockIndex = 0; blockIndex < analysis->blockCount; blockIndex++) {
        const BasicBlock *block = analysis->blocks[blockIndex];
        for (TACInstruction *current = block->firstInstruction; current != NULL; current = current->next) {
            bool canBeDeadStore = isDeadStoreCandidate(tacList, current);
            bool isDead = (canBeDeadStore && isDeadDefinitionInBlock(analysis, block, current));
            if (isDead) {
                deadInstructions[deadCount++] = current;
            }
            if (current == block->lastInstruction) {
                break;
            }
        }
    }

    freeLiveVariableAnalysis(analysis);

    if (isOptimizerDebugEnabled && deadCount > 0) {
        printTACWithDeletedInRed(tacList, deadInstructions, deadCount, "Variables y Asignaciones Muertas (Live Variables)");
    }

    return unlinkInstructionList(tacList, deadInstructions, deadCount);
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

    if (isOptimizerDebugEnabled && selfCount > 0) {
        printTACWithDeletedInRed(tacList, selfList, selfCount, "Autoasignaciones Redundantes (x = x)");
    }

    return unlinkInstructionList(tacList, selfList, selfCount);
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

    if (isOptimizerDebugEnabled && jumpCount > 0) {
        printTACWithDeletedInRed(tacList, jumpList, jumpCount, "Saltos Redundantes a la Siguiente Etiqueta");
    }

    return unlinkInstructionList(tacList, jumpList, jumpCount);
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
    entry->hasSymbol = (destination->symbol != NULL);
    entry->isGlobal = (destination->symbol != NULL && destination->symbol->isGlobal);
    entry->value = *value;
    table->count++;
}

static void forgetGlobalVariables(KnownConstantTable *table, const TACList *tacList) {
    int keptEntriesCount = 0;
    for (int i = 0; i < table->count; i++) {
        bool isGlobal = false;
        if (table->entries[i].destinationKind == OPERAND_VARIABLE) {
            if (table->entries[i].hasSymbol) {
                isGlobal = table->entries[i].isGlobal;
            } else {
                isGlobal = isGlobalVariable(tacList, table->entries[i].name);
            }
        }
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

static int asInt(const TACOperand *operand) {
    return operand->value.intValue;
}

static bool evaluateBinary(TACOperation operation, const TACOperand *firstOperand,
                           const TACOperand *secondOperand, TACOperand *folded) {
    switch (operation) {
        case TAC_ADD:
        case TAC_SUB:
        case TAC_MUL: {
            int leftValue = asInt(firstOperand);
            int rightValue = asInt(secondOperand);
            int resultValue = (operation == TAC_ADD) ? leftValue + rightValue
                            : (operation == TAC_SUB) ? leftValue - rightValue
                                                     : leftValue * rightValue;
            *folded = tacConstantInt(resultValue);
            return true;
        }

        case TAC_DIV:
        case TAC_MOD: {
            int leftValue = asInt(firstOperand);
            int rightValue = asInt(secondOperand);
            bool isZeroDivisor = (rightValue == 0);
            bool isSignedOverflow = (leftValue == INT_MIN && rightValue == -1);
            if (isZeroDivisor || isSignedOverflow) {
                return false;
            }
            int resultValue = (operation == TAC_DIV) ? leftValue / rightValue
                                                     : leftValue % rightValue;
            *folded = tacConstantInt(resultValue);
            return true;
        }

        case TAC_LT:
            *folded = tacConstantBool(asInt(firstOperand) < asInt(secondOperand));
            return true;

        case TAC_GT:
            *folded = tacConstantBool(asInt(firstOperand) > asInt(secondOperand));
            return true;

        case TAC_EQ:
            *folded = tacConstantBool(asInt(firstOperand) == asInt(secondOperand));
            return true;

        default:
            return false;
    }
}

static bool evaluateUnary(TACOperation operation, const TACOperand *operand, TACOperand *folded) {
    switch (operation) {
        case TAC_UMINUS:
            *folded = tacConstantInt(-asInt(operand));
            return true;

        case TAC_NOT:
            *folded = tacConstantBool(!asInt(operand));
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

static int unlinkInstructionList(TACList *tacList, TACInstruction **instructions, size_t count) {
    if (instructions == NULL || count == 0) {
        free(instructions);
        return 0;
    }

    int changes = 0;
    size_t targetIndex = 0;
    TACInstruction *previous = NULL;
    TACInstruction *current = tacList->head;

    while (current != NULL && targetIndex < count) {
        bool isTarget = (current == instructions[targetIndex]);
        if (isTarget) {
            current = unlinkInstruction(tacList, previous, current);
            targetIndex++;
            changes++;
        } else {
            previous = current;
            current = current->next;
        }
    }

    free(instructions);
    return changes;
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
    TACInstruction **deadLabelList = malloc(sizeof(TACInstruction *) * (size_t)tacList->count);
    if (deadLabelList == NULL) {
        return 0;
    }
    size_t deadLabelCount = 0;

    for (TACInstruction *current = tacList->head; current != NULL; current = current->next) {
        bool isLabel = (current->operation == TAC_LABEL);
        if (isLabel && !isLabelReferenced(tacList, current->result.name)) {
            deadLabelList[deadLabelCount++] = current;
        }
    }

    if (isOptimizerDebugEnabled && deadLabelCount > 0) {
        printTACWithDeletedInRed(tacList, deadLabelList, deadLabelCount, "Etiquetas sin Referencias");
    }

    return unlinkInstructionList(tacList, deadLabelList, deadLabelCount);
}

static bool isLabelReferenced(const TACList *tacList, const char *labelName) {
    for (const TACInstruction *instruction = tacList->head; instruction != NULL; instruction = instruction->next) {
        if (instruction->operation == TAC_GOTO && strcmp(instruction->firstOperand.name, labelName) == 0) {
            return true;
        }
        if ((instruction->operation == TAC_IF_FALSE || instruction->operation == TAC_IF_TRUE) &&
            strcmp(instruction->secondOperand.name, labelName) == 0) {
            return true;
        }
    }
    return false;
}

static bool isGlobalVariable(const TACList *tacList, const char *variableName) {
    if (tacList == NULL || variableName == NULL) {
        return false;
    }
    for (const TACInstruction *current = tacList->head; current != NULL; current = current->next) {
        if (current->operation == TAC_GLOBAL_DECL) {
            bool matchesName = (strcmp(current->result.name, variableName) == 0);
            if (matchesName) {
                return true;
            }
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

    bool isGlobal = false;
    if (instruction->result.kind == OPERAND_VARIABLE) {
        if (instruction->result.symbol != NULL) {
            isGlobal = instruction->result.symbol->isGlobal;
        } else {
            isGlobal = isGlobalVariable(tacList, instruction->result.name);
        }
    }
    if (isGlobal) {
        return false;
    }

    return true;
}

static void printSingleInstruction(FILE *output, const TACInstruction *instruction) {
    if (instruction == NULL) return;

    char line[TAC_INSTRUCTION_TEXT_LENGTH];
    formatTACInstruction(line, sizeof(line), instruction);
    fprintf(output, "%s", line);
}

static void printTACWithDeletedInRed(const TACList *tacList,
                                    TACInstruction **deletedList,
                                    size_t deletedCount,
                                    const char *reason) {
    if (!isOptimizerDebugEnabled || tacList == NULL || deletedCount == 0) {
        return;
    }

    debugPrintf(isOptimizerDebugEnabled, "\n[OPT] === Codigo Intermedio TAC: %s (%zu eliminada%s) ===\n",
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
            printf("  [ELIMINADA] ");
            printSingleInstruction(stdout, current);
        } else {
            printf("              ");
            printSingleInstruction(stdout, current);
        }
    }
    printf("======================================================================\n\n");
}
