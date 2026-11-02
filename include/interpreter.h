#ifndef INTERPRETER_H
#define INTERPRETER_H

/**
 * @file interpreter.h
 * @brief Intérprete para la ejecución directa del Árbol de Sintaxis Abstracta (AST).
 */

#include "ast.h"
#include "symbol_table.h"

/**
 * @brief Interpreta y ejecuta el AST a partir de la raíz dada.
 *
 * Registra variables globales y declaraciones de métodos en el ámbito raíz y ejecuta
 * el método `main`.
 *
 * @param root Raíz del AST del programa.
 * @param parent Ámbito padre (NULL para el ámbito global).
 * @return Puntero al ámbito generado con los símbolos y valores finales.
 */
Scope *interpreteSemantics(NodeAST *root, Scope *parent);

#endif // INTERPRETER_H
