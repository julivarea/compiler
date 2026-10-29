#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "object_code.h"
#include "asm_instruction.h"
#include "string_builder.h"


static const char *ARG_REGISTERS_32[] = {
    "%edi", "%esi", "%edx", "%ecx", "%r8d", "%r9d"
};
static const int MAX_ARG_REGISTERS = 6; // implements extra variables in frames

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

    frame->returnType = TYPE_VOID;
    frame->slotCapacity = 16;
    frame->slotCount = 0;
    frame->slots = (StackSlot *)malloc(sizeof(StackSlot) * frame->slotCapacity);
    frame->nextOffset = 0;
    frame->frameSize = 0;
    frame->paramCount = 0;
    frame->pendingParamCount = 0;
    frame->maxPendingParams = 0;
    frame->integerParamCount = 0;
    frame->floatParamCount = 0;
    frame->stackParamCount = 0;

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

/**
 * @brief Reserva un slot anónimo en el frame, sin nombre asociado.
 *
 * Lo usan los argumentos pendientes de una llamada: no son variables ni
 * temporales del TAC, así que no se buscan por nombre, solo por posición.
 *
 * @param frame Frame de la función.
 * @return Desplazamiento relativo a %rbp del slot reservado.
 */
static int addAnonymousSlot(FunctionFrame *frame) {
    if (frame == NULL) return 0;

    frame->nextOffset += 4;
    return -frame->nextOffset;
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

/* ------------------------------------------------------------------ */
/* Resolucion de operandos y errores internos                          */
/* ------------------------------------------------------------------ */

#define MAX_GLOBAL_VARIABLES 256

/* Nombres calificados de las variables globales, tomados de los TAC_GLOBAL_DECL
 * en la pasada previa. Es lo que permite distinguir "esto es global" de "esto
 * deberia tener un slot y no lo tiene". */
static char globalVariableNames[MAX_GLOBAL_VARIABLES][TAC_NAME_LENGTH];
static int globalVariableCount = 0;

static int objectCodeErrors = 0;

int getObjectCodeErrors(void) {
    return objectCodeErrors;
}

static void registerGlobalVariable(const TACOperand *op) {
    if (globalVariableCount >= MAX_GLOBAL_VARIABLES) return;

    strncpy(globalVariableNames[globalVariableCount], op->name, TAC_NAME_LENGTH - 1);
    globalVariableNames[globalVariableCount][TAC_NAME_LENGTH - 1] = '\0';
    globalVariableCount++;
}

static int isDeclaredGlobal(const TACOperand *op) {
    for (int i = 0; i < globalVariableCount; i++) {
        if (strcmp(globalVariableNames[i], op->name) == 0) {
            return 1;
        }
    }
    return 0;
}

/* Donde vive el valor de un operando. */
typedef enum {
    LOCATION_UNRESOLVED, /**< no se pudo ubicar: error interno del compilador */
    LOCATION_STACK,      /**< en un slot del frame, en offset(%rbp) */
    LOCATION_GLOBAL      /**< en el segmento de datos, en nombre(%rip) */
} OperandLocation;

/*
 * Decide donde vive un operando que no es constante.
 *
 * Antes se asumia que todo lo que no tuviera slot era global. Eso hacia que un
 * temporal o una local que por algun bug no hubieran quedado registrados se
 * emitieran como `movl t3(%rip), %eax`: assembly sintacticamente valido, que
 * compila, y que recien falla en el enlazador con `undefined reference to
 * 't3'`, sin ninguna pista de que el problema estaba en el generador. Peor aun
 * si existiera una global con ese nombre, porque entonces ni siquiera falla:
 * lee la variable equivocada.
 *
 * Ahora ser global se verifica contra la lista de TAC_GLOBAL_DECL, y lo que no
 * es ni una cosa ni la otra se reporta como error interno.
 */
static OperandLocation resolveOperandLocation(const TACOperand *op, const FunctionFrame *frame, int *offset) {
    *offset = getSlotOffset(frame, op->name);
    if (*offset != 0) {
        return LOCATION_STACK;
    }

    if (op->kind == OPERAND_VARIABLE && isDeclaredGlobal(op)) {
        return LOCATION_GLOBAL;
    }

    fprintf(stderr,
            "\033[1;31mError interno del generador de codigo objeto:\033[0m el operando '%s'"
            " no tiene lugar asignado en el frame de '%s' ni esta declarado como variable global.\n",
            op->name[0] != '\0' ? op->name : "(sin nombre)",
            (frame != NULL && frame->functionName[0] != '\0') ? frame->functionName : "(sin funcion)");
    objectCodeErrors++;

    return LOCATION_UNRESOLVED;
}

/* ------------------------------------------------------------------ */
/* Punto flotante (SSE, precision simple)                              */
/* ------------------------------------------------------------------ */

/*
 * Los reales se manejan con el banco de registros XMM y mnemonicos propios
 * (movss, addss, comiss...), completamente separados del camino entero que
 * modela `asm_instruction.h`. Por eso esta parte emite texto directamente en
 * vez de construir AsmInstruction: son dos juegos de instrucciones distintos y
 * mezclarlos en el mismo enum obligaria a duplicar el enum de registros.
 */

#define MAX_FLOAT_CONSTANTS 256
#define MAX_XMM_ARG_REGISTERS 8

static const char *XMM_ARG_REGISTERS[] = {
    "%xmm0", "%xmm1", "%xmm2", "%xmm3", "%xmm4", "%xmm5", "%xmm6", "%xmm7"
};

typedef struct {
    float value;
    char label[16];
} FloatConstant;

static FloatConstant floatConstants[MAX_FLOAT_CONSTANTS];
static int floatConstantCount = 0;

/* Patron de bits IEEE-754 del real, que es lo que entiende la directiva .long */
static unsigned int getFloatBits(float value) {
    union { float asFloat; unsigned int asBits; } conversion;
    conversion.asFloat = value;
    return conversion.asBits;
}

/*
 * Registra una constante real y devuelve la etiqueta con la que referenciarla.
 *
 * `movss` no acepta operandos inmediatos, asi que todo literal real tiene que
 * vivir en memoria. Se reutiliza la etiqueta si el mismo valor ya aparecio.
 */
static const char *internFloatConstant(float value) {
    for (int i = 0; i < floatConstantCount; i++) {
        if (getFloatBits(floatConstants[i].value) == getFloatBits(value)) {
            return floatConstants[i].label;
        }
    }

    if (floatConstantCount >= MAX_FLOAT_CONSTANTS) {
        return floatConstants[0].label; /* acotado por el prescan; defensivo */
    }

    FloatConstant *constant = &floatConstants[floatConstantCount];
    constant->value = value;
    snprintf(constant->label, sizeof(constant->label), ".LC%d", floatConstantCount);
    floatConstantCount++;

    return constant->label;
}

static int isFloatOperand(const TACOperand *op) {
    return op != NULL && op->type == TYPE_FLOAT;
}

static const char *getGlobalName(const TACOperand *op);
static void storeOperand(const TACOperand *op, const char *reg, const FunctionFrame *frame);

/* Carga un operando real en el registro XMM indicado. */
static void loadFloatOperand(const TACOperand *op, const char *xmmRegister, const FunctionFrame *frame) {
    if (op == NULL) return;

    if (op->kind == OPERAND_CONSTANT) {
        buildStringf("    movss %s(%%rip), %s\n", internFloatConstant(op->value.float_val), xmmRegister);
        return;
    }

    int offset;
    OperandLocation location = resolveOperandLocation(op, frame, &offset);
    if (location == LOCATION_UNRESOLVED) return;

    if (location == LOCATION_STACK) {
        buildStringf("    movss %d(%%rbp), %s\n", offset, xmmRegister);
    } else {
        buildStringf("    movss %s(%%rip), %s\n", getGlobalName(op), xmmRegister);
    }
}

/* Guarda el contenido de un registro XMM en el destino del operando. */
static void storeFloatOperand(const TACOperand *op, const char *xmmRegister, const FunctionFrame *frame) {
    if (op == NULL) return;

    int offset;
    OperandLocation location = resolveOperandLocation(op, frame, &offset);
    if (location == LOCATION_UNRESOLVED) return;

    if (location == LOCATION_STACK) {
        buildStringf("    movss %s, %d(%%rbp)\n", xmmRegister, offset);
    } else {
        buildStringf("    movss %s, %s(%%rip)\n", xmmRegister, getGlobalName(op));
    }
}

/* result = arg1 <op> arg2 con aritmetica de simple precision. */
static void emitFloatBinaryOp(const TACInstruction *inst, const char *mnemonic, const FunctionFrame *frame) {
    loadFloatOperand(&inst->firstOperand, "%xmm0", frame);
    loadFloatOperand(&inst->secondOperand, "%xmm1", frame);
    buildStringf("    %s %%xmm1, %%xmm0\n", mnemonic);
    storeFloatOperand(&inst->result, "%xmm0", frame);
}

/*
 * Comparacion de reales. `comiss` deja el resultado en los flags igual que una
 * comparacion sin signo, asi que las condiciones son `seta`/`setb` y no las
 * `setg`/`setl` con signo que usa el camino entero. El resultado es un boolean,
 * o sea un entero, y por eso se guarda con el store entero.
 */
static void emitFloatRelationalOp(const TACInstruction *inst, const char *setcc, const FunctionFrame *frame) {
    loadFloatOperand(&inst->firstOperand, "%xmm0", frame);
    loadFloatOperand(&inst->secondOperand, "%xmm1", frame);
    buildStringf("    comiss %%xmm1, %%xmm0\n");
    buildStringf("    %s %%al\n", setcc);
    buildStringf("    movzbl %%al, %%eax\n");
    storeOperand(&inst->result, "%eax", frame);
}

static const char *getGlobalName(const TACOperand *op) {
    if (op->symbol != NULL && op->symbol->id != NULL) {
        return op->symbol->id;
    }
    return op->name;
}

static void loadOperand(const TACOperand *op, const char *reg, const FunctionFrame *frame) {
    if (op == NULL) return;

    if (op->kind == OPERAND_CONSTANT) { // en caso de que sea valor inmediato
        AsmInstruction immediate = asmLoad(opRegStr(reg), opImm(op->value.int_val));
        emitAsm(&immediate);
        return;
    }

    int offset;
    OperandLocation location = resolveOperandLocation(op, frame, &offset);
    if (location == LOCATION_UNRESOLVED) return;

    AsmInstruction inst = (location == LOCATION_STACK)
        ? asmLoad(opRegStr(reg), opMem(offset))
        : asmLoad(opRegStr(reg), opGlobal(getGlobalName(op)));
    emitAsm(&inst);
}

static void storeOperand(const TACOperand *op, const char *reg, const FunctionFrame *frame) {
    if (op == NULL) return;

    int offset;
    OperandLocation location = resolveOperandLocation(op, frame, &offset);
    if (location == LOCATION_UNRESOLVED) return;

    AsmInstruction inst = (location == LOCATION_STACK)
        ? asmStore(opMem(offset), opRegStr(reg))
        : asmStore(opGlobal(getGlobalName(op)), opRegStr(reg));
    emitAsm(&inst);
}

/**
 * @brief Obtiene la cadena representativa de un registro para el emisor.
 */
static const char *getRegName(AsmRegister reg) {
    switch (reg) {
        case ASM_RAX: return "%eax";
        case ASM_RDX: return "%edx";
        case ASM_RCX: return "%ecx";
        default:      return "%eax";
    }
}

/**
 * @brief Carga un operando TAC (constante, stack slot o global) en el registro x86 especificado.
 * @param reg Registro destino (ej. ASM_RAX -> %eax).
 * @param op Operando TAC a cargar.
 * @param frame Marco de pila de la función actual.
 */
static void loadRegisterBasedOnOperand(AsmRegister reg, const TACOperand *op, const FunctionFrame *frame) {
    loadOperand(op, getRegName(reg), frame);
}

/**
 * @brief Guarda el contenido de un registro x86 en la posición del operando TAC destino (stack o global).
 * @param op Operando TAC de destino.
 * @param reg Registro origen cuyo valor se guardará.
 * @param frame Marco de pila de la función actual.
 */
static void storeRegisterToOperand(const TACOperand *op, AsmRegister reg, const FunctionFrame *frame) {
    storeOperand(op, getRegName(reg), frame);
}

/**
 * @brief Abstracción para operaciones binarias aritmético-lógicas (ADD, SUB, MUL, AND, OR):
 * 1. Carga arg1 en %eax
 * 2. Carga arg2 en %edx
 * 3. Ejecuta la operación: op %edx, %eax
 * 4. Guarda %eax en result
 * @param inst Instrucción TAC binaria.
 * @param op Código de operación Assembly.
 * @param frame Marco de pila actual.
 */
static void emitBinaryOp(const TACInstruction *inst, AsmOpcode op, const FunctionFrame *frame) {
    loadRegisterBasedOnOperand(ASM_RAX, &inst->firstOperand, frame);
    loadRegisterBasedOnOperand(ASM_RDX, &inst->secondOperand, frame);
    AsmInstruction asmInst = asmOpReg(op, ASM_RAX, ASM_RDX);
    emitAsm(&asmInst);
    storeRegisterToOperand(&inst->result, ASM_RAX, frame);
}

/**
 * @brief Abstracción para operaciones relacionales (<, >, ==):
 * 1. Carga arg1 en %eax
 * 2. Carga arg2 en %edx
 * 3. cmpl %edx, %eax
 * 4. set<cond> %al + movzbl %al, %eax
 * 5. Guarda el booleano resultante en result
 * @param inst Instrucción TAC relacional.
 * @param setcc Mnemónico de condición (ej. "setl", "setg", "sete").
 * @param frame Marco de pila actual.
 */
static void emitRelationalOp(const TACInstruction *inst, const char *setcc, const FunctionFrame *frame) {
    loadRegisterBasedOnOperand(ASM_RAX, &inst->firstOperand, frame);
    loadRegisterBasedOnOperand(ASM_RDX, &inst->secondOperand, frame);
    AsmInstruction cmpInst = asmOpReg(ASM_CMP, ASM_RAX, ASM_RDX);
    emitAsm(&cmpInst);
    buildStringf("    %s %%al\n", setcc);
    buildStringf("    movzbl %%al, %%eax\n");
    storeRegisterToOperand(&inst->result, ASM_RAX, frame);
}

/**
 * @brief Abstracción para división entera y módulo, que en x86-64 son la misma
 * instrucción leída desde registros distintos.
 *
 * `idivl divisor` divide el par %edx:%eax por el divisor, deja el cociente en
 * %eax y el resto en %edx. De ahí tres condiciones que hay que respetar:
 *
 * 1. El dividendo va en %eax.
 * 2. Antes de dividir hay que extender su signo a %edx con `cltd`; sin eso,
 *    %edx conserva basura y el cociente de cualquier división sale mal.
 * 3. El divisor nunca puede ir en %edx, justamente porque `cltd` lo pisa. Por
 *    eso se usa %ecx como registro auxiliar, y no el %edx que usan el resto de
 *    las operaciones binarias.
 *
 * @param inst Instrucción TAC de división o módulo.
 * @param frame Marco de pila actual.
 * @param resultReg Registro del que se toma el resultado: ASM_RAX para el
 *        cociente (TAC_DIV) o ASM_RDX para el resto (TAC_MOD).
 */
static void emitDivisionOp(const TACInstruction *inst, const FunctionFrame *frame, AsmRegister resultReg) {
    loadRegisterBasedOnOperand(ASM_RAX, &inst->firstOperand, frame); /* dividendo */
    loadRegisterBasedOnOperand(ASM_RCX, &inst->secondOperand, frame); /* divisor */

    AsmInstruction signExtend = asmCltd();
    emitAsm(&signExtend);

    AsmInstruction divide = asmOpUnary(ASM_IDIV, ASM_RCX);
    emitAsm(&divide);

    storeRegisterToOperand(&inst->result, resultReg, frame);
}

/**
 * @brief Traduce un `param` del TAC: copia el valor del argumento a un slot
 *        propio del frame y lo apila como pendiente.
 *
 * El valor se copia **en el momento**, no se recuerda el operando para leerlo
 * recién antes del `call`. La diferencia importa cuando un argumento posterior
 * tiene efectos colaterales sobre uno anterior: en `f(g, modifica())`, donde
 * `modifica()` escribe la global `g`, la letra exige que los argumentos se
 * evalúen de izquierda a derecha, así que `g` tiene que leerse antes de que
 * `modifica()` corra.
 */
static void emitParam(const TACInstruction *inst, FunctionFrame *frame) {
    if (frame->pendingParamCount >= MAX_PENDING_PARAMS) {
        return; /* el prescan ya acotó esto; defensivo */
    }

    int slot = frame->pendingParamSlots[frame->pendingParamCount];

    if (isFloatOperand(&inst->firstOperand)) {
        loadFloatOperand(&inst->firstOperand, "%xmm0", frame);
        buildStringf("    movss %%xmm0, %d(%%rbp)\n", slot);
    } else {
        loadRegisterBasedOnOperand(ASM_RAX, &inst->firstOperand, frame);
        AsmInstruction store = asmStore(opMem(slot), opReg(ASM_RAX));
        emitAsm(&store);
    }

    frame->pendingParamTypes[frame->pendingParamCount] = inst->firstOperand.type;
    frame->pendingParamCount++;
}

/**
 * @brief Traduce un `call` del TAC siguiendo la convención System V AMD64.
 *
 * Consume los últimos N argumentos pendientes, siendo N la aridad que trae la
 * propia instrucción TAC: eso es lo que hace que las llamadas anidadas no se
 * pisen entre sí, porque los argumentos de la llamada interna siempre son los
 * más recientes cuando aparece su `call`.
 *
 * Los primeros seis argumentos van en `%edi, %esi, %edx, %ecx, %r8d, %r9d` y
 * el resto por la pila, en orden inverso. El stack tiene que quedar alineado a
 * 16 bytes en el momento del `call`, así que si la cantidad de argumentos
 * empujados es impar se agrega un relleno de 8 bytes.
 */
static void emitCall(const TACInstruction *inst, FunctionFrame *frame) {
    int argumentCount = (inst->secondOperand.kind == OPERAND_CONSTANT) ? inst->secondOperand.value.int_val : 0;
    if (argumentCount > frame->pendingParamCount) {
        argumentCount = frame->pendingParamCount;
    }

    int base = frame->pendingParamCount - argumentCount;

    /* System V clasifica cada argumento por separado: los enteros consumen los
     * seis registros de proposito general y los reales los ocho XMM. Un
     * argumento va a la pila solo cuando se agotaron los registros de *su*
     * clase, asi que la cuenta no se puede hacer con un unico indice. */
    int registerIndex[MAX_PENDING_PARAMS];
    int goesToStack[MAX_PENDING_PARAMS];
    int integerIndex = 0;
    int floatIndex = 0;
    int stackArguments = 0;

    for (int i = 0; i < argumentCount; i++) {
        if (frame->pendingParamTypes[base + i] == TYPE_FLOAT) {
            goesToStack[i] = (floatIndex >= MAX_XMM_ARG_REGISTERS);
            registerIndex[i] = goesToStack[i] ? -1 : floatIndex++;
        } else {
            goesToStack[i] = (integerIndex >= MAX_ARG_REGISTERS);
            registerIndex[i] = goesToStack[i] ? -1 : integerIndex++;
        }
        if (goesToStack[i]) stackArguments++;
    }

    int padding = (stackArguments % 2 == 1) ? 8 : 0;
    if (padding > 0) {
        buildStringf("    subq $%d, %%rsp\n", padding);
    }

    /* Del último al primero de los que van por pila: el callee espera el primero
     * de ellos en 16(%rbp). */
    for (int i = argumentCount - 1; i >= 0; i--) {
        if (!goesToStack[i]) continue;

        AsmInstruction load = asmLoad(opReg(ASM_RAX), opMem(frame->pendingParamSlots[base + i]));
        emitAsm(&load);
        buildStringf("    pushq %%rax\n");
    }

    /* Los que van en registros, ya sin riesgo de que una llamada intermedia los pise. */
    for (int i = 0; i < argumentCount; i++) {
        if (goesToStack[i]) continue;

        int slot = frame->pendingParamSlots[base + i];
        if (frame->pendingParamTypes[base + i] == TYPE_FLOAT) {
            buildStringf("    movss %d(%%rbp), %s\n", slot, XMM_ARG_REGISTERS[registerIndex[i]]);
        } else {
            AsmInstruction load = asmLoad(opRegStr(ARG_REGISTERS_32[registerIndex[i]]), opMem(slot));
            emitAsm(&load);
        }
    }

    buildStringf("    call %s\n", inst->firstOperand.name);

    if (stackArguments > 0 || padding > 0) {
        buildStringf("    addq $%d, %%rsp\n", stackArguments * 8 + padding);
    }

    frame->pendingParamCount = base;

    if (inst->result.kind != OPERAND_NONE) {
        if (isFloatOperand(&inst->result)) {
            storeFloatOperand(&inst->result, "%xmm0", frame);
        } else {
            storeOperand(&inst->result, "%eax", frame);
        }
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

    /* El sistema operativo toma %eax como codigo de salida del proceso. En un
     * `void main()` nadie escribio ese registro con un valor deliberado, asi
     * que conservaria el residuo de la ultima operacion y un programa correcto
     * terminaria con un exit code distinto de cero. El resto de los metodos no
     * necesitan esto: los no-void siempre retornan un valor, y en los void el
     * llamador ignora %eax. */
    if (frame != NULL && frame->returnType == TYPE_VOID
        && strcmp(frame->functionName, "main") == 0) {
        AsmInstruction zero = asmLoadImm(ASM_RAX, 0);
        emitAsm(&zero);
    }

    buildStringf("    leave\n");
    buildStringf("    ret\n");
}

void generateInstruction(const TACInstruction *instruction, FunctionFrame *frame) {
    if (instruction == NULL || frame == NULL) return;

    switch (instruction->operation) {
        case TAC_PARAM_DECL: {
            int offset = getSlotOffset(frame, instruction->firstOperand.name);
            int isFloat = isFloatOperand(&instruction->firstOperand);

            if (offset != 0) {
                if (isFloat && frame->floatParamCount < MAX_XMM_ARG_REGISTERS) {
                    buildStringf("    movss %s, %d(%%rbp)\n",
                                 XMM_ARG_REGISTERS[frame->floatParamCount], offset);
                } else if (!isFloat && frame->integerParamCount < MAX_ARG_REGISTERS) {
                    buildStringf("    movl %s, %d(%%rbp)\n",
                                 ARG_REGISTERS_32[frame->integerParamCount], offset);
                } else {
                    /* Se agotaron los registros de su clase: el llamador lo dejó
                     * en su propio frame. 16(%rbp) saltea el %rbp guardado y la
                     * direccion de retorno, y de ahi en adelante van de a 8. */
                    int callerOffset = 16 + frame->stackParamCount * 8;
                    buildStringf("    movl %d(%%rbp), %%eax\n", callerOffset);
                    buildStringf("    movl %%eax, %d(%%rbp)\n", offset);
                }
            }

            if (isFloat && frame->floatParamCount < MAX_XMM_ARG_REGISTERS) frame->floatParamCount++;
            else if (!isFloat && frame->integerParamCount < MAX_ARG_REGISTERS) frame->integerParamCount++;
            else frame->stackParamCount++;

            frame->paramCount++;
            break;
        }

        case TAC_LOCAL_DECL:
            break;

        case TAC_ASSIGN:
            if (isFloatOperand(&instruction->result)) {
                loadFloatOperand(&instruction->firstOperand, "%xmm0", frame);
                storeFloatOperand(&instruction->result, "%xmm0", frame);
            } else {
                loadRegisterBasedOnOperand(ASM_RAX, &instruction->firstOperand, frame);
                storeRegisterToOperand(&instruction->result, ASM_RAX, frame);
            }
            break;

        case TAC_ADD:
            if (isFloatOperand(&instruction->result)) emitFloatBinaryOp(instruction, "addss", frame);
            else emitBinaryOp(instruction, ASM_ADD, frame);
            break;

        case TAC_SUB:
            if (isFloatOperand(&instruction->result)) emitFloatBinaryOp(instruction, "subss", frame);
            else emitBinaryOp(instruction, ASM_SUB, frame);
            break;

        case TAC_MUL:
            if (isFloatOperand(&instruction->result)) emitFloatBinaryOp(instruction, "mulss", frame);
            else emitBinaryOp(instruction, ASM_IMUL, frame);
            break;

        case TAC_DIV:
            /* En enteros el cociente queda en %eax; en reales es una division directa */
            if (isFloatOperand(&instruction->result)) emitFloatBinaryOp(instruction, "divss", frame);
            else emitDivisionOp(instruction, frame, ASM_RAX);
            break;

        case TAC_MOD:
            /* El resto queda en %edx */
            emitDivisionOp(instruction, frame, ASM_RDX);
            break;

        case TAC_NEG: {
            if (isFloatOperand(&instruction->result)) {
                /* Se niega restando desde cero: xorps con la mascara de signo
                 * seria una instruccion menos, pero exigiria otra constante en
                 * .rodata y aca pesa mas la claridad que el ciclo de mas. */
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
            AsmInstruction inst = asmOpImm(ASM_XOR, ASM_RAX, 1);
            emitAsm(&inst);
            storeRegisterToOperand(&instruction->result, ASM_RAX, frame);
            break;
        }

        /* El resultado de un relacional es boolean, asi que el tipo que decide
         * como compararlos esta en los operandos, no en el destino. */
        case TAC_LT:
            if (isFloatOperand(&instruction->firstOperand)) emitFloatRelationalOp(instruction, "setb", frame);
            else emitRelationalOp(instruction, "setl", frame);
            break;

        case TAC_GT:
            if (isFloatOperand(&instruction->firstOperand)) emitFloatRelationalOp(instruction, "seta", frame);
            else emitRelationalOp(instruction, "setg", frame);
            break;

        case TAC_EQ:
            if (isFloatOperand(&instruction->firstOperand)) emitFloatRelationalOp(instruction, "sete", frame);
            else emitRelationalOp(instruction, "sete", frame);
            break;

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

        case TAC_RETURN:
            if (instruction->firstOperand.kind != OPERAND_NONE) {
                /* La convencion devuelve los enteros en %eax y los reales en %xmm0 */
                if (isFloatOperand(&instruction->firstOperand)) {
                    loadFloatOperand(&instruction->firstOperand, "%xmm0", frame);
                } else {
                    loadOperand(&instruction->firstOperand, "%eax", frame);
                }
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
        if (inst->operation == TAC_GLOBAL_DECL) {
            if (!hasGlobals) {
                buildStringf(".data\n");
                hasGlobals = 1;
            }
            registerGlobalVariable(&inst->firstOperand);

            const char *name = getGlobalName(&inst->firstOperand);
            buildStringf(".globl %s\n", name); // emite para poder usado de forma global
            buildStringf(".align 4\n"); // guarda 4 bytes de lugar
            buildStringf("%s:\n", name);
            buildStringf("    .long 0\n"); // en el lenguaje siempre comienzan vacios
        }
    }

    if (hasGlobals) {
        buildStringf("\n");
    }
}

/*
 * Recorre el TAC registrando todas las constantes reales y emite la seccion de
 * solo lectura con sus patrones de bits.
 *
 * Se hace en una pasada previa porque la seccion tiene que estar completa antes
 * de empezar a emitir codigo que la referencie.
 */
static void emitFloatConstantPool(const TACList *tacList) {
    floatConstantCount = 0;

    for (const TACInstruction *inst = tacList->head; inst != NULL; inst = inst->next) {
        const TACOperand *operands[] = { &inst->firstOperand, &inst->secondOperand, &inst->result };

        for (int i = 0; i < 3; i++) {
            if (operands[i]->kind == OPERAND_CONSTANT && operands[i]->type == TYPE_FLOAT) {
                internFloatConstant(operands[i]->value.float_val);
            }
        }
    }

    if (floatConstantCount == 0) return;

    buildStringf(".section .rodata\n");
    for (int i = 0; i < floatConstantCount; i++) {
        buildStringf(".align 4\n");
        buildStringf("%s:\n", floatConstants[i].label);
        buildStringf("    .long %u\n", getFloatBits(floatConstants[i].value));
    }
    buildStringf("\n");
}

void generateAssembly(const TACList *tacList) {
    if (tacList == NULL) return;

    globalVariableCount = 0;
    objectCodeErrors = 0;

    /* Constantes reales y variables globales, antes del codigo */
    emitFloatConstantPool(tacList);
    emitGlobals(tacList);

    buildStringf(".text\n");

    const TACInstruction *instruction = tacList->head;

    while (instruction != NULL) {

        if (instruction->operation == TAC_BEGIN_FUNC) {

            FunctionFrame *frame = createFrame(instruction->result.name);
            /* tacMethod() guarda el tipo de retorno en el operando del begin_func */
            frame->returnType = instruction->result.type;
            const TACInstruction *current = instruction->next; // recorrer cuerpo de la funcion

            while (current != NULL && current->operation != TAC_END_FUNC) {

                switch (current->operation) {

                    case TAC_PARAM_DECL:
                        addSlot(frame, &current->firstOperand);
                        break;

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

            /* Pico de argumentos acumulados sin su call: define cuantos slots
             * scratch hace falta reservar para el pasaje de parametros. */
            int pending = 0;
            for (const TACInstruction *scan = instruction->next;
                 scan != NULL && scan->operation != TAC_END_FUNC;
                 scan = scan->next) {

                if (scan->operation == TAC_PARAM) {
                    pending++;
                    if (pending > frame->maxPendingParams) {
                        frame->maxPendingParams = pending;
                    }
                } else if (scan->operation == TAC_CALL) {
                    int arity = (scan->secondOperand.kind == OPERAND_CONSTANT) ? scan->secondOperand.value.int_val : 0;
                    pending -= arity;
                    if (pending < 0) pending = 0;
                }
            }

            if (frame->maxPendingParams > MAX_PENDING_PARAMS) {
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

    /* Sin esta seccion el enlazador asume que el programa necesita un stack
     * ejecutable y avisa en cada compilacion. Como la etapa de assembly invoca
     * a gcc, ese aviso romperia el requisito de que una compilacion exitosa no
     * imprima nada. */
    buildStringf("\n.section .note.GNU-stack,\"\",@progbits\n");
}
