#include "object_code.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "asm_instruction.h"
#include "string_builder.h"

#define MAX_GLOBAL_VARIABLES 256
#define MAX_FLOAT_CONSTANTS 256
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

static char globalVariableNames[MAX_GLOBAL_VARIABLES][TAC_NAME_LENGTH];
static int globalVariableCount = 0;
static int objectCodeErrors = 0;

static FloatConstant floatConstants[MAX_FLOAT_CONSTANTS];
static int floatConstantCount = 0;

static void emitFloatConstantPool(const TACList *tacList);
static void emitGlobals(const TACList *tacList);
static void emitParam(const TACInstruction *instruction, FunctionFrame *frame);
static void emitCall(const TACInstruction *instruction, FunctionFrame *frame);
static void emitBinaryOp(const TACInstruction *instruction, AsmOpcode opcode, const FunctionFrame *frame);
static void emitFloatBinaryOp(const TACInstruction *instruction, const char *mnemonic, const FunctionFrame *frame);
static void emitDivisionOp(const TACInstruction *instruction, const FunctionFrame *frame, AsmRegister resultRegister);
static void emitRelationalOp(const TACInstruction *instruction, const char *conditionMnemonic, const FunctionFrame *frame);
static void emitFloatRelationalOp(const TACInstruction *instruction, const char *conditionMnemonic, const FunctionFrame *frame);
static void loadRegisterBasedOnOperand(AsmRegister registerType, const TACOperand *operand, const FunctionFrame *frame);
static void storeRegisterToOperand(const TACOperand *operand, AsmRegister registerType, const FunctionFrame *frame);
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
static const char *getRegisterName(AsmRegister registerType);

void generateAssembly(const TACList *tacList) {
    bool isNullList = (tacList == NULL);
    if (isNullList) return;

    globalVariableCount = 0;
    objectCodeErrors = 0;

    emitFloatConstantPool(tacList);
    emitGlobals(tacList);

    buildStringf(".text\n");

    const TACInstruction *instruction = tacList->head;

    while (instruction != NULL) {
        bool isFunctionStart = (instruction->operation == TAC_BEGIN_FUNC);
        if (isFunctionStart) {
            FunctionFrame *frame = createFrame(instruction->result.name);
            frame->returnType = instruction->result.type;
            const TACInstruction *current = instruction->next;

            while (current != NULL && current->operation != TAC_END_FUNC) {
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
                    case TAC_NEG:
                    case TAC_NOT:
                    case TAC_INT_TO_FLOAT:
                    case TAC_LT:
                    case TAC_GT:
                    case TAC_EQ:
                    case TAC_AND:
                    case TAC_OR:
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

                current = current->next;
            }

            int pendingCount = 0;
            for (const TACInstruction *scan = instruction->next;
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

            bool exceedsLimit = (frame->maxPendingParams > MAX_PENDING_PARAMS);
            if (exceedsLimit) {
                frame->maxPendingParams = MAX_PENDING_PARAMS;
            }
            for (int i = 0; i < frame->maxPendingParams; i++) {
                frame->pendingParamSlots[i] = addAnonymousSlot(frame);
            }

            frame->frameSize = ALIGN_UP(frame->nextOffset, 16);

            emitPrologue(frame);

            current = instruction->next;
            while (current != NULL && current->operation != TAC_END_FUNC) {
                generateInstruction(current, frame);
                current = current->next;
            }

            emitEpilogue(frame);
            freeFrame(frame);

            if (current != NULL) {
                instruction = current;
            } else {
                break;
            }
        }

        instruction = instruction->next;
    }

    /* Sin esta sección el enlazador asume que el stack debe ser ejecutable. */
    buildStringf("\n.section .note.GNU-stack,\"\",@progbits\n");
}

int getObjectCodeErrors(void) {
    return objectCodeErrors;
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

    /* El sistema operativo toma %eax como código de salida del proceso en void main(). */
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
        case TAC_PARAM_DECL: {
            int offset = getSlotOffset(frame, instruction->firstOperand.name);
            bool isFloat = isFloatOperand(&instruction->firstOperand);

            bool hasOffset = (offset != 0);
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
                    /* Se agotaron los registros: el llamador lo dejó en su stack. */
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

            frame->paramCount++;
            break;
        }

        case TAC_LOCAL_DECL:
            break;

        case TAC_ASSIGN: {
            bool isFloat = isFloatOperand(&instruction->result);
            if (isFloat) {
                loadFloatOperand(&instruction->firstOperand, "%xmm0", frame);
                storeFloatOperand(&instruction->result, "%xmm0", frame);
            } else {
                loadRegisterBasedOnOperand(ASM_RAX, &instruction->firstOperand, frame);
                storeRegisterToOperand(&instruction->result, ASM_RAX, frame);
            }
            break;
        }

        case TAC_ADD: {
            bool isFloat = isFloatOperand(&instruction->result);
            if (isFloat) {
                emitFloatBinaryOp(instruction, "addss", frame);
            } else {
                emitBinaryOp(instruction, ASM_ADD, frame);
            }
            break;
        }

        case TAC_SUB: {
            bool isFloat = isFloatOperand(&instruction->result);
            if (isFloat) {
                emitFloatBinaryOp(instruction, "subss", frame);
            } else {
                emitBinaryOp(instruction, ASM_SUB, frame);
            }
            break;
        }

        case TAC_MUL: {
            bool isFloat = isFloatOperand(&instruction->result);
            if (isFloat) {
                emitFloatBinaryOp(instruction, "mulss", frame);
            } else {
                emitBinaryOp(instruction, ASM_IMUL, frame);
            }
            break;
        }

        case TAC_DIV: {
            bool isFloat = isFloatOperand(&instruction->result);
            if (isFloat) {
                emitFloatBinaryOp(instruction, "divss", frame);
            } else {
                emitDivisionOp(instruction, frame, ASM_RAX);
            }
            break;
        }

        case TAC_MOD:
            emitDivisionOp(instruction, frame, ASM_RDX);
            break;

        case TAC_NEG: {
            bool isFloat = isFloatOperand(&instruction->result);
            if (isFloat) {
                buildStringf("    pxor %%xmm0, %%xmm0\n");
                loadFloatOperand(&instruction->firstOperand, "%xmm1", frame);
                buildStringf("    subss %%xmm1, %%xmm0\n");
                storeFloatOperand(&instruction->result, "%xmm0", frame);
                break;
            }
            loadRegisterBasedOnOperand(ASM_RAX, &instruction->firstOperand, frame);
            AsmInstruction negate = asmOpUnary(ASM_NEG, ASM_RAX);
            emitAsm(&negate);
            storeRegisterToOperand(&instruction->result, ASM_RAX, frame);
            break;
        }

        case TAC_NOT: {
            loadRegisterBasedOnOperand(ASM_RAX, &instruction->firstOperand, frame);
            AsmInstruction instructionAsm = asmOpImm(ASM_XOR, ASM_RAX, 1);
            emitAsm(&instructionAsm);
            storeRegisterToOperand(&instruction->result, ASM_RAX, frame);
            break;
        }

        case TAC_LT: {
            bool isFloat = isFloatOperand(&instruction->firstOperand);
            if (isFloat) {
                emitFloatRelationalOp(instruction, "setb", frame);
            } else {
                emitRelationalOp(instruction, "setl", frame);
            }
            break;
        }

        case TAC_GT: {
            bool isFloat = isFloatOperand(&instruction->firstOperand);
            if (isFloat) {
                emitFloatRelationalOp(instruction, "seta", frame);
            } else {
                emitRelationalOp(instruction, "setg", frame);
            }
            break;
        }

        case TAC_EQ: {
            bool isFloat = isFloatOperand(&instruction->firstOperand);
            if (isFloat) {
                emitFloatRelationalOp(instruction, "sete", frame);
            } else {
                emitRelationalOp(instruction, "sete", frame);
            }
            break;
        }

        case TAC_AND:
            emitBinaryOp(instruction, ASM_AND, frame);
            break;

        case TAC_OR:
            emitBinaryOp(instruction, ASM_OR, frame);
            break;

        case TAC_INT_TO_FLOAT:
            loadOperand(&instruction->firstOperand, "%eax", frame);
            buildStringf("    cvtsi2ss %%eax, %%xmm0\n");
            storeFloatOperand(&instruction->result, "%xmm0", frame);
            break;

        case TAC_LABEL:
            buildStringf(".%s:\n", instruction->result.name);
            break;

        case TAC_GOTO:
            buildStringf("    jmp .%s\n", instruction->firstOperand.name);
            break;

        case TAC_IF_FALSE:
            loadOperand(&instruction->firstOperand, "%eax", frame);
            buildStringf("    cmpl $0, %%eax\n");
            buildStringf("    je .%s\n", instruction->secondOperand.name);
            break;

        case TAC_IF_TRUE:
            loadOperand(&instruction->firstOperand, "%eax", frame);
            buildStringf("    cmpl $0, %%eax\n");
            buildStringf("    jne .%s\n", instruction->secondOperand.name);
            break;

        case TAC_PARAM:
            emitParam(instruction, frame);
            break;

        case TAC_CALL:
            emitCall(instruction, frame);
            break;

        case TAC_RETURN: {
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
            break;
        }

        default:
            break;
    }
}

/* Copia el argumento de la llamada en su slot correspondiente del frame. */
static void emitParam(const TACInstruction *instruction, FunctionFrame *frame) {
    bool reachesLimit = (frame->pendingParamCount >= MAX_PENDING_PARAMS);
    if (reachesLimit) {
        return;
    }

    int slot = frame->pendingParamSlots[frame->pendingParamCount];
    bool isFloat = isFloatOperand(&instruction->firstOperand);

    if (isFloat) {
        loadFloatOperand(&instruction->firstOperand, "%xmm0", frame);
        buildStringf("    movss %%xmm0, %d(%%rbp)\n", slot);
    } else {
        loadRegisterBasedOnOperand(ASM_RAX, &instruction->firstOperand, frame);
        AsmInstruction store = asmStore(opMem(slot), opReg(ASM_RAX));
        emitAsm(&store);
    }

    frame->pendingParamTypes[frame->pendingParamCount] = instruction->firstOperand.type;
    frame->pendingParamCount++;
}

/* Consume los argumentos según System V AMD64 y emite la llamada. */
static void emitCall(const TACInstruction *instruction, FunctionFrame *frame) {
    int argumentCount = (instruction->secondOperand.kind == OPERAND_CONSTANT) ? instruction->secondOperand.value.intValue : 0;
    bool exceedsPending = (argumentCount > frame->pendingParamCount);
    if (exceedsPending) {
        argumentCount = frame->pendingParamCount;
    }

    int base = frame->pendingParamCount - argumentCount;

    int registerIndex[MAX_PENDING_PARAMS];
    int goesToStack[MAX_PENDING_PARAMS];
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

    bool needsAlignmentPadding = (stackArguments % 2 == 1);
    int padding = needsAlignmentPadding ? 8 : 0;
    if (needsAlignmentPadding) {
        buildStringf("    subq $%d, %%rsp\n", padding);
    }

    for (int i = argumentCount - 1; i >= 0; i--) {
        if (!goesToStack[i]) continue;

        AsmInstruction load = asmLoad(opReg(ASM_RAX), opMem(frame->pendingParamSlots[base + i]));
        emitAsm(&load);
        buildStringf("    pushq %%rax\n");
    }

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

    buildStringf("    call %s\n", instruction->firstOperand.name);

    bool needsStackCleanup = (stackArguments > 0 || padding > 0);
    if (needsStackCleanup) {
        buildStringf("    addq $%d, %%rsp\n", stackArguments * 8 + padding);
    }

    frame->pendingParamCount = base;

    bool hasResult = (instruction->result.kind != OPERAND_NONE);
    if (hasResult) {
        bool isFloat = isFloatOperand(&instruction->result);
        if (isFloat) {
            storeFloatOperand(&instruction->result, "%xmm0", frame);
        } else {
            storeOperand(&instruction->result, "%eax", frame);
        }
    }
}

static void emitBinaryOp(const TACInstruction *instruction, AsmOpcode opcode, const FunctionFrame *frame) {
    loadRegisterBasedOnOperand(ASM_RAX, &instruction->firstOperand, frame);
    loadRegisterBasedOnOperand(ASM_RDX, &instruction->secondOperand, frame);
    AsmInstruction asmInstruction = asmOpReg(opcode, ASM_RAX, ASM_RDX);
    emitAsm(&asmInstruction);
    storeRegisterToOperand(&instruction->result, ASM_RAX, frame);
}

static void emitFloatBinaryOp(const TACInstruction *instruction, const char *mnemonic, const FunctionFrame *frame) {
    loadFloatOperand(&instruction->firstOperand, "%xmm0", frame);
    loadFloatOperand(&instruction->secondOperand, "%xmm1", frame);
    buildStringf("    %s %%xmm1, %%xmm0\n", mnemonic);
    storeFloatOperand(&instruction->result, "%xmm0", frame);
}

/* idivl divide %edx:%eax; cociente en %eax y resto en %edx. Usa %ecx para no pisar %edx. */
static void emitDivisionOp(const TACInstruction *instruction, const FunctionFrame *frame, AsmRegister resultRegister) {
    loadRegisterBasedOnOperand(ASM_RAX, &instruction->firstOperand, frame);
    loadRegisterBasedOnOperand(ASM_RCX, &instruction->secondOperand, frame);

    AsmInstruction signExtend = asmCltd();
    emitAsm(&signExtend);

    AsmInstruction divide = asmOpUnary(ASM_IDIV, ASM_RCX);
    emitAsm(&divide);

    storeRegisterToOperand(&instruction->result, resultRegister, frame);
}

static void emitRelationalOp(const TACInstruction *instruction, const char *conditionMnemonic, const FunctionFrame *frame) {
    loadRegisterBasedOnOperand(ASM_RAX, &instruction->firstOperand, frame);
    loadRegisterBasedOnOperand(ASM_RDX, &instruction->secondOperand, frame);
    AsmInstruction cmpInstruction = asmOpReg(ASM_CMP, ASM_RAX, ASM_RDX);
    emitAsm(&cmpInstruction);
    buildStringf("    %s %%al\n", conditionMnemonic);
    buildStringf("    movzbl %%al, %%eax\n");
    storeRegisterToOperand(&instruction->result, ASM_RAX, frame);
}

static void emitFloatRelationalOp(const TACInstruction *instruction, const char *conditionMnemonic, const FunctionFrame *frame) {
    loadFloatOperand(&instruction->firstOperand, "%xmm0", frame);
    loadFloatOperand(&instruction->secondOperand, "%xmm1", frame);
    buildStringf("    comiss %%xmm1, %%xmm0\n");
    buildStringf("    %s %%al\n", conditionMnemonic);
    buildStringf("    movzbl %%al, %%eax\n");
    storeOperand(&instruction->result, "%eax", frame);
}

static void loadRegisterBasedOnOperand(AsmRegister registerType, const TACOperand *operand, const FunctionFrame *frame) {
    loadOperand(operand, getRegisterName(registerType), frame);
}

static void storeRegisterToOperand(const TACOperand *operand, AsmRegister registerType, const FunctionFrame *frame) {
    storeOperand(operand, getRegisterName(registerType), frame);
}

static void loadOperand(const TACOperand *operand, const char *registerName, const FunctionFrame *frame) {
    if (operand == NULL) return;

    bool isImmediate = (operand->kind == OPERAND_CONSTANT);
    if (isImmediate) {
        AsmInstruction immediate = asmLoad(opRegStr(registerName), opImm(operand->value.intValue));
        emitAsm(&immediate);
        return;
    }

    int offset;
    OperandLocation location = resolveOperandLocation(operand, frame, &offset);
    if (location == LOCATION_UNRESOLVED) return;

    AsmInstruction instruction = (location == LOCATION_STACK)
        ? asmLoad(opRegStr(registerName), opMem(offset))
        : asmLoad(opRegStr(registerName), opGlobal(getGlobalName(operand)));
    emitAsm(&instruction);
}

static void storeOperand(const TACOperand *operand, const char *registerName, const FunctionFrame *frame) {
    if (operand == NULL) return;

    int offset;
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

    int offset;
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

    int offset;
    OperandLocation location = resolveOperandLocation(operand, frame, &offset);
    if (location == LOCATION_UNRESOLVED) return;

    if (location == LOCATION_STACK) {
        buildStringf("    movss %s, %d(%%rbp)\n", xmmRegister, offset);
    } else {
        buildStringf("    movss %s, %s(%%rip)\n", xmmRegister, getGlobalName(operand));
    }
}

static OperandLocation resolveOperandLocation(const TACOperand *operand, const FunctionFrame *frame, int *offset) {
    *offset = getSlotOffset(frame, operand->name);
    bool isInStack = (*offset != 0);
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
    for (int i = 0; i < globalVariableCount; i++) {
        bool matchesGlobal = (strcmp(globalVariableNames[i], operand->name) == 0);
        if (matchesGlobal) {
            return true;
        }
    }
    return false;
}

static void registerGlobalVariable(const TACOperand *operand) {
    bool reachesLimit = (globalVariableCount >= MAX_GLOBAL_VARIABLES);
    if (reachesLimit) return;

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

    bool reachesLimit = (floatConstantCount >= MAX_FLOAT_CONSTANTS);
    if (reachesLimit) {
        return floatConstants[0].label;
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

static const char *getRegisterName(AsmRegister registerType) {
    switch (registerType) {
        case ASM_RAX: return "%eax";
        case ASM_RDX: return "%edx";
        case ASM_RCX: return "%ecx";
        default:      return "%eax";
    }
}
