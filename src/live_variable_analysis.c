#include "live_variable_analysis.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void partitionBasicBlocks(LiveVariableAnalysis *analysis);
static void collectVariables(LiveVariableAnalysis *analysis);
static void buildControlFlowEdges(LiveVariableAnalysis *analysis);
static void computeBlockUseAndDef(LiveVariableAnalysis *analysis, const BasicBlock *block, VariableSet *use, VariableSet *def);
static void solveFixedPointLiveness(LiveVariableAnalysis *analysis, const VariableSet *useTable, const VariableSet *defTable);
static BasicBlock *findBlockByLabel(const LiveVariableAnalysis *analysis, const char *labelName);
static int getVariableIndex(const LiveVariableAnalysis *analysis, const char *name);
static int registerVariable(LiveVariableAnalysis *analysis, const char *name);
static bool isCandidate(const TACOperand *operand);
static bool isLeader(const TACInstruction *instruction, const TACInstruction *previous);

LiveVariableAnalysis *createLiveVariableAnalysis(const TACList *tacList) {
    if (tacList == NULL || tacList->head == NULL) return NULL;

    LiveVariableAnalysis *analysis = calloc(1, sizeof(LiveVariableAnalysis));
    if (analysis == NULL) return NULL;

    analysis->tacList = tacList;
    collectVariables(analysis);
    partitionBasicBlocks(analysis);
    if (analysis->blockCount == 0) {
        free(analysis);
        return NULL;
    }

    buildControlFlowEdges(analysis);

    VariableSet *useTable = calloc(analysis->blockCount, sizeof(VariableSet));
    VariableSet *defTable = calloc(analysis->blockCount, sizeof(VariableSet));
    if (useTable == NULL || defTable == NULL) {
        free(useTable);
        free(defTable);
        freeLiveVariableAnalysis(analysis);
        return NULL;
    }

    for (size_t i = 0; i < analysis->blockCount; i++) {
        computeBlockUseAndDef(analysis, analysis->blocks[i], &useTable[i], &defTable[i]);
    }

    solveFixedPointLiveness(analysis, useTable, defTable);

    free(useTable);
    free(defTable);
    return analysis;
}

void freeLiveVariableAnalysis(LiveVariableAnalysis *analysis) {
    if (analysis == NULL) return;
    for (size_t i = 0; i < analysis->blockCount; i++) {
        free(analysis->blocks[i]);
    }
    free(analysis->blocks);
    free(analysis);
}

BasicBlock *findInstructionBlock(const LiveVariableAnalysis *analysis, const TACInstruction *instruction) {
    if (analysis == NULL || instruction == NULL) return NULL;

    for (size_t i = 0; i < analysis->blockCount; i++) {
        BasicBlock *block = analysis->blocks[i];
        for (const TACInstruction *curr = block->firstInstruction; curr != NULL; curr = curr->next) {
            if (curr == instruction) return block;
            if (curr == block->lastInstruction) break;
        }
    }
    return NULL;
}

bool isVariableLiveIn(const LiveVariableAnalysis *analysis, const BasicBlock *block, const char *variableName) {
    if (analysis == NULL || block == NULL || variableName == NULL) return false;
    int index = getVariableIndex(analysis, variableName);
    return (index >= 0) && ((block->liveIn & (1ULL << index)) != 0);
}

bool isVariableLiveOut(const LiveVariableAnalysis *analysis, const BasicBlock *block, const char *variableName) {
    if (analysis == NULL || block == NULL || variableName == NULL) return false;
    int index = getVariableIndex(analysis, variableName);
    return (index >= 0) && ((block->liveOut & (1ULL << index)) != 0);
}

bool isDeadDefinitionInBlock(const LiveVariableAnalysis *analysis, const BasicBlock *block, const TACInstruction *instruction) {
    if (analysis == NULL || block == NULL || instruction == NULL || !isCandidate(&instruction->result)) return false;

    int varIndex = getVariableIndex(analysis, instruction->result.name);
    if (varIndex < 0) return false;

    // verifica si la variable se lee o se reescribe antes de terminar el bloque.
    if (instruction != block->lastInstruction) {
        for (const TACInstruction *curr = instruction->next; curr != NULL; curr = curr->next) {
            bool uses = (isCandidate(&curr->firstOperand) && strcmp(curr->firstOperand.name, instruction->result.name) == 0) ||
                        (isCandidate(&curr->secondOperand) && strcmp(curr->secondOperand.name, instruction->result.name) == 0);
            if (uses) return false;

            bool redefines = (isCandidate(&curr->result) && strcmp(curr->result.name, instruction->result.name) == 0);
            if (redefines) return true;

            if (curr == block->lastInstruction) break;
        }
    }

    // criterio de código muerto: no se lee en B y no pertenece a LIVE_OUT(B).
    return (block->liveOut & (1ULL << varIndex)) == 0;
}

bool isDeadDefinition(const LiveVariableAnalysis *analysis, const TACInstruction *instruction) {
    if (analysis == NULL || instruction == NULL) return false;

    BasicBlock *block = findInstructionBlock(analysis, instruction);
    if (block == NULL) return false;

    return isDeadDefinitionInBlock(analysis, block, instruction);
}

static void collectVariables(LiveVariableAnalysis *analysis) {
    for (const TACInstruction *curr = analysis->tacList->head; curr != NULL; curr = curr->next) {
        if (isCandidate(&curr->result))        registerVariable(analysis, curr->result.name);
        if (isCandidate(&curr->firstOperand))   registerVariable(analysis, curr->firstOperand.name);
        if (isCandidate(&curr->secondOperand))  registerVariable(analysis, curr->secondOperand.name);
    }
}

static void partitionBasicBlocks(LiveVariableAnalysis *analysis) {
    size_t capacity = 8;
    analysis->blocks = malloc(sizeof(BasicBlock *) * capacity);
    if (analysis->blocks == NULL) return;

    TACInstruction *curr = analysis->tacList->head;
    TACInstruction *prev = NULL;
    int nextId = 0;

    while (curr != NULL) {
        while (curr != NULL && !isLeader(curr, prev)) {
            prev = curr;
            curr = curr->next;
        }
        if (curr == NULL) break;

        BasicBlock *block = calloc(1, sizeof(BasicBlock));
        if (block == NULL) break;
        block->id = nextId++;
        block->firstInstruction = curr;

        TACInstruction *blockCurr = curr;
        while (blockCurr->next != NULL) {
            bool ends = (blockCurr->operation == TAC_GOTO || blockCurr->operation == TAC_IF_FALSE ||
                         blockCurr->operation == TAC_IF_TRUE || blockCurr->operation == TAC_RETURN ||
                         blockCurr->operation == TAC_END_FUNC);
            if (ends || isLeader(blockCurr->next, blockCurr)) break;
            blockCurr = blockCurr->next;
        }
        block->lastInstruction = blockCurr;

        if (analysis->blockCount >= capacity) {
            capacity *= 2;
            BasicBlock **reallocated = realloc(analysis->blocks, sizeof(BasicBlock *) * capacity);
            if (reallocated == NULL) { free(block); break; }
            analysis->blocks = reallocated;
        }
        analysis->blocks[analysis->blockCount++] = block;

        prev = blockCurr;
        curr = blockCurr->next;
    }
}

static void buildControlFlowEdges(LiveVariableAnalysis *analysis) {
    for (size_t i = 0; i < analysis->blockCount; i++) {
        BasicBlock *block = analysis->blocks[i];
        TACInstruction *last = block->lastInstruction;

        if (last->operation == TAC_GOTO) {
            BasicBlock *target = findBlockByLabel(analysis, last->firstOperand.name);
            if (target != NULL) block->successors[block->successorCount++] = target;
        } else if (last->operation == TAC_IF_FALSE || last->operation == TAC_IF_TRUE) {
            BasicBlock *target = findBlockByLabel(analysis, last->secondOperand.name);
            if (target != NULL) block->successors[block->successorCount++] = target;
            if (i + 1 < analysis->blockCount) block->successors[block->successorCount++] = analysis->blocks[i + 1];
        } else if (last->operation != TAC_RETURN && last->operation != TAC_END_FUNC) {
            if (i + 1 < analysis->blockCount) block->successors[block->successorCount++] = analysis->blocks[i + 1];
        }
    }
}

static void computeBlockUseAndDef(LiveVariableAnalysis *analysis, const BasicBlock *block, VariableSet *use, VariableSet *def) {
    *use = 0;
    *def = 0;

    for (const TACInstruction *curr = block->firstInstruction; curr != NULL; curr = curr->next) {
        bool isDecl = (curr->operation == TAC_GLOBAL_DECL ||
                       curr->operation == TAC_LOCAL_DECL ||
                       curr->operation == TAC_PARAM_DECL);
        if (!isDecl) {
            // USE(B): variables leídas en el bloque antes de cualquier definición previa.
            if (isCandidate(&curr->firstOperand)) {
                int id = getVariableIndex(analysis, curr->firstOperand.name);
                if (id >= 0 && (*def & (1ULL << id)) == 0) *use |= (1ULL << id);
            }
            if (isCandidate(&curr->secondOperand)) {
                int id = getVariableIndex(analysis, curr->secondOperand.name);
                if (id >= 0 && (*def & (1ULL << id)) == 0) *use |= (1ULL << id);
            }
            // DEF(B): variables asignadas en el bloque antes de haber sido utilizadas.
            if (isCandidate(&curr->result)) {
                int id = getVariableIndex(analysis, curr->result.name);
                if (id >= 0 && (*use & (1ULL << id)) == 0) *def |= (1ULL << id);
            }
        }
        if (curr == block->lastInstruction) break;
    }
}

static void solveFixedPointLiveness(LiveVariableAnalysis *analysis, const VariableSet *useTable, const VariableSet *defTable) {
    bool hasChanged = true;

    // algoritmo de punto fijo: itera hasta que ningún conjunto IN/OUT sufra cambios.
    while (hasChanged) {
        hasChanged = false;
        // análisis backward: se recorren los bloques en orden inverso (hacia atrás).
        for (long i = (long)analysis->blockCount - 1; i >= 0; i--) {
            BasicBlock *block = analysis->blocks[i];

            // ecuacion 1: LIVE_OUT(B) = Union(LIVE_IN(S)) para todo sucesor S de B.
            VariableSet newOut = 0;
            for (size_t s = 0; s < block->successorCount; s++) {
                newOut |= block->successors[s]->liveIn;
            }

            // ecuacion 2: LIVE_IN(B) = USE(B) U (LIVE_OUT(B) - DEF(B)).
            VariableSet newIn = useTable[i] | (newOut & ~defTable[i]);

            if (newIn != block->liveIn || newOut != block->liveOut) {
                hasChanged = true;
                block->liveIn = newIn;
                block->liveOut = newOut;
            }
        }
    }
}

static BasicBlock *findBlockByLabel(const LiveVariableAnalysis *analysis, const char *labelName) {
    if (analysis == NULL || labelName == NULL) return NULL;
    for (size_t i = 0; i < analysis->blockCount; i++) {
        BasicBlock *block = analysis->blocks[i];
        if (block->firstInstruction->operation == TAC_LABEL &&
            strcmp(block->firstInstruction->result.name, labelName) == 0) {
            return block;
        }
    }
    return NULL;
}

static int getVariableIndex(const LiveVariableAnalysis *analysis, const char *name) {
    if (analysis == NULL || name == NULL) return -1;
    for (size_t i = 0; i < analysis->variableCount; i++) {
        if (strcmp(analysis->variableNames[i], name) == 0) return (int)i;
    }
    return -1;
}

static int registerVariable(LiveVariableAnalysis *analysis, const char *name) {
    int existing = getVariableIndex(analysis, name);
    if (existing >= 0) return existing;

    if (analysis->variableCount < MAX_LIVENESS_VARIABLES) {
        int index = (int)analysis->variableCount++;
        snprintf(analysis->variableNames[index], TAC_NAME_LENGTH, "%s", name);
        return index;
    }
    return -1;
}

static bool isCandidate(const TACOperand *operand) {
    return (operand != NULL) && (operand->kind == OPERAND_VARIABLE || operand->kind == OPERAND_TEMP);
}

static bool isLeader(const TACInstruction *instruction, const TACInstruction *previous) {
    if (previous == NULL || instruction->operation == TAC_LABEL || instruction->operation == TAC_BEGIN_FUNC) return true;
    return (previous->operation == TAC_GOTO || previous->operation == TAC_IF_FALSE ||
            previous->operation == TAC_IF_TRUE || previous->operation == TAC_RETURN ||
            previous->operation == TAC_END_FUNC);
}

static void printInstructionText(FILE *output, const TACInstruction *instruction) {
    if (instruction == NULL) return;

    char line[TAC_INSTRUCTION_TEXT_LENGTH];
    formatTACInstruction(line, sizeof(line), instruction);
    fprintf(output, "%s", line);
}

static void printVariableSet(FILE *output, const LiveVariableAnalysis *analysis, VariableSet set) {
    fprintf(output, "{ ");
    bool first = true;
    for (size_t i = 0; i < analysis->variableCount; i++) {
        if ((set & (1ULL << i)) != 0) {
            if (!first) {
                fprintf(output, ", ");
            }
            fprintf(output, "%s", analysis->variableNames[i]);
            first = false;
        }
    }
    fprintf(output, " }\n");
}

void printLiveVariableAnalysis(const LiveVariableAnalysis *analysis, FILE *output) {
    if (analysis == NULL) return;
    if (output == NULL) output = stdout;

    fprintf(output, "\n======================================================================\n");
    fprintf(output, "Grafo de Flujo de Control (CFG) y Analisis de Variables Vivas\n");
    fprintf(output, "======================================================================\n");
    fprintf(output, "Variables candidatas (%zu): ", analysis->variableCount);
    for (size_t i = 0; i < analysis->variableCount; i++) {
        fprintf(output, "%s%s", analysis->variableNames[i], (i + 1 < analysis->variableCount) ? ", " : "\n");
    }
    if (analysis->variableCount == 0) fprintf(output, "(ninguna)\n");
    fprintf(output, "Total de bloques basicos: %zu\n\n", analysis->blockCount);

    VariableSet *useTable = calloc(analysis->blockCount, sizeof(VariableSet));
    VariableSet *defTable = calloc(analysis->blockCount, sizeof(VariableSet));
    if (useTable != NULL && defTable != NULL) {
        for (size_t i = 0; i < analysis->blockCount; i++) {
            computeBlockUseAndDef((LiveVariableAnalysis *)analysis, analysis->blocks[i], &useTable[i], &defTable[i]);
        }
    }

    for (size_t i = 0; i < analysis->blockCount; i++) {
        const BasicBlock *block = analysis->blocks[i];
        fprintf(output, "--------------------------------------------------\n");
        fprintf(output, "[Bloque %d]\n", block->id);
        fprintf(output, "  Instrucciones:\n");
        for (const TACInstruction *curr = block->firstInstruction; curr != NULL; curr = curr->next) {
            fprintf(output, "    ");
            printInstructionText(output, curr);
            if (curr == block->lastInstruction) break;
        }

        fprintf(output, "  Sucesores (%zu): ", block->successorCount);
        if (block->successorCount == 0) {
            fprintf(output, "(ninguno - fin de funcion/retorno)\n");
        } else {
            for (size_t s = 0; s < block->successorCount; s++) {
                fprintf(output, "Bloque %d%s", block->successors[s]->id, (s + 1 < block->successorCount) ? ", " : "\n");
            }
        }

        if (useTable != NULL && defTable != NULL) {
            fprintf(output, "  USE:      ");
            printVariableSet(output, analysis, useTable[i]);
            fprintf(output, "  DEF:      ");
            printVariableSet(output, analysis, defTable[i]);
        }
        fprintf(output, "  LIVE_IN:  ");
        printVariableSet(output, analysis, block->liveIn);
        fprintf(output, "  LIVE_OUT: ");
        printVariableSet(output, analysis, block->liveOut);
    }
    fprintf(output, "======================================================================\n\n");

    free(useTable);
    free(defTable);
}
