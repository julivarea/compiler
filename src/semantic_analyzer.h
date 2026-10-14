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
 * @param root Nodo raíz del AST a analizar.
 * @param parent Scope padre (NULL para el scope global).
 * @param returnType Tipo de retorno esperado en el ámbito actual.
 * @return Scope* Ámbito resultante creado para la raíz analizada.
 */
Scope* analyzeSemantics(NodeAST *root, Scope *parent, DataType returnType);

/**
 * @brief Verifica que el programa contenga la definición del método 'main',
 *        que sea de tipo método y que no reciba parámetros.
 *
 * @param globalScope Scope global generado tras analyzeSemantics.
 */
void checkMainMethodExists(Scope *globalScope);

#endif // SEMANTIC_ANALYZER_H
