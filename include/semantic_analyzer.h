#ifndef SEMANTIC_ANALYZER_H
#define SEMANTIC_ANALYZER_H

/**
 * @file semantic_analyzer.h
 * @brief Pase de análisis semántico sobre el Árbol de Sintaxis Abstracta (AST).
 */

#include "types.h"
#include "ast.h"
#include "symbol_table.h"

/**
 * @brief Realiza el análisis semántico recorriendo el AST a partir de un nodo raíz.
 *
 * Construye la jerarquía de scopes, verifica declaraciones duplicadas o no declaradas,
 * y valida las reglas semánticas de las sentencias del lenguaje.
 *
 * Cuando se invoca sobre el scope global (@p parent == NULL) valida además la
 * presencia y la firma del método 'main', de modo que un único llamado alcanza
 * para dejar el programa completamente verificado.
 *
 * @param root Nodo raíz del AST a analizar.
 * @param parent Scope padre (NULL para el scope global).
 * @return Scope* Ámbito resultante creado para la raíz analizada.
 */
Scope* analyzeSemantics(NodeAST *root, Scope *parent);

#endif // SEMANTIC_ANALYZER_H
