#include "live_variable_analysis.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void collectVariables(LiveVariableAnalysis *analysis);
static void partitionBasicBlocks(LiveVariableAnalysis *analysis);
static void buildControlFlowEdges(LiveVariableAnalysis *analysis);
static void computeBlockUseAndDef(const LiveVariableAnalysis *analysis, BasicBlock *block);
static void solveFixedPointLiveness(LiveVariableAnalysis *analysis);
static void printBasicBlock(FILE *output, const LiveVariableAnalysis *analysis, const BasicBlock *block);
static void collectOperandVariable(LiveVariableAnalysis *analysis, const TACOperand *operand);
static bool isLeader(const TACInstruction *instruction, const TACInstruction *previous);
static void addSuccessor(BasicBlock *block, BasicBlock *successor);
static BasicBlock *findBlockByLabel(const LiveVariableAnalysis *analysis, const char *labelName);
static bool isDeclaration(const TACInstruction *instruction);
static int getOperandVariableIndex(const LiveVariableAnalysis *analysis, const TACOperand *operand);
static bool instructionReadsVariable(const TACInstruction *instruction, const char *variableName);
static bool instructionWritesVariable(const TACInstruction *instruction, const char *variableName);
static void printVariableSet(FILE *output, const LiveVariableAnalysis *analysis, VariableSet set);
static int registerVariable(LiveVariableAnalysis *analysis, const char *name);
static bool endsBasicBlock(const TACInstruction *instruction);
static bool isCandidate(const TACOperand *operand);
static bool isVariableInSet(VariableSet set, int variableIndex);
static int getVariableIndex(const LiveVariableAnalysis *analysis, const char *name);

LiveVariableAnalysis *createLiveVariableAnalysis(const TACList *tacList) {
    if (tacList == NULL || tacList->head == NULL) return NULL;

    LiveVariableAnalysis *analysis = calloc(1, sizeof(*analysis));
    if (analysis == NULL) return NULL;

    analysis->tacList = tacList;

    collectVariables(analysis);
    partitionBasicBlocks(analysis);
    if (analysis->blockCount == 0) {
        freeLiveVariableAnalysis(analysis);
        return NULL;
    }

    buildControlFlowEdges(analysis);

    for (size_t i = 0; i < analysis->blockCount; i++) {
        computeBlockUseAndDef(analysis, analysis->blocks[i]);
    }

    solveFixedPointLiveness(analysis);
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

bool isDeadDefinition(const LiveVariableAnalysis *analysis, const TACInstruction *instruction) {
    if (analysis == NULL || instruction == NULL) return false;

    BasicBlock *block = findInstructionBlock(analysis, instruction);
    if (block == NULL) return false;

    return isDeadDefinitionInBlock(analysis, block, instruction);
}

BasicBlock *findInstructionBlock(const LiveVariableAnalysis *analysis, const TACInstruction *instruction) {
    if (analysis == NULL || instruction == NULL) return NULL;

    for (size_t i = 0; i < analysis->blockCount; i++) {
        BasicBlock *block = analysis->blocks[i];
        for (const TACInstruction *current = block->firstInstruction; current != NULL; current = current->next) {
            if (current == instruction) return block;
            if (current == block->lastInstruction) break;
        }
    }
    return NULL;
}

bool isDeadDefinitionInBlock(const LiveVariableAnalysis *analysis, const BasicBlock *block, const TACInstruction *instruction) {
    if (analysis == NULL || block == NULL || instruction == NULL) return false;

    int variableIndex = getOperandVariableIndex(analysis, &instruction->result);
    if (variableIndex < 0) return false;

    const char *variableName = instruction->result.name;

    // Dentro del bloque: una lectura conserva el valor; una reescritura lo descarta.
    if (instruction != block->lastInstruction) {
        for (const TACInstruction *current = instruction->next; current != NULL; current = current->next) {
            if (instructionReadsVariable(current, variableName)) return false;
            if (instructionWritesVariable(current, variableName)) return true;
            if (current == block->lastInstruction) break;
        }
    }

    // Al final del bloque, la definición solo importa si el valor sale vivo.
    return !isVariableInSet(block->liveOut, variableIndex);
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

    for (size_t i = 0; i < analysis->blockCount; i++) {
        printBasicBlock(output, analysis, analysis->blocks[i]);
    }
    fprintf(output, "======================================================================\n\n");
}

static void collectVariables(LiveVariableAnalysis *analysis) {
    for (const TACInstruction *current = analysis->tacList->head; current != NULL; current = current->next) {
        collectOperandVariable(analysis, &current->result);
        collectOperandVariable(analysis, &current->firstOperand);
        collectOperandVariable(analysis, &current->secondOperand);
    }
}

static void partitionBasicBlocks(LiveVariableAnalysis *analysis) {
    size_t capacity = 8;
    analysis->blocks = malloc(sizeof(BasicBlock *) * capacity);
    if (analysis->blocks == NULL) return;

    TACInstruction *previous = NULL;
    TACInstruction *current = analysis->tacList->head;
    int nextId = 0;

    while (current != NULL) {
        while (current != NULL && !isLeader(current, previous)) {
            previous = current;
            current = current->next;
        }
        if (current == NULL) break;

        BasicBlock *block = calloc(1, sizeof(*block));
        if (block == NULL) break;

        block->id = nextId++;
        block->firstInstruction = current;

        // El bloque termina antes del próximo líder, que puede seguir a un terminador o a una etiqueta.
        TACInstruction *last = current;
        while (last->next != NULL && !isLeader(last->next, last)) {
            last = last->next;
        }
        block->lastInstruction = last;

        if (analysis->blockCount >= capacity) {
            capacity *= 2;
            BasicBlock **reallocated = realloc(analysis->blocks, sizeof(BasicBlock *) * capacity);
            if (reallocated == NULL) {
                free(block);
                break;
            }
            analysis->blocks = reallocated;
        }
        analysis->blocks[analysis->blockCount++] = block;

        previous = last;
        current = last->next;
    }
}

static void buildControlFlowEdges(LiveVariableAnalysis *analysis) {
    for (size_t i = 0; i < analysis->blockCount; i++) {
        BasicBlock *block = analysis->blocks[i];
        const TACInstruction *last = block->lastInstruction;
        BasicBlock *nextBlock = (i + 1 < analysis->blockCount) ? analysis->blocks[i + 1] : NULL;

        if (last->operation == TAC_GOTO) {
            addSuccessor(block, findBlockByLabel(analysis, last->firstOperand.name));
        } else if (last->operation == TAC_IF_FALSE || last->operation == TAC_IF_TRUE) {
            addSuccessor(block, findBlockByLabel(analysis, last->secondOperand.name));
            addSuccessor(block, nextBlock);
        } else if (last->operation != TAC_RETURN && last->operation != TAC_END_FUNC) {
            addSuccessor(block, nextBlock);
        }
    }
}

static void computeBlockUseAndDef(const LiveVariableAnalysis *analysis, BasicBlock *block) {
    block->use = 0;
    block->def = 0;

    for (const TACInstruction *current = block->firstInstruction; current != NULL; current = current->next) {
        bool isDeclarationInstruction = isDeclaration(current);
        if (!isDeclarationInstruction) {
            // USE(B): variables leídas antes de cualquier definición dentro del bloque.

            int firstIndex = getOperandVariableIndex(analysis, &current->firstOperand);
            if (firstIndex >= 0 && !isVariableInSet(block->def, firstIndex)) {
                block->use |= (1ULL << firstIndex);
            }

            int secondIndex = getOperandVariableIndex(analysis, &current->secondOperand);
            if (secondIndex >= 0 && !isVariableInSet(block->def, secondIndex)) {
                block->use |= (1ULL << secondIndex);
            }

            // DEF(B): variables asignadas antes de haber sido leídas dentro del bloque.
            int resultIndex = getOperandVariableIndex(analysis, &current->result);
            if (resultIndex >= 0 && !isVariableInSet(block->use, resultIndex)) {
                block->def |= (1ULL << resultIndex);
            }
        }

        if (current == block->lastInstruction) break;
    }
}

static void solveFixedPointLiveness(LiveVariableAnalysis *analysis) {
    bool hasChanged = true;

    // Análisis backward: se itera hasta que ningún LIVE_IN/LIVE_OUT cambie.
    while (hasChanged) {
        hasChanged = false;

        for (size_t i = analysis->blockCount; i-- > 0;) {
            BasicBlock *block = analysis->blocks[i];

            // LIVE_OUT(B) = unión de LIVE_IN(S) para cada sucesor S.
            VariableSet newOut = 0;
            for (size_t s = 0; s < block->successorCount; s++) {
                newOut |= block->successors[s]->liveIn;
            }

            // LIVE_IN(B) = USE(B) ∪ (LIVE_OUT(B) − DEF(B)).
            VariableSet newIn = block->use | (newOut & ~block->def);

            if (newIn != block->liveIn || newOut != block->liveOut) {
                hasChanged = true;
                block->liveIn = newIn;
                block->liveOut = newOut;
            }
        }
    }
}

static void printBasicBlock(FILE *output, const LiveVariableAnalysis *analysis, const BasicBlock *block) {
    fprintf(output, "--------------------------------------------------\n");
    fprintf(output, "[Bloque %d]\n", block->id);

    fprintf(output, "  Instrucciones:\n");
    for (const TACInstruction *current = block->firstInstruction; current != NULL; current = current->next) {
        fprintf(output, "    ");
        fprintTACInstruction(output, current);
        if (current == block->lastInstruction) break;
    }

    fprintf(output, "  Sucesores (%zu): ", block->successorCount);
    if (block->successorCount == 0) {
        fprintf(output, "(ninguno - fin de funcion/retorno)\n");
    } else {
        for (size_t s = 0; s < block->successorCount; s++) {
            fprintf(output, "Bloque %d%s", block->successors[s]->id, (s + 1 < block->successorCount) ? ", " : "\n");
        }
    }

    fprintf(output, "  USE:      ");
    printVariableSet(output, analysis, block->use);
    fprintf(output, "  DEF:      ");
    printVariableSet(output, analysis, block->def);

    fprintf(output, "  LIVE_IN:  ");
    printVariableSet(output, analysis, block->liveIn);
    fprintf(output, "  LIVE_OUT: ");
    printVariableSet(output, analysis, block->liveOut);
}

static void collectOperandVariable(LiveVariableAnalysis *analysis, const TACOperand *operand) {
    if (isCandidate(operand)) {
        registerVariable(analysis, operand->name);
    }
}

static bool isLeader(const TACInstruction *instruction, const TACInstruction *previous) {
    if (previous == NULL) return true;

    bool startsBlock = (instruction->operation == TAC_LABEL || instruction->operation == TAC_BEGIN_FUNC);
    return startsBlock || endsBasicBlock(previous);
}

static void addSuccessor(BasicBlock *block, BasicBlock *successor) {
    if (successor == NULL) return;
    block->successors[block->successorCount++] = successor;
}

static BasicBlock *findBlockByLabel(const LiveVariableAnalysis *analysis, const char *labelName) {
    if (analysis == NULL || labelName == NULL) return NULL;

    for (size_t i = 0; i < analysis->blockCount; i++) {
        BasicBlock *block = analysis->blocks[i];
        bool startsWithLabel = (block->firstInstruction->operation == TAC_LABEL);
        if (startsWithLabel && strcmp(block->firstInstruction->result.name, labelName) == 0) {
            return block;
        }
    }
    return NULL;
}

static bool isDeclaration(const TACInstruction *instruction) {
    return instruction->operation == TAC_GLOBAL_DECL ||
           instruction->operation == TAC_LOCAL_DECL ||
           instruction->operation == TAC_PARAM_DECL;
}

static int getOperandVariableIndex(const LiveVariableAnalysis *analysis, const TACOperand *operand) {
    if (!isCandidate(operand)) return -1;
    return getVariableIndex(analysis, operand->name);
}

static bool instructionReadsVariable(const TACInstruction *instruction, const char *variableName) {
    bool readsFirst = isCandidate(&instruction->firstOperand) &&
                      strcmp(instruction->firstOperand.name, variableName) == 0;
    bool readsSecond = isCandidate(&instruction->secondOperand) &&
                       strcmp(instruction->secondOperand.name, variableName) == 0;
    return readsFirst || readsSecond;
}

static bool instructionWritesVariable(const TACInstruction *instruction, const char *variableName) {
    return isCandidate(&instruction->result) &&
           strcmp(instruction->result.name, variableName) == 0;
}

static void printVariableSet(FILE *output, const LiveVariableAnalysis *analysis, VariableSet set) {
    fprintf(output, "{ ");

    bool first = true;
    for (size_t i = 0; i < analysis->variableCount; i++) {
        if (isVariableInSet(set, (int)i)) {
            if (!first) fprintf(output, ", ");
            fprintf(output, "%s", analysis->variableNames[i]);
            first = false;
        }
    }

    fprintf(output, " }\n");
}

static int registerVariable(LiveVariableAnalysis *analysis, const char *name) {
    int existingIndex = getVariableIndex(analysis, name);
    if (existingIndex >= 0) return existingIndex;

    if (analysis->variableCount >= MAX_LIVENESS_VARIABLES) return -1;

    int index = (int)analysis->variableCount++;
    snprintf(analysis->variableNames[index], TAC_NAME_LENGTH, "%s", name);
    return index;
}

static bool endsBasicBlock(const TACInstruction *instruction) {
    return instruction->operation == TAC_GOTO ||
           instruction->operation == TAC_IF_FALSE ||
           instruction->operation == TAC_IF_TRUE ||
           instruction->operation == TAC_RETURN ||
           instruction->operation == TAC_END_FUNC;
}

static bool isCandidate(const TACOperand *operand) {
    return operand != NULL &&
           (operand->kind == OPERAND_VARIABLE || operand->kind == OPERAND_TEMP);
}

static bool isVariableInSet(VariableSet set, int variableIndex) {
    return (set & (1ULL << variableIndex)) != 0;
}

static int getVariableIndex(const LiveVariableAnalysis *analysis, const char *name) {
    if (analysis == NULL || name == NULL) return -1;

    for (size_t i = 0; i < analysis->variableCount; i++) {
        if (strcmp(analysis->variableNames[i], name) == 0) return (int)i;
    }
    return -1;
}
