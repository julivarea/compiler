#ifndef INTERMEDIATE_CODE_H
#define INTERMEDIATE_CODE_H

/**
 * @file intermediate_code.h
 * @brief Pase de generación de código intermedio (TAC) sobre el Árbol de Sintaxis Abstracta (AST).
 */

#include <stdbool.h>

#include "ast.h"
#include "tac.h"

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

/**
 * @brief Indica si la generación se abortó por un tipo fuera del alcance de la etapa.
 *
 * El enunciado acota el código intermedio y el código objeto a tipos enteros y
 * lógicos, así que un programa que usa `float` se rechaza en lugar de generarle
 * código. Permite al llamador distinguir ese rechazo —ya reportado con su línea—
 * de una falla interna del generador.
 *
 * @return true si la última generación se abortó por un tipo no soportado.
 */
bool hasUnsupportedTypeError(void);

#endif // INTERMEDIATE_CODE_H
