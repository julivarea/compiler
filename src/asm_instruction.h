#ifndef ASM_INSTRUCTION_H
#define ASM_INSTRUCTION_H

#include <string.h>

/**
 * @file asm_instruction.h
 * @brief Estructuras y constructores de instrucciones x86-64 para carga, guardado y operaciones aritmético-lógicas.
 *
 * Este módulo define el modelo de datos para representar instrucciones Assembly x86-64
 * orientadas a operaciones sobre registros, constantes numéricas y accesos al stack frame.
 */

/**
 * @brief Códigos de operación (opcodes) de Assembly x86-64 soportados.
 */
typedef enum {
    ASM_MOV,    /**< Movimiento / transferencia de datos: movl src, dest */
    ASM_ADD,    /**< Suma entera: addl src, dest */
    ASM_SUB,    /**< Resta entera: subl src, dest */
    ASM_IMUL,   /**< Multiplicación entera con signo: imull src, dest */
    ASM_IDIV,   /**< División entera con signo: idivl src */
    ASM_NEG,    /**< Negación aritmética a complemento a dos: negl dest */
    ASM_AND,    /**< Conjunción a nivel de bits: andl src, dest */
    ASM_OR,     /**< Disyunción a nivel de bits: orl src, dest */
    ASM_XOR,    /**< Disyunción exclusiva (útil para negación booleana): xorl src, dest */
    ASM_NOT,    /**< Negación a nivel de bits complemento a uno: notl dest */
    ASM_CMP,    /**< Comparación entera (modifica flags): cmpl src, dest */
    ASM_SETE,   /**< Seteo condicional si igual / cero (ZF=1): sete dest (8 bits) */
    ASM_SETNE,  /**< Seteo condicional si distinto / no cero (ZF=0): setne dest (8 bits) */
    ASM_SETL,   /**< Seteo condicional si menor con signo: setl dest (8 bits) */
    ASM_SETLE,  /**< Seteo condicional si menor o igual con signo: setle dest (8 bits) */
    ASM_SETG,   /**< Seteo condicional si mayor con signo: setg dest (8 bits) */
    ASM_SETGE,  /**< Seteo condicional si mayor o igual con signo: setge dest (8 bits) */
    ASM_MOVZBL, /**< Extensión de cero de byte a doubleword: movzbl src8, dest32 */
    ASM_CLTD    /**< Extensión de signo de %eax a %edx:%eax para división idivl: cltd */
} AsmOpcode;

/**
 * @brief Registros x86-64 (en su variante de 32 bits) empleados en operaciones.
 */
typedef enum {
    ASM_RAX, /**< %eax: registro acumulador / destino principal de operaciones */
    ASM_RDX, /**< %edx: segundo operando en operaciones binarias */
    ASM_RCX  /**< %ecx: divisor en idivl / registro auxiliar */
} AsmRegister;

/**
 * @brief Discriminador para el tipo de operando en Assembly.
 */
typedef enum {
    ASM_OP_NONE = 0, /**< Operando ausente (para instrucciones unarias como negl) */
    ASM_OP_REG,      /**< Operando ubicado en un registro de la CPU (%eax, %edx, etc.) */
    ASM_OP_MEM,      /**< Operando en memoria relativo al marco de pila (offset(%rbp)) */
    ASM_OP_GLOBAL,   /**< Operando en memoria para variable global (nombre(%rip)) */
    ASM_OP_IMM       /**< Valor constante numérico inmediato ($val) */
} AsmOperandType;

/**
 * @brief Representa un operando individual de una instrucción Assembly.
 */
typedef struct {
    AsmOperandType type; /**< Tipo de operando */
    union {
        AsmRegister reg;        /**< Identificador del registro cuando type == ASM_OP_REG */
        int offset;            /**< Desplazamiento respecto a %rbp cuando type == ASM_OP_MEM (ej: -4) */
        int immediate;         /**< Valor numérico constante cuando type == ASM_OP_IMM (ej: 1, 42) */
        const char *globalName; /**< Nombre de la variable global cuando type == ASM_OP_GLOBAL */
    };
    char customReg[16]; /**< Nombre explícito del registro (ej: "%edi", "%esi", etc.) */
} AsmOperand;

/**
 * @brief Representación intermedia de una instrucción de Assembly x86-64.
 *
 * En sintaxis AT&T (GNU Assembler / GAS), el formato de emisión textual es:
 *     <mnemonic> <src>, <dest>
 */
typedef struct {
    AsmOpcode opcode;  /**< Código de operación a ejecutar */
    AsmOperand dest;   /**< Operando destino (registro o memoria) */
    AsmOperand src;    /**< Operando origen (registro, memoria, inmediato o NONE) */
} AsmInstruction;

/* --- Helpers rápidos de construcción de operandos --- */

/**
 * @brief Construye un operando de tipo registro mediante enum.
 * @param r Registro x86 seleccionado.
 * @return AsmOperand configurado como registro.
 */
static inline AsmOperand opReg(AsmRegister r) {
    AsmOperand op;
    memset(&op, 0, sizeof(op));
    op.type = ASM_OP_REG;
    op.reg = r;
    return op;
}

/**
 * @brief Construye un operando de tipo registro por nombre de cadena (ej: "%edi", "%eax").
 * @param name Nombre del registro con prefijo '%'.
 * @return AsmOperand configurado como registro.
 */
static inline AsmOperand opRegStr(const char *name) {
    AsmOperand op;
    memset(&op, 0, sizeof(op));
    op.type = ASM_OP_REG;
    if (name != NULL) {
        strncpy(op.customReg, name, sizeof(op.customReg) - 1);
    }
    return op;
}

/**
 * @brief Construye un operando de memoria relativo a %rbp.
 * @param offset Desplazamiento en bytes respecto a %rbp (ej: -4, -8).
 * @return AsmOperand configurado como memoria de stack.
 */
static inline AsmOperand opMem(int offset) {
    AsmOperand op;
    memset(&op, 0, sizeof(op));
    op.type = ASM_OP_MEM;
    op.offset = offset;
    return op;
}

/**
 * @brief Construye un operando inmediato constante.
 * @param val Valor numérico entero.
 * @return AsmOperand configurado como constante inmediata.
 */
static inline AsmOperand opImm(int val) {
    AsmOperand op;
    memset(&op, 0, sizeof(op));
    op.type = ASM_OP_IMM;
    op.immediate = val;
    return op;
}

/**
 * @brief Construye un operando de memoria relativo a %rip para variables globales.
 * @param name Nombre del identificador global.
 * @return AsmOperand configurado como global.
 */
static inline AsmOperand opGlobal(const char *name) {
    AsmOperand op;
    memset(&op, 0, sizeof(op));
    op.type = ASM_OP_GLOBAL;
    op.globalName = name;
    return op;
}

/** Alias para compatibilidad */
static inline AsmOperand opConstant(const char *globalVariable) {
    return opGlobal(globalVariable);
}

/**
 * @brief Construye un operando vacío (sin uso).
 * @return AsmOperand de tipo ASM_OP_NONE.
 */
static inline AsmOperand opNone(void) {
    AsmOperand op;
    memset(&op, 0, sizeof(op));
    op.type = ASM_OP_NONE;
    return op;
}

/**
 * @brief Crea una instrucción de carga genérica (movl src, dest).
 * @param dest Operando destino (generalmente un registro).
 * @param src Operando origen (inmediato, memoria o global).
 * @return AsmInstruction configurada para carga.
 */
AsmInstruction asmLoad(AsmOperand dest, AsmOperand src);

/**
 * @brief Crea una instrucción de carga de un valor inmediato a un registro.
 *
 * Genera la instrucción: `movl $val, destReg`.
 *
 * @param destReg Registro destino donde se almacenará la constante.
 * @param val Valor entero constante.
 * @return AsmInstruction configurada para cargar una constante.
 */
AsmInstruction asmLoadImm(AsmRegister destReg, int val);

/**
 * @brief Crea una instrucción de guardado genérica (movl src, dest).
 * @param dest Operando destino (memoria o global).
 * @param src Operando origen (generalmente un registro).
 * @return AsmInstruction configurada para escribir en memoria.
 */
AsmInstruction asmStore(AsmOperand dest, AsmOperand src);

/**
 * @brief Crea una operación binaria entre dos registros de la CPU.
 *
 * Genera la instrucción: `<op> srcReg, destReg` (destReg = destReg <op> srcReg).
 *
 * @param op Código de operación (ej. ASM_ADD, ASM_SUB, ASM_OR).
 * @param dest Registro destino que se modifica con el resultado.
 * @param src Registro origen que opera sobre el destino.
 * @return AsmInstruction configurada para operar entre registros.
 */
AsmInstruction asmOpReg(AsmOpcode op, AsmRegister dest, AsmRegister src);

/**
 * @brief Crea una operación aritmética o lógica con una constante inmediata.
 *
 * Genera la instrucción: `<op> $imm, destReg` (destReg = destReg <op> imm).
 *
 * @param op Código de operación (ej. ASM_XOR para negación booleana con $1).
 * @param dest Registro destino modificado por la operación.
 * @param imm Valor constante inmediato.
 * @return AsmInstruction configurada con operando inmediato.
 */
AsmInstruction asmOpImm(AsmOpcode op, AsmRegister dest, int imm);

/**
 * @brief Crea una operación unaria sobre un registro.
 *
 * Genera la instrucción: `<op> destReg` (ej. negl %eax, idivl %ecx).
 *
 * @param op Código de operación unaria (ej. ASM_NEG, ASM_NOT, ASM_IDIV).
 * @param dest Registro objetivo sobre el cual se aplica la operación.
 * @return AsmInstruction unaria configurada.
 */
AsmInstruction asmOpUnary(AsmOpcode op, AsmRegister dest);

/**
 * @brief Crea una instrucción de seteo condicional (sete, setne, setl, setle, setg, setge).
 *
 * Genera la instrucción: `<setcc> %reg8` (ej: sete %al).
 *
 * @param op Código de operación de seteo condicional (ej. ASM_SETE, ASM_SETL).
 * @param dest Registro cuyo byte inferior se asignará (ej: ASM_RAX -> %al).
 * @return AsmInstruction configurada para el seteo condicional.
 */
AsmInstruction asmSetCC(AsmOpcode op, AsmRegister dest);

/**
 * @brief Crea una instrucción de extensión de cero de byte a long (32 bits).
 *
 * Genera la instrucción: `movzbl %src8, %dest32` (ej: movzbl %al, %eax).
 *
 * @param dest Registro de destino de 32 bits (ej: ASM_RAX -> %eax).
 * @param src Registro de origen de 8 bits (ej: ASM_RAX -> %al).
 * @return AsmInstruction configurada para extensión con ceros.
 */
AsmInstruction asmMovzbl(AsmRegister dest, AsmRegister src);

/**
 * @brief Crea una instrucción de extensión de signo cltd (%eax a %edx:%eax).
 *
 * Genera la instrucción: `cltd`.
 *
 * @return AsmInstruction configurada para cltd.
 */
AsmInstruction asmCltd(void);

/**
 * @brief Emite la representación textual de una AsmInstruction al string_builder en sintaxis AT&T (GAS).
 *
 * @param inst Puntero a la instrucción a emitir.
 */
void emitAsm(const AsmInstruction *inst);

#endif /* ASM_INSTRUCTION_H */
