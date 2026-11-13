#ifndef AST_BUILDER_H
#define AST_BUILDER_H

/**
 * @file ast_builder.h
 * @brief Constructores de árboles AST para el Arrange de las etapas que reciben un AST.
 *
 * Permiten armar a mano la entrada de cada etapa sin pasar por el lexer ni el parser,
 * para que un test del análisis semántico o del generador de TAC falle solo por un
 * defecto de la etapa bajo prueba. Hay dos familias:
 *
 *  - AST sin anotar, como lo deja el parser: referencias con un símbolo provisorio
 *    (uid 0) que solo lleva el nombre. Es la entrada del análisis semántico.
 *  - AST anotado, como lo deja el análisis semántico: declaraciones registradas con
 *    su uid, referencias apuntando al símbolo de su declaración y tipos resueltos.
 *    Es la entrada del generador de código intermedio.
 *
 * Las funciones variádicas reciben primero la cantidad de elementos. Los nombres se
 * copian, así que el llamador puede pasar literales. El árbol se libera con freeAST.
 */

#include "ast.h"

/* ------------------------------------------------------------------------
 * AST sin anotar (salida del parser)
 * ------------------------------------------------------------------------ */

/** @brief Raíz del programa: un BLOCK_NODE con las declaraciones globales y los métodos. */
NodeAST *astProgram(int count, ...);

/** @brief Bloque con sus declaraciones y sentencias, en ese orden. */
NodeAST *astBlock(int count, ...);

/** @brief Declaración de método con símbolo sin registrar. */
NodeAST *astMethod(DataType returnType, const char *name, NodeAST *parameters, NodeAST *body);

/** @brief Contenedor de parámetros formales (PARAMETERS_NODE). */
NodeAST *astParameters(int count, ...);

/** @brief Declaración de variable o parámetro con símbolo sin registrar. */
NodeAST *astDeclaration(DataType type, const char *name);

/** @brief Referencia a un identificador con símbolo provisorio. */
NodeAST *astIdentifier(const char *name);

/** @brief Asignación con símbolo provisorio en el destino. */
NodeAST *astAssignment(const char *name, NodeAST *value);

/** @brief Llamada a método con símbolo provisorio y sus argumentos. */
NodeAST *astCall(const char *name, int count, ...);

/** @brief Literales enteros, booleanos y reales. */
NodeAST *astInt(int value);
NodeAST *astBool(int value);
NodeAST *astFloat(float value);

/**
 * @brief Operación binaria o unaria.
 *
 * Anota el tipo del nodo como lo haría el chequeo de tipos: BOOL para relacionales y
 * lógicos, y el tipo del operando izquierdo para los aritméticos. En un AST sin anotar
 * el análisis semántico lo vuelve a calcular.
 */
NodeAST *astBinary(OperationType operation, NodeAST *left, NodeAST *right);
NodeAST *astUnary(OperationType operation, NodeAST *operand);

/** @brief Sentencias de control y retorno (@p value o @p elseBlock pueden ser NULL). */
NodeAST *astReturn(NodeAST *value);
NodeAST *astIf(NodeAST *condition, NodeAST *ifBlock, NodeAST *elseBlock);
NodeAST *astWhile(NodeAST *condition, NodeAST *body);

/* ------------------------------------------------------------------------
 * AST anotado (salida del análisis semántico)
 * ------------------------------------------------------------------------ */

/** @brief Declaración de variable global registrada con @p uid. */
NodeAST *astGlobal(DataType type, const char *name, int uid);

/** @brief Declaración de variable local o parámetro registrada con @p uid. */
NodeAST *astLocal(DataType type, const char *name, int uid);

/**
 * @brief Declaración de método registrada con @p uid y cuerpo vacío.
 *
 * El cuerpo se completa con astAppend, lo que permite que el propio cuerpo invoque al
 * método (recursión) con astInvoke.
 */
NodeAST *astMethodDeclaration(DataType returnType, const char *name, int uid, NodeAST *parameters);

/** @brief Agrega sentencias al final del cuerpo de un método o de un bloque. */
void astAppend(NodeAST *methodOrBlock, int count, ...);

/** @brief Referencia resuelta a la variable declarada en @p declaration. */
NodeAST *astUse(NodeAST *declaration);

/** @brief Asignación resuelta a la variable declarada en @p declaration. */
NodeAST *astStore(NodeAST *declaration, NodeAST *value);

/** @brief Llamada resuelta al método declarado en @p methodDeclaration. */
NodeAST *astInvoke(NodeAST *methodDeclaration, int count, ...);

#endif // AST_BUILDER_H
