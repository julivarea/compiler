#include "object_code.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "string_builder.h"

#define MAX_ARG_REGISTERS 6

static const char *ARG_REGISTERS_32[] = {
    "%edi", "%esi", "%edx", "%ecx", "%r8d", "%r9d"
};

typedef enum {
    LOCATION_UNRESOLVED,
    LOCATION_STACK,
    LOCATION_GLOBAL
} OperandLocation;

static int objectCodeErrors = 0;

// Optimizaciones activas en la corrida emitidas en Assembly
static OptimizationSet activeOptimizations = OPTIMIZATION_NONE;

// Prototipos de submetodos para aplicar Step-Down Rule
static bool buildFrameLayout(FunctionFrame *frame, const TACInstruction *functionStart);
static void allocateLocalSlots(FunctionFrame *frame, const TACInstruction *functionStart);
static bool allocatePendingParamSlots(FunctionFrame *frame, const TACInstruction *functionStart);
static const TACInstruction *emitFunction(FunctionFrame *frame, const TACInstruction *functionStart);

static void emitParamDeclaration(const TACInstruction *instruction, FunctionFrame *frame);
static void emitAssignment(const TACInstruction *instruction, const FunctionFrame *frame);
static void emitMultiplication(const TACInstruction *instruction, const FunctionFrame *frame);
static void emitUnaryMinus(const TACInstruction *instruction, const FunctionFrame *frame);
static void emitNot(const TACInstruction *instruction, const FunctionFrame *frame);
static void emitLabel(const TACInstruction *instruction);
static void emitGoto(const TACInstruction *instruction);
static void emitConditionalJump(const TACInstruction *instruction, const char *jumpMnemonic, const FunctionFrame *frame);
static bool getRelationalMnemonics(TACOperation operation,
                                   const char **setMnemonic,
                                   const char **jumpTrue,
                                   const char **jumpFalse);
static bool canFuseConditionAndJump(const TACInstruction *inst, const TACInstruction *next);
static void emitFusedComparisonAndJump(const TACInstruction *relInst,
                                      const TACInstruction *jumpInst,
                                      const FunctionFrame *frame);
static void emitParam(const TACInstruction *instruction, FunctionFrame *frame);
static void emitCall(const TACInstruction *instruction, FunctionFrame *frame);
static void emitReturn(const TACInstruction *instruction, const FunctionFrame *frame);

static void emitCallStackArguments(const FunctionFrame *frame, int base, int argumentCount);
static void emitCallRegisterArguments(const FunctionFrame *frame, int base, int registerArguments);
static void emitCallResult(const TACInstruction *instruction, const FunctionFrame *frame);

static void emitBinaryOp(const TACInstruction *instruction, const char *opMnemonic, const FunctionFrame *frame);
static bool emitStrengthReducedMultiplication(const TACInstruction *instruction, const FunctionFrame *frame);
static int powerOfTwoExponent(int value);
static void emitDivisionOp(const TACInstruction *instruction, const FunctionFrame *frame, const char *resultRegisterName);
static void emitRelationalOp(const TACInstruction *instruction, const FunctionFrame *frame);

static void loadOperand(const TACOperand *operand, const char *registerName, const FunctionFrame *frame);
static void storeOperand(const TACOperand *operand, const char *registerName, const FunctionFrame *frame);

static OperandLocation resolveOperandLocation(const TACOperand *operand, const FunctionFrame *frame, int *offset);
static bool isDeclaredGlobal(const TACOperand *operand);
static const char *getGlobalName(const TACOperand *operand);

static void emitGlobals(const TACList *tacList);

void generateAssembly(const TACList *tacList, OptimizationSet enabled) {
    bool isNullList = (tacList == NULL);
    if (isNullList) return;

    objectCodeErrors = 0;
    activeOptimizations = enabled;

    emitGlobals(tacList);

    buildStringf(".text\n");

    const TACInstruction *instruction = tacList->head;
    while (instruction != NULL) {
        bool isFunctionStart = (instruction->operation == TAC_BEGIN_FUNC);
        if (isFunctionStart) {
            FunctionFrame *frame = createFrame(instruction->result.name);
            if (frame == NULL) {
                fprintf(stderr,
                        "\033[1;31mError interno del generador de codigo objeto:\033[0m"
                        " no se pudo reservar memoria para el frame de '%s'.\n",
                        instruction->result.name);
                objectCodeErrors++;
                break;
            }

            frame->returnType = instruction->result.type;

            if (!buildFrameLayout(frame, instruction)) {
                fprintf(stderr,
                        "\033[1;31mError interno del generador de codigo objeto:\033[0m"
                        " no se pudo reservar memoria para los argumentos pendientes de '%s'.\n",
                        instruction->result.name);
                objectCodeErrors++;
                freeFrame(frame);
                break;
            }
            instruction = emitFunction(frame, instruction);

            freeFrame(frame);
            continue;
        }

        instruction = instruction->next;
    }

    buildStringf("\n.section .note.GNU-stack,\"\",@progbits\n");
}

int getObjectCodeErrors(void) {
    return objectCodeErrors;
}

static bool buildFrameLayout(FunctionFrame *frame, const TACInstruction *functionStart) {
    allocateLocalSlots(frame, functionStart);
    if (!allocatePendingParamSlots(frame, functionStart)) {
        return false;
    }
    frame->frameSize = ALIGN_UP(frame->nextOffset, 16);
    return true;
}

static void allocateLocalSlots(FunctionFrame *frame, const TACInstruction *functionStart) {
    for (const TACInstruction *current = functionStart->next;
         current != NULL && current->operation != TAC_END_FUNC;
         current = current->next) {

        bool isLocalOrParam = (current->operation == TAC_PARAM_DECL ||
                               current->operation == TAC_LOCAL_DECL);
        bool isTemporary = (current->result.kind == OPERAND_TEMP);
        if (isLocalOrParam || isTemporary) {
            addSlot(frame, &current->result);
        }
    }
}

static bool allocatePendingParamSlots(FunctionFrame *frame, const TACInstruction *functionStart) {
    int pendingCount = 0;
    for (const TACInstruction *scan = functionStart->next;
         scan != NULL && scan->operation != TAC_END_FUNC;
         scan = scan->next) {

        if (scan->operation == TAC_PARAM) {
            pendingCount++;
            bool exceedsPeak = (pendingCount > frame->maxPendingParams);
            if (exceedsPeak) {
                frame->maxPendingParams = pendingCount;
            }
        } else if (scan->operation == TAC_CALL) {
            int arity = (scan->secondOperand.kind == OPERAND_CONSTANT) ? scan->secondOperand.value.intValue : 0;
            pendingCount -= arity;
            if (pendingCount < 0) {
                pendingCount = 0;
            }
        }
    }

    if (!ensurePendingParamCapacity(frame, frame->maxPendingParams)) {
        return false;
    }
    for (int i = 0; i < frame->maxPendingParams; i++) {
        frame->pendingParamSlots[i] = addAnonymousSlot(frame);
    }
    return true;
}

static const TACInstruction *emitFunction(FunctionFrame *frame, const TACInstruction *functionStart) {
    emitPrologue(frame);

    const TACInstruction *current = functionStart->next;
    while (current != NULL && current->operation != TAC_END_FUNC) {
        if (canFuseConditionAndJump(current, current->next)) {
            emitFusedComparisonAndJump(current, current->next, frame);
            current = current->next->next;
        } else {
            generateInstruction(current, frame);
            current = current->next;
        }
    }

    emitEpilogue(frame);
    return current;
}

void emitPrologue(const FunctionFrame *frame) {
    if (frame == NULL) return;

    buildStringf("\n.globl %s\n", frame->functionName);
    buildStringf(".type %s, @function\n", frame->functionName);
    buildStringf("%s:\n", frame->functionName);
    buildStringf("    pushq %%rbp\n");
    buildStringf("    movq %%rsp, %%rbp\n");
    bool hasFrameSize = (frame->frameSize > 0);
    if (hasFrameSize) {
        buildStringf("    subq $%d, %%rsp\n", frame->frameSize);
    }
}

void emitEpilogue(const FunctionFrame *frame) {
    bool hasFunctionName = (frame != NULL && frame->functionName[0] != '\0');
    if (hasFunctionName) {
        buildStringf(".L_%s_epilogue:\n", frame->functionName);
    }

    // El sistema operativo toma %eax como codigo de salida del proceso en void main()
    bool isVoidMain = (frame != NULL && frame->returnType == TYPE_VOID
                       && strcmp(frame->functionName, "main") == 0);
    if (isVoidMain) {
        buildStringf("    movl $0, %%eax\n");
    }

    buildStringf("    leave\n");
    buildStringf("    ret\n");
}

void generateInstruction(const TACInstruction *instruction, FunctionFrame *frame) {
    bool isInvalidInstruction = (instruction == NULL || frame == NULL);
    if (isInvalidInstruction) return;

    switch (instruction->operation) {
        case TAC_PARAM_DECL:
            emitParamDeclaration(instruction, frame);
            break;

        case TAC_LOCAL_DECL:
            break;

        case TAC_ASSIGN:
            emitAssignment(instruction, frame);
            break;

        case TAC_ADD:
            emitBinaryOp(instruction, "addl", frame);
            break;

        case TAC_SUB:
            emitBinaryOp(instruction, "subl", frame);
            break;

        case TAC_MUL:
            emitMultiplication(instruction, frame);
            break;

        case TAC_DIV:
            emitDivisionOp(instruction, frame, "%eax");
            break;

        case TAC_MOD:
            emitDivisionOp(instruction, frame, "%edx");
            break;

        case TAC_UMINUS:
            emitUnaryMinus(instruction, frame);
            break;

        case TAC_NOT:
            emitNot(instruction, frame);
            break;

        case TAC_LT:
        case TAC_GT:
        case TAC_EQ:
            emitRelationalOp(instruction, frame);
            break;

        case TAC_LABEL:
            emitLabel(instruction);
            break;

        case TAC_GOTO:
            emitGoto(instruction);
            break;

        case TAC_IF_FALSE:
            emitConditionalJump(instruction, "je", frame);
            break;

        case TAC_IF_TRUE:
            emitConditionalJump(instruction, "jne", frame);
            break;

        case TAC_PARAM:
            emitParam(instruction, frame);
            break;

        case TAC_CALL:
            emitCall(instruction, frame);
            break;

        case TAC_RETURN:
            emitReturn(instruction, frame);
            break;

        default:
            break;
    }
}

static void emitParamDeclaration(const TACInstruction *instruction, FunctionFrame *frame) {
    int offset = 0;
    bool hasOffset = getSlotOffset(frame, instruction->result.name, &offset);

    if (hasOffset) {
        bool fitsInGeneralRegister = (frame->integerParamCount < MAX_ARG_REGISTERS);

        if (fitsInGeneralRegister) {
            buildStringf("    movl %s, %d(%%rbp)\n",
                         ARG_REGISTERS_32[frame->integerParamCount], offset);
        } else {
            // Se agotaron los registros: el llamador lo ubico en su stack
            int callerOffset = 16 + frame->stackParamCount * 8;
            buildStringf("    movl %d(%%rbp), %%eax\n", callerOffset);
            buildStringf("    movl %%eax, %d(%%rbp)\n", offset);
        }
    }

    if (frame->integerParamCount < MAX_ARG_REGISTERS) {
        frame->integerParamCount++;
    } else {
        frame->stackParamCount++;
    }
}

static void emitAssignment(const TACInstruction *instruction, const FunctionFrame *frame) {
    loadOperand(&instruction->firstOperand, "%eax", frame);
    storeOperand(&instruction->result, "%eax", frame);
}

static void emitMultiplication(const TACInstruction *instruction, const FunctionFrame *frame) {
    bool wasReduced = emitStrengthReducedMultiplication(instruction, frame);
    if (!wasReduced) {
        emitBinaryOp(instruction, "imull", frame);
    }
}

static void emitUnaryMinus(const TACInstruction *instruction, const FunctionFrame *frame) {
    loadOperand(&instruction->firstOperand, "%eax", frame);
    buildStringf("    negl %%eax\n");
    storeOperand(&instruction->result, "%eax", frame);
}

static void emitNot(const TACInstruction *instruction, const FunctionFrame *frame) {
    loadOperand(&instruction->firstOperand, "%eax", frame);
    buildStringf("    xorl $1, %%eax\n");
    storeOperand(&instruction->result, "%eax", frame);
}

static void emitLabel(const TACInstruction *instruction) {
    buildStringf(".%s:\n", instruction->result.name);
}

static void emitGoto(const TACInstruction *instruction) {
    buildStringf("    jmp .%s\n", instruction->firstOperand.name);
}

static void emitConditionalJump(const TACInstruction *instruction, const char *jumpMnemonic, const FunctionFrame *frame) {
    loadOperand(&instruction->firstOperand, "%eax", frame);
    buildStringf("    cmpl $0, %%eax\n");
    buildStringf("    %s .%s\n", jumpMnemonic, instruction->secondOperand.name);
}

static bool getRelationalMnemonics(TACOperation operation,
                                   const char **setMnemonic,
                                   const char **jumpTrue,
                                   const char **jumpFalse) {
    switch (operation) {
        case TAC_LT:
            if (setMnemonic != NULL) *setMnemonic = "setl";
            if (jumpTrue != NULL)    *jumpTrue    = "jl";
            if (jumpFalse != NULL)   *jumpFalse   = "jge";
            return true;
        case TAC_GT:
            if (setMnemonic != NULL) *setMnemonic = "setg";
            if (jumpTrue != NULL)    *jumpTrue    = "jg";
            if (jumpFalse != NULL)   *jumpFalse   = "jle";
            return true;
        case TAC_EQ:
            if (setMnemonic != NULL) *setMnemonic = "sete";
            if (jumpTrue != NULL)    *jumpTrue    = "je";
            if (jumpFalse != NULL)   *jumpFalse   = "jne";
            return true;
        default:
            return false;
    }
}

static bool canFuseConditionAndJump(const TACInstruction *inst, const TACInstruction *next) {
    bool isEnabled = isOptimizationEnabled(activeOptimizations, OPTIMIZATION_PEEPHOLE);
    if (!isEnabled || inst == NULL || next == NULL) return false;

    bool isRelational = getRelationalMnemonics(inst->operation, NULL, NULL, NULL);
    bool isConditionalJump = (next->operation == TAC_IF_FALSE ||
                              next->operation == TAC_IF_TRUE);

    if (!isRelational || !isConditionalJump) return false;

    return (inst->result.kind == OPERAND_TEMP &&
            strcmp(inst->result.name, next->firstOperand.name) == 0);
}

static void emitFusedComparisonAndJump(const TACInstruction *relInst,
                                      const TACInstruction *jumpInst,
                                      const FunctionFrame *frame) {
    const char *jumpTrue = NULL;
    const char *jumpFalse = NULL;
    if (!getRelationalMnemonics(relInst->operation, NULL, &jumpTrue, &jumpFalse)) {
        return;
    }

    loadOperand(&relInst->firstOperand, "%eax", frame);
    loadOperand(&relInst->secondOperand, "%edx", frame);
    buildStringf("    cmpl %%edx, %%eax\n");

    const char *jumpMnemonic = (jumpInst->operation == TAC_IF_FALSE) ? jumpFalse : jumpTrue;
    buildStringf("    %s .%s\n", jumpMnemonic, jumpInst->secondOperand.name);
}

static void emitParam(const TACInstruction *instruction, FunctionFrame *frame) {
    if (!ensurePendingParamCapacity(frame, frame->pendingParamCount + 1)) {
        fprintf(stderr,
                "\033[1;31mError interno del generador de codigo objeto:\033[0m"
                " no se pudo reservar memoria para el argumento '%s'.\n",
                instruction->firstOperand.name);
        objectCodeErrors++;
        return;
    }
    int slot = frame->pendingParamSlots[frame->pendingParamCount];

    loadOperand(&instruction->firstOperand, "%eax", frame);
    buildStringf("    movl %%eax, %d(%%rbp)\n", slot);

    frame->pendingParamCount++;
}

static void emitReturn(const TACInstruction *instruction, const FunctionFrame *frame) {
    bool hasReturnOperand = (instruction->firstOperand.kind != OPERAND_NONE);
    if (hasReturnOperand) {
        loadOperand(&instruction->firstOperand, "%eax", frame);
    }
    buildStringf("    jmp .L_%s_epilogue\n", frame->functionName);
}

static void emitCall(const TACInstruction *instruction, FunctionFrame *frame) {
    int argumentCount = (instruction->secondOperand.kind == OPERAND_CONSTANT) ? instruction->secondOperand.value.intValue : 0;
    bool exceedsPending = (argumentCount > frame->pendingParamCount);
    if (exceedsPending) {
        argumentCount = frame->pendingParamCount;
    }

    int base = frame->pendingParamCount - argumentCount;
    int stackArguments = (argumentCount > MAX_ARG_REGISTERS) ? (argumentCount - MAX_ARG_REGISTERS) : 0;
    int registerArguments = (argumentCount > MAX_ARG_REGISTERS) ? MAX_ARG_REGISTERS : argumentCount;

    bool needsAlignmentPadding = (stackArguments % 2 == 1);
    int padding = needsAlignmentPadding ? 8 : 0;
    if (needsAlignmentPadding) {
        buildStringf("    subq $%d, %%rsp\n", padding);
    }

    emitCallStackArguments(frame, base, argumentCount);
    emitCallRegisterArguments(frame, base, registerArguments);

    buildStringf("    call %s\n", instruction->firstOperand.name);

    bool needsStackCleanup = (stackArguments > 0 || padding > 0);
    if (needsStackCleanup) {
        buildStringf("    addq $%d, %%rsp\n", stackArguments * 8 + padding);
    }

    frame->pendingParamCount = base;

    emitCallResult(instruction, frame);
}

static void emitCallStackArguments(const FunctionFrame *frame, int base, int argumentCount) {
    for (int i = argumentCount - 1; i >= MAX_ARG_REGISTERS; i--) {
        buildStringf("    movl %d(%%rbp), %%eax\n", frame->pendingParamSlots[base + i]);
        buildStringf("    pushq %%rax\n");
    }
}

static void emitCallRegisterArguments(const FunctionFrame *frame, int base, int registerArguments) {
    for (int i = 0; i < registerArguments; i++) {
        int slot = frame->pendingParamSlots[base + i];
        buildStringf("    movl %d(%%rbp), %s\n", slot, ARG_REGISTERS_32[i]);
    }
}

static void emitCallResult(const TACInstruction *instruction, const FunctionFrame *frame) {
    bool hasResult = (instruction->result.kind != OPERAND_NONE);
    if (!hasResult) return;

    storeOperand(&instruction->result, "%eax", frame);
}

static void emitBinaryOp(const TACInstruction *instruction, const char *opMnemonic, const FunctionFrame *frame) {
    loadOperand(&instruction->firstOperand, "%eax", frame);
    loadOperand(&instruction->secondOperand, "%edx", frame);
    buildStringf("    %s %%edx, %%eax\n", opMnemonic);
    storeOperand(&instruction->result, "%eax", frame);
}

static bool emitStrengthReducedMultiplication(const TACInstruction *instruction, const FunctionFrame *frame) {
    bool isEnabled = isOptimizationEnabled(activeOptimizations, OPTIMIZATION_STRENGTH);
    if (!isEnabled) return false;

    const TACOperand *first = &instruction->firstOperand;
    const TACOperand *second = &instruction->secondOperand;

    const TACOperand *constantOperand = NULL;
    const TACOperand *valueOperand = NULL;

    if (second->kind == OPERAND_CONSTANT) {
        constantOperand = second;
        valueOperand = first;
    } else if (first->kind == OPERAND_CONSTANT) {
        constantOperand = first;
        valueOperand = second;
    } else {
        return false;
    }

    int multiplier = constantOperand->value.intValue;

    if (multiplier == 0) {
        buildStringf("    xorl %%eax, %%eax\n");
        storeOperand(&instruction->result, "%eax", frame);
        return true;
    }

    if (multiplier == 1) {
        loadOperand(valueOperand, "%eax", frame);
        storeOperand(&instruction->result, "%eax", frame);
        return true;
    }

    int exponent = powerOfTwoExponent(multiplier);
    bool canShift = (exponent > 0);
    if (!canShift) return false;

    loadOperand(valueOperand, "%eax", frame);
    buildStringf("    sall $%d, %%eax\n", exponent);
    storeOperand(&instruction->result, "%eax", frame);

    return true;
}

static int powerOfTwoExponent(int value) {
    bool isPowerOfTwo = (value > 0 && (value & (value - 1)) == 0);
    if (!isPowerOfTwo) return -1;

    int exponent = 0;
    while ((value >> exponent) > 1) {
        exponent++;
    }
    return exponent;
}

static void emitDivisionOp(const TACInstruction *instruction, const FunctionFrame *frame, const char *resultRegisterName) {
    loadOperand(&instruction->firstOperand, "%eax", frame);
    loadOperand(&instruction->secondOperand, "%ecx", frame);

    buildStringf("    cltd\n");
    buildStringf("    idivl %%ecx\n");

    storeOperand(&instruction->result, resultRegisterName, frame);
}

static void emitRelationalOp(const TACInstruction *instruction, const FunctionFrame *frame) {
    const char *conditionMnemonic = NULL;
    if (!getRelationalMnemonics(instruction->operation, &conditionMnemonic, NULL, NULL)) {
        return;
    }

    loadOperand(&instruction->firstOperand, "%eax", frame);
    loadOperand(&instruction->secondOperand, "%edx", frame);
    buildStringf("    cmpl %%edx, %%eax\n");
    buildStringf("    %s %%al\n", conditionMnemonic);
    buildStringf("    movzbl %%al, %%eax\n");
    storeOperand(&instruction->result, "%eax", frame);
}

static void loadOperand(const TACOperand *operand, const char *registerName, const FunctionFrame *frame) {
    if (operand == NULL) return;

    bool isImmediate = (operand->kind == OPERAND_CONSTANT);
    if (isImmediate) {
        buildStringf("    movl $%d, %s\n", operand->value.intValue, registerName);
        return;
    }

    int offset = 0;
    OperandLocation location = resolveOperandLocation(operand, frame, &offset);
    if (location == LOCATION_UNRESOLVED) return;

    if (location == LOCATION_STACK) {
        buildStringf("    movl %d(%%rbp), %s\n", offset, registerName);
    } else {
        buildStringf("    movl %s(%%rip), %s\n", getGlobalName(operand), registerName);
    }
}

static void storeOperand(const TACOperand *operand, const char *registerName, const FunctionFrame *frame) {
    if (operand == NULL) return;

    int offset = 0;
    OperandLocation location = resolveOperandLocation(operand, frame, &offset);
    if (location == LOCATION_UNRESOLVED) return;

    if (location == LOCATION_STACK) {
        buildStringf("    movl %s, %d(%%rbp)\n", registerName, offset);
    } else {
        buildStringf("    movl %s, %s(%%rip)\n", registerName, getGlobalName(operand));
    }
}

static OperandLocation resolveOperandLocation(const TACOperand *operand, const FunctionFrame *frame, int *offset) {
    bool isInStack = getSlotOffset(frame, operand->name, offset);
    if (isInStack) {
        return LOCATION_STACK;
    }

    bool isGlobal = (operand->kind == OPERAND_VARIABLE && isDeclaredGlobal(operand));
    if (isGlobal) {
        return LOCATION_GLOBAL;
    }

    fprintf(stderr,
            "\033[1;31mError interno del generador de codigo objeto:\033[0m el operando '%s'"
            " no tiene lugar asignado en el frame de '%s' ni esta declarado como variable global.\n",
            operand->name[0] != '\0' ? operand->name : "(sin nombre)",
            (frame != NULL && frame->functionName[0] != '\0') ? frame->functionName : "(sin funcion)");
    objectCodeErrors++;

    return LOCATION_UNRESOLVED;
}

static bool isDeclaredGlobal(const TACOperand *operand) {
    return operand != NULL && operand->symbol != NULL && operand->symbol->isGlobal;
}

static const char *getGlobalName(const TACOperand *operand) {
    bool hasSymbolId = (operand->symbol != NULL && operand->symbol->id != NULL);
    if (hasSymbolId) {
        return operand->symbol->id;
    }
    return operand->name;
}

static void emitGlobals(const TACList *tacList) {
    if (tacList == NULL) return;

    bool hasGlobals = false;
    for (const TACInstruction *instruction = tacList->head; instruction != NULL; instruction = instruction->next) {
        if (instruction->operation == TAC_GLOBAL_DECL) {
            if (!hasGlobals) {
                buildStringf(".data\n");
                hasGlobals = true;
            }

            const char *name = getGlobalName(&instruction->result);
            buildStringf(".globl %s\n", name);
            buildStringf(".align 4\n");
            buildStringf("%s:\n", name);
            buildStringf("    .long 0\n");
        }
    }

    if (hasGlobals) {
        buildStringf("\n");
    }
}
