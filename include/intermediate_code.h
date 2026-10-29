#ifndef INTERMEDIATE_CODE_H
#define INTERMEDIATE_CODE_H

/**
 * @file intermediate_code.h
 * @brief Recorrido del AST que construye el código de tres direcciones.
 *
 * La estructura de datos del TAC vive en `tac.h`; este módulo solo la llena.
 * El resultado se obtiene con getIntermediateCode() y lo consumen las etapas
 * siguientes (intérprete, optimizador, generador x86-64) o printTACList() si
 * lo que se quiere es la versión en texto.
 */

#include "ast.h"
#include "types.h"
#include "tac.h"

/**
 * @brief Genera el código intermedio del programa completo.
 *
 * Descarta el código generado en una invocación anterior y reinicia los
 * contadores de temporales y etiquetas.
 *
 * @param root Nodo raíz del AST (el bloque de declaraciones del programa).
 */
void generateIntermediateCode(NodeAST *root);

/**
 * @brief Devuelve el código intermedio de la última generación.
 * @return La lista de instrucciones, o NULL si todavía no se generó ninguna.
 */
const TACList *getIntermediateCode(void);

/**
 * @brief Genera el código de una expresión.
 * @return El operando donde quedó el resultado (un temporal, una variable o
 *         una constante).
 */
TACOperand generateExpressionCode(NodeAST *node);

/**
 * @brief Genera el código de una sentencia.
 */
void generateStatementCode(NodeAST *node);

#endif // INTERMEDIATE_CODE_H
