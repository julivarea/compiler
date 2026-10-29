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
 * @brief Comprueba si un tipo actual puede ser asignado a un tipo esperado (e.g. casteo int a float).
 */
int isAssignable(DataType expected, DataType actual);

/**
 * @brief Resuelve el tipo resultante de una operación binaria dados los tipos de sus operandos.
 */
DataType resolveBinaryOperation(OperationType operator, DataType left, DataType right);

/**
 * @brief Analiza una expresión en el AST y devuelve su DataType inferido.
 */
DataType analyzeExpression(NodeAST *node, Scope *scope);

/**
 * @brief Analiza un operador binario o unario en el AST y valida los tipos de sus operandos.
 */
DataType analyzeBinaryOperator(NodeAST *node, Scope *scope);

/**
 * @brief Analiza la llamada a un método (argumentos vs parámetros esperados) y su tipo de retorno.
 * @param isExpression 1 si la llamada se usa dentro de una expresion, 0 si es como sentencia (statement).
 */
DataType analyzeMethodCall(NodeAST *node, Scope *scope, int isExpression);

/**
 * @brief Analiza el uso de un identificador en una expresión y verifica que haya sido declarado.
 */
DataType analyzeIdentifier(NodeAST *node, Scope *scope);

#endif // TYPE_CHECKER_H
