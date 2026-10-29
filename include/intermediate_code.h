#ifndef INTERMEDIATE_CODE_H
#define INTERMEDIATE_CODE_H

/**
 * @file intermediate_code.h
 * @brief Pase de generación de código intermedio (TAC) sobre el Árbol de Sintaxis Abstracta (AST).
 */

#include "ast.h"
#include "tac_list.h"

/**
 * @brief Genera el código intermedio de tres direcciones para el programa completo.
 *
 * Recorre el AST a partir del bloque raíz, descarta cualquier secuencia de código
 * generada en una invocación previa y reinicia los contadores internos de
 * temporales y etiquetas.
 *
 * @param root Nodo raíz del AST a procesar (acepta NULL, en cuyo caso no emite código).
 */
void generateIntermediateCode(NodeAST *root);

/**
 * @brief Devuelve la lista de instrucciones TAC generada en la última invocación.
 *
 * Permite acceder a la estructura de código intermedio producida por @ref generateIntermediateCode
 * para su posterior procesamiento por las fases subsiguientes del compilador
 * (intérprete, optimizador o generador de código de máquina x86-64).
 *
 * @return Puntero constante a la lista de instrucciones TAC, o NULL si todavía no se ejecutó la generación.
 */
const TACList *getIntermediateCode(void);

#endif // INTERMEDIATE_CODE_H
