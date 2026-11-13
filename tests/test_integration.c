/**
 * @file test_integration.c
 * @brief Pruebas de punta a punta: programa C-TDS -> assembly x86-64 -> ejecutable.
 *
 * Es la única suite que atraviesa todas las etapas del compilador. Las suites unitarias
 * ya verifican el detalle de cada etapa con entradas armadas a mano; esta solo comprueba
 * lo que ninguna de ellas puede ver por separado:
 *  - que las etapas encajan entre sí (un programa mínimo corre de principio a fin);
 *  - que el programa generado se comporta como el fuente, incluyendo lo que depende del
 *    ABI y de la pila y solo se detecta ejecutando;
 *  - que optimizar no cambia el resultado (mismo código de salida con y sin optimizaciones);
 *  - los límites de capacidad del generador.
 *
 * Los casos son pocos y simples a propósito: cada comportamiento fino ya está cubierto
 * en la suite de su etapa. Un fallo acá sin un fallo unitario señala un problema de
 * integración entre etapas, o un caso que ninguna suite unitaria cubre todavía.
 */

#define _GNU_SOURCE
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
#include "tac.h"
#include "optimizer.h"
#include "object_code.h"
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
 * Helpers del arnés de compilación y ejecución
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

    if (optimizations != OPTIMIZATION_NONE) {
        optimizeIntermediateCode((TACList *)tacList, optimizations);
    }

    resetBuildString();
    generateAssembly(tacList, optimizations);
    return getBuiltString();
}

static int runAssembly(const char *assemblyCode) {
    if (assemblyCode == NULL || assemblyCode[0] == '\0') {
        return -999;
    }

    char assemblyFile[] = "/tmp/test_opt_asm_XXXXXX.s";
    int asmFd = mkstemps(assemblyFile, 2);
    if (asmFd < 0) return -998;
    ssize_t written = write(asmFd, assemblyCode, strlen(assemblyCode));
    (void)written;
    close(asmFd);

    char binaryFile[] = "/tmp/test_opt_bin_XXXXXX";
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

// Verifica que el programa compile y retorne el mismo valor con y sin optimizaciones
static void assertSemanticEquivalence(const char *source, OptimizationSet optSet, int expectedExitCode) {
    const char *asmNoOpt = compileToAssembly(source, OPTIMIZATION_NONE);
    TEST_ASSERT_NOT_NULL(asmNoOpt);
    int exitNoOpt = runAssembly(asmNoOpt);
    TEST_ASSERT_EQUAL_INT(expectedExitCode, exitNoOpt);

    const char *asmOpt = compileToAssembly(source, optSet);
    TEST_ASSERT_NOT_NULL(asmOpt);
    int exitOpt = runAssembly(asmOpt);
    TEST_ASSERT_EQUAL_INT(expectedExitCode, exitOpt);
    TEST_ASSERT_EQUAL_INT(exitNoOpt, exitOpt);
}

static int compileAndRun(const char *source) {
    const char *assemblyCode = compileToAssembly(source, OPTIMIZATION_NONE);
    return runAssembly(assemblyCode);
}

static int compileAndRunFile(const char *filePath) {
    FILE *file = fopen(filePath, "r");
    if (file == NULL) return -999;
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *buffer = malloc(length + 1);
    if (buffer == NULL) {
        fclose(file);
        return -998;
    }
    size_t readBytes = fread(buffer, 1, length, file);
    buffer[readBytes] = '\0';
    fclose(file);
    int result = compileAndRun(buffer);
    free(buffer);
    return result;
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

static char *generate300GlobalVariablesProgram(void) {
    char *buffer = malloc(16384);
    int offset = 0;
    for (int i = 0; i < 300; i++) {
        offset += snprintf(buffer + offset, 16384 - offset, "int g%d;\n", i);
    }
    snprintf(buffer + offset, 16384 - offset, "int main() {\n  g299 = 7;\n  return g299;\n}\n");
    return buffer;
}

void setUp(void) {}

void tearDown(void) {}

/* ========================================================================
 * Grupo 1: Programas completos de punta a punta
 * ======================================================================== */

// Un programa mínimo atraviesa lexer, parser, semántica, TAC y código objeto y ejecuta
void test_integration_minimal_program_runs(void) {
    const char *source = "int main() { int x; x = 40; return x + 2; }";
    TEST_ASSERT_EQUAL_INT(42, compileAndRun(source));
}

// Los programas de examples/ compilan, ejecutan y terminan con código 0
void test_integration_canonical_examples(void) {
    TEST_ASSERT_EQUAL_INT(0, compileAndRunFile("examples/factorial.ctds"));
    TEST_ASSERT_EQUAL_INT(0, compileAndRunFile("examples/programa_sin_error.ctds"));
}

// Llamadas anidadas y pasaje de argumentos por pila (>6) segun System V AMD64
void test_integration_calls_and_stack_parameters(void) {
    const char *nested = "int doble(int x) { return x * 2; }\n"
                         "int sumar(int a, int b) { return a + b; }\n"
                         "int main() { return sumar(3, doble(doble(2))); }";
    TEST_ASSERT_EQUAL_INT(11, compileAndRun(nested));

    const char *stack = "int s(int a, int b, int c, int d, int e, int f, int g, int h) {\n"
                        "    return a + b + c + d + e + f + g + h;\n"
                        "}\n"
                        "int main() { return s(1, 2, 3, 4, 5, 6, 7, 8); }";
    TEST_ASSERT_EQUAL_INT(36, compileAndRun(stack));
}

// Sombreado y ambitos anidados resueltos de la semantica al assembly
void test_integration_shadowing_and_scopes(void) {
    const char *source = "int g;\n"
                         "int f(int g) { return g + 1; }\n"
                         "int main() {\n"
                         "    int r, x;\n"
                         "    g = 100;\n"
                         "    r = f(4);\n"
                         "    x = 1;\n"
                         "    { int x; x = 2; }\n"
                         "    if (r == 5 && x == 1 && g == 100) { return 1; }\n"
                         "    return 0;\n"
                         "}";
    TEST_ASSERT_EQUAL_INT(1, compileAndRun(source));
}

// El cortocircuito no ejecuta el segundo operando cuando el primero decide el resultado
void test_integration_short_circuit_side_effects(void) {
    const char *source = "int c;\n"
                         "boolean inc() { c = c + 1; return true; }\n"
                         "int main() {\n"
                         "    boolean b1, b2, b3;\n"
                         "    c = 0;\n"
                         "    b1 = false && inc();\n"
                         "    b2 = true || inc();\n"
                         "    b3 = true && inc();\n"
                         "    if (c == 1 && !b1 && b2 && b3) { return 1; }\n"
                         "    return 0;\n"
                         "}";
    TEST_ASSERT_EQUAL_INT(1, compileAndRun(source));
}

/* ========================================================================
 * Grupo 2: Limites de capacidad del generador
 * ======================================================================== */

// 70 argumentos salientes acumulados con pasaje por pila
void test_integration_limit_70_arguments(void) {
    char *source = generate70ParamsProgram();
    int result = compileAndRun(source);
    free(source);
    TEST_ASSERT_EQUAL_INT(70, result);
}

// 300 variables globales con almacenamiento estatico
void test_integration_limit_300_global_variables(void) {
    char *source = generate300GlobalVariablesProgram();
    int result = compileAndRun(source);
    free(source);
    TEST_ASSERT_EQUAL_INT(7, result);
}

/* ========================================================================
 * Grupo 3: Equivalencia semantica con y sin optimizaciones
 * ======================================================================== */

// Recursion: el optimizador no altera llamadas ni retornos
void test_integration_optimizations_preserve_recursion(void) {
    const char *source =
        "int fact(int n) {\n"
        "    if (n == 1) {\n"
        "        return 1;\n"
        "    }\n"
        "    return n * fact(n - 1);\n"
        "}\n"
        "int main() {\n"
        "    return fact(4);\n"
        "}\n";
    assertSemanticEquivalence(source, OPTIMIZATION_ALL, 24);
}

// Bucles anidados con if/else: el flujo de control sobrevive a las optimizaciones
void test_integration_optimizations_preserve_control_flow(void) {
    const char *source =
        "int main() {\n"
        "    int sum;\n"
        "    int i;\n"
        "    int j;\n"
        "    sum = 0;\n"
        "    i = 0;\n"
        "    while (i < 3) {\n"
        "        j = 0;\n"
        "        while (j < 3) {\n"
        "            if (i == j) {\n"
        "                sum = sum + 2;\n"
        "            } else {\n"
        "                sum = sum + 1;\n"
        "            }\n"
        "            j = j + 1;\n"
        "        }\n"
        "        i = i + 1;\n"
        "    }\n"
        "    return sum;\n"
        "}\n";
    assertSemanticEquivalence(source, OPTIMIZATION_ALL, 12);
}

// Globales modificadas por una llamada: la propagacion no debe usar valores viejos
void test_integration_optimizations_preserve_global_state(void) {
    const char *source =
        "int g1;\n"
        "int g2;\n"
        "void mutate(int step) {\n"
        "    g1 = g1 + step;\n"
        "    g2 = g2 * step;\n"
        "}\n"
        "int main() {\n"
        "    int x;\n"
        "    g1 = 10;\n"
        "    g2 = 5;\n"
        "    mutate(2);\n"
        "    x = g1 + 3;\n"
        "    if (g1 == 12 && g2 == 10 && x == 15) {\n"
        "        return 1;\n"
        "    }\n"
        "    return 0;\n"
        "}\n";
    assertSemanticEquivalence(source, OPTIMIZATION_ALL, 1);
}

int main(void) {
    UNITY_BEGIN();

    /* Grupo 1: Programas completos de punta a punta */
    RUN_TEST(test_integration_minimal_program_runs);
    RUN_TEST(test_integration_canonical_examples);
    RUN_TEST(test_integration_calls_and_stack_parameters);
    RUN_TEST(test_integration_shadowing_and_scopes);
    RUN_TEST(test_integration_short_circuit_side_effects);

    /* Grupo 2: Limites de capacidad del generador */
    RUN_TEST(test_integration_limit_70_arguments);
    RUN_TEST(test_integration_limit_300_global_variables);

    /* Grupo 3: Equivalencia semantica con y sin optimizaciones */
    RUN_TEST(test_integration_optimizations_preserve_recursion);
    RUN_TEST(test_integration_optimizations_preserve_control_flow);
    RUN_TEST(test_integration_optimizations_preserve_global_state);

    return UNITY_END();
}
