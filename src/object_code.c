#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "object_code.h"
#include "string_builder.h"


static const char *ARG_REGISTERS_32[] = {
    "%edi", "%esi", "%edx", "%ecx", "%r8d", "%r9d"
};
static const int MAX_ARG_REGISTERS = 6; // implements extra variables in frames

static int callArgIndex = 0;

FunctionFrame *createFrame(const char *funcName) {
    FunctionFrame *frame = (FunctionFrame *)malloc(sizeof(FunctionFrame));
    if (frame == NULL) {
        return NULL;
    }

    if (funcName != NULL) {
        strncpy(frame->functionName, funcName, TAC_NAME_LENGTH - 1);
        frame->functionName[TAC_NAME_LENGTH - 1] = '\0';
    } else {
        frame->functionName[0] = '\0';
    }

    frame->slotCapacity = 16;
    frame->slotCount = 0;
    frame->slots = (StackSlot *)malloc(sizeof(StackSlot) * frame->slotCapacity);
    frame->nextOffset = 0;
    frame->frameSize = 0;
    frame->paramCount = 0;

    return frame;
}

void freeFrame(FunctionFrame *frame) {
    if (frame == NULL) return;

    if (frame->slots != NULL) {
        free(frame->slots);
        frame->slots = NULL;
    }
    free(frame);
}

int addSlot(FunctionFrame *frame, const TACOperand *operand) {
    if (frame == NULL || operand == NULL || operand->name[0] == '\0') {
        return 0;
    }

    for (int i = 0; i < frame->slotCount; i++) {
        if (strcmp(frame->slots[i].name, operand->name) == 0) {
            return frame->slots[i].offset;
        }
    }

    if (frame->slotCount >= frame->slotCapacity) {
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

int getSlotOffset(const FunctionFrame *frame, const char *name) {
    if (frame == NULL || name == NULL) return 0;

    for (int i = 0; i < frame->slotCount; i++) {
        if (strcmp(frame->slots[i].name, name) == 0) {
            return frame->slots[i].offset;
        }
    }
    return 0;
}

static const char *getGlobalName(const TACOperand *op) {
    if (op->symbol != NULL && op->symbol->id != NULL) {
        return op->symbol->id;
    }
    return op->name;
}

static void loadOperand(const TACOperand *op, const char *reg, const FunctionFrame *frame) {
    if (op == NULL) return;

    if (op->kind == OPERAND_CONSTANT) {
        buildStringf("    movl $%d, %s\n", op->value.int_val, reg);
    } else {
        int offset = getSlotOffset(frame, op->name);
        if (offset != 0) {
            buildStringf("    movl %d(%%rbp), %s\n", offset, reg);
        } else {
            /* Variable global */
            const char *name = getGlobalName(op);
            buildStringf("    movl %s(%%rip), %s\n", name, reg);
        }
    }
}

static void storeOperand(const TACOperand *op, const char *reg, const FunctionFrame *frame) {
    if (op == NULL) return;

    int offset = getSlotOffset(frame, op->name);
    if (offset != 0) {
        buildStringf("    movl %s, %d(%%rbp)\n", reg, offset);
    } else {
        /* Variable global */
        const char *name = getGlobalName(op);
        buildStringf("    movl %s, %s(%%rip)\n", reg, name);
    }
}

void emitPrologue(const FunctionFrame *frame) {
    if (frame == NULL) return;

    buildStringf("\n.globl %s\n", frame->functionName);
    buildStringf(".type %s, @function\n", frame->functionName);
    buildStringf("%s:\n", frame->functionName);
    buildStringf("    pushq %%rbp\n");
    buildStringf("    movq %%rsp, %%rbp\n");
    if (frame->frameSize > 0) {
        buildStringf("    subq $%d, %%rsp\n", frame->frameSize);
    }
}

void emitEpilogue(const FunctionFrame *frame) {
    if (frame != NULL && frame->functionName[0] != '\0') {
        buildStringf(".L_%s_epilogue:\n", frame->functionName);
    }
    buildStringf("    leave\n");
    buildStringf("    ret\n");
}

void generateInstruction(const TACInstruction *instruction, FunctionFrame *frame) {
    if (instruction == NULL || frame == NULL) return;

    switch (instruction->op) {
        case TAC_PARAM_DECL:
            if (frame->paramCount < MAX_ARG_REGISTERS) {
                int offset = getSlotOffset(frame, instruction->arg1.name);
                if (offset != 0) {
                    buildStringf("    movl %s, %d(%%rbp)\n",
                                 ARG_REGISTERS_32[frame->paramCount], offset);
                }
                frame->paramCount++;
            }
            break;

        case TAC_LOCAL_DECL:
            break;

        case TAC_ASSIGN:
            loadOperand(&instruction->arg1, "%eax", frame);
            storeOperand(&instruction->result, "%eax", frame);
            break;

        case TAC_ADD: {
            loadOperand(&instruction->arg1, "%eax", frame);
            loadOperand(&instruction->arg2, "%edx", frame);
            AsmInstruction inst = asmOpReg(ASM_ADD, ASM_RAX, ASM_RDX);
            emitAsm(&inst);
            storeOperand(&instruction->result, "%eax", frame);
            break;
        }

        case TAC_SUB: {
            loadOperand(&instruction->arg1, "%eax", frame);
            loadOperand(&instruction->arg2, "%edx", frame);
            AsmInstruction inst = asmOpReg(ASM_SUB, ASM_RAX, ASM_RDX);
            emitAsm(&inst);
            storeOperand(&instruction->result, "%eax", frame);
            break;
        }

        case TAC_MUL: {
            loadOperand(&instruction->arg1, "%eax", frame);
            loadOperand(&instruction->arg2, "%edx", frame);
            AsmInstruction inst = asmOpReg(ASM_IMUL, ASM_RAX, ASM_RDX);
            emitAsm(&inst);
            storeOperand(&instruction->result, "%eax", frame);
            break;
        }

        case TAC_DIV: {
            loadOperand(&instruction->arg1, "%eax", frame);
            buildStringf("    cltd\n");
            loadOperand(&instruction->arg2, "%ecx", frame);
            AsmInstruction inst = asmOpUnary(ASM_IDIV, ASM_RCX);
            emitAsm(&inst);
            storeOperand(&instruction->result, "%eax", frame);
            break;
        }

        case TAC_MOD: {
            loadOperand(&instruction->arg1, "%eax", frame);
            buildStringf("    cltd\n");
            loadOperand(&instruction->arg2, "%ecx", frame);
            AsmInstruction inst = asmOpUnary(ASM_IDIV, ASM_RCX);
            emitAsm(&inst);
            storeOperand(&instruction->result, "%edx", frame);
            break;
        }

        case TAC_NEG: {
            loadOperand(&instruction->arg1, "%eax", frame);
            AsmInstruction inst = asmOpUnary(ASM_NEG, ASM_RAX);
            emitAsm(&inst);
            storeOperand(&instruction->result, "%eax", frame);
            break;
        }

        // b = !a, b = !5
        case TAC_NOT: {
        // analizamos si tenemos que recuperar el valor de la variable o si es inmediato
        switch(instruction->arg1->operandKind){
            case OPERAND_TEMP, OPERAND_VARIABLE:
            asmLoad(ASM_RAX, getSlotOffset(instruction->arg1->name));
            break;

            case OPERAND_CONSTANT:
             asmLoadImm(ASM_RAX,value);
            break;
        }
        // definimos que operaciones corresponden segun el tipo del argumento
        switch (instruction->arg1->dataType) {
        case TYPE_BOOL:
            asmXorImm(ASM_RAX, 1);
            break;

        case TYPE_INT:
            asmCmpImm(ASM_RAX, 0);
            asmSetCC(ASM_SETE, ASM_RAX);
            break;
        }
        // guardamos el resultado extrayendolo del rax
        asmStore(getSlotOffset(frame, instruction->result->name));
        }
        break;


        case TAC_LT:
            loadOperand(&instruction->arg1, "%eax", frame);
            loadOperand(&instruction->arg2, "%edx", frame);
            buildStringf("    cmpl %%edx, %%eax\n");
            buildStringf("    setl %%al\n");
            buildStringf("    movzbl %%al, %%eax\n");
            storeOperand(&instruction->result, "%eax", frame);
            break;

        case TAC_GT:
            loadOperand(&instruction->arg1, "%eax", frame);
            loadOperand(&instruction->arg2, "%edx", frame);
            buildStringf("    cmpl %%edx, %%eax\n");
            buildStringf("    setg %%al\n");
            buildStringf("    movzbl %%al, %%eax\n");
            storeOperand(&instruction->result, "%eax", frame);
            break;

        case TAC_EQ:
            loadOperand(&instruction->arg1, "%eax", frame);
            loadOperand(&instruction->arg2, "%edx", frame);
            buildStringf("    cmpl %%edx, %%eax\n");
            buildStringf("    sete %%al\n");
            buildStringf("    movzbl %%al, %%eax\n");
            storeOperand(&instruction->result, "%eax", frame);
            break;

        case TAC_AND: {
            loadOperand(&instruction->arg1, "%eax", frame);
            loadOperand(&instruction->arg2, "%edx", frame);
            AsmInstruction inst = asmOpReg(ASM_AND, ASM_RAX, ASM_RDX);
            emitAsm(&inst);
            storeOperand(&instruction->result, "%eax", frame);
            break;
        }

        case TAC_OR: {
            loadOperand(&instruction->arg1, "%eax", frame);
            loadOperand(&instruction->arg2, "%edx", frame);
            AsmInstruction inst = asmOpReg(ASM_OR, ASM_RAX, ASM_RDX);
            emitAsm(&inst);
            storeOperand(&instruction->result, "%eax", frame);
            break;
        }

        case TAC_INT_TO_FLOAT:
            loadOperand(&instruction->arg1, "%eax", frame);
            buildStringf("    cvtsi2ss %%eax, %%xmm0\n");
            break;

        case TAC_LABEL:
            buildStringf(".%s:\n", instruction->result.name);
            break;

        case TAC_GOTO:
            buildStringf("    jmp .%s\n", instruction->arg1.name);
            break;

        case TAC_IF_FALSE:
            loadOperand(&instruction->arg1, "%eax", frame);
            buildStringf("    cmpl $0, %%eax\n");
            buildStringf("    je .%s\n", instruction->arg2.name);
            break;

        case TAC_IF_TRUE:
            loadOperand(&instruction->arg1, "%eax", frame);
            buildStringf("    cmpl $0, %%eax\n");
            buildStringf("    jne .%s\n", instruction->arg2.name);
            break;

        case TAC_PARAM:
            if (callArgIndex < MAX_ARG_REGISTERS) {
                loadOperand(&instruction->arg1, ARG_REGISTERS_32[callArgIndex], frame);
                callArgIndex++;
            }
            break;

        case TAC_CALL:
            buildStringf("    call %s\n", instruction->arg1.name);
            callArgIndex = 0;
            if (instruction->result.kind != OPERAND_NONE) {
                storeOperand(&instruction->result, "%eax", frame);
            }
            break;

        case TAC_RETURN:
            if (instruction->arg1.kind != OPERAND_NONE) {
                loadOperand(&instruction->arg1, "%eax", frame);
            }
            buildStringf("    jmp .L_%s_epilogue\n", frame->functionName);
            break;

        default:
            break;
    }
}

static void emitGlobals(const TACList *tacList) {
    if (tacList == NULL) return;

    int hasGlobals = 0;
    for (const TACInstruction *inst = tacList->head; inst != NULL; inst = inst->next) {
        if (inst->op == TAC_GLOBAL_DECL) {
            if (!hasGlobals) {
                buildStringf(".data\n");
                hasGlobals = 1;
            }
            const char *name = getGlobalName(&inst->arg1);
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

void generateAssembly(const TACList *tacList) {
    if (tacList == NULL) return;

    /* Emisión de variables globales si existen */
    emitGlobals(tacList);

    buildStringf(".text\n");

    const TACInstruction *instruction = tacList->head;

    while (instruction != NULL) {

        if (instruction->op == TAC_BEGIN_FUNC) {

            FunctionFrame *frame = createFrame(instruction->result.name);
            const TACInstruction *current = instruction->next; // recorrer cuerpo de la funcion

            while (current != NULL && current->op != TAC_END_FUNC) {

                switch (current->op) {

                    case TAC_PARAM_DECL:
                        addSlot(frame, &current->arg1);
                        break;

                    case TAC_LOCAL_DECL:
                        addSlot(frame, &current->arg1);
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
                    case TAC_CALL:
                        if (current->result.kind == OPERAND_TEMP) {
                            addSlot(frame, &current->result);
                        }
                        break;

                    default:
                        break;
                }

                current = current->next;
            }

            frame->frameSize = ALIGN_UP(frame->nextOffset, 16);

            emitPrologue(frame);

            current = instruction->next;

            while (current != NULL && current->op != TAC_END_FUNC) {

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
}
