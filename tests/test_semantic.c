/**
 * @file test_semantic.c
 * @brief Pruebas unitarias del análisis semántico y verificación de tipos (C-TDS).
 *
 * Valida las 14 reglas semánticas y decisiones de diseño del lenguaje C-TDS:
 *  - Gestión de ámbitos, unicidad de identificadores, sombreado y destinos de asignación.
 *  - Existencia obligatoria y signatura del método principal main().
 *  - Invocación de métodos: aridad, chequeo de tipos de argumentos y coerción int -> float.
 *  - Restricciones sobre métodos void y retornos con/sin expresión.
 *  - Garantía de retorno en todos los caminos para métodos no-void.
 *  - Sistema de tipos en expresiones: operadores aritméticos, módulo, relacionales,
 *    igualdad estricta y operadores lógicos.
 *  - Compatibilidad de asignación y promociones válidas.
 *
 * Cada test arma a mano el AST sin anotar que produciría el parser (ver
 * support/ast_builder.h), así que solo ejercita el análisis semántico: un defecto del
 * lexer o del parser no puede hacer fallar esta suite. El comentario de cada test
 * muestra el programa C-TDS equivalente.
 */

#include "unity.h"
#include "ast.h"
#include "ast_builder.h"
#include "semantic_analyzer.h"
#include "symbol_table.h"
#include "error_handler.h"
#include <stdio.h>
#include <stdlib.h>

extern int semantic_errors;

static NodeAST *program = NULL;

static int analyze(NodeAST *root) {
    program = root;
    analyzeSemantics(root, NULL);
    return semantic_errors;
}

/* void main() { <statements> } */
static NodeAST *mainWith(NodeAST *body) {
    return astMethod(TYPE_VOID, "main", astParameters(0), body);
}

static NodeAST *emptyMain(void) {
    return mainWith(astBlock(0));
}

void setUp(void) {
    resetErrorHandler();
    resetSymbolUids();
    program = NULL;
}

void tearDown(void) {
    freeAST(program);
    program = NULL;
}

/* ------------------------------------------------------------------------
 * Grupo 1: Ámbitos, Declaraciones y Visibilidad (Reglas 1, 2, 8 y Shadowing)
 * ------------------------------------------------------------------------ */

// Variable duplicada en el mismo bloque es rechazada (Regla 1)
// void main() { int x; int x; }
void test_duplicate_declaration_in_same_scope_fails(void) {
    NodeAST *root = astProgram(1, mainWith(astBlock(2,
        astDeclaration(TYPE_INT, "x"),
        astDeclaration(TYPE_INT, "x"))));
    TEST_ASSERT_GREATER_THAN_INT(0, analyze(root));
}

// Variable local con el mismo nombre que un parámetro formal es rechazada (mismo ámbito)
// int suma(int base) { int base; return base; } void main() {}
void test_parameter_and_local_variable_collision_fails(void) {
    NodeAST *suma = astMethod(TYPE_INT, "suma",
        astParameters(1, astDeclaration(TYPE_INT, "base")),
        astBlock(2, astDeclaration(TYPE_INT, "base"), astReturn(astIdentifier("base"))));
    TEST_ASSERT_GREATER_THAN_INT(0, analyze(astProgram(2, suma, emptyMain())));
}

// Uso de una variable no declarada en una expresión es rechazado (Regla 2)
// void main() { int x; x = total + 1; }
void test_undeclared_variable_fails(void) {
    NodeAST *root = astProgram(1, mainWith(astBlock(2,
        astDeclaration(TYPE_INT, "x"),
        astAssignment("x", astBinary(OP_ADD, astIdentifier("total"), astInt(1))))));
    TEST_ASSERT_GREATER_THAN_INT(0, analyze(root));
}

// Llamada a un método no declarado es rechazada (Regla 2)
// void main() { noExiste(); }
void test_undeclared_method_fails(void) {
    NodeAST *root = astProgram(1, mainWith(astBlock(1, astCall("noExiste", 0))));
    TEST_ASSERT_GREATER_THAN_INT(0, analyze(root));
}

// Sombreado (shadowing) de variable en bloque anidado interno es válido
// void main() { int x; x = 1; { int x; x = 2; } }
void test_shadowing_in_nested_block_is_valid(void) {
    NodeAST *inner = astBlock(2, astDeclaration(TYPE_INT, "x"), astAssignment("x", astInt(2)));
    NodeAST *root = astProgram(1, mainWith(astBlock(3,
        astDeclaration(TYPE_INT, "x"),
        astAssignment("x", astInt(1)),
        inner)));
    TEST_ASSERT_EQUAL_INT(0, analyze(root));
}

// Asignar un valor a un identificador que corresponde a un método es rechazado (Regla 8)
// int calculo() { return 1; } void main() { calculo = 5; }
void test_assignment_to_method_name_fails(void) {
    NodeAST *calculo = astMethod(TYPE_INT, "calculo", astParameters(0), astBlock(1, astReturn(astInt(1))));
    NodeAST *root = astProgram(2, calculo, mainWith(astBlock(1, astAssignment("calculo", astInt(5)))));
    TEST_ASSERT_GREATER_THAN_INT(0, analyze(root));
}

// Usar el nombre de un método como valor en una expresión es rechazado (Defecto 4.3)
// int f() { return 1; } void main() { int x; x = f + 1; }
void test_method_used_as_value_in_expression_fails(void) {
    NodeAST *f = astMethod(TYPE_INT, "f", astParameters(0), astBlock(1, astReturn(astInt(1))));
    NodeAST *root = astProgram(2, f, mainWith(astBlock(2,
        astDeclaration(TYPE_INT, "x"),
        astAssignment("x", astBinary(OP_ADD, astIdentifier("f"), astInt(1))))));
    TEST_ASSERT_GREATER_THAN_INT(0, analyze(root));
}

/* ------------------------------------------------------------------------
 * Grupo 2: Método Principal main (Regla 3)
 * ------------------------------------------------------------------------ */

// Programa mínimo con método main void sin parámetros es válido
// void main() {}
void test_valid_minimal_program_with_main_passes(void) {
    TEST_ASSERT_EQUAL_INT(0, analyze(astProgram(1, emptyMain())));
}

// Programa sin definición del método main es rechazado (Regla 3)
// int helper() { return 1; }
void test_missing_main_method_fails(void) {
    NodeAST *helper = astMethod(TYPE_INT, "helper", astParameters(0), astBlock(1, astReturn(astInt(1))));
    TEST_ASSERT_GREATER_THAN_INT(0, analyze(astProgram(1, helper)));
}

// Método main con parámetros formales es rechazado (Regla 3)
// void main(int argumento) {}
void test_main_method_with_parameters_fails(void) {
    NodeAST *main = astMethod(TYPE_VOID, "main", astParameters(1, astDeclaration(TYPE_INT, "argumento")), astBlock(0));
    TEST_ASSERT_GREATER_THAN_INT(0, analyze(astProgram(1, main)));
}

// Identificador main declarado como variable en lugar de método es rechazado (Regla 3)
// int main;
void test_main_declared_as_variable_fails(void) {
    TEST_ASSERT_GREATER_THAN_INT(0, analyze(astProgram(1, astDeclaration(TYPE_INT, "main"))));
}

/* ------------------------------------------------------------------------
 * Grupo 3: Invocación de Métodos y Parámetros (Reglas 4, 5)
 * ------------------------------------------------------------------------ */

/* int duplicar(int n) { return n * 2; } */
static NodeAST *duplicarMethod(void) {
    return astMethod(TYPE_INT, "duplicar",
        astParameters(1, astDeclaration(TYPE_INT, "n")),
        astBlock(1, astReturn(astBinary(OP_MUL, astIdentifier("n"), astInt(2)))));
}

// Llamada con más argumentos que los declarados es rechazada (Regla 4)
// int duplicar(int n) { return n * 2; } void main() { duplicar(1, 2); }
void test_method_call_argument_count_mismatch_fails(void) {
    NodeAST *root = astProgram(2, duplicarMethod(),
        mainWith(astBlock(1, astCall("duplicar", 2, astInt(1), astInt(2)))));
    TEST_ASSERT_GREATER_THAN_INT(0, analyze(root));
}

// Argumento de tipo incompatible es rechazado (Regla 4)
// int duplicar(int n) { return n * 2; } void main() { duplicar(true); }
void test_method_call_incompatible_argument_type_fails(void) {
    NodeAST *root = astProgram(2, duplicarMethod(),
        mainWith(astBlock(1, astCall("duplicar", 1, astBool(1)))));
    TEST_ASSERT_GREATER_THAN_INT(0, analyze(root));
}

// Coerción automática de argumento int a parámetro float es válida (Regla 14)
// float mitad(float n) { return n / 2.0; } void main() { mitad(10); }
void test_method_call_coercion_int_to_float_passes(void) {
    NodeAST *mitad = astMethod(TYPE_FLOAT, "mitad",
        astParameters(1, astDeclaration(TYPE_FLOAT, "n")),
        astBlock(1, astReturn(astBinary(OP_DIV, astIdentifier("n"), astFloat(2.0f)))));
    NodeAST *root = astProgram(2, mitad, mainWith(astBlock(1, astCall("mitad", 1, astInt(10)))));
    TEST_ASSERT_EQUAL_INT(0, analyze(root));
}

// Método void utilizado dentro de una expresión es rechazado (Regla 5)
// void proceso() {} void main() { int x; x = proceso(); }
void test_void_method_used_in_expression_fails(void) {
    NodeAST *proceso = astMethod(TYPE_VOID, "proceso", astParameters(0), astBlock(0));
    NodeAST *root = astProgram(2, proceso, mainWith(astBlock(2,
        astDeclaration(TYPE_INT, "x"),
        astAssignment("x", astCall("proceso", 0)))));
    TEST_ASSERT_GREATER_THAN_INT(0, analyze(root));
}

// Invocación a una variable como si fuera un método es rechazada
// void main() { int contador; contador(); }
void test_calling_variable_as_method_fails(void) {
    NodeAST *root = astProgram(1, mainWith(astBlock(2,
        astDeclaration(TYPE_INT, "contador"),
        astCall("contador", 0))));
    TEST_ASSERT_GREATER_THAN_INT(0, analyze(root));
}

/* ------------------------------------------------------------------------
 * Grupo 4: Retorno y Flujo de Control (Reglas 6, 7 y Garantía de Retorno)
 * ------------------------------------------------------------------------ */

// Método void con return que contiene expresión es rechazado (Regla 6)
// void proceso() { return 5; } void main() {}
void test_void_method_with_return_value_fails(void) {
    NodeAST *proceso = astMethod(TYPE_VOID, "proceso", astParameters(0), astBlock(1, astReturn(astInt(5))));
    TEST_ASSERT_GREATER_THAN_INT(0, analyze(astProgram(2, proceso, emptyMain())));
}

// Método no-void con return vacío sin expresión es rechazado (Regla 6)
// int calculo() { return; } void main() {}
void test_non_void_method_empty_return_fails(void) {
    NodeAST *calculo = astMethod(TYPE_INT, "calculo", astParameters(0), astBlock(1, astReturn(NULL)));
    TEST_ASSERT_GREATER_THAN_INT(0, analyze(astProgram(2, calculo, emptyMain())));
}

// Expresión en return con tipo incompatible con la firma es rechazada (Regla 7)
// int calculo() { return true; } void main() {}
void test_return_type_mismatch_fails(void) {
    NodeAST *calculo = astMethod(TYPE_INT, "calculo", astParameters(0), astBlock(1, astReturn(astBool(1))));
    TEST_ASSERT_GREATER_THAN_INT(0, analyze(astProgram(2, calculo, emptyMain())));
}

// Retornar int en método float con coerción implícita es válido (Regla 14)
// float calculo() { return 42; } void main() {}
void test_return_int_in_float_method_passes(void) {
    NodeAST *calculo = astMethod(TYPE_FLOAT, "calculo", astParameters(0), astBlock(1, astReturn(astInt(42))));
    TEST_ASSERT_EQUAL_INT(0, analyze(astProgram(2, calculo, emptyMain())));
}

// Método no-void sin ninguna sentencia return es rechazado
// int calculo() { int x; x = 1; } void main() {}
void test_non_void_method_without_any_return_fails(void) {
    NodeAST *calculo = astMethod(TYPE_INT, "calculo", astParameters(0),
        astBlock(2, astDeclaration(TYPE_INT, "x"), astAssignment("x", astInt(1))));
    TEST_ASSERT_GREATER_THAN_INT(0, analyze(astProgram(2, calculo, emptyMain())));
}

// Método no-void donde el return está solo en un if sin else no garantiza retorno y es rechazado
// int calculo(boolean condicion) { if (condicion) { return 1; } } void main() {}
void test_non_void_method_if_without_else_does_not_guarantee_return_fails(void) {
    NodeAST *calculo = astMethod(TYPE_INT, "calculo",
        astParameters(1, astDeclaration(TYPE_BOOL, "condicion")),
        astBlock(1, astIf(astIdentifier("condicion"), astBlock(1, astReturn(astInt(1))), NULL)));
    TEST_ASSERT_GREATER_THAN_INT(0, analyze(astProgram(2, calculo, emptyMain())));
}

// Método no-void con return en ambas ramas de un if-else es válido
// int calculo(boolean c) { if (c) { return 1; } else { return 2; } } void main() {}
void test_non_void_method_return_in_both_branches_passes(void) {
    NodeAST *calculo = astMethod(TYPE_INT, "calculo",
        astParameters(1, astDeclaration(TYPE_BOOL, "c")),
        astBlock(1, astIf(astIdentifier("c"),
                          astBlock(1, astReturn(astInt(1))),
                          astBlock(1, astReturn(astInt(2))))));
    TEST_ASSERT_EQUAL_INT(0, analyze(astProgram(2, calculo, emptyMain())));
}

// Retornos múltiples consecutivos son permitidos sin error (Decisión de diseño 3)
// int calculo() { return 1; return 2; } void main() {}
void test_duplicate_consecutive_return_statements_pass(void) {
    NodeAST *calculo = astMethod(TYPE_INT, "calculo", astParameters(0),
        astBlock(2, astReturn(astInt(1)), astReturn(astInt(2))));
    TEST_ASSERT_EQUAL_INT(0, analyze(astProgram(2, calculo, emptyMain())));
}

/* ------------------------------------------------------------------------
 * Grupo 5: Sistema de Tipos en Expresiones y Asignaciones (Reglas 9-14)
 * ------------------------------------------------------------------------ */

/* void main() { <type> <name>; <name> = <value>; } */
static NodeAST *mainAssigning(DataType type, const char *name, NodeAST *value) {
    return astProgram(1, mainWith(astBlock(2, astDeclaration(type, name), astAssignment(name, value))));
}

// Condición numérica en sentencia if es rechazada (Regla 9)
// void main() { if (1) {} }
void test_control_condition_not_boolean_fails(void) {
    NodeAST *root = astProgram(1, mainWith(astBlock(1, astIf(astInt(1), astBlock(0), NULL))));
    TEST_ASSERT_GREATER_THAN_INT(0, analyze(root));
}

// Operadores aritméticos con operandos booleanos son rechazados (Regla 10)
// void main() { int x; x = 1 + true; }
void test_arithmetic_with_boolean_operands_fails(void) {
    TEST_ASSERT_GREATER_THAN_INT(0, analyze(mainAssigning(TYPE_INT, "x", astBinary(OP_ADD, astInt(1), astBool(1)))));
}

// Operación aritmética mixta int + float produce float con coerción (Regla 14)
// void main() { float total; total = 5 + 2.5; }
void test_arithmetic_mixed_int_and_float_coercion_passes(void) {
    TEST_ASSERT_EQUAL_INT(0, analyze(mainAssigning(TYPE_FLOAT, "total", astBinary(OP_ADD, astInt(5), astFloat(2.5f)))));
}

// Operador módulo con operando float es rechazado (Decisión de diseño 2)
// void main() { float f; f = 3.5 % 2; }
void test_modulo_operator_with_float_fails(void) {
    TEST_ASSERT_GREATER_THAN_INT(0, analyze(mainAssigning(TYPE_FLOAT, "f", astBinary(OP_MOD, astFloat(3.5f), astInt(2)))));
}

// Comparación relacional con operandos booleanos es rechazada (Regla 10)
// void main() { boolean b; b = true < false; }
void test_relational_operator_with_boolean_fails(void) {
    TEST_ASSERT_GREATER_THAN_INT(0, analyze(mainAssigning(TYPE_BOOL, "b", astBinary(OP_LT, astBool(1), astBool(0)))));
}

// Operador de igualdad == entre int y float es rechazado (Regla 11 y Decisión 1)
// void main() { boolean b; b = 1 == 1.5; }
void test_equality_operator_mixed_types_fails(void) {
    TEST_ASSERT_GREATER_THAN_INT(0, analyze(mainAssigning(TYPE_BOOL, "b", astBinary(OP_EQ, astInt(1), astFloat(1.5f)))));
}

// Operadores lógicos && y || con operandos enteros son rechazados (Regla 12)
// void main() { boolean b; b = 1 && 2; }
void test_logical_operators_require_boolean_operands_fails(void) {
    TEST_ASSERT_GREATER_THAN_INT(0, analyze(mainAssigning(TYPE_BOOL, "b", astBinary(OP_AND, astInt(1), astInt(2)))));
}

// Asignación de tipos incompatibles es rechazada (Regla 13)
// void main() { int x; x = true; }
void test_assignment_incompatible_types_fails(void) {
    TEST_ASSERT_GREATER_THAN_INT(0, analyze(mainAssigning(TYPE_INT, "x", astBool(1))));
}

// Asignación de int a variable float con coerción implícita es válida (Regla 14)
// void main() { float total; total = 10; }
void test_assignment_coercion_int_to_float_passes(void) {
    TEST_ASSERT_EQUAL_INT(0, analyze(mainAssigning(TYPE_FLOAT, "total", astInt(10))));
}

int main(void) {
    UNITY_BEGIN();

    // Grupo 1: Ámbitos y Declaraciones
    RUN_TEST(test_duplicate_declaration_in_same_scope_fails);
    RUN_TEST(test_parameter_and_local_variable_collision_fails);
    RUN_TEST(test_undeclared_variable_fails);
    RUN_TEST(test_undeclared_method_fails);
    RUN_TEST(test_shadowing_in_nested_block_is_valid);
    RUN_TEST(test_assignment_to_method_name_fails);
    RUN_TEST(test_method_used_as_value_in_expression_fails);

    // Grupo 2: Método Principal main
    RUN_TEST(test_valid_minimal_program_with_main_passes);
    RUN_TEST(test_missing_main_method_fails);
    RUN_TEST(test_main_method_with_parameters_fails);
    RUN_TEST(test_main_declared_as_variable_fails);

    // Grupo 3: Invocación de Métodos y Parámetros
    RUN_TEST(test_method_call_argument_count_mismatch_fails);
    RUN_TEST(test_method_call_incompatible_argument_type_fails);
    RUN_TEST(test_method_call_coercion_int_to_float_passes);
    RUN_TEST(test_void_method_used_in_expression_fails);
    RUN_TEST(test_calling_variable_as_method_fails);

    // Grupo 4: Retorno y Flujo de Control
    RUN_TEST(test_void_method_with_return_value_fails);
    RUN_TEST(test_non_void_method_empty_return_fails);
    RUN_TEST(test_return_type_mismatch_fails);
    RUN_TEST(test_return_int_in_float_method_passes);
    RUN_TEST(test_non_void_method_without_any_return_fails);
    RUN_TEST(test_non_void_method_if_without_else_does_not_guarantee_return_fails);
    RUN_TEST(test_non_void_method_return_in_both_branches_passes);
    RUN_TEST(test_duplicate_consecutive_return_statements_pass);

    // Grupo 5: Sistema de Tipos en Expresiones y Asignaciones
    RUN_TEST(test_control_condition_not_boolean_fails);
    RUN_TEST(test_arithmetic_with_boolean_operands_fails);
    RUN_TEST(test_arithmetic_mixed_int_and_float_coercion_passes);
    RUN_TEST(test_modulo_operator_with_float_fails);
    RUN_TEST(test_relational_operator_with_boolean_fails);
    RUN_TEST(test_equality_operator_mixed_types_fails);
    RUN_TEST(test_logical_operators_require_boolean_operands_fails);
    RUN_TEST(test_assignment_incompatible_types_fails);
    RUN_TEST(test_assignment_coercion_int_to_float_passes);

    return UNITY_END();
}
