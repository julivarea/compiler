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
 * @param root Nodo raíz del AST a procesar (acepta NULL, en cuyo caso retorna NULL).
 * @return Puntero constante a la lista de instrucciones TAC generadas, o NULL si root es NULL.
 */
const TACList *generateIntermediateCode(NodeAST *root);

#endif // INTERMEDIATE_CODE_H
