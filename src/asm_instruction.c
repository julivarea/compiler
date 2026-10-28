/**
 * @file asm_instruction.c
 * @brief Implementación de constructores y formateo de instrucciones x86-64.
 */

#include <stdio.h>
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

AsmInstruction asmSetCC(AsmOpcode op, AsmRegister dest) {
    AsmInstruction inst = { op, opReg(dest), opNone() };
    return inst;
}

AsmInstruction asmMovzbl(AsmRegister dest, AsmRegister src) {
    AsmInstruction inst = { ASM_MOVZBL, opReg(dest), opReg(src) };
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
 * @brief Obtiene la cadena representativa del registro en 8 bits (byte bajo) para sintaxis AT&T.
 * @param reg Registro x86 a traducir.
 * @return Cadena con el nombre del registro de 8 bits (ej: "%al", "%dl", "%cl").
 */
static const char *getRegisterName8(AsmRegister reg) {
    switch (reg) {
        case ASM_RAX: return "%al";
        case ASM_RDX: return "%dl";
        case ASM_RCX: return "%cl";
        default:      return "%al";
    }
}

/**
 * @brief Verifica si un opcode corresponde a una instrucción condicional setCC.
 */
static int isSetCc(AsmOpcode op) {
    return op == ASM_SETE || op == ASM_SETNE ||
           op == ASM_SETL || op == ASM_SETLE ||
           op == ASM_SETG || op == ASM_SETGE;
}

/**
 * @brief Obtiene el mnemónico x86 del código de operación.
 * @param op Código de operación.
 * @return Cadena con el mnemónico (ej: "addl", "sete", "movzbl").
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
        case ASM_NOT:    return "notl";
        case ASM_CMP:    return "cmpl";
        case ASM_SETE:   return "sete";
        case ASM_SETNE:  return "setne";
        case ASM_SETL:   return "setl";
        case ASM_SETLE:  return "setle";
        case ASM_SETG:   return "setg";
        case ASM_SETGE:  return "setge";
        case ASM_MOVZBL: return "movzbl";
        case ASM_CLTD:   return "cltd";
        default:         return "nop";
    }
}

/**
 * @brief Convierte un operando a su representación en texto para GNU Assembler.
 * @param op Puntero al operando a formatear.
 * @param buf Buffer de destino.
 * @param size Tamaño del buffer.
 */
static void formatOperand(const AsmOperand *op, char *buf, size_t size) {
    switch (op->type) {
        case ASM_OP_REG:
            if (op->customReg[0] != '\0') {
                snprintf(buf, size, "%s", op->customReg);
            } else {
                snprintf(buf, size, "%s", getRegisterName(op->reg));
            }
            break;
        case ASM_OP_MEM:
            snprintf(buf, size, "%d(%%rbp)", op->offset);
            break;
        case ASM_OP_IMM:
            snprintf(buf, size, "$%d", op->immediate);
            break;
        case ASM_OP_GLOBAL:
            snprintf(buf, size, "%s(%%rip)", op->globalName ? op->globalName : "");
            break;
        case ASM_OP_NONE:
        default:
            buf[0] = '\0';
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

    /* Instrucción condicional setCC sobre registro de 8 bits (ej: sete %al) */
    if (isSetCc(inst->opcode)) {
        const char *reg8 = (inst->dest.type == ASM_OP_REG)
                           ? getRegisterName8(inst->dest.reg)
                           : "%al";
        buildStringf("    %s %s\n", opStr, reg8);
        return;
    }

    /* Extensión con ceros movzbl: src8 -> dest32 (ej: movzbl %al, %eax) */
    if (inst->opcode == ASM_MOVZBL) {
        const char *src8 = (inst->src.type == ASM_OP_REG)
                           ? getRegisterName8(inst->src.reg)
                           : ((inst->dest.type == ASM_OP_REG)
                              ? getRegisterName8(inst->dest.reg)
                              : "%al");
        const char *dest32 = (inst->dest.type == ASM_OP_REG)
                             ? getRegisterName(inst->dest.reg)
                             : "%eax";
        buildStringf("    movzbl %s, %s\n", src8, dest32);
        return;
    }

    char destBuf[32];
    formatOperand(&inst->dest, destBuf, sizeof(destBuf));

    /* Instrucción unaria (ej: negl %eax, idivl %ecx) */
    if (inst->src.type == ASM_OP_NONE) {
        buildStringf("    %s %s\n", opStr, destBuf);
        return;
    }

    /* Instrucción binaria en sintaxis AT&T: opcode source, destination (ej: addl %edx, %eax) */
    char srcBuf[32];
    formatOperand(&inst->src, srcBuf, sizeof(srcBuf));
    buildStringf("    %s %s, %s\n", opStr, srcBuf, destBuf);
}
