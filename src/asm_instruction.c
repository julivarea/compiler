/**
 * @file asm_instruction.c
 * @brief Implementación de constructores y formateo de instrucciones x86-64.
 */

#include "asm_instruction.h"
#include "string_builder.h"

AsmInstruction asmLoad(AsmOperand dest, AsmOperand src) {
    AsmInstruction inst = { ASM_MOV, dest, src };
    return inst;
}

AsmInstruction asmLoadImm(AsmRegister destReg, int val) {
    AsmInstruction inst = { ASM_MOV, opReg(destReg), opImm(val) };
    return inst;
}

AsmInstruction asmStore(AsmOperand dest, AsmOperand src) {
    AsmInstruction inst = { ASM_MOV, dest, src };
    return inst;
}

AsmInstruction asmOpReg(AsmOpcode op, AsmRegister dest, AsmRegister src) {
    AsmInstruction inst = { op, opReg(dest), opReg(src) };
    return inst;
}

AsmInstruction asmOpImm(AsmOpcode op, AsmRegister dest, int imm) {
    AsmInstruction inst = { op, opReg(dest), opImm(imm) };
    return inst;
}

AsmInstruction asmOpUnary(AsmOpcode op, AsmRegister dest) {
    AsmInstruction inst = { op, opReg(dest), opNone() };
    return inst;
}

AsmInstruction asmCltd(void) {
    AsmInstruction inst = { ASM_CLTD, opNone(), opNone() };
    return inst;
}

/**
 * @brief Obtiene la cadena representativa del registro en 32 bits para sintaxis AT&T.
 * @param reg Registro x86 a traducir.
 * @return Cadena con el nombre del registro (ej: "%eax").
 */
static const char *getRegisterName(AsmRegister reg) {
    switch (reg) {
        case ASM_RAX: return "%eax";
        case ASM_RDX: return "%edx";
        case ASM_RCX: return "%ecx";
        default:      return "%eax";
    }
}

/**
 * @brief Obtiene el mnemónico x86 del código de operación.
 * @param op Código de operación.
 * @return Cadena con el mnemónico (ej: "addl", "cltd").
 */
static const char *getOpcodeMnemonic(AsmOpcode op) {
    switch (op) {
        case ASM_MOV:    return "movl";
        case ASM_ADD:    return "addl";
        case ASM_SUB:    return "subl";
        case ASM_IMUL:   return "imull";
        case ASM_IDIV:   return "idivl";
        case ASM_NEG:    return "negl";
        case ASM_AND:    return "andl";
        case ASM_OR:     return "orl";
        case ASM_XOR:    return "xorl";
        case ASM_CMP:    return "cmpl";
        case ASM_CLTD:   return "cltd";
        default:         return "nop";
    }
}

/**
 * @brief Emite un operando en su representación en texto para GNU Assembler.
 * @param op Puntero al operando a emitir.
 */
static void emitOperand(const AsmOperand *op) {
    switch (op->type) {
        case ASM_OP_REG:
            buildStringf("%s", op->customReg[0] != '\0' ? op->customReg : getRegisterName(op->reg));
            break;
        case ASM_OP_MEM:
            buildStringf("%d(%%rbp)", op->offset);
            break;
        case ASM_OP_IMM:
            buildStringf("$%d", op->immediate);
            break;
        case ASM_OP_GLOBAL:
            buildStringf("%s(%%rip)", op->globalName ? op->globalName : "");
            break;
        case ASM_OP_NONE:
        default:
            break;
    }
}

void emitAsm(const AsmInstruction *inst) {
    if (inst == NULL) return;

    /* Instrucción sin operandos (ej: cltd) */
    if (inst->opcode == ASM_CLTD) {
        buildStringf("    cltd\n");
        return;
    }

    const char *opStr = getOpcodeMnemonic(inst->opcode);

    /* Instrucción unaria (ej: negl %eax, idivl %ecx) */
    if (inst->src.type == ASM_OP_NONE) {
        buildStringf("    %s ", opStr);
        emitOperand(&inst->dest);
        buildStringf("\n");
        return;
    }

    /* Instrucción binaria en sintaxis AT&T: opcode source, destination (ej: addl %edx, %eax) */
    buildStringf("    %s ", opStr);
    emitOperand(&inst->src);
    buildStringf(", ");
    emitOperand(&inst->dest);
    buildStringf("\n");
}
