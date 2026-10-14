#ifndef INTERMEDIATE_CODE_H
#define INTERMEDIATE_CODE_H

#include "ast.h"
#include "types.h"

/**
 * @brief Estructura que representa el resultado de evaluar una expresión en TAC.
 * 'place' almacena el nombre de la variable, constante o temporal (ej: "t0", "x", "42").
 */
typedef struct {
    char place[32];
} ExprResult;

/**
 * @brief Genera un nuevo nombre de variable temporal único (t0, t1, t2...).
 */
char* newTemp(void);

/**
 * @brief Genera una nueva etiqueta única para saltos (L0, L1, L2...).
 */
char* newLabel(void);

/**
 * @brief Reinicia los contadores de temporales y etiquetas.
 */
void resetTempAndLabelCounters(void);

/**
 * @brief Convierte un OperationType a su representación en texto ("+", "-", "*", etc.).
 */
const char* getOperatorSymbol(OperationType op);

/**
 * @brief Genera código de tres direcciones (TAC) para un nodo de expresión.
 * @param node Nodo de la expresión (ID_NODE, CONSTANT_NODE, BINARYOPERATOR_NODE, METHOD_CALL_NODE).
 * @return ExprResult con el 'place' donde reside el resultado.
 */
ExprResult generateExpressionCode(NodeAST *node);

/**
 * @brief Genera código de tres direcciones (TAC) para una sentencia.
 * @param node Nodo de sentencia (ASSIGNMENT_NODE, IF_ELSE_NODE, WHILE_NODE, RETURN_NODE, etc.).
 */
void generateStatementCode(NodeAST *node);

/**
 * @brief Punto de entrada principal para generar código intermedio recorriendo el AST completo.
 * @param root Nodo raíz del AST.
 */
void generateIntermediateCode(NodeAST *root);

#endif // INTERMEDIATE_CODE_H
