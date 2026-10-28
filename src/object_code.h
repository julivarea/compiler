#ifndef OBJECT_CODE_H
#define OBJECT_CODE_H

/**
 * @file object_code.h
 * @brief Generación de código objeto (Assembly x86-64) a partir de código intermedio (TAC).
 *
 * Este módulo recorre las instrucciones de tres direcciones (TAC) y traduce cada
 * una a instrucciones de Assembly x86-64 compatibles con GNU Assembler (GAS).
 * Gestiona el layout del stack frame para cada función (variables locales,
 * parámetros y temporales) y la emisión de datos globales y código.
 */

#include "tac.h"
#include "asm_instruction.h"

/**
 * Macro para alinear un valor hacia arriba a un múltiplo de 'alignment'.
 */
#define ALIGN_UP(value, alignment) (((value) + (alignment) - 1) & ~((alignment) - 1))

/**
 * @brief Representa una posición en el stack frame para una variable local,
 *        parámetro o temporal.
 */
typedef struct {
    char name[TAC_NAME_LENGTH];
    int offset; /**< Desplazamiento relativo a %rbp (ej. -4, -8, etc.) */
} StackSlot;

/**
 * @brief Cantidad máxima de argumentos acumulados sin su `call` correspondiente.
 *
 * Acota el anidamiento de llamadas dentro de los argumentos de otra llamada.
 * Con 64 entran casos como `f(g(h(...)), ...)` con mucho margen.
 */
#define MAX_PENDING_PARAMS 64

/**
 * @brief Información del stack frame de una función.
 */
typedef struct {
    char functionName[TAC_NAME_LENGTH];
    DataType returnType; /**< Tipo de retorno del método, necesario en el epílogo */
    StackSlot *slots;
    int slotCount;
    int slotCapacity;
    int nextOffset;   /**< Acumulador de bytes ocupados en el stack */
    int frameSize;    /**< Tamaño total del frame alineado a 16 bytes */
    int paramCount;   /**< Contador de parámetros procesados en el prólogo */

    /**
     * Pila de argumentos pendientes de la llamada en curso.
     *
     * Cada `param` del TAC copia su valor a un slot propio del frame y apila
     * aquí el desplazamiento de ese slot; el `call` consume los últimos N,
     * siendo N la aridad que trae la propia instrucción. Es una pila y no un
     * contador porque los argumentos de una llamada anidada se emiten en medio
     * de los de la llamada que la contiene.
     */
    int pendingParamSlots[MAX_PENDING_PARAMS];
    int pendingParamCount; /**< Argumentos apilados sin su `call` */
    int maxPendingParams;  /**< Pico de la pila, calculado antes de emitir */
} FunctionFrame;


/**
 * @brief Crea e inicializa un nuevo stack frame para una función.
 * @param funcName Nombre de la función.
 * @return Puntero al FunctionFrame creado, o NULL si falló la reserva de memoria.
 */
FunctionFrame *createFrame(const char *funcName);

/**
 * @brief Libera la memoria ocupada por un stack frame y sus slots.
 * @param frame Frame a liberar.
 */
void freeFrame(FunctionFrame *frame);

/**
 * @brief Agrega un slot en el stack frame para un operando si no existe previamente.
 * @param frame Frame de la función.
 * @param operand Operando a registrar (variable local, parámetro o temporal).
 * @return Desplazamiento relativo a %rbp asignado al slot.
 */
int addSlot(FunctionFrame *frame, const TACOperand *operand);

/**
 * @brief Obtiene el desplazamiento relativo a %rbp de un identificador en el frame.
 * @param frame Frame de la función.
 * @param name Nombre del operando buscado.
 * @return Desplazamiento en el stack (negativo), o 0 si no se encuentra.
 */
int getSlotOffset(const FunctionFrame *frame, const char *name);

/* --- Emisión de Assembly --- */

/**
 * @brief Emite el prólogo de una función en Assembly x86-64.
 * @param frame Frame con los datos de la función y tamaño del stack.
 */
void emitPrologue(const FunctionFrame *frame);

/**
 * @brief Emite el epílogo de una función en Assembly x86-64.
 * @param frame Frame de la función.
 */
void emitEpilogue(const FunctionFrame *frame);

/**
 * @brief Genera las instrucciones Assembly correspondientes a una instrucción TAC.
 * @param instruction Instrucción TAC a traducir.
 * @param frame Frame de la función actual.
 */
void generateInstruction(const TACInstruction *instruction, FunctionFrame *frame);

/**
 * @brief Recorre la lista de TAC y genera el código Assembly x86-64 completo.
 * @param tacList Lista de instrucciones de tres direcciones.
 */
void generateAssembly(const TACList *tacList);

/* Alias para compatibilidad con la especificación en pseudocódigo */
#define CREATE_FRAME(name)             createFrame(name)
#define FREE_FRAME(frame)              freeFrame(frame)
#define ADD_SLOT(frame, op)            addSlot((frame), (op))
#define EMIT_PROLOGUE(frame)           emitPrologue(frame)
#define EMIT_EPILOGUE(frame)           emitEpilogue(frame)
#define GENERATE_INSTRUCTION(inst, f)  generateInstruction((inst), (f))
#define GENERATE_ASSEMBLY(list)        generateAssembly(list)

#endif /* OBJECT_CODE_H */
