#ifndef FUNCTION_FRAME_H
#define FUNCTION_FRAME_H

/**
 * @file function_frame.h
 * @brief Estructuras y operaciones del marco de activación (stack frame) y ranuras de pila.
 */

#include "tac_operand.h"
#include "types.h"

/**
 * @brief Macro para alinear un valor hacia arriba a un múltiplo de 'alignment'.
 */
#define ALIGN_UP(value, alignment) (((value) + (alignment) - 1) & ~((alignment) - 1))

/**
 * @brief Cantidad máxima de argumentos acumulados sin su llamada asociada.
 *
 * Acota el anidamiento de llamadas dentro de los argumentos de otra llamada.
 */
#define MAX_PENDING_PARAMS 64

/**
 * @brief Representa una posición en el stack frame para una variable o temporal.
 */
typedef struct StackSlot {
    char name[TAC_NAME_LENGTH];
    int offset; /**< Desplazamiento relativo a %rbp (ej. -4, -8, etc.). */
} StackSlot;

/**
 * @brief Información del marco de activación (stack frame) de una función.
 */
typedef struct FunctionFrame {
    char functionName[TAC_NAME_LENGTH];
    DataType returnType; /**< Tipo de retorno del método, necesario en el epílogo. */
    StackSlot *slots;
    int slotCount;
    int slotCapacity;
    int nextOffset;   /**< Acumulador de bytes ocupados en el stack. */
    int frameSize;    /**< Tamaño total del frame alineado a 16 bytes. */
    int paramCount;   /**< Contador de parámetros procesados en el prólogo. */

    /**
     * Pila de argumentos pendientes de la llamada en curso.
     *
     * Cada param del TAC copia su valor a un slot propio del frame y apila
     * aquí el desplazamiento de ese slot; el call consume los últimos N.
     */
    int pendingParamSlots[MAX_PENDING_PARAMS];
    DataType pendingParamTypes[MAX_PENDING_PARAMS]; /**< Clase de cada argumento: entera o real. */
    int pendingParamCount; /**< Argumentos apilados sin su llamada correspondiente. */
    int maxPendingParams;  /**< Pico de la pila, calculado antes de emitir. */

    int integerParamCount; /**< Parámetros formales enteros ya ubicados en el prólogo. */
    int floatParamCount;   /**< Parámetros formales reales ya ubicados en el prólogo. */
    int stackParamCount;   /**< Parámetros formales que llegaron por la pila. */
} FunctionFrame;

/**
 * @brief Crea e inicializa un nuevo marco de activación para una función.
 *
 * Reserva memoria dinámica para una estructura FunctionFrame, inicializa
 * sus contadores, dimensiones y la tabla dinámica de ranuras (StackSlot).
 *
 * @param functionName Nombre identificador de la función (acepta NULL).
 * @return Puntero al nuevo FunctionFrame creado, o NULL si falló la asignación de memoria.
 */
FunctionFrame *createFrame(const char *functionName);

/**
 * @brief Libera la memoria ocupada por un marco de activación y sus ranuras.
 *
 * Libera el arreglo dinámico de ranuras de memoria asociado y la estructura
 * del marco si el puntero @p frame no es nulo.
 *
 * @param frame Puntero al marco de activación a liberar (acepta NULL).
 */
void freeFrame(FunctionFrame *frame);

/**
 * @brief Agrega una ranura en el marco para un operando si no existe previamente.
 *
 * Busca si el operando ya posee un desplazamiento asignado en @p frame. Si no
 * existe, asigna una nueva ranura incrementando el desplazamiento del stack frame.
 *
 * @param frame Puntero al marco de la función en edición.
 * @param operand Operando a registrar (variable local, parámetro o temporal).
 * @return Desplazamiento negativo asignado respecto a %rbp, o 0 si ocurrió un error.
 */
int addSlot(FunctionFrame *frame, const TACOperand *operand);

/**
 * @brief Reserva una ranura anónima en el marco, sin nombre asociado.
 *
 * Lo usan los argumentos pendientes de una llamada: no son variables ni
 * temporales del TAC, así que se buscan por posición en la pila de argumentos.
 *
 * @param frame Puntero al marco de la función.
 * @return Desplazamiento relativo a %rbp del slot reservado.
 */
int addAnonymousSlot(FunctionFrame *frame);

/**
 * @brief Obtiene el desplazamiento relativo a %rbp de un operando en el marco.
 *
 * Recorre linealmente las ranuras registradas en @p frame en busca del identificador
 * indicado por @p name.
 *
 * @param frame Puntero al marco de la función analizada.
 * @param name Nombre del operando buscado.
 * @return Desplazamiento relativo a %rbp (negativo), o 0 si no se encuentra.
 */
int getSlotOffset(const FunctionFrame *frame, const char *name);

#endif // FUNCTION_FRAME_H
