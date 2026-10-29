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

#include "types.h"
#include "tac_list.h"

/* ================================================================== */
/* 1. Función pública de entrada y consultas de la fase               */
/* ================================================================== */

/**
 * @brief Recorre la lista de TAC y genera el código Assembly x86-64 completo.
 *
 * Itera sobre la secuencia completa de cuádruplas TAC, identificando declaraciones
 * globales, subrutinas, asignaciones, saltos condicionales y llamadas a funciones.
 * Emite las directivas de ensamblador requeridas y el código ejecutable hacia
 * la salida estándar o acumulador correspondiente.
 *
 * @param tacList Puntero a la lista de instrucciones de tres direcciones a traducir.
 */
void generateAssembly(const TACList *tacList);

/**
 * @brief Obtiene la cantidad de errores internos detectados durante la última generación.
 *
 * Reporta inconsistencias del compilador encontradas durante el proceso de emisión,
 * como operandos sin asignación en memoria o desbordamientos del marco. Un valor
 * distinto de cero indica que el código ensamblador generado no es confiable.
 *
 * @return Cantidad de errores internos registrados durante la generación.
 */
int getObjectCodeErrors(void);

/* ================================================================== */
/* 2. Estructuras de datos del marco de activación (Stack Frame)      */
/* ================================================================== */

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

/* ================================================================== */
/* 3. Ciclo de vida y operaciones del marco de activación             */
/* ================================================================== */

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

/* ================================================================== */
/* 4. Suboperaciones de emisión y traducción                          */
/* ================================================================== */

/**
 * @brief Emite las instrucciones de prólogo de una función en Assembly x86-64.
 *
 * Configura el puntero de base (%rbp), reserva el espacio del marco alineado
 * a 16 bytes y transfiere los parámetros recibidos por registro o pila a sus ranuras.
 *
 * @param frame Puntero al marco con la configuración de la función actual.
 */
void emitPrologue(const FunctionFrame *frame);

/**
 * @brief Emite las instrucciones de epílogo de una función en Assembly x86-64.
 *
 * Restaura el estado del marco de activación (%rsp y %rbp), ajusta el valor
 * de retorno correspondiente y emite la instrucción de retorno ret.
 *
 * @param frame Puntero al marco de la función cuyo epílogo se emite.
 */
void emitEpilogue(const FunctionFrame *frame);

/**
 * @brief Genera las instrucciones Assembly correspondientes a una cuádrupla TAC.
 *
 * Traduce una instrucción elemental de tres direcciones (@p instruction) a su
 * equivalente en lenguaje ensamblador x86-64 utilizando los desplazamientos de @p frame.
 *
 * @param instruction Puntero a la instrucción TAC a traducir.
 * @param frame Puntero al marco de la función en curso.
 */
void generateInstruction(const TACInstruction *instruction, FunctionFrame *frame);

#endif // OBJECT_CODE_H
