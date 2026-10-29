#ifndef TYPE_CHECKER_H
#define TYPE_CHECKER_H

/**
 * @file type_checker.h
 * @brief Deducción, compatibilidad e inferencia de tipos para expresiones.
 */

#include "types.h"
#include "ast.h"
#include "symbol_table.h"

/**
 * @brief Analiza una expresión en el AST deduciendo e infiriendo su tipo semántico.
 *
 * Recorre recursivamente el subárbol de la expresión según la categoría del nodo
 * (identificadores, constantes literales, llamadas a métodos u operaciones aritmético-lógicas).
 * Valida la existencia de identificadores en el @p scope correspondiente, delega el chequeo
 * de llamadas y operaciones, anota el tipo resultante en el propio nodo y reporta errores
 * semánticos en caso de incompatibilidad.
 *
 * @param node Nodo del AST que representa la raíz de la expresión a analizar.
 * @param scope Ámbito o tabla de símbolos activo para la resolución de nombres.
 * @return Tipo de dato inferido para la expresión, o TYPE_ERROR ante fallos semánticos.
 */
DataType analyzeExpression(NodeAST *node, Scope *scope);

/**
 * @brief Valida la llamada a un método comprobando argumentos contra parámetros formales.
 *
 * Verifica que el método exista en el @p scope y pertenezca a la categoría de símbolo correcta.
 * Comprueba que la aridad coincida con la firma declarada y que cada argumento sea compatible
 * o asignable al parámetro esperado. Si @p isExpression es verdadero, rechaza métodos cuyo tipo
 * de retorno sea void. Reapunta el símbolo del nodo a la declaración original para facilitar
 * las fases posteriores de generación de código.
 *
 * @param node Nodo de llamada a método en el AST (METHOD_CALL_NODE).
 * @param scope Ámbito activo donde se busca la declaración del método.
 * @param isExpression Indicador del contexto de invocación (1 si se usa como expresión, 0 si se invoca como sentencia).
 * @return Tipo de retorno del método analizado, o TYPE_ERROR si la llamada es inválida.
 */
DataType analyzeMethodCall(NodeAST *node, Scope *scope, int isExpression);

/**
 * @brief Comprueba si un tipo de dato actual es compatible o asignable a un tipo esperado.
 *
 * Determina la validez de una asignación o pasaje de parámetros evaluando igualdad exacta
 * de tipos o conversiones implícitas permitidas por el lenguaje (por ejemplo, coerción
 * de tipo entero a tipo punto flotante).
 *
 * @param expected Tipo de dato destino o esperado.
 * @param actual Tipo de dato origen o producido por la expresión.
 * @return 1 si el tipo actual puede ser asignado al esperado, o 0 en caso contrario.
 */
int isAssignable(DataType expected, DataType actual);

#endif // TYPE_CHECKER_H
