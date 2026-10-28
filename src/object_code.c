#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "object_code.h"
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

static const char *getGlobalName(const TACOperand *op) {
    if (op->symbol != NULL && op->symbol->id != NULL) {
        return op->symbol->id;
    }
    return op->name;
}

static void loadOperand(const TACOperand *op, const char *reg, const FunctionFrame *frame) {
    if (op == NULL) return;

    AsmInstruction inst;
    if (op->kind == OPERAND_CONSTANT) { // en caso de que sea valor inmediato
        inst = asmLoad(opRegStr(reg), opImm(op->value.int_val));
    } else {
        int offset = getSlotOffset(frame, op->name);
        if (offset != 0) {
            inst = asmLoad(opRegStr(reg), opMem(offset));
        } else {
            const char *name = getGlobalName(op);
            inst = asmLoad(opRegStr(reg), opGlobal(name));
        }
    }
    emitAsm(&inst);
}

static void storeOperand(const TACOperand *op, const char *reg, const FunctionFrame *frame) {
    if (op == NULL) return;

    int offset = getSlotOffset(frame, op->name);
    AsmInstruction inst;
    if (offset != 0) { // variable local, parámetro o temporal en el stack
        inst = asmStore(opMem(offset), opRegStr(reg));
    } else {           // no está en el frame: es variable global
        const char *name = getGlobalName(op);
        inst = asmStore(opGlobal(name), opRegStr(reg));
    }
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
    loadRegisterBasedOnOperand(ASM_RAX, &inst->arg1, frame);
    loadRegisterBasedOnOperand(ASM_RDX, &inst->arg2, frame);
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
    loadRegisterBasedOnOperand(ASM_RAX, &inst->arg1, frame);
    loadRegisterBasedOnOperand(ASM_RDX, &inst->arg2, frame);
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
    loadRegisterBasedOnOperand(ASM_RAX, &inst->arg1, frame); /* dividendo */
    loadRegisterBasedOnOperand(ASM_RCX, &inst->arg2, frame); /* divisor */

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

    loadRegisterBasedOnOperand(ASM_RAX, &inst->arg1, frame);
    AsmInstruction store = asmStore(opMem(slot), opReg(ASM_RAX));
    emitAsm(&store);

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
    int argumentCount = (inst->arg2.kind == OPERAND_CONSTANT) ? inst->arg2.value.int_val : 0;
    if (argumentCount > frame->pendingParamCount) {
        argumentCount = frame->pendingParamCount;
    }

    int base = frame->pendingParamCount - argumentCount;
    int stackArguments = (argumentCount > MAX_ARG_REGISTERS) ? argumentCount - MAX_ARG_REGISTERS : 0;
    int padding = (stackArguments % 2 == 1) ? 8 : 0;

    if (padding > 0) {
        buildStringf("    subq $%d, %%rsp\n", padding);
    }

    /* Del último al séptimo: el callee espera el 7mo en 16(%rbp). */
    for (int i = argumentCount - 1; i >= MAX_ARG_REGISTERS; i--) {
        AsmInstruction load = asmLoad(opReg(ASM_RAX), opMem(frame->pendingParamSlots[base + i]));
        emitAsm(&load);
        buildStringf("    pushq %%rax\n");
    }

    /* Los primeros seis, ya sin riesgo de que una llamada intermedia los pise. */
    int registerArguments = (argumentCount < MAX_ARG_REGISTERS) ? argumentCount : MAX_ARG_REGISTERS;
    for (int i = 0; i < registerArguments; i++) {
        AsmInstruction load = asmLoad(opRegStr(ARG_REGISTERS_32[i]), opMem(frame->pendingParamSlots[base + i]));
        emitAsm(&load);
    }

    buildStringf("    call %s\n", inst->arg1.name);

    if (stackArguments > 0 || padding > 0) {
        buildStringf("    addq $%d, %%rsp\n", stackArguments * 8 + padding);
    }

    frame->pendingParamCount = base;

    if (inst->result.kind != OPERAND_NONE) {
        storeOperand(&inst->result, "%eax", frame);
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

    switch (instruction->op) {
        case TAC_PARAM_DECL: {
            int offset = getSlotOffset(frame, instruction->arg1.name);
            if (offset != 0) {
                if (frame->paramCount < MAX_ARG_REGISTERS) {
                    /* Los primeros seis llegan en registros. */
                    buildStringf("    movl %s, %d(%%rbp)\n",
                                 ARG_REGISTERS_32[frame->paramCount], offset);
                } else {
                    /* Del séptimo en adelante los dejó el llamador en su propio
                     * frame: 16(%rbp) saltea el %rbp guardado y la direccion de
                     * retorno, y de ahi en adelante van de a 8 bytes. */
                    int callerOffset = 16 + (frame->paramCount - MAX_ARG_REGISTERS) * 8;
                    buildStringf("    movl %d(%%rbp), %%eax\n", callerOffset);
                    buildStringf("    movl %%eax, %d(%%rbp)\n", offset);
                }
            }
            frame->paramCount++;
            break;
        }

        case TAC_LOCAL_DECL:
            break;

        case TAC_ASSIGN:
            loadRegisterBasedOnOperand(ASM_RAX, &instruction->arg1, frame);
            storeRegisterToOperand(&instruction->result, ASM_RAX, frame);
            break;

        case TAC_ADD:
            emitBinaryOp(instruction, ASM_ADD, frame);
            break;

        case TAC_SUB:
            emitBinaryOp(instruction, ASM_SUB, frame);
            break;

        case TAC_MUL:
            emitBinaryOp(instruction, ASM_IMUL, frame);
            break;

        case TAC_DIV:
            /* El cociente queda en %eax */
            emitDivisionOp(instruction, frame, ASM_RAX);
            break;

        case TAC_MOD:
            /* El resto queda en %edx */
            emitDivisionOp(instruction, frame, ASM_RDX);
            break;

        case TAC_NEG: {
            loadRegisterBasedOnOperand(ASM_RAX, &instruction->arg1, frame);
            AsmInstruction negate = asmOpUnary(ASM_NEG, ASM_RAX);
            emitAsm(&negate);
            storeRegisterToOperand(&instruction->result, ASM_RAX, frame);
            break;
        }

        case TAC_NOT: {
            loadRegisterBasedOnOperand(ASM_RAX, &instruction->arg1, frame);
            AsmInstruction inst = asmOpImm(ASM_XOR, ASM_RAX, 1);
            emitAsm(&inst);
            storeRegisterToOperand(&instruction->result, ASM_RAX, frame);
            break;
        }

        case TAC_LT:
            emitRelationalOp(instruction, "setl", frame);
            break;

        case TAC_GT:
            emitRelationalOp(instruction, "setg", frame);
            break;

        case TAC_EQ:
            emitRelationalOp(instruction, "sete", frame);
            break;

        case TAC_AND:
            emitBinaryOp(instruction, ASM_AND, frame);
            break;

        case TAC_OR:
            emitBinaryOp(instruction, ASM_OR, frame);
            break;

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
            emitParam(instruction, frame);
            break;

        case TAC_CALL:
            emitCall(instruction, frame);
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

void generateAssembly(const TACList *tacList) {
    if (tacList == NULL) return;

    /* Emisión de variables globales si existen */
    emitGlobals(tacList);

    buildStringf(".text\n");

    const TACInstruction *instruction = tacList->head;

    while (instruction != NULL) {

        if (instruction->op == TAC_BEGIN_FUNC) {

            FunctionFrame *frame = createFrame(instruction->result.name);
            /* tacMethod() guarda el tipo de retorno en el operando del begin_func */
            frame->returnType = instruction->result.type;
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

            /* Pico de argumentos acumulados sin su call: define cuantos slots
             * scratch hace falta reservar para el pasaje de parametros. */
            int pending = 0;
            for (const TACInstruction *scan = instruction->next;
                 scan != NULL && scan->op != TAC_END_FUNC;
                 scan = scan->next) {

                if (scan->op == TAC_PARAM) {
                    pending++;
                    if (pending > frame->maxPendingParams) {
                        frame->maxPendingParams = pending;
                    }
                } else if (scan->op == TAC_CALL) {
                    int arity = (scan->arg2.kind == OPERAND_CONSTANT) ? scan->arg2.value.int_val : 0;
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

    /* Sin esta seccion el enlazador asume que el programa necesita un stack
     * ejecutable y avisa en cada compilacion. Como la etapa de assembly invoca
     * a gcc, ese aviso romperia el requisito de que una compilacion exitosa no
     * imprima nada. */
    buildStringf("\n.section .note.GNU-stack,\"\",@progbits\n");
}
