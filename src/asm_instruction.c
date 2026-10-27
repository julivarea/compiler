/**
 * @file asm_instruction.c
 * @brief Implementación de constructores y formateo de instrucciones x86-64.
 */

#include <stdio.h>
#include "asm_instruction.h"
#include "string_builder.h"

AsmInstruction asmLoad(AsmRegister destReg, int stackOffset) {
    AsmInstruction inst = { ASM_MOV, opReg(destReg), opMem(stackOffset) };
    return inst;
}

AsmInstruction asmLoadImm(AsmRegister destReg, int val) {
    AsmInstruction inst = { ASM_MOV, opReg(destReg), opImm(val) };
    return inst;
}

AsmInstruction asmStore(int stackOffset, AsmRegister srcReg) {
    AsmInstruction inst = { ASM_MOV, opMem(stackOffset), opReg(srcReg) };
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
 * @brief Obtiene el mnemónico x86 de 32 bits (sufijo 'l') del código de operación.
 * @param op Código de operación.
 * @return Cadena con el mnemónico (ej: "addl", "xorl").
 */
static const char *getOpcodeMnemonic(AsmOpcode op) {
    switch (op) {
        case ASM_MOV:  return "movl";
        case ASM_ADD:  return "addl";
        case ASM_SUB:  return "subl";
        case ASM_IMUL: return "imull";
        case ASM_IDIV: return "idivl";
        case ASM_NEG:  return "negl";
        case ASM_AND:  return "andl";
        case ASM_OR:   return "orl";
        case ASM_XOR:  return "xorl";
        case ASM_NOT:  return "notl";
        case ASM_CMP:  return "cmpl";
        default:       return "nop";
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
            snprintf(buf, size, "%s", getRegisterName(op->reg));
            break;
        case ASM_OP_MEM:
            snprintf(buf, size, "%d(%%rbp)", op->offset);
            break;
        case ASM_OP_IMM:
            snprintf(buf, size, "$%d", op->immediate);
            break;
        case ASM_OP_NONE:
        default:
            buf[0] = '\0';
            break;
    }
}

void emitAsm(const AsmInstruction *inst) {
    if (inst == NULL) return;

    const char *opStr = getOpcodeMnemonic(inst->opcode);

    char destBuf[32];
    formatOperand(&inst->dest, destBuf, sizeof(destBuf));

    /* Instrucción unaria (ej: negl %eax) */
    if (inst->src.type == ASM_OP_NONE) {
        buildStringf("    %s %s\n", opStr, destBuf);
        return;
    }

    /* Instrucción binaria en sintaxis AT&T: opcode source, destination (ej: addl %edx, %eax) */
    char srcBuf[32];
    formatOperand(&inst->src, srcBuf, sizeof(srcBuf));
    buildStringf("    %s %s, %s\n", opStr, srcBuf, destBuf);
}
