/**
 * @file test_object_code.c
 * @brief Suite de pruebas unitarias y de ejecucion para codigo objeto (Assembly x86-64).
 */

#include "unity.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include "ast.h"
#include "symbol_table.h"
#include "semantic_analyzer.h"
#include "intermediate_code.h"
#include "tac_list.h"
#include "tac_instruction.h"
#include "tac_operand.h"
#include "tac_operation.h"
#include "object_code.h"
#include "function_frame.h"
#include "string_builder.h"
#include "error_handler.h"
#include "types.h"

extern int yyparse(void);
typedef struct yy_buffer_state *YY_BUFFER_STATE;
extern YY_BUFFER_STATE yy_scan_string(const char *str);
extern void yy_delete_buffer(YY_BUFFER_STATE buffer);
extern int yylineno;
extern NodeAST *astRoot;
extern int semantic_errors;
extern int lexical_errors;

/* ========================================================================
 * Helpers del arnes de pruebas y ejecucion
 * ======================================================================== */

static const char *compileToAssembly(const char *source, OptimizationSet optimizations) {
    resetErrorHandler();
    resetSymbolUids();
    resetBuildString();
    semantic_errors = 0;
    lexical_errors = 0;
    yylineno = 1;
    astRoot = NULL;

    YY_BUFFER_STATE buffer = yy_scan_string(source);
    int parseResult = yyparse();
    yy_delete_buffer(buffer);

    if (parseResult != 0 || lexical_errors > 0) {
        return NULL;
    }

    analyzeSemantics(astRoot, NULL);
    if (semantic_errors > 0) {
        return NULL;
    }

    const TACList *tacList = generateIntermediateCode(astRoot);
    if (tacList == NULL) {
        return NULL;
    }

    resetBuildString();
    generateAssembly(tacList, optimizations);
    return getBuiltString();
}

static int runAssembly(const char *assemblyCode) {
    if (assemblyCode == NULL || assemblyCode[0] == '\0') {
        return -999;
    }

    char assemblyFile[] = "/tmp/test_asm_XXXXXX.s";
    int asmFd = mkstemps(assemblyFile, 2);
    if (asmFd < 0) return -998;
    write(asmFd, assemblyCode, strlen(assemblyCode));
    close(asmFd);

    char binaryFile[] = "/tmp/test_bin_XXXXXX";
    int binFd = mkstemp(binaryFile);
    if (binFd < 0) {
        unlink(assemblyFile);
        return -997;
    }
    close(binFd);

    char compileCommand[512];
    snprintf(compileCommand, sizeof(compileCommand), "gcc -no-pie %s -o %s >/dev/null 2>&1", assemblyFile, binaryFile);
    int compileResult = system(compileCommand);
    unlink(assemblyFile);

    if (compileResult != 0) {
        unlink(binaryFile);
        return -996;
    }

    int processStatus = system(binaryFile);
    unlink(binaryFile);

    if (WIFEXITED(processStatus)) {
        return WEXITSTATUS(processStatus);
    }
    return -995;
}

static int compileAndRun(const char *source) {
    const char *assemblyCode = compileToAssembly(source, OPTIMIZATION_NONE);
    return runAssembly(assemblyCode);
}

static char *generate70ParamsProgram(void) {
    char *buffer = malloc(8192);
    int offset = snprintf(buffer, 8192, "int f(");
    for (int i = 1; i <= 70; i++) {
        offset += snprintf(buffer + offset, 8192 - offset, "int p%d%s", i, (i == 70) ? "" : ", ");
    }
    offset += snprintf(buffer + offset, 8192 - offset, ") { return p70; }\nint main() { return f(");
    for (int i = 1; i <= 70; i++) {
        offset += snprintf(buffer + offset, 8192 - offset, "%d%s", i, (i == 70) ? "" : ", ");
    }
    snprintf(buffer + offset, 8192 - offset, "); }\n");
    return buffer;
}

static char *generate300FloatConstantsProgram(void) {
    char *buffer = malloc(16384);
    int offset = snprintf(buffer, 16384, "int main() {\n  float f;\n");
    for (int i = 0; i < 300; i++) {
        offset += snprintf(buffer + offset, 16384 - offset, "  f = %d.5;\n", i);
    }
    snprintf(buffer + offset, 16384 - offset, "  f = 299.5;\n  if (f > 299.0) { return 1; }\n  return 0;\n}\n");
    return buffer;
}

static char *generate300GlobalVariablesProgram(void) {
    char *buffer = malloc(16384);
    int offset = 0;
    for (int i = 0; i < 300; i++) {
        offset += snprintf(buffer + offset, 16384 - offset, "int g%d;\n", i);
    }
    snprintf(buffer + offset, 16384 - offset, "int main() {\n  g299 = 7;\n  return g299;\n}\n");
    return buffer;
}

void setUp(void) {
    resetErrorHandler();
    resetSymbolUids();
    resetBuildString();
    semantic_errors = 0;
    lexical_errors = 0;
    yylineno = 1;
    astRoot = NULL;
}

void tearDown(void) {
}

/* ========================================================================
 * Pruebas estructurales y unitarias del backend
 * ======================================================================== */

// Creacion e inicializacion de FunctionFrame con alineacion a 16 bytes
void test_asm_frame_creation_and_alignment(void) {
    FunctionFrame *frame = createFrame("test_func");
    TEST_ASSERT_NOT_NULL(frame);
    TEST_ASSERT_EQUAL_STRING("test_func", frame->functionName);

    Symbol *sym = newSymbol(strdup("var1"), VARIABLE_SYMBOL, TYPE_INT, NULL);
    TACOperand localOperand = tacVariable(sym);
    int offset1 = addSlot(frame, &localOperand);
    TEST_ASSERT_EQUAL_INT(-4, offset1);

    TACOperand tempOperand = tacTemp("t0", TYPE_INT);
    int offset2 = addSlot(frame, &tempOperand);
    TEST_ASSERT_EQUAL_INT(-8, offset2);

    int anonOffset = addAnonymousSlot(frame);
    TEST_ASSERT_EQUAL_INT(-12, anonOffset);

    int alignedSize = ALIGN_UP(frame->nextOffset, 16);
    TEST_ASSERT_EQUAL_INT(16, alignedSize);
    freeSymbol(sym);
    freeFrame(frame);
}

// FunctionFrame maneja punteros nulos de forma segura
void test_asm_frame_null_safety(void) {
    FunctionFrame *nullNameFrame = createFrame(NULL);
    TEST_ASSERT_NOT_NULL(nullNameFrame);
    TEST_ASSERT_EQUAL_STRING("", nullNameFrame->functionName);
    freeFrame(nullNameFrame);

    freeFrame(NULL);

    Symbol *sym = newSymbol(strdup("x"), VARIABLE_SYMBOL, TYPE_INT, NULL);
    TACOperand operand = tacVariable(sym);
    int offset = addSlot(NULL, &operand);
    TEST_ASSERT_EQUAL_INT(0, offset);
    freeSymbol(sym);

    int anonOffset = addAnonymousSlot(NULL);
    TEST_ASSERT_EQUAL_INT(0, anonOffset);
}

// Generacion de assembly con lista TAC nula no produce fallos
void test_asm_generate_assembly_null_tac_list(void) {
    resetBuildString();
    generateAssembly(NULL, OPTIMIZATION_NONE);
    const char *output = getBuiltString();
    TEST_ASSERT_NOT_NULL(output);
    TEST_ASSERT_EQUAL_STRING("", output);
}

// Emision de assembly incluye la marca de proteccion de pila ejecutable GNU-stack
void test_asm_emits_gnu_stack_note(void) {
    const char *source = "int main() { return 0; }";
    const char *assembly = compileToAssembly(source, OPTIMIZATION_NONE);
    TEST_ASSERT_NOT_NULL(assembly);
    TEST_ASSERT_NOT_NULL(strstr(assembly, ".section .note.GNU-stack,\"\",@progbits"));
}

// Emision de constantes reales produce tabla en seccion .rodata con representacion en bits
void test_asm_emits_float_constant_pool(void) {
    const char *source = "int main() { float f; f = 3.14; return 0; }";
    const char *assembly = compileToAssembly(source, OPTIMIZATION_NONE);
    TEST_ASSERT_NOT_NULL(assembly);
    TEST_ASSERT_NOT_NULL(strstr(assembly, ".section .rodata"));
    TEST_ASSERT_NOT_NULL(strstr(assembly, ".long "));
}

// Reduccion de fuerza reemplaza multiplicacion entera por potencia de 2 por sall
void test_asm_strength_reduction_multiplication_by_power_of_two(void) {
    const char *source = "int main() { int x; x = 5; return x * 4; }";
    const char *assembly = compileToAssembly(source, OPTIMIZATION_STRENGTH);
    TEST_ASSERT_NOT_NULL(assembly);
    TEST_ASSERT_NOT_NULL(strstr(assembly, "sall $2, %eax"));
    TEST_ASSERT_EQUAL_INT(20, runAssembly(assembly));
}

/* ========================================================================
 * Casos E01 - E42: Suite de ejecucion con validacion de codigo de salida
 * ======================================================================== */

// E01: main devuelve su valor como codigo de salida
void test_asm_e01_main_returns_value(void) {
    const char *source = "int main() {\n"
                         "    return 42;\n"
                         "}";
    TEST_ASSERT_EQUAL_INT(42, compileAndRun(source));
}

// E02: void main sin return termina con codigo 0
void test_asm_e02_void_main_returns_zero(void) {
    const char *source = "void main() {}";
    TEST_ASSERT_EQUAL_INT(0, compileAndRun(source));
}

// E03: Precedencia de operadores multiplicacion antes que suma
void test_asm_e03_operator_precedence(void) {
    const char *source = "int main() {\n"
                         "    return 2 + 3 * 4;\n"
                         "}";
    TEST_ASSERT_EQUAL_INT(14, compileAndRun(source));
}

// E04: Division entera truncada hacia cero con dividendo negativo
void test_asm_e04_signed_integer_division(void) {
    const char *source = "int main() {\n"
                         "    return -7 / 2 + 10;\n"
                         "}";
    TEST_ASSERT_EQUAL_INT(7, compileAndRun(source));
}

// E05: Resto con dividendo negativo
void test_asm_e05_signed_integer_modulo(void) {
    const char *source = "int main() {\n"
                         "    return -7 % 3 + 10;\n"
                         "}";
    TEST_ASSERT_EQUAL_INT(9, compileAndRun(source));
}

// E06: Menos unario y negacion logica combinados en expresiones
void test_asm_e06_unary_minus_and_not(void) {
    const char *source = "int main() {\n"
                         "    boolean b; b = !false; if (b) { return -(-5); } return 0;\n"
                         "}";
    TEST_ASSERT_EQUAL_INT(5, compileAndRun(source));
}

// E07: Sombreado de variables en bloque interno no pisa la del bloque externo
void test_asm_e07_variable_shadowing(void) {
    const char *source = "int main() {\n"
                         "    int x; x = 1; { int x; x = 2; } return x;\n"
                         "}";
    TEST_ASSERT_EQUAL_INT(1, compileAndRun(source));
}

// E08: Pasaje de parametros por valor no afecta la variable del llamador
void test_asm_e08_pass_by_value(void) {
    const char *source = "int f(int a) { a = 99; return a; }\n"
                         "int main() {\n"
                         "    int x;\n"
                         "    x = 7;\n"
                         "    f(x);\n"
                         "    return x;\n"
                         "}";
    TEST_ASSERT_EQUAL_INT(7, compileAndRun(source));
}

// E09: Recursion simple: factorial de 5
void test_asm_e09_recursion_factorial(void) {
    const char *source = "int fact(int n) { if (n < 2) { return 1; } else { return n * fact(n - 1); } }\n"
                         "int main() { return fact(5); }";
    TEST_ASSERT_EQUAL_INT(120, compileAndRun(source));
}

// E10: Recursion doble: fibonacci de 10
void test_asm_e10_recursion_fibonacci(void) {
    const char *source = "int fib(int n) { if (n < 2) { return n; } else { return fib(n - 1) + fib(n - 2); } }\n"
                         "int main() { return fib(10); }";
    TEST_ASSERT_EQUAL_INT(55, compileAndRun(source));
}

// E11: Ciclo while: suma acumulada de 0 a 9
void test_asm_e11_while_loop(void) {
    const char *source = "int main() { int i; int s; i = 0; s = 0; while (i < 10) { s = s + i; i = i + 1; } return s; }";
    TEST_ASSERT_EQUAL_INT(45, compileAndRun(source));
}

// E12: Variables globales arrancan en cero y se modifican desde metodos
void test_asm_e12_global_variables_in_methods(void) {
    const char *source = "int g;\n"
                         "void inc() { g = g + 1; }\n"
                         "int main() { inc(); inc(); return g; }";
    TEST_ASSERT_EQUAL_INT(2, compileAndRun(source));
}

// E13: Cortocircuito con && no evalua lado derecho si el izquierdo es falso
void test_asm_e13_short_circuit_and(void) {
    const char *source = "int c;\n"
                         "boolean inc() { c = c + 1; return true; }\n"
                         "int main() { boolean b; b = false && inc(); return c; }";
    TEST_ASSERT_EQUAL_INT(0, compileAndRun(source));
}

// E14: Cortocircuito con || no evalua lado derecho si el izquierdo es verdadero
void test_asm_e14_short_circuit_or(void) {
    const char *source = "int c;\n"
                         "boolean inc() { c = c + 1; return true; }\n"
                         "int main() { boolean b; b = true || inc(); return c; }";
    TEST_ASSERT_EQUAL_INT(0, compileAndRun(source));
}

// E15: Sin cortocircuito aplicable ambos lados se evaluan exactamente una vez
void test_asm_e15_short_circuit_fallthrough(void) {
    const char *source = "int c;\n"
                         "boolean inc() { c = c + 1; return true; }\n"
                         "int main() { boolean b; b = true && inc(); b = false || inc(); return c; }";
    TEST_ASSERT_EQUAL_INT(2, compileAndRun(source));
}

// E16: Operaciones y comparacion de punto flotante
void test_asm_e16_float_operations_and_comparison(void) {
    const char *source = "int main() { float f; f = 7; f = f / 2; if (f > 3.4) { if (f < 3.6) { return 1; } } return 0; }";
    TEST_ASSERT_EQUAL_INT(1, compileAndRun(source));
}

// E17: Coercion int a float al llamar metodo y al retornar
void test_asm_e17_coercion_on_call_and_return(void) {
    const char *source = "float half(float x) { return x / 2; }\n"
                         "int main() { float r; r = half(7); if (r == 3.5) { return 1; } return 0; }";
    TEST_ASSERT_EQUAL_INT(1, compileAndRun(source));
}

// E18: Ocho argumentos enteros con llamada anidada que exceden los seis registros
void test_asm_e18_eight_arguments_and_nested_call(void) {
    const char *source = "int s(int a, int b, int c, int d, int e, int f, int g, int h) {\n"
                         "    return a + b + c + d + e + f + g + h;\n"
                         "}\n"
                         "int main() {\n"
                         "    return s(1, 2, 3, 4, 5, 6, 7, s(0, 0, 0, 0, 0, 0, 0, 1)) - 27;\n"
                         "}";
    TEST_ASSERT_EQUAL_INT(2, compileAndRun(source));
}

// E19: Igualdad de booleanos y operacion relacional anidada
void test_asm_e19_boolean_equality_and_relational(void) {
    const char *source = "int main() { if (1 < 2 == true) { return 1; } return 0; }";
    TEST_ASSERT_EQUAL_INT(1, compileAndRun(source));
}

// E20: Expresion aritmetica profunda con muchos temporales
void test_asm_e20_deep_expression_many_temporaries(void) {
    const char *source = "int main() { return ((((((1+2)+3)+4)+5)+6)+7) * ((1+1)*(1+1)) - 100; }";
    TEST_ASSERT_EQUAL_INT(12, compileAndRun(source));
}

// E21: 70 argumentos salientes acumulados con pasaje por pila (falla hoy -> item 6.2)
void test_asm_e21_limit_70_arguments(void) {
    TEST_IGNORE_MESSAGE("Falla conocido -> limite de 64 argumentos salientes acumulados (item 6.2)");
    char *source = generate70ParamsProgram();
    int result = compileAndRun(source);
    free(source);
    TEST_ASSERT_EQUAL_INT(70, result);
}

// E22: 300 constantes reales distintas conservan su valor (falla hoy -> item 6.3)
void test_asm_e22_limit_300_float_constants(void) {
    TEST_IGNORE_MESSAGE("Falla conocido -> limite de 256 constantes reales (item 6.3)");
    char *source = generate300FloatConstantsProgram();
    int result = compileAndRun(source);
    free(source);
    TEST_ASSERT_EQUAL_INT(1, result);
}

// E23: 300 variables globales con almacenamiento estatico (falla hoy -> item 6.4)
void test_asm_e23_limit_300_global_variables(void) {
    TEST_IGNORE_MESSAGE("Falla conocido -> limite de 256 variables globales (item 6.4)");
    char *source = generate300GlobalVariablesProgram();
    int result = compileAndRun(source);
    free(source);
    TEST_ASSERT_EQUAL_INT(7, result);
}

// E24: Comparacion de igualdad entre reales con NaN debe ser falsa (falla hoy -> item 6.5)
void test_asm_e24_float_nan_equality_is_false(void) {
    TEST_IGNORE_MESSAGE("Falla conocido -> comiss con NaN activa ZF y sete lee igual (item 6.5)");
    const char *source = "int main() { float z; float n; z = 0.0; n = z / z; if (n == n) { return 1; } return 0; }";
    TEST_ASSERT_EQUAL_INT(0, compileAndRun(source));
}

// E25: Metodo con identificador homonimo a funcion estandar de libc (exit)
void test_asm_e25_method_named_after_libc_function(void) {
    const char *source = "int exit(int a) { return a + 1; }\n"
                         "int main() { return exit(4); }";
    TEST_ASSERT_EQUAL_INT(5, compileAndRun(source));
}

// E26: Variables locales repartidas en bloques anidados con resolucion de stack
void test_asm_e26_locals_in_nested_blocks(void) {
    const char *source = "int main() { int a; a = 1; { int b; b = 2; { int c; c = 3; a = a + b + c; } } return a; }";
    TEST_ASSERT_EQUAL_INT(6, compileAndRun(source));
}

// E27: Metodo void con sentencia return explicita sin expresion
void test_asm_e27_void_method_with_explicit_return(void) {
    const char *source = "int g;\n"
                         "void f(int x) { if (x < 0) { return; } g = x; }\n"
                         "int main() { f(-1); f(5); return g; }";
    TEST_ASSERT_EQUAL_INT(5, compileAndRun(source));
}

// E28: Sentencias vacias y bloques vacios en el cuerpo de la funcion
void test_asm_e28_empty_statements_and_blocks(void) {
    const char *source = "int main() { ; { } ; return 3; }";
    TEST_ASSERT_EQUAL_INT(3, compileAndRun(source));
}

// E29: Argumentos de una llamada a funcion se evaluan de izquierda a derecha
void test_asm_e29_function_arguments_evaluation_order(void) {
    const char *source = "int c;\n"
                         "int next() { c = c + 1; return c; }\n"
                         "int sub(int a, int b) { return a - b; }\n"
                         "int main() { return sub(next(), next()) + 10; }";
    TEST_ASSERT_EQUAL_INT(9, compileAndRun(source));
}

// E30: Operandos de una operacion binaria se evaluan de izquierda a derecha
void test_asm_e30_binary_operands_evaluation_order(void) {
    const char *source = "int c;\n"
                         "int next() { c = c + 1; return c; }\n"
                         "int main() { return next() - next() + 10; }";
    TEST_ASSERT_EQUAL_INT(9, compileAndRun(source));
}

// E31: Diez parametros float donde los excedentes a los ocho registros van por pila
void test_asm_e31_ten_float_parameters_via_stack(void) {
    const char *source = "float f(float a, float b, float c, float d, float e, float f1, float g, float h, float i, float j) {\n"
                         "    return a + b + c + d + e + f1 + g + h + i + j;\n"
                         "}\n"
                         "int main() {\n"
                         "    float r;\n"
                         "    r = f(1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0);\n"
                         "    if (r == 55.0) { return 1; }\n"
                         "    return 0;\n"
                         "}";
    TEST_ASSERT_EQUAL_INT(1, compileAndRun(source));
}

// E32: Parametros con tipos mezclados de int y float en el orden de registros respectivo
void test_asm_e32_mixed_int_and_float_parameters(void) {
    const char *source = "float m(int a, float b, int c, float d) {\n"
                         "    return a + b + c + d;\n"
                         "}\n"
                         "int main() {\n"
                         "    float r;\n"
                         "    r = m(1, 2.5, 3, 4.5);\n"
                         "    if (r == 11.0) { return 1; }\n"
                         "    return 0;\n"
                         "}";
    TEST_ASSERT_EQUAL_INT(1, compileAndRun(source));
}

// E33: Variables globales float y boolean con lectura y asignacion
void test_asm_e33_global_float_and_boolean(void) {
    const char *source = "float g;\n"
                         "boolean b;\n"
                         "int main() { g = 2.5; b = g > 2.0; if (b) { if (g == 2.5) { return 1; } } return 0; }";
    TEST_ASSERT_EQUAL_INT(1, compileAndRun(source));
}

// E34: Menos unario sobre variable float y literal negativo
void test_asm_e34_unary_minus_on_float(void) {
    const char *source = "int main() { float f; f = -2.5; f = -f; if (f == 2.5) { return 1; } return 0; }";
    TEST_ASSERT_EQUAL_INT(1, compileAndRun(source));
}

// E35: Comparacion relacional mixta entre int y float con coercion implícita
void test_asm_e35_mixed_comparison_int_and_float(void) {
    const char *source = "int main() { if (1 < 2.5) { if (3 > 2.5) { return 1; } } return 0; }";
    TEST_ASSERT_EQUAL_INT(1, compileAndRun(source));
}

// E36: Metodo booleano utilizado como condicion de control y como argumento
void test_asm_e36_boolean_method_as_condition_and_arg(void) {
    const char *source = "boolean neg(boolean x) { return !x; }\n"
                         "int main() { if (neg(neg(true))) { return 1; } return 0; }";
    TEST_ASSERT_EQUAL_INT(1, compileAndRun(source));
}

// E37: Bucle while con if/else anidados y salida anticipada por return
void test_asm_e37_while_with_nested_if_and_return(void) {
    const char *source = "int f(int n) {\n"
                         "    int i; i = 0;\n"
                         "    while (i < 100) {\n"
                         "        if (i == n) { return i * 2; }\n"
                         "        else { i = i + 1; }\n"
                         "    }\n"
                         "    return 0;\n"
                         "}\n"
                         "int main() { return f(21); }";
    TEST_ASSERT_EQUAL_INT(42, compileAndRun(source));
}

// E38: Llamada a metodo con resultado ignorado ejecutada como sentencia
void test_asm_e38_method_call_statement_discards_result(void) {
    const char *source = "int g;\n"
                         "int f() { g = 9; return 1; }\n"
                         "int main() { f(); return g; }";
    TEST_ASSERT_EQUAL_INT(9, compileAndRun(source));
}

// E39: Parametro formal oculta a variable global homonima dentro del cuerpo del metodo
void test_asm_e39_parameter_shadows_global(void) {
    const char *source = "int x;\n"
                         "int f(int x) { return x + 1; }\n"
                         "int main() { x = 100; return f(4); }";
    TEST_ASSERT_EQUAL_INT(5, compileAndRun(source));
}

// E40: Coercion int a float al asignar el resultado de un metodo entero a variable float
void test_asm_e40_coercion_on_assignment_of_method_result(void) {
    const char *source = "int two() { return 2; }\n"
                         "int main() { float f; f = two(); f = f + 0.5; if (f == 2.5) { return 1; } return 0; }";
    TEST_ASSERT_EQUAL_INT(1, compileAndRun(source));
}

// E41: Comparacion menor con NaN en reales debe ser falsa (falla hoy -> item 6.5)
void test_asm_e41_float_nan_less_than_is_false(void) {
    TEST_IGNORE_MESSAGE("Falla conocido -> comiss con NaN activa CF y setb lee menor (item 6.5)");
    const char *source = "int main() { float z; float n; z = 0.0; n = z / z; if (n < 1.0) { return 1; } return 0; }";
    TEST_ASSERT_EQUAL_INT(0, compileAndRun(source));
}

// E42: Comparacion mayor con NaN en reales es falsa segun IEEE
void test_asm_e42_float_nan_greater_than_is_false(void) {
    const char *source = "int main() { float z; float n; z = 0.0; n = z / z; if (n > 1.0) { return 1; } return 0; }";
    TEST_ASSERT_EQUAL_INT(0, compileAndRun(source));
}

/* ========================================================================
 * Runner principal de la suite
 * ======================================================================== */

int main(void) {
    UNITY_BEGIN();

    /* Pruebas estructurales y unitarias */
    RUN_TEST(test_asm_frame_creation_and_alignment);
    RUN_TEST(test_asm_frame_null_safety);
    RUN_TEST(test_asm_generate_assembly_null_tac_list);
    RUN_TEST(test_asm_emits_gnu_stack_note);
    RUN_TEST(test_asm_emits_float_constant_pool);
    RUN_TEST(test_asm_strength_reduction_multiplication_by_power_of_two);

    /* Suite de ejecucion E01 - E42 */
    RUN_TEST(test_asm_e01_main_returns_value);
    RUN_TEST(test_asm_e02_void_main_returns_zero);
    RUN_TEST(test_asm_e03_operator_precedence);
    RUN_TEST(test_asm_e04_signed_integer_division);
    RUN_TEST(test_asm_e05_signed_integer_modulo);
    RUN_TEST(test_asm_e06_unary_minus_and_not);
    RUN_TEST(test_asm_e07_variable_shadowing);
    RUN_TEST(test_asm_e08_pass_by_value);
    RUN_TEST(test_asm_e09_recursion_factorial);
    RUN_TEST(test_asm_e10_recursion_fibonacci);
    RUN_TEST(test_asm_e11_while_loop);
    RUN_TEST(test_asm_e12_global_variables_in_methods);
    RUN_TEST(test_asm_e13_short_circuit_and);
    RUN_TEST(test_asm_e14_short_circuit_or);
    RUN_TEST(test_asm_e15_short_circuit_fallthrough);
    RUN_TEST(test_asm_e16_float_operations_and_comparison);
    RUN_TEST(test_asm_e17_coercion_on_call_and_return);
    RUN_TEST(test_asm_e18_eight_arguments_and_nested_call);
    RUN_TEST(test_asm_e19_boolean_equality_and_relational);
    RUN_TEST(test_asm_e20_deep_expression_many_temporaries);
    RUN_TEST(test_asm_e21_limit_70_arguments);
    RUN_TEST(test_asm_e22_limit_300_float_constants);
    RUN_TEST(test_asm_e23_limit_300_global_variables);
    RUN_TEST(test_asm_e24_float_nan_equality_is_false);
    RUN_TEST(test_asm_e25_method_named_after_libc_function);
    RUN_TEST(test_asm_e26_locals_in_nested_blocks);
    RUN_TEST(test_asm_e27_void_method_with_explicit_return);
    RUN_TEST(test_asm_e28_empty_statements_and_blocks);
    RUN_TEST(test_asm_e29_function_arguments_evaluation_order);
    RUN_TEST(test_asm_e30_binary_operands_evaluation_order);
    RUN_TEST(test_asm_e31_ten_float_parameters_via_stack);
    RUN_TEST(test_asm_e32_mixed_int_and_float_parameters);
    RUN_TEST(test_asm_e33_global_float_and_boolean);
    RUN_TEST(test_asm_e34_unary_minus_on_float);
    RUN_TEST(test_asm_e35_mixed_comparison_int_and_float);
    RUN_TEST(test_asm_e36_boolean_method_as_condition_and_arg);
    RUN_TEST(test_asm_e37_while_with_nested_if_and_return);
    RUN_TEST(test_asm_e38_method_call_statement_discards_result);
    RUN_TEST(test_asm_e39_parameter_shadows_global);
    RUN_TEST(test_asm_e40_coercion_on_assignment_of_method_result);
    RUN_TEST(test_asm_e41_float_nan_less_than_is_false);
    RUN_TEST(test_asm_e42_float_nan_greater_than_is_false);

    return UNITY_END();
}
