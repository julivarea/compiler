#ifndef OBJECT_CODE_H
#define OBJECT_CODE_H

/**
 * @file object_code.h
 * @brief Generación de código objeto (Assembly x86-64) a partir de código intermedio (TAC).
 *
 * Este módulo recorre las instrucciones de tres direcciones (TAC) y traduce cada
 * una a instrucciones de Assembly x86-64 compatibles con GNU Assembler (GAS).
 * Gestiona la emisión de datos globales, constantes y código de funciones.
 */

#include "function_frame.h"
#include "tac_list.h"
#include "types.h"

/* ================================================================== */
/* 1. Función pública de entrada y consultas de la fase               */
/* ================================================================== */

/**
 * @brief Recorre la lista de TAC y genera el código Assembly x86-64 completo.
 *
 * @param tacList Puntero a la lista de instrucciones de tres direcciones a traducir.
 */
void generateAssembly(const TACList *tacList);

/**
 * @brief Obtiene la cantidad de errores internos detectados durante la última generación.
 *
 * @return Cantidad de errores internos registrados durante la generación.
 */
int getObjectCodeErrors(void);

/* ================================================================== */
/* 2. Suboperaciones de emisión y traducción                          */
/* ================================================================== */

/**
 * @brief Emite las instrucciones de prólogo de una función en Assembly x86-64.
 *
 * @param frame Puntero al marco con la configuración de la función actual.
 */
void emitPrologue(const FunctionFrame *frame);

/**
 * @brief Emite las instrucciones de epílogo de una función en Assembly x86-64.
 *
 * @param frame Puntero al marco de la función cuyo epílogo se emite.
 */
void emitEpilogue(const FunctionFrame *frame);

/**
 * @brief Genera las instrucciones Assembly correspondientes a una cuádrupla TAC.
 *
 * @param instruction Puntero a la instrucción TAC a traducir.
 * @param frame Puntero al marco de la función en curso.
 */
void generateInstruction(const TACInstruction *instruction, FunctionFrame *frame);

#endif // OBJECT_CODE_H
