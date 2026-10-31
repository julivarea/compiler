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
 */

#include "unity.h"
#include "ast.h"
#include "semantic_analyzer.h"
#include "error_handler.h"
#include <stdio.h>
#include <stdlib.h>

extern int yyparse(void);
typedef struct yy_buffer_state *YY_BUFFER_STATE;
extern YY_BUFFER_STATE yy_scan_string(const char *str);
extern void yy_delete_buffer(YY_BUFFER_STATE buffer);
extern int yylineno;
extern NodeAST *astRoot;
extern int semantic_errors;

static int runSemanticAnalysis(const char *source) {
    resetErrorHandler();
    semantic_errors = 0;
    yylineno = 1;
    astRoot = NULL;

    YY_BUFFER_STATE buffer = yy_scan_string(source);
    int parseResult = yyparse();
    yy_delete_buffer(buffer);

    if (parseResult != 0) {
        return -1;
    }

    analyzeSemantics(astRoot, NULL);

    return semantic_errors;
}

void setUp(void) {
    resetErrorHandler();
    semantic_errors = 0;
    yylineno = 1;
    astRoot = NULL;
}

void tearDown(void) {
    freeAST(astRoot);
    astRoot = NULL;
}

/* ------------------------------------------------------------------------
 * Grupo 1: Ámbitos, Declaraciones y Visibilidad (Reglas 1, 2, 8 y Shadowing)
 * ------------------------------------------------------------------------ */

// Variable duplicada en el mismo bloque es rechazada (Regla 1)
void test_duplicate_declaration_in_same_scope_fails(void) {
    const char *source = "void main() { int x; int x; }";
    TEST_ASSERT_GREATER_THAN_INT(0, runSemanticAnalysis(source));
}

// Variable local con el mismo nombre que un parámetro formal es rechazada (mismo ámbito)
void test_parameter_and_local_variable_collision_fails(void) {
    const char *source = "int suma(int base) { int base; return base; } void main() {}";
    TEST_ASSERT_GREATER_THAN_INT(0, runSemanticAnalysis(source));
}

// Uso de una variable no declarada en una expresión es rechazado (Regla 2)
void test_undeclared_variable_fails(void) {
    const char *source = "void main() { int x; x = total + 1; }";
    TEST_ASSERT_GREATER_THAN_INT(0, runSemanticAnalysis(source));
}

// Llamada a un método no declarado es rechazada (Regla 2)
void test_undeclared_method_fails(void) {
    const char *source = "void main() { noExiste(); }";
    TEST_ASSERT_GREATER_THAN_INT(0, runSemanticAnalysis(source));
}

// Sombreado (shadowing) de variable en bloque anidado interno es válido
void test_shadowing_in_nested_block_is_valid(void) {
    const char *source = "void main() { int x; x = 1; { int x; x = 2; } }";
    TEST_ASSERT_EQUAL_INT(0, runSemanticAnalysis(source));
}

// Asignar un valor a un identificador que corresponde a un método es rechazado (Regla 8)
void test_assignment_to_method_name_fails(void) {
    const char *source = "int calculo() { return 1; } void main() { calculo = 5; }";
    TEST_ASSERT_GREATER_THAN_INT(0, runSemanticAnalysis(source));
}

// Usar el nombre de un método como valor en una expresión es rechazado (Defecto 4.3)
void test_method_used_as_value_in_expression_fails(void) {
    const char *source = "int f() { return 1; } void main() { int x; x = f + 1; }";
    TEST_ASSERT_GREATER_THAN_INT(0, runSemanticAnalysis(source));
}

/* ------------------------------------------------------------------------
 * Grupo 2: Método Principal main (Regla 3)
 * ------------------------------------------------------------------------ */

// Programa mínimo con método main void sin parámetros es válido
void test_valid_minimal_program_with_main_passes(void) {
    const char *source = "void main() {}";
    TEST_ASSERT_EQUAL_INT(0, runSemanticAnalysis(source));
}

// Programa sin definición del método main es rechazado (Regla 3)
void test_missing_main_method_fails(void) {
    const char *source = "int helper() { return 1; }";
    TEST_ASSERT_GREATER_THAN_INT(0, runSemanticAnalysis(source));
}

// Método main con parámetros formales es rechazado (Regla 3)
void test_main_method_with_parameters_fails(void) {
    const char *source = "void main(int argumento) {}";
    TEST_ASSERT_GREATER_THAN_INT(0, runSemanticAnalysis(source));
}

// Identificador main declarado como variable en lugar de método es rechazado (Regla 3)
void test_main_declared_as_variable_fails(void) {
    const char *source = "int main;";
    TEST_ASSERT_GREATER_THAN_INT(0, runSemanticAnalysis(source));
}

/* ------------------------------------------------------------------------
 * Grupo 3: Invocación de Métodos y Parámetros (Reglas 4, 5)
 * ------------------------------------------------------------------------ */

// Llamada con más argumentos que los declarados es rechazada (Regla 4)
void test_method_call_argument_count_mismatch_fails(void) {
    const char *source = "int duplicar(int n) { return n * 2; } void main() { duplicar(1, 2); }";
    TEST_ASSERT_GREATER_THAN_INT(0, runSemanticAnalysis(source));
}

// Argumento de tipo incompatible es rechazado (Regla 4)
void test_method_call_incompatible_argument_type_fails(void) {
    const char *source = "int duplicar(int n) { return n * 2; } void main() { duplicar(true); }";
    TEST_ASSERT_GREATER_THAN_INT(0, runSemanticAnalysis(source));
}

// Coerción automática de argumento int a parámetro float es válida (Regla 14)
void test_method_call_coercion_int_to_float_passes(void) {
    const char *source = "float mitad(float n) { return n / 2.0; } void main() { mitad(10); }";
    TEST_ASSERT_EQUAL_INT(0, runSemanticAnalysis(source));
}

// Método void utilizado dentro de una expresión es rechazado (Regla 5)
void test_void_method_used_in_expression_fails(void) {
    const char *source = "void proceso() {} void main() { int x; x = proceso(); }";
    TEST_ASSERT_GREATER_THAN_INT(0, runSemanticAnalysis(source));
}

// Invocación a una variable como si fuera un método es rechazada
void test_calling_variable_as_method_fails(void) {
    const char *source = "void main() { int contador; contador(); }";
    TEST_ASSERT_GREATER_THAN_INT(0, runSemanticAnalysis(source));
}

/* ------------------------------------------------------------------------
 * Grupo 4: Retorno y Flujo de Control (Reglas 6, 7 y Garantía de Retorno)
 * ------------------------------------------------------------------------ */

// Método void con return que contiene expresión es rechazado (Regla 6)
void test_void_method_with_return_value_fails(void) {
    const char *source = "void proceso() { return 5; } void main() {}";
    TEST_ASSERT_GREATER_THAN_INT(0, runSemanticAnalysis(source));
}

// Método no-void con return vacío sin expresión es rechazado (Regla 6)
void test_non_void_method_empty_return_fails(void) {
    const char *source = "int calculo() { return; } void main() {}";
    TEST_ASSERT_GREATER_THAN_INT(0, runSemanticAnalysis(source));
}

// Expresión en return con tipo incompatible con la firma es rechazada (Regla 7)
void test_return_type_mismatch_fails(void) {
    const char *source = "int calculo() { return true; } void main() {}";
    TEST_ASSERT_GREATER_THAN_INT(0, runSemanticAnalysis(source));
}

// Retornar int en método float con coerción implícita es válido (Regla 14)
void test_return_int_in_float_method_passes(void) {
    const char *source = "float calculo() { return 42; } void main() {}";
    TEST_ASSERT_EQUAL_INT(0, runSemanticAnalysis(source));
}

// Método no-void sin ninguna sentencia return es rechazado
void test_non_void_method_without_any_return_fails(void) {
    const char *source = "int calculo() { int x; x = 1; } void main() {}";
    TEST_ASSERT_GREATER_THAN_INT(0, runSemanticAnalysis(source));
}

// Método no-void donde el return está solo en un if sin else no garantiza retorno y es rechazado
void test_non_void_method_if_without_else_does_not_guarantee_return_fails(void) {
    const char *source = "int calculo(boolean condicion) { if (condicion) { return 1; } } void main() {}";
    TEST_ASSERT_GREATER_THAN_INT(0, runSemanticAnalysis(source));
}

// Método no-void con return en ambas ramas de un if-else es válido
void test_non_void_method_return_in_both_branches_passes(void) {
    const char *source = "int calculo(boolean c) { if (c) { return 1; } else { return 2; } } void main() {}";
    TEST_ASSERT_EQUAL_INT(0, runSemanticAnalysis(source));
}

// Retornos múltiples consecutivos son permitidos sin error (Decisión de diseño 3)
void test_duplicate_consecutive_return_statements_pass(void) {
    const char *source = "int calculo() { return 1; return 2; } void main() {}";
    TEST_ASSERT_EQUAL_INT(0, runSemanticAnalysis(source));
}

/* ------------------------------------------------------------------------
 * Grupo 5: Sistema de Tipos en Expresiones y Asignaciones (Reglas 9-14)
 * ------------------------------------------------------------------------ */

// Condición numérica en sentencia if es rechazada (Regla 9)
void test_control_condition_not_boolean_fails(void) {
    const char *source = "void main() { if (1) {} }";
    TEST_ASSERT_GREATER_THAN_INT(0, runSemanticAnalysis(source));
}

// Operadores aritméticos con operandos booleanos son rechazados (Regla 10)
void test_arithmetic_with_boolean_operands_fails(void) {
    const char *source = "void main() { int x; x = 1 + true; }";
    TEST_ASSERT_GREATER_THAN_INT(0, runSemanticAnalysis(source));
}

// Operación aritmética mixta int + float produce float con coerción (Regla 14)
void test_arithmetic_mixed_int_and_float_coercion_passes(void) {
    const char *source = "void main() { float total; total = 5 + 2.5; }";
    TEST_ASSERT_EQUAL_INT(0, runSemanticAnalysis(source));
}

// Operador módulo con operando float es rechazado (Decisión de diseño 2)
void test_modulo_operator_with_float_fails(void) {
    const char *source = "void main() { float f; f = 3.5 % 2; }";
    TEST_ASSERT_GREATER_THAN_INT(0, runSemanticAnalysis(source));
}

// Comparación relacional con operandos booleanos es rechazada (Regla 10)
void test_relational_operator_with_boolean_fails(void) {
    const char *source = "void main() { boolean b; b = true < false; }";
    TEST_ASSERT_GREATER_THAN_INT(0, runSemanticAnalysis(source));
}

// Operador de igualdad == entre int y float es rechazado (Regla 11 y Decisión 1)
void test_equality_operator_mixed_types_fails(void) {
    const char *source = "void main() { boolean b; b = 1 == 1.5; }";
    TEST_ASSERT_GREATER_THAN_INT(0, runSemanticAnalysis(source));
}

// Operadores lógicos && y || con operandos enteros son rechazados (Regla 12)
void test_logical_operators_require_boolean_operands_fails(void) {
    const char *source = "void main() { boolean b; b = 1 && 2; }";
    TEST_ASSERT_GREATER_THAN_INT(0, runSemanticAnalysis(source));
}

// Asignación de tipos incompatibles es rechazada (Regla 13)
void test_assignment_incompatible_types_fails(void) {
    const char *source = "void main() { int x; x = true; }";
    TEST_ASSERT_GREATER_THAN_INT(0, runSemanticAnalysis(source));
}

// Asignación de int a variable float con coerción implícita es válida (Regla 14)
void test_assignment_coercion_int_to_float_passes(void) {
    const char *source = "void main() { float total; total = 10; }";
    TEST_ASSERT_EQUAL_INT(0, runSemanticAnalysis(source));
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
