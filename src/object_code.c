#include "object_code.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "asm_instruction.h"
#include "string_builder.h"

#define MAX_ARG_REGISTERS 6
#define MAX_XMM_ARG_REGISTERS 8

static const char *ARG_REGISTERS_32[] = {
    "%edi", "%esi", "%edx", "%ecx", "%r8d", "%r9d"
};

static const char *XMM_ARG_REGISTERS[] = {
    "%xmm0", "%xmm1", "%xmm2", "%xmm3", "%xmm4", "%xmm5", "%xmm6", "%xmm7"
};

typedef struct {
    float value;
    char label[16];
} FloatConstant;

typedef enum {
    LOCATION_UNRESOLVED,
    LOCATION_STACK,
    LOCATION_GLOBAL
} OperandLocation;

static char **globalVariableNames = NULL;
static int globalVariableCount = 0;
static int globalVariableCapacity = 0;
static int objectCodeErrors = 0;

// Optimizaciones activas en la corrida emitidas en Assembly
static OptimizationSet activeOptimizations = OPTIMIZATION_NONE;

static FloatConstant *floatConstants = NULL;
static int floatConstantCount = 0;
static int floatConstantCapacity = 0;

// Prototipos de submetodos para aplicar Step-Down Rule
static bool buildFrameLayout(FunctionFrame *frame, const TACInstruction *functionStart);
static void allocateLocalSlots(FunctionFrame *frame, const TACInstruction *functionStart);
static bool allocatePendingParamSlots(FunctionFrame *frame, const TACInstruction *functionStart);
static const TACInstruction *emitFunction(FunctionFrame *frame, const TACInstruction *functionStart);

static void emitParamDeclaration(const TACInstruction *instruction, FunctionFrame *frame);
static void emitAssignment(const TACInstruction *instruction, const FunctionFrame *frame);
static void emitAddition(const TACInstruction *instruction, const FunctionFrame *frame);
static void emitSubtraction(const TACInstruction *instruction, const FunctionFrame *frame);
static void emitMultiplication(const TACInstruction *instruction, const FunctionFrame *frame);
static void emitDivision(const TACInstruction *instruction, const FunctionFrame *frame);
static void emitModulo(const TACInstruction *instruction, const FunctionFrame *frame);
static void emitUnaryMinus(const TACInstruction *instruction, const FunctionFrame *frame);
static void emitNot(const TACInstruction *instruction, const FunctionFrame *frame);
static void emitLessThan(const TACInstruction *instruction, const FunctionFrame *frame);
static void emitGreaterThan(const TACInstruction *instruction, const FunctionFrame *frame);
static void emitEquality(const TACInstruction *instruction, const FunctionFrame *frame);
static void emitIntToFloat(const TACInstruction *instruction, const FunctionFrame *frame);
static void emitLabel(const TACInstruction *instruction);
static void emitGoto(const TACInstruction *instruction);
static void emitIfFalse(const TACInstruction *instruction, const FunctionFrame *frame);
static void emitIfTrue(const TACInstruction *instruction, const FunctionFrame *frame);
static void emitParam(const TACInstruction *instruction, FunctionFrame *frame);
static void emitCall(const TACInstruction *instruction, FunctionFrame *frame);
static void emitReturn(const TACInstruction *instruction, const FunctionFrame *frame);

static void classifyCallArguments(const FunctionFrame *frame, int base, int argumentCount,
                                  int *registerIndex, int *goesToStack, int *outStackArguments);
static void emitCallStackArguments(const FunctionFrame *frame, int base, int argumentCount,
                                   const int *goesToStack);
static void emitCallRegisterArguments(const FunctionFrame *frame, int base, int argumentCount,
                                      const int *registerIndex, const int *goesToStack);
static void emitCallResult(const TACInstruction *instruction, const FunctionFrame *frame);

static void emitBinaryOp(const TACInstruction *instruction, AsmOpcode opcode, const FunctionFrame *frame);
static bool emitStrengthReducedMultiplication(const TACInstruction *instruction, const FunctionFrame *frame);
static int powerOfTwoExponent(int value);
static void emitFloatBinaryOp(const TACInstruction *instruction, const char *mnemonic, const FunctionFrame *frame);
static void emitDivisionOp(const TACInstruction *instruction, const FunctionFrame *frame, const char *resultRegisterName);
static void emitRelationalOp(const TACInstruction *instruction, const char *conditionMnemonic, const FunctionFrame *frame);
static void emitFloatRelationalOp(const TACInstruction *instruction, const char *conditionMnemonic, const FunctionFrame *frame);

static void loadOperand(const TACOperand *operand, const char *registerName, const FunctionFrame *frame);
static void storeOperand(const TACOperand *operand, const char *registerName, const FunctionFrame *frame);
static void loadFloatOperand(const TACOperand *operand, const char *xmmRegister, const FunctionFrame *frame);
static void storeFloatOperand(const TACOperand *operand, const char *xmmRegister, const FunctionFrame *frame);

static OperandLocation resolveOperandLocation(const TACOperand *operand, const FunctionFrame *frame, int *offset);
static bool isDeclaredGlobal(const TACOperand *operand);
static void registerGlobalVariable(const TACOperand *operand);
static const char *getGlobalName(const TACOperand *operand);
static bool isFloatOperand(const TACOperand *operand);
static const char *internFloatConstant(float value);
static unsigned int getFloatBits(float value);

static void resetGlobalVariables(void);
static void resetFloatConstants(void);
static void emitFloatConstantPool(const TACList *tacList);
static void emitGlobals(const TACList *tacList);

void generateAssembly(const TACList *tacList, OptimizationSet enabled) {
    bool isNullList = (tacList == NULL);
    if (isNullList) return;

    resetGlobalVariables();
    resetFloatConstants();
    objectCodeErrors = 0;
    activeOptimizations = enabled;

    emitFloatConstantPool(tacList);
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

    // Seccion note.GNU-stack para evitar stack ejecutable
    buildStringf("\n.section .note.GNU-stack,\"\",@progbits\n");

    resetGlobalVariables();
    resetFloatConstants();
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

        switch (current->operation) {
            case TAC_PARAM_DECL:
            case TAC_LOCAL_DECL:
                addSlot(frame, &current->firstOperand);
                break;

            case TAC_ASSIGN:
            case TAC_ADD:
            case TAC_SUB:
            case TAC_MUL:
            case TAC_DIV:
            case TAC_MOD:
            case TAC_UMINUS:
            case TAC_NOT:
            case TAC_INT_TO_FLOAT:
            case TAC_LT:
            case TAC_GT:
            case TAC_EQ:
            case TAC_CALL: {
                bool isTemporary = (current->result.kind == OPERAND_TEMP);
                if (isTemporary) {
                    addSlot(frame, &current->result);
                }
                break;
            }

            default:
                break;
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
        generateInstruction(current, frame);
        current = current->next;
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
        AsmInstruction zero = asmLoadImm(ASM_RAX, 0);
        emitAsm(&zero);
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
            emitAddition(instruction, frame);
            break;

        case TAC_SUB:
            emitSubtraction(instruction, frame);
            break;

        case TAC_MUL:
            emitMultiplication(instruction, frame);
            break;

        case TAC_DIV:
            emitDivision(instruction, frame);
            break;

        case TAC_MOD:
            emitModulo(instruction, frame);
            break;

        case TAC_UMINUS:
            emitUnaryMinus(instruction, frame);
            break;

        case TAC_NOT:
            emitNot(instruction, frame);
            break;

        case TAC_LT:
            emitLessThan(instruction, frame);
            break;

        case TAC_GT:
            emitGreaterThan(instruction, frame);
            break;

        case TAC_EQ:
            emitEquality(instruction, frame);
            break;

        case TAC_INT_TO_FLOAT:
            emitIntToFloat(instruction, frame);
            break;

        case TAC_LABEL:
            emitLabel(instruction);
            break;

        case TAC_GOTO:
            emitGoto(instruction);
            break;

        case TAC_IF_FALSE:
            emitIfFalse(instruction, frame);
            break;

        case TAC_IF_TRUE:
            emitIfTrue(instruction, frame);
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

// Submetodos de emision por cuadupla TAC

static void emitParamDeclaration(const TACInstruction *instruction, FunctionFrame *frame) {
    int offset = 0;
    bool hasOffset = getSlotOffset(frame, instruction->firstOperand.name, &offset);
    bool isFloat = isFloatOperand(&instruction->firstOperand);

    if (hasOffset) {
        bool fitsInXmm = (isFloat && frame->floatParamCount < MAX_XMM_ARG_REGISTERS);
        bool fitsInGeneralRegister = (!isFloat && frame->integerParamCount < MAX_ARG_REGISTERS);

        if (fitsInXmm) {
            buildStringf("    movss %s, %d(%%rbp)\n",
                         XMM_ARG_REGISTERS[frame->floatParamCount], offset);
        } else if (fitsInGeneralRegister) {
            buildStringf("    movl %s, %d(%%rbp)\n",
                         ARG_REGISTERS_32[frame->integerParamCount], offset);
        } else {
            // Se agotaron los registros: el llamador lo ubico en su stack
            int callerOffset = 16 + frame->stackParamCount * 8;
            buildStringf("    movl %d(%%rbp), %%eax\n", callerOffset);
            buildStringf("    movl %%eax, %d(%%rbp)\n", offset);
        }
    }

    if (isFloat && frame->floatParamCount < MAX_XMM_ARG_REGISTERS) {
        frame->floatParamCount++;
    } else if (!isFloat && frame->integerParamCount < MAX_ARG_REGISTERS) {
        frame->integerParamCount++;
    } else {
        frame->stackParamCount++;
    }
}

static void emitAssignment(const TACInstruction *instruction, const FunctionFrame *frame) {
    bool isFloat = isFloatOperand(&instruction->result);
    if (isFloat) {
        loadFloatOperand(&instruction->firstOperand, "%xmm0", frame);
        storeFloatOperand(&instruction->result, "%xmm0", frame);
    } else {
        loadOperand(&instruction->firstOperand, "%eax", frame);
        storeOperand(&instruction->result, "%eax", frame);
    }
}

static void emitAddition(const TACInstruction *instruction, const FunctionFrame *frame) {
    bool isFloat = isFloatOperand(&instruction->result);
    if (isFloat) {
        emitFloatBinaryOp(instruction, "addss", frame);
    } else {
        emitBinaryOp(instruction, ASM_ADD, frame);
    }
}

static void emitSubtraction(const TACInstruction *instruction, const FunctionFrame *frame) {
    bool isFloat = isFloatOperand(&instruction->result);
    if (isFloat) {
        emitFloatBinaryOp(instruction, "subss", frame);
    } else {
        emitBinaryOp(instruction, ASM_SUB, frame);
    }
}

static void emitMultiplication(const TACInstruction *instruction, const FunctionFrame *frame) {
    bool isFloat = isFloatOperand(&instruction->result);
    if (isFloat) {
        emitFloatBinaryOp(instruction, "mulss", frame);
        return;
    }

    bool wasReduced = emitStrengthReducedMultiplication(instruction, frame);
    if (!wasReduced) {
        emitBinaryOp(instruction, ASM_IMUL, frame);
    }
}

static void emitDivision(const TACInstruction *instruction, const FunctionFrame *frame) {
    bool isFloat = isFloatOperand(&instruction->result);
    if (isFloat) {
        emitFloatBinaryOp(instruction, "divss", frame);
    } else {
        emitDivisionOp(instruction, frame, "%eax");
    }
}

static void emitModulo(const TACInstruction *instruction, const FunctionFrame *frame) {
    emitDivisionOp(instruction, frame, "%edx");
}

static void emitUnaryMinus(const TACInstruction *instruction, const FunctionFrame *frame) {
    bool isFloat = isFloatOperand(&instruction->result);
    if (isFloat) {
        buildStringf("    pxor %%xmm0, %%xmm0\n");
        loadFloatOperand(&instruction->firstOperand, "%xmm1", frame);
        buildStringf("    subss %%xmm1, %%xmm0\n");
        storeFloatOperand(&instruction->result, "%xmm0", frame);
        return;
    }
    loadOperand(&instruction->firstOperand, "%eax", frame);
    AsmInstruction negate = asmOpUnary(ASM_NEG, ASM_RAX);
    emitAsm(&negate);
    storeOperand(&instruction->result, "%eax", frame);
}

static void emitNot(const TACInstruction *instruction, const FunctionFrame *frame) {
    loadOperand(&instruction->firstOperand, "%eax", frame);
    AsmInstruction instructionAsm = asmOpImm(ASM_XOR, ASM_RAX, 1);
    emitAsm(&instructionAsm);
    storeOperand(&instruction->result, "%eax", frame);
}

static void emitLessThan(const TACInstruction *instruction, const FunctionFrame *frame) {
    bool isFloat = isFloatOperand(&instruction->firstOperand);
    if (isFloat) {
        emitFloatRelationalOp(instruction, "setb", frame);
    } else {
        emitRelationalOp(instruction, "setl", frame);
    }
}

static void emitGreaterThan(const TACInstruction *instruction, const FunctionFrame *frame) {
    bool isFloat = isFloatOperand(&instruction->firstOperand);
    if (isFloat) {
        emitFloatRelationalOp(instruction, "seta", frame);
    } else {
        emitRelationalOp(instruction, "setg", frame);
    }
}

static void emitEquality(const TACInstruction *instruction, const FunctionFrame *frame) {
    bool isFloat = isFloatOperand(&instruction->firstOperand);
    if (isFloat) {
        emitFloatRelationalOp(instruction, "sete", frame);
    } else {
        emitRelationalOp(instruction, "sete", frame);
    }
}

static void emitIntToFloat(const TACInstruction *instruction, const FunctionFrame *frame) {
    loadOperand(&instruction->firstOperand, "%eax", frame);
    buildStringf("    cvtsi2ss %%eax, %%xmm0\n");
    storeFloatOperand(&instruction->result, "%xmm0", frame);
}

static void emitLabel(const TACInstruction *instruction) {
    buildStringf(".%s:\n", instruction->result.name);
}

static void emitGoto(const TACInstruction *instruction) {
    buildStringf("    jmp .%s\n", instruction->firstOperand.name);
}

static void emitIfFalse(const TACInstruction *instruction, const FunctionFrame *frame) {
    loadOperand(&instruction->firstOperand, "%eax", frame);
    buildStringf("    cmpl $0, %%eax\n");
    buildStringf("    je .%s\n", instruction->secondOperand.name);
}

static void emitIfTrue(const TACInstruction *instruction, const FunctionFrame *frame) {
    loadOperand(&instruction->firstOperand, "%eax", frame);
    buildStringf("    cmpl $0, %%eax\n");
    buildStringf("    jne .%s\n", instruction->secondOperand.name);
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

    bool isFloat = isFloatOperand(&instruction->firstOperand);
    if (isFloat) {
        loadFloatOperand(&instruction->firstOperand, "%xmm0", frame);
        buildStringf("    movss %%xmm0, %d(%%rbp)\n", slot);
    } else {
        loadOperand(&instruction->firstOperand, "%eax", frame);
        AsmInstruction store = asmStore(opMem(slot), opReg(ASM_RAX));
        emitAsm(&store);
    }

    frame->pendingParamTypes[frame->pendingParamCount] = instruction->firstOperand.type;
    frame->pendingParamCount++;
}

static void emitReturn(const TACInstruction *instruction, const FunctionFrame *frame) {
    bool hasReturnOperand = (instruction->firstOperand.kind != OPERAND_NONE);
    if (hasReturnOperand) {
        bool isFloat = isFloatOperand(&instruction->firstOperand);
        if (isFloat) {
            loadFloatOperand(&instruction->firstOperand, "%xmm0", frame);
        } else {
            loadOperand(&instruction->firstOperand, "%eax", frame);
        }
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
    int *registerIndex = NULL;
    int *goesToStack = NULL;
    int stackArguments = 0;

    if (argumentCount > 0) {
        registerIndex = (int *)malloc(sizeof(int) * (size_t)argumentCount);
        goesToStack = (int *)malloc(sizeof(int) * (size_t)argumentCount);
        if (registerIndex == NULL || goesToStack == NULL) {
            free(registerIndex);
            free(goesToStack);
            fprintf(stderr,
                    "\033[1;31mError interno del generador de codigo objeto:\033[0m"
                    " no se pudo reservar memoria para clasificar los argumentos de '%s'.\n",
                    instruction->firstOperand.name);
            objectCodeErrors++;
            return;
        }
        classifyCallArguments(frame, base, argumentCount, registerIndex, goesToStack, &stackArguments);
    }

    bool needsAlignmentPadding = (stackArguments % 2 == 1);
    int padding = needsAlignmentPadding ? 8 : 0;
    if (needsAlignmentPadding) {
        buildStringf("    subq $%d, %%rsp\n", padding);
    }

    emitCallStackArguments(frame, base, argumentCount, goesToStack);
    emitCallRegisterArguments(frame, base, argumentCount, registerIndex, goesToStack);

    buildStringf("    call %s\n", instruction->firstOperand.name);

    bool needsStackCleanup = (stackArguments > 0 || padding > 0);
    if (needsStackCleanup) {
        buildStringf("    addq $%d, %%rsp\n", stackArguments * 8 + padding);
    }

    frame->pendingParamCount = base;

    if (registerIndex != NULL) free(registerIndex);
    if (goesToStack != NULL) free(goesToStack);

    emitCallResult(instruction, frame);
}

static void classifyCallArguments(const FunctionFrame *frame, int base, int argumentCount,
                                  int *registerIndex, int *goesToStack, int *outStackArguments) {
    int integerIndex = 0;
    int floatIndex = 0;
    int stackArguments = 0;

    for (int i = 0; i < argumentCount; i++) {
        bool isFloat = (frame->pendingParamTypes[base + i] == TYPE_FLOAT);
        if (isFloat) {
            goesToStack[i] = (floatIndex >= MAX_XMM_ARG_REGISTERS);
            registerIndex[i] = goesToStack[i] ? -1 : floatIndex++;
        } else {
            goesToStack[i] = (integerIndex >= MAX_ARG_REGISTERS);
            registerIndex[i] = goesToStack[i] ? -1 : integerIndex++;
        }
        if (goesToStack[i]) {
            stackArguments++;
        }
    }

    if (outStackArguments != NULL) {
        *outStackArguments = stackArguments;
    }
}

static void emitCallStackArguments(const FunctionFrame *frame, int base, int argumentCount,
                                   const int *goesToStack) {
    if (goesToStack == NULL) return;

    for (int i = argumentCount - 1; i >= 0; i--) {
        if (!goesToStack[i]) continue;

        AsmInstruction load = asmLoad(opReg(ASM_RAX), opMem(frame->pendingParamSlots[base + i]));
        emitAsm(&load);
        buildStringf("    pushq %%rax\n");
    }
}

static void emitCallRegisterArguments(const FunctionFrame *frame, int base, int argumentCount,
                                      const int *registerIndex, const int *goesToStack) {
    if (registerIndex == NULL || goesToStack == NULL) return;

    for (int i = 0; i < argumentCount; i++) {
        if (goesToStack[i]) continue;

        int slot = frame->pendingParamSlots[base + i];
        bool isFloat = (frame->pendingParamTypes[base + i] == TYPE_FLOAT);
        if (isFloat) {
            buildStringf("    movss %d(%%rbp), %s\n", slot, XMM_ARG_REGISTERS[registerIndex[i]]);
        } else {
            AsmInstruction load = asmLoad(opRegStr(ARG_REGISTERS_32[registerIndex[i]]), opMem(slot));
            emitAsm(&load);
        }
    }
}

static void emitCallResult(const TACInstruction *instruction, const FunctionFrame *frame) {
    bool hasResult = (instruction->result.kind != OPERAND_NONE);
    if (!hasResult) return;

    bool isFloat = isFloatOperand(&instruction->result);
    if (isFloat) {
        storeFloatOperand(&instruction->result, "%xmm0", frame);
    } else {
        storeOperand(&instruction->result, "%eax", frame);
    }
}

static void emitBinaryOp(const TACInstruction *instruction, AsmOpcode opcode, const FunctionFrame *frame) {
    loadOperand(&instruction->firstOperand, "%eax", frame);
    loadOperand(&instruction->secondOperand, "%edx", frame);
    AsmInstruction asmInstruction = asmOpReg(opcode, ASM_RAX, ASM_RDX);
    emitAsm(&asmInstruction);
    storeOperand(&instruction->result, "%eax", frame);
}

static bool emitStrengthReducedMultiplication(const TACInstruction *instruction, const FunctionFrame *frame) {
    bool isEnabled = isOptimizationEnabled(activeOptimizations, OPTIMIZATION_STRENGTH);
    if (!isEnabled) return false;

    const TACOperand *first = &instruction->firstOperand;
    const TACOperand *second = &instruction->secondOperand;

    const TACOperand *constantOperand = NULL;
    const TACOperand *valueOperand = NULL;

    if (second->kind == OPERAND_CONSTANT && second->type != TYPE_FLOAT) {
        constantOperand = second;
        valueOperand = first;
    } else if (first->kind == OPERAND_CONSTANT && first->type != TYPE_FLOAT) {
        constantOperand = first;
        valueOperand = second;
    } else {
        return false;
    }

    int multiplier = constantOperand->value.intValue;

    if (multiplier == 0) {
        AsmInstruction clear = asmOpReg(ASM_XOR, ASM_RAX, ASM_RAX);
        emitAsm(&clear);
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

static void emitFloatBinaryOp(const TACInstruction *instruction, const char *mnemonic, const FunctionFrame *frame) {
    loadFloatOperand(&instruction->firstOperand, "%xmm0", frame);
    loadFloatOperand(&instruction->secondOperand, "%xmm1", frame);
    buildStringf("    %s %%xmm1, %%xmm0\n", mnemonic);
    storeFloatOperand(&instruction->result, "%xmm0", frame);
}

static void emitDivisionOp(const TACInstruction *instruction, const FunctionFrame *frame, const char *resultRegisterName) {
    loadOperand(&instruction->firstOperand, "%eax", frame);
    loadOperand(&instruction->secondOperand, "%ecx", frame);

    AsmInstruction signExtend = asmCltd();
    emitAsm(&signExtend);

    AsmInstruction divide = asmOpUnary(ASM_IDIV, ASM_RCX);
    emitAsm(&divide);

    storeOperand(&instruction->result, resultRegisterName, frame);
}

static void emitRelationalOp(const TACInstruction *instruction, const char *conditionMnemonic, const FunctionFrame *frame) {
    loadOperand(&instruction->firstOperand, "%eax", frame);
    loadOperand(&instruction->secondOperand, "%edx", frame);
    AsmInstruction cmpInstruction = asmOpReg(ASM_CMP, ASM_RAX, ASM_RDX);
    emitAsm(&cmpInstruction);
    buildStringf("    %s %%al\n", conditionMnemonic);
    buildStringf("    movzbl %%al, %%eax\n");
    storeOperand(&instruction->result, "%eax", frame);
}

static void emitFloatRelationalOp(const TACInstruction *instruction, const char *conditionMnemonic, const FunctionFrame *frame) {
    loadFloatOperand(&instruction->firstOperand, "%xmm0", frame);
    loadFloatOperand(&instruction->secondOperand, "%xmm1", frame);
    buildStringf("    comiss %%xmm1, %%xmm0\n");
    buildStringf("    %s %%al\n", conditionMnemonic);

    // En IEEE-754 las comparaciones con NaN son falsas; setnp filtra el flag de paridad
    bool requiresUnorderedCheck = (strcmp(conditionMnemonic, "sete") == 0 || strcmp(conditionMnemonic, "setb") == 0);
    if (requiresUnorderedCheck) {
        buildStringf("    setnp %%dl\n");
        buildStringf("    andb %%dl, %%al\n");
    }

    buildStringf("    movzbl %%al, %%eax\n");
    storeOperand(&instruction->result, "%eax", frame);
}

static void loadOperand(const TACOperand *operand, const char *registerName, const FunctionFrame *frame) {
    if (operand == NULL) return;

    bool isImmediate = (operand->kind == OPERAND_CONSTANT);
    if (isImmediate) {
        AsmInstruction immediate = asmLoad(opRegStr(registerName), opImm(operand->value.intValue));
        emitAsm(&immediate);
        return;
    }

    int offset = 0;
    OperandLocation location = resolveOperandLocation(operand, frame, &offset);
    if (location == LOCATION_UNRESOLVED) return;

    AsmInstruction instruction = (location == LOCATION_STACK)
        ? asmLoad(opRegStr(registerName), opMem(offset))
        : asmLoad(opRegStr(registerName), opGlobal(getGlobalName(operand)));
    emitAsm(&instruction);
}

static void storeOperand(const TACOperand *operand, const char *registerName, const FunctionFrame *frame) {
    if (operand == NULL) return;

    int offset = 0;
    OperandLocation location = resolveOperandLocation(operand, frame, &offset);
    if (location == LOCATION_UNRESOLVED) return;

    AsmInstruction instruction = (location == LOCATION_STACK)
        ? asmStore(opMem(offset), opRegStr(registerName))
        : asmStore(opGlobal(getGlobalName(operand)), opRegStr(registerName));
    emitAsm(&instruction);
}

static void loadFloatOperand(const TACOperand *operand, const char *xmmRegister, const FunctionFrame *frame) {
    if (operand == NULL) return;

    bool isImmediate = (operand->kind == OPERAND_CONSTANT);
    if (isImmediate) {
        buildStringf("    movss %s(%%rip), %s\n", internFloatConstant(operand->value.floatValue), xmmRegister);
        return;
    }

    int offset = 0;
    OperandLocation location = resolveOperandLocation(operand, frame, &offset);
    if (location == LOCATION_UNRESOLVED) return;

    if (location == LOCATION_STACK) {
        buildStringf("    movss %d(%%rbp), %s\n", offset, xmmRegister);
    } else {
        buildStringf("    movss %s(%%rip), %s\n", getGlobalName(operand), xmmRegister);
    }
}

static void storeFloatOperand(const TACOperand *operand, const char *xmmRegister, const FunctionFrame *frame) {
    if (operand == NULL) return;

    int offset = 0;
    OperandLocation location = resolveOperandLocation(operand, frame, &offset);
    if (location == LOCATION_UNRESOLVED) return;

    if (location == LOCATION_STACK) {
        buildStringf("    movss %s, %d(%%rbp)\n", xmmRegister, offset);
    } else {
        buildStringf("    movss %s, %s(%%rip)\n", xmmRegister, getGlobalName(operand));
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

static void resetGlobalVariables(void) {
    if (globalVariableNames != NULL) {
        for (int i = 0; i < globalVariableCount; i++) {
            if (globalVariableNames[i] != NULL) {
                free(globalVariableNames[i]);
            }
        }
        free(globalVariableNames);
        globalVariableNames = NULL;
    }
    globalVariableCount = 0;
    globalVariableCapacity = 0;
}

static void resetFloatConstants(void) {
    if (floatConstants != NULL) {
        free(floatConstants);
        floatConstants = NULL;
    }
    floatConstantCount = 0;
    floatConstantCapacity = 0;
}

static bool isDeclaredGlobal(const TACOperand *operand) {
    if (operand == NULL || operand->name[0] == '\0' || globalVariableNames == NULL) {
        return false;
    }
    for (int i = 0; i < globalVariableCount; i++) {
        if (globalVariableNames[i] != NULL && strcmp(globalVariableNames[i], operand->name) == 0) {
            return true;
        }
    }
    return false;
}

static void registerGlobalVariable(const TACOperand *operand) {
    if (operand == NULL || operand->name[0] == '\0') return;

    if (globalVariableCount >= globalVariableCapacity) {
        int newCapacity = (globalVariableCapacity == 0) ? 64 : globalVariableCapacity * 2;
        char **newNames = (char **)realloc(globalVariableNames, sizeof(char *) * (size_t)newCapacity);
        if (newNames == NULL) return;
        globalVariableNames = newNames;
        globalVariableCapacity = newCapacity;
    }

    globalVariableNames[globalVariableCount] = (char *)malloc(TAC_NAME_LENGTH);
    if (globalVariableNames[globalVariableCount] == NULL) return;

    strncpy(globalVariableNames[globalVariableCount], operand->name, TAC_NAME_LENGTH - 1);
    globalVariableNames[globalVariableCount][TAC_NAME_LENGTH - 1] = '\0';
    globalVariableCount++;
}

static const char *getGlobalName(const TACOperand *operand) {
    bool hasSymbolId = (operand->symbol != NULL && operand->symbol->id != NULL);
    if (hasSymbolId) {
        return operand->symbol->id;
    }
    return operand->name;
}

static bool isFloatOperand(const TACOperand *operand) {
    return operand != NULL && operand->type == TYPE_FLOAT;
}

static const char *internFloatConstant(float value) {
    for (int i = 0; i < floatConstantCount; i++) {
        bool hasIdenticalBits = (getFloatBits(floatConstants[i].value) == getFloatBits(value));
        if (hasIdenticalBits) {
            return floatConstants[i].label;
        }
    }

    if (floatConstantCount >= floatConstantCapacity) {
        int newCapacity = (floatConstantCapacity == 0) ? 64 : floatConstantCapacity * 2;
        FloatConstant *newArray = (FloatConstant *)realloc(floatConstants, sizeof(FloatConstant) * (size_t)newCapacity);
        if (newArray == NULL) {
            return (floatConstantCount > 0) ? floatConstants[0].label : "";
        }
        floatConstants = newArray;
        floatConstantCapacity = newCapacity;
    }

    FloatConstant *constant = &floatConstants[floatConstantCount];
    constant->value = value;
    snprintf(constant->label, sizeof(constant->label), ".LC%d", floatConstantCount);
    floatConstantCount++;

    return constant->label;
}

static unsigned int getFloatBits(float value) {
    union { float asFloat; unsigned int asBits; } conversion;
    conversion.asFloat = value;
    return conversion.asBits;
}

static void emitFloatConstantPool(const TACList *tacList) {
    floatConstantCount = 0;

    for (const TACInstruction *instruction = tacList->head; instruction != NULL; instruction = instruction->next) {
        const TACOperand *operands[] = { &instruction->firstOperand, &instruction->secondOperand, &instruction->result };

        for (int i = 0; i < 3; i++) {
            bool isFloatConstant = (operands[i]->kind == OPERAND_CONSTANT && operands[i]->type == TYPE_FLOAT);
            if (isFloatConstant) {
                internFloatConstant(operands[i]->value.floatValue);
            }
        }
    }

    bool hasNoConstants = (floatConstantCount == 0);
    if (hasNoConstants) return;

    buildStringf(".section .rodata\n");
    for (int i = 0; i < floatConstantCount; i++) {
        buildStringf(".align 4\n");
        buildStringf("%s:\n", floatConstants[i].label);
        buildStringf("    .long %u\n", getFloatBits(floatConstants[i].value));
    }
    buildStringf("\n");
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
            registerGlobalVariable(&instruction->firstOperand);

            const char *name = getGlobalName(&instruction->firstOperand);
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
