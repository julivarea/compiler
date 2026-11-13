/**
 * @file test_optimizer.c
 * @brief Suite de pruebas integrales para el motor de optimizaciones sobre TAC y Assembly.
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

// Helpers del arnes de compilacion y ejecucion

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

// Helper para compilar a TAC y obtener la lista con o sin optimizaciones
static TACList *compileToOptimizedTAC(const char *source, OptimizationSet optSet) {
    resetErrorHandler();
    resetSymbolUids();
    semantic_errors = 0;
    lexical_errors = 0;
    yylineno = 1;
    astRoot = NULL;

    YY_BUFFER_STATE buffer = yy_scan_string(source);
    int parseResult = yyparse();
    yy_delete_buffer(buffer);

    if (parseResult != 0 || lexical_errors > 0) return NULL;

    analyzeSemantics(astRoot, NULL);
    if (semantic_errors > 0) return NULL;

    TACList *tacList = (TACList *)generateIntermediateCode(astRoot);
    if (tacList != NULL && optSet != OPTIMIZATION_NONE) {
        optimizeIntermediateCode(tacList, optSet);
    }
    return tacList;
}

static int countInstructions(const TACList *list) {
    if (list == NULL) return 0;
    int count = 0;
    for (TACInstruction *inst = list->head; inst != NULL; inst = inst->next) {
        count++;
    }
    return count;
}

static int countOperations(const TACList *list, TACOperation op) {
    if (list == NULL) return 0;
    int count = 0;
    for (TACInstruction *inst = list->head; inst != NULL; inst = inst->next) {
        if (inst->operation == op) {
            count++;
        }
    }
    return count;
}

static const TACInstruction *findAssignmentTo(const TACList *tacList, const char *varPrefix) {
    if (tacList == NULL || varPrefix == NULL) return NULL;
    size_t prefixLen = strlen(varPrefix);
    for (const TACInstruction *inst = tacList->head; inst != NULL; inst = inst->next) {
        if (inst->operation == TAC_ASSIGN && inst->result.name[0] != '\0') {
            if (strncmp(inst->result.name, varPrefix, prefixLen) == 0) {
                char nextChar = inst->result.name[prefixLen];
                if (nextChar == '\0' || nextChar == '@') {
                    return inst;
                }
            }
        }
    }
    return NULL;
}

static const TACInstruction *findAssignmentWithConstant(const TACList *tacList, const char *varPrefix, int expectedConstant) {
    if (tacList == NULL || varPrefix == NULL) return NULL;
    size_t prefixLen = strlen(varPrefix);
    for (const TACInstruction *inst = tacList->head; inst != NULL; inst = inst->next) {
        if (inst->operation == TAC_ASSIGN && inst->result.name[0] != '\0') {
            if (strncmp(inst->result.name, varPrefix, prefixLen) == 0) {
                char nextChar = inst->result.name[prefixLen];
                if (nextChar == '\0' || nextChar == '@') {
                    if (inst->firstOperand.kind == OPERAND_CONSTANT &&
                        inst->firstOperand.value.intValue == expectedConstant) {
                        return inst;
                    }
                }
            }
        }
    }
    return NULL;
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

// ------------------------------------------------------------------------
// Grupo 1: Constant Folding (Plegado de Constantes)
// ------------------------------------------------------------------------

// Aritmetica constante entera se pliega en compilacion conservando equivalencia semantica
void test_opt_const_fold_arithmetic_equivalence(void) {
    const char *source =
        "int main() {\n"
        "    int res;\n"
        "    res = 2 + 3 * 4 + (20 / 4) - (10 % 3);\n"
        "    return res;\n"
        "}\n";
    // 2 + 12 + 5 - 1 = 18
    TACList *tac = compileToOptimizedTAC(source, OPTIMIZATION_CONST_FOLD | OPTIMIZATION_CONST_PROP);
    TEST_ASSERT_NOT_NULL(tac);
    // Todas las operaciones aritmeticas sobre constantes se plegaron
    TEST_ASSERT_EQUAL_INT(0, countOperations(tac, TAC_ADD));
    TEST_ASSERT_EQUAL_INT(0, countOperations(tac, TAC_MUL));
    TEST_ASSERT_EQUAL_INT(0, countOperations(tac, TAC_DIV));
    TEST_ASSERT_EQUAL_INT(0, countOperations(tac, TAC_MOD));
    // La variable res recibe directamente el resultado constante 18
    TEST_ASSERT_NOT_NULL(findAssignmentWithConstant(tac, "res", 18));
    assertSemanticEquivalence(source, OPTIMIZATION_CONST_FOLD | OPTIMIZATION_CONST_PROP, 18);
}

// Operaciones relacionales y logicas entre constantes se pliegan correctamente
void test_opt_const_fold_boolean_and_relational(void) {
    const char *source =
        "int main() {\n"
        "    boolean a;\n"
        "    boolean b;\n"
        "    a = 10 > 5;\n"
        "    b = 2 < 4;\n"
        "    if (a && b) {\n"
        "        return 1;\n"
        "    }\n"
        "    return 0;\n"
        "}\n";
    TACList *tac = compileToOptimizedTAC(source, OPTIMIZATION_CONST_FOLD | OPTIMIZATION_CONST_PROP);
    TEST_ASSERT_NOT_NULL(tac);
    // Operaciones relacionales sobre constantes se plegaron en tiempo de compilacion
    TEST_ASSERT_EQUAL_INT(0, countOperations(tac, TAC_GT));
    TEST_ASSERT_EQUAL_INT(0, countOperations(tac, TAC_LT));
    // Las variables booleanas a y b reciben el valor 1 (true) directamente
    TEST_ASSERT_NOT_NULL(findAssignmentWithConstant(tac, "a", 1));
    TEST_ASSERT_NOT_NULL(findAssignmentWithConstant(tac, "b", 1));
    assertSemanticEquivalence(source, OPTIMIZATION_CONST_FOLD | OPTIMIZATION_CONST_PROP, 1);
}

// Operaciones unarias entre constantes (menos unario y not) se pliegan
void test_opt_const_fold_unary_operators(void) {
    const char *source =
        "int main() {\n"
        "    int x;\n"
        "    int res;\n"
        "    x = -5;\n"
        "    res = -x + 15;\n"
        "    return res;\n"
        "}\n";
    TACList *tac = compileToOptimizedTAC(source, OPTIMIZATION_CONST_FOLD | OPTIMIZATION_CONST_PROP);
    TEST_ASSERT_NOT_NULL(tac);
    // Menos unario sobre constante plegado
    TEST_ASSERT_EQUAL_INT(0, countOperations(tac, TAC_UMINUS));
    // La variable x recibe directamente el valor constante -5
    TEST_ASSERT_NOT_NULL(findAssignmentWithConstant(tac, "x", -5));
    // La variable res recibe 5 + 15 = 20
    TEST_ASSERT_NOT_NULL(findAssignmentWithConstant(tac, "res", 20));
    assertSemanticEquivalence(source, OPTIMIZATION_CONST_FOLD | OPTIMIZATION_CONST_PROP, 20);
}

// Plegado de constantes reduce cuadruplas aritmeticas a asignaciones de constantes
void test_opt_const_fold_reduces_arithmetic_quads(void) {
    const char *source =
        "int main() {\n"
        "    int k;\n"
        "    k = 10 + 20;\n"
        "    return k;\n"
        "}\n";
    TACList *tacNoOpt = compileToOptimizedTAC(source, OPTIMIZATION_NONE);
    TEST_ASSERT_NOT_NULL(tacNoOpt);
    int addsNoOpt = countOperations(tacNoOpt, TAC_ADD);
    TEST_ASSERT_TRUE(addsNoOpt >= 1);

    TACList *tacOpt = compileToOptimizedTAC(source, OPTIMIZATION_CONST_FOLD | OPTIMIZATION_CONST_PROP);
    TEST_ASSERT_NOT_NULL(tacOpt);
    int addsOpt = countOperations(tacOpt, TAC_ADD);
    TEST_ASSERT_EQUAL_INT(0, addsOpt);
    // La variable k almacena 30 directamente sin sumas en tiempo de ejecucion
    TEST_ASSERT_NOT_NULL(findAssignmentWithConstant(tacOpt, "k", 30));
    assertSemanticEquivalence(source, OPTIMIZATION_CONST_FOLD | OPTIMIZATION_CONST_PROP, 30);
}

// Division y modulo con desbordamiento (INT32_MIN / -1 e INT32_MIN % -1) no se pliegan y no abortan el compilador
void test_opt_const_fold_signed_overflow_does_not_crash(void) {
    const char *sourceDiv =
        "int main() {\n"
        "    int minVal;\n"
        "    int res;\n"
        "    minVal = -2147483647 - 1;\n"
        "    res = minVal / -1;\n"
        "    return res;\n"
        "}\n";

    TACList *tacDiv = compileToOptimizedTAC(sourceDiv, OPTIMIZATION_CONST_FOLD | OPTIMIZATION_CONST_PROP);
    TEST_ASSERT_NOT_NULL(tacDiv);
    // La division no debe plegarse debido al desbordamiento
    TEST_ASSERT_EQUAL_INT(1, countOperations(tacDiv, TAC_DIV));

    const char *sourceMod =
        "int main() {\n"
        "    int minVal;\n"
        "    int res;\n"
        "    minVal = -2147483647 - 1;\n"
        "    res = minVal % -1;\n"
        "    return res;\n"
        "}\n";

    TACList *tacMod = compileToOptimizedTAC(sourceMod, OPTIMIZATION_CONST_FOLD | OPTIMIZATION_CONST_PROP);
    TEST_ASSERT_NOT_NULL(tacMod);
    // El modulo no debe plegarse debido al desbordamiento
    TEST_ASSERT_EQUAL_INT(1, countOperations(tacMod, TAC_MOD));
}

// ------------------------------------------------------------------------
// Grupo 2: Constant Propagation (Propagacion de Constantes)
// ------------------------------------------------------------------------

// Propagacion de constantes en cadena lineal de variables
void test_opt_const_prop_linear_sequence(void) {
    const char *source =
        "int main() {\n"
        "    int a;\n"
        "    int b;\n"
        "    int c;\n"
        "    a = 7;\n"
        "    b = a + 3;\n"
        "    c = b * 2;\n"
        "    return c;\n"
        "}\n";
    TACList *tac = compileToOptimizedTAC(source, OPTIMIZATION_CONST_PROP | OPTIMIZATION_CONST_FOLD);
    TEST_ASSERT_NOT_NULL(tac);
    // Cada variable en la cadena lineal recibe su constante calculada
    TEST_ASSERT_NOT_NULL(findAssignmentWithConstant(tac, "a", 7));
    TEST_ASSERT_NOT_NULL(findAssignmentWithConstant(tac, "b", 10));
    TEST_ASSERT_NOT_NULL(findAssignmentWithConstant(tac, "c", 20));
    // Se elimino toda operacion de suma y multiplicacion en tiempo de ejecucion
    TEST_ASSERT_EQUAL_INT(0, countOperations(tac, TAC_ADD));
    TEST_ASSERT_EQUAL_INT(0, countOperations(tac, TAC_MUL));
    assertSemanticEquivalence(source, OPTIMIZATION_CONST_PROP | OPTIMIZATION_CONST_FOLD, 20);
}

// Reasignacion de variable actualiza el valor propagado
void test_opt_const_prop_reassignment_preserves_latest(void) {
    const char *source =
        "int main() {\n"
        "    int x;\n"
        "    int y;\n"
        "    x = 5;\n"
        "    x = 15;\n"
        "    y = x + 5;\n"
        "    return y;\n"
        "}\n";
    TACList *tac = compileToOptimizedTAC(source, OPTIMIZATION_CONST_PROP | OPTIMIZATION_CONST_FOLD);
    TEST_ASSERT_NOT_NULL(tac);
    // La variable y toma 15 + 5 = 20 (no el valor previo 5)
    TEST_ASSERT_NOT_NULL(findAssignmentWithConstant(tac, "y", 20));
    assertSemanticEquivalence(source, OPTIMIZATION_CONST_PROP | OPTIMIZATION_CONST_FOLD, 20);
}

// Las constantes no se propagan indebidamente a traves de saltos y bucles
void test_opt_const_prop_while_loop_boundary(void) {
    const char *source =
        "int main() {\n"
        "    int sum;\n"
        "    int i;\n"
        "    sum = 0;\n"
        "    i = 1;\n"
        "    while (i < 6) {\n"
        "        sum = sum + i;\n"
        "        i = i + 1;\n"
        "    }\n"
        "    return sum;\n"
        "}\n";
    TACList *tac = compileToOptimizedTAC(source, OPTIMIZATION_CONST_PROP | OPTIMIZATION_CONST_FOLD);
    TEST_ASSERT_NOT_NULL(tac);
    // Dentro del bucle las sumas NO se plegaron porque las variables varian en cada iteracion
    TEST_ASSERT_TRUE(countOperations(tac, TAC_ADD) >= 2);
    assertSemanticEquivalence(source, OPTIMIZATION_CONST_PROP | OPTIMIZATION_CONST_FOLD, 15);
}

// Llamadas a metodos invalidan variables globales conocidas
void test_opt_const_prop_function_call_invalidates_globals(void) {
    const char *source =
        "int g;\n"
        "void modifyGlobal() {\n"
        "    g = 42;\n"
        "}\n"
        "int main() {\n"
        "    g = 10;\n"
        "    modifyGlobal();\n"
        "    return g;\n"
        "}\n";
    TACList *tac = compileToOptimizedTAC(source, OPTIMIZATION_CONST_PROP);
    TEST_ASSERT_NOT_NULL(tac);
    // El return g en main debe leer g de memoria y no propagar el valor viejo 10
    assertSemanticEquivalence(source, OPTIMIZATION_CONST_PROP, 42);
}

// Llamadas a metodos no invalidan variables locales del llamador
void test_opt_const_prop_function_call_preserves_local_variables(void) {
    const char *source =
        "int dummy() {\n"
        "    return 99;\n"
        "}\n"
        "int main() {\n"
        "    int localVal;\n"
        "    int discard;\n"
        "    localVal = 10;\n"
        "    discard = dummy();\n"
        "    return localVal + 5;\n"
        "}\n";
    TACList *tac = compileToOptimizedTAC(source, OPTIMIZATION_CONST_PROP | OPTIMIZATION_CONST_FOLD);
    TEST_ASSERT_NOT_NULL(tac);
    // localVal + 5 se pliega a 15 porque localVal se preservo a traves del call dummy()
    TEST_ASSERT_EQUAL_INT(0, countOperations(tac, TAC_ADD));
    assertSemanticEquivalence(source, OPTIMIZATION_CONST_PROP | OPTIMIZATION_CONST_FOLD, 15);
}

// ------------------------------------------------------------------------
// Grupo 3: Dead Code Elimination (Codigo Inalcanzable y Dead Stores)
// ------------------------------------------------------------------------

// Sentencias tras un return en el mismo bloque son eliminadas
void test_opt_dead_code_after_return(void) {
    const char *source =
        "int main() {\n"
        "    int x;\n"
        "    x = 42;\n"
        "    return x;\n"
        "    x = 100;\n"
        "    return x;\n"
        "}\n";
    TACList *tacNoOpt = compileToOptimizedTAC(source, OPTIMIZATION_NONE);
    TEST_ASSERT_NOT_NULL(findAssignmentWithConstant(tacNoOpt, "x", 100));

    TACList *tacOpt = compileToOptimizedTAC(source, OPTIMIZATION_DEAD_CODE);
    TEST_ASSERT_NOT_NULL(tacOpt);
    // La asignacion muerta x = 100 fue completamente eliminada del TAC
    TEST_ASSERT_NULL(findAssignmentWithConstant(tacOpt, "x", 100));
    // Solo sobrevive la asignacion viva x = 42
    TEST_ASSERT_NOT_NULL(findAssignmentWithConstant(tacOpt, "x", 42));
    assertSemanticEquivalence(source, OPTIMIZATION_DEAD_CODE, 42);
}

// Sentencias muertas tras return en ambas ramas de un if-else
void test_opt_dead_code_both_branches_of_if_else(void) {
    const char *source =
        "int check(int val) {\n"
        "    int deadA;\n"
        "    int deadB;\n"
        "    if (val > 0) {\n"
        "        return 10;\n"
        "        deadA = 1;\n"
        "    } else {\n"
        "        return 20;\n"
        "        deadB = 2;\n"
        "    }\n"
        "    return 30;\n"
        "}\n"
        "int main() {\n"
        "    return check(5) + check(-3);\n"
        "}\n";
    TACList *tacOpt = compileToOptimizedTAC(source, OPTIMIZATION_DEAD_CODE);
    TEST_ASSERT_NOT_NULL(tacOpt);
    // Las sentencias muertas deadA = 1 y deadB = 2 fueron eliminadas
    TEST_ASSERT_NULL(findAssignmentWithConstant(tacOpt, "deadA", 1));
    TEST_ASSERT_NULL(findAssignmentWithConstant(tacOpt, "deadB", 2));
    assertSemanticEquivalence(source, OPTIMIZATION_DEAD_CODE, 30);
}

// Asignaciones a variables locales nunca leidas se eliminan por analisis de vivacidad
void test_opt_dead_code_dead_store_elimination(void) {
    const char *source =
        "int main() {\n"
        "    int unused;\n"
        "    int active;\n"
        "    unused = 100;\n"
        "    active = 7;\n"
        "    return active;\n"
        "}\n";
    TACList *tacNoOpt = compileToOptimizedTAC(source, OPTIMIZATION_NONE);
    TEST_ASSERT_NOT_NULL(findAssignmentTo(tacNoOpt, "unused"));

    TACList *tacOpt = compileToOptimizedTAC(source, OPTIMIZATION_DEAD_CODE);
    TEST_ASSERT_NOT_NULL(tacOpt);
    // La asignacion a unused fue eliminada por ser dead store
    TEST_ASSERT_NULL(findAssignmentTo(tacOpt, "unused"));
    // La variable active se preserva intacta con su valor 7
    TEST_ASSERT_NOT_NULL(findAssignmentWithConstant(tacOpt, "active", 7));
    assertSemanticEquivalence(source, OPTIMIZATION_DEAD_CODE, 7);
}

// Dead code elimination reduce el numero de instrucciones en el TAC
void test_opt_dead_code_reduces_instruction_count(void) {
    const char *source =
        "int main() {\n"
        "    int a;\n"
        "    int b;\n"
        "    return 1;\n"
        "    a = 2;\n"
        "    b = 3;\n"
        "    return b;\n"
        "}\n";
    TACList *tacNoOpt = compileToOptimizedTAC(source, OPTIMIZATION_NONE);
    int countNoOpt = countInstructions(tacNoOpt);

    TACList *tacOpt = compileToOptimizedTAC(source, OPTIMIZATION_DEAD_CODE);
    int countOpt = countInstructions(tacOpt);

    TEST_ASSERT_TRUE(countOpt < countNoOpt);
    assertSemanticEquivalence(source, OPTIMIZATION_DEAD_CODE, 1);
}

// Muchas referencias a etiquetas (superando 256) preservan las etiquetas activas sin pérdidas
void test_opt_dead_code_many_labels_beyond_static_buffer(void) {
    char *source = malloc(65536);
    TEST_ASSERT_NOT_NULL(source);

    int offset = snprintf(source, 65536, "int main() {\n    int x;\n    x = 0;\n");
    for (int i = 0; i < 300; i++) {
        offset += snprintf(source + offset, 65536 - (size_t)offset,
                           "    if (x == %d) { x = %d; }\n", i, i + 1);
    }
    snprintf(source + offset, 65536 - (size_t)offset,
             "    if (x == 300) { return 42; }\n    return 0;\n}\n");

    assertSemanticEquivalence(source, OPTIMIZATION_DEAD_CODE, 42);

    free(source);
}

// ------------------------------------------------------------------------
// Grupo 4: Constant Branch Folding (Condiciones Constantes en Control de Flujo)
// ------------------------------------------------------------------------

// if (true) ejecuta directamente el cuerpo sin evaluar condicion
void test_opt_const_branch_if_true(void) {
    const char *source =
        "int main() {\n"
        "    int x;\n"
        "    x = 1;\n"
        "    if (true) {\n"
        "        x = 25;\n"
        "    } else {\n"
        "        x = 50;\n"
        "    }\n"
        "    return x;\n"
        "}\n";
    TACList *tacOpt = compileToOptimizedTAC(source, OPTIMIZATION_CONST_BRANCH | OPTIMIZATION_DEAD_CODE);
    TEST_ASSERT_NOT_NULL(tacOpt);
    // La rama muerta else (x = 50) fue completamente eliminada
    TEST_ASSERT_NULL(findAssignmentWithConstant(tacOpt, "x", 50));
    // La variable x toma el valor 25 de la rama ejecutada
    TEST_ASSERT_NOT_NULL(findAssignmentWithConstant(tacOpt, "x", 25));
    assertSemanticEquivalence(source, OPTIMIZATION_CONST_BRANCH | OPTIMIZATION_DEAD_CODE, 25);
}

// if (false) descarta la rama then y toma la rama else
void test_opt_const_branch_if_false(void) {
    const char *source =
        "int main() {\n"
        "    int x;\n"
        "    x = 1;\n"
        "    if (false) {\n"
        "        x = 25;\n"
        "    } else {\n"
        "        x = 50;\n"
        "    }\n"
        "    return x;\n"
        "}\n";
    TACList *tacOpt = compileToOptimizedTAC(source, OPTIMIZATION_CONST_BRANCH | OPTIMIZATION_DEAD_CODE);
    TEST_ASSERT_NOT_NULL(tacOpt);
    // La rama muerta then (x = 25) fue completamente eliminada
    TEST_ASSERT_NULL(findAssignmentWithConstant(tacOpt, "x", 25));
    // La variable x toma el valor 50 de la rama else
    TEST_ASSERT_NOT_NULL(findAssignmentWithConstant(tacOpt, "x", 50));
    assertSemanticEquivalence(source, OPTIMIZATION_CONST_BRANCH | OPTIMIZATION_DEAD_CODE, 50);
}

// Condicion relacional que se pliega a constante simplifica el condicional
void test_opt_const_branch_folded_condition(void) {
    const char *source =
        "int main() {\n"
        "    int res;\n"
        "    if (100 > 50 && 2 == 2) {\n"
        "        res = 77;\n"
        "    } else {\n"
        "        res = 11;\n"
        "    }\n"
        "    return res;\n"
        "}\n";
    TACList *tacOpt = compileToOptimizedTAC(source, OPTIMIZATION_ALL);
    TEST_ASSERT_NOT_NULL(tacOpt);
    // La rama muerta else fue eliminada por completo
    TEST_ASSERT_NULL(findAssignmentWithConstant(tacOpt, "res", 11));
    // El retorno final devuelve directamente la constante 77
    const TACInstruction *retInst = NULL;
    for (const TACInstruction *inst = tacOpt->head; inst != NULL; inst = inst->next) {
        if (inst->operation == TAC_RETURN) retInst = inst;
    }
    TEST_ASSERT_NOT_NULL(retInst);
    TEST_ASSERT_EQUAL(OPERAND_CONSTANT, retInst->firstOperand.kind);
    TEST_ASSERT_EQUAL_INT(77, retInst->firstOperand.value.intValue);
    assertSemanticEquivalence(source, OPTIMIZATION_ALL, 77);
}

// while (false) nunca ejecuta el cuerpo y el salto se elimina
void test_opt_const_branch_while_false(void) {
    const char *source =
        "int main() {\n"
        "    int x;\n"
        "    x = 13;\n"
        "    while (false) {\n"
        "        x = 99;\n"
        "    }\n"
        "    return x;\n"
        "}\n";
    TACList *tacOpt = compileToOptimizedTAC(source, OPTIMIZATION_CONST_BRANCH | OPTIMIZATION_DEAD_CODE);
    TEST_ASSERT_NOT_NULL(tacOpt);
    // El cuerpo inalcanzable del while (x = 99) fue eliminado
    TEST_ASSERT_NULL(findAssignmentWithConstant(tacOpt, "x", 99));
    TEST_ASSERT_NOT_NULL(findAssignmentWithConstant(tacOpt, "x", 13));
    assertSemanticEquivalence(source, OPTIMIZATION_CONST_BRANCH | OPTIMIZATION_DEAD_CODE, 13);
}

// ------------------------------------------------------------------------
// Grupo 5: Self-Assignment Elimination (Asignaciones a Si Misma)
// ------------------------------------------------------------------------

// Sentencias x = x sobre variables locales y globales se eliminan
void test_opt_self_assign_local_and_global(void) {
    const char *source =
        "int g;\n"
        "int main() {\n"
        "    int x;\n"
        "    x = 10;\n"
        "    x = x;\n"
        "    g = 20;\n"
        "    g = g;\n"
        "    return x + g;\n"
        "}\n";
    TACList *tacNoOpt = compileToOptimizedTAC(source, OPTIMIZATION_NONE);
    int countNoOpt = countInstructions(tacNoOpt);

    TACList *tacOpt = compileToOptimizedTAC(source, OPTIMIZATION_SELF_ASSIGN);
    TEST_ASSERT_NOT_NULL(tacOpt);
    int countOpt = countInstructions(tacOpt);
    // Se removieron exactamente las 2 autoasignaciones redundantes
    TEST_ASSERT_EQUAL_INT(countNoOpt - 2, countOpt);
    // Las variables x y g conservan sus valores originales asignados
    TEST_ASSERT_NOT_NULL(findAssignmentWithConstant(tacOpt, "x", 10));
    TEST_ASSERT_NOT_NULL(findAssignmentWithConstant(tacOpt, "g", 20));
    assertSemanticEquivalence(source, OPTIMIZATION_SELF_ASSIGN, 30);
}

// Autoasignacion sobre parametros formales
void test_opt_self_assign_parameter(void) {
    const char *source =
        "int helper(int p) {\n"
        "    p = p;\n"
        "    return p * 2;\n"
        "}\n"
        "int main() {\n"
        "    return helper(8);\n"
        "}\n";
    TACList *tacNoOpt = compileToOptimizedTAC(source, OPTIMIZATION_NONE);
    int countNoOpt = countInstructions(tacNoOpt);

    TACList *tacOpt = compileToOptimizedTAC(source, OPTIMIZATION_SELF_ASSIGN);
    TEST_ASSERT_NOT_NULL(tacOpt);
    int countOpt = countInstructions(tacOpt);
    // La autoasignacion p = p fue eliminada
    TEST_ASSERT_EQUAL_INT(countNoOpt - 1, countOpt);
    assertSemanticEquivalence(source, OPTIMIZATION_SELF_ASSIGN, 16);
}

// Ocultamiento de variables con mismo nombre (shadowing) no se confunde con autoasignacion
void test_opt_self_assign_shadowing_distinction(void) {
    const char *source =
        "int x;\n"
        "int main() {\n"
        "    int y;\n"
        "    x = 5;\n"
        "    { int x; x = 10; y = x; }\n"
        "    return x + y;\n"
        "}\n";
    // x global (5) + y (10) = 15
    TACList *tacOpt = compileToOptimizedTAC(source, OPTIMIZATION_SELF_ASSIGN);
    TEST_ASSERT_NOT_NULL(tacOpt);
    // La asignacion de x local a y no se considera autoasignacion
    TEST_ASSERT_NOT_NULL(findAssignmentTo(tacOpt, "y"));
    assertSemanticEquivalence(source, OPTIMIZATION_SELF_ASSIGN, 15);
}

// ------------------------------------------------------------------------
// Grupo 6: Peephole Optimization (Mirilla)
// ------------------------------------------------------------------------

// Salto redundante a la etiqueta inmediatamente siguiente se remueve
void test_opt_peephole_redundant_jump_to_next_label(void) {
    const char *source =
        "int main() {\n"
        "    int a;\n"
        "    a = 3;\n"
        "    if (a > 5) {\n"
        "        a = 10;\n"
        "    }\n"
        "    return a;\n"
        "}\n";
    TACList *tacNoOpt = compileToOptimizedTAC(source, OPTIMIZATION_NONE);
    int countNoOpt = countInstructions(tacNoOpt);

    TACList *tacOpt = compileToOptimizedTAC(source, OPTIMIZATION_PEEPHOLE);
    TEST_ASSERT_NOT_NULL(tacOpt);
    int countOpt = countInstructions(tacOpt);
    // La pasada de mirilla remueve saltos innecesarios
    TEST_ASSERT_TRUE(countOpt <= countNoOpt);
    assertSemanticEquivalence(source, OPTIMIZATION_PEEPHOLE, 3);
}

// ------------------------------------------------------------------------
// Grupo 7: Strength Reduction (Reduccion de Fuerza)
// ------------------------------------------------------------------------

// Multiplicacion por potencias de dos se reduce a desplazamientos aritmeticos
void test_opt_strength_reduction_powers_of_two(void) {
    const char *source =
        "int main() {\n"
        "    int x;\n"
        "    x = 3;\n"
        "    return (x * 2) + (x * 4) + (x * 8);\n"
        "}\n";
    const char *asmNoOpt = compileToAssembly(source, OPTIMIZATION_NONE);
    TEST_ASSERT_NOT_NULL(asmNoOpt);
    TEST_ASSERT_TRUE(strstr(asmNoOpt, "imull") != NULL);

    const char *asmOpt = compileToAssembly(source, OPTIMIZATION_STRENGTH);
    TEST_ASSERT_NOT_NULL(asmOpt);
    // En assembly se emitieron desplazamientos sall/shll
    TEST_ASSERT_TRUE(strstr(asmOpt, "sall") != NULL || strstr(asmOpt, "shll") != NULL);
    // No permanecen imull con multiplicadores potencia de 2
    TEST_ASSERT_NULL(strstr(asmOpt, "imull $2"));
    TEST_ASSERT_NULL(strstr(asmOpt, "imull $4"));
    TEST_ASSERT_NULL(strstr(asmOpt, "imull $8"));
    assertSemanticEquivalence(source, OPTIMIZATION_STRENGTH, 42);
}

// Multiplicacion conmutativa (constante a izquierda o derecha) se reduce por igual
void test_opt_strength_reduction_commutative(void) {
    const char *source =
        "int main() {\n"
        "    int x;\n"
        "    x = 5;\n"
        "    return (2 * x) + (4 * x);\n"
        "}\n";
    const char *asmOpt = compileToAssembly(source, OPTIMIZATION_STRENGTH);
    TEST_ASSERT_NOT_NULL(asmOpt);
    TEST_ASSERT_TRUE(strstr(asmOpt, "sall") != NULL || strstr(asmOpt, "shll") != NULL);
    assertSemanticEquivalence(source, OPTIMIZATION_STRENGTH, 30);
}

// Multiplicacion por cero y por uno se reducen a xor o asignacion
void test_opt_strength_reduction_zero_and_one(void) {
    const char *source =
        "int main() {\n"
        "    int x;\n"
        "    x = 33;\n"
        "    return (x * 1) + (x * 0);\n"
        "}\n";
    const char *asmOpt = compileToAssembly(source, OPTIMIZATION_STRENGTH);
    TEST_ASSERT_NOT_NULL(asmOpt);
    // Multiplicacion por 0 se reduce a xorl
    TEST_ASSERT_TRUE(strstr(asmOpt, "xorl") != NULL);
    assertSemanticEquivalence(source, OPTIMIZATION_STRENGTH, 33);
}

// Multiplicacion de numeros negativos preserva signo y equivalencia
void test_opt_strength_reduction_negative_values(void) {
    const char *source =
        "int main() {\n"
        "    int x;\n"
        "    int r;\n"
        "    x = -3;\n"
        "    r = x * 4;\n"
        "    return r + 20;\n"
        "}\n";
    // -12 + 20 = 8
    assertSemanticEquivalence(source, OPTIMIZATION_STRENGTH, 8);
}

// Multiplicacion por factores que no son potencia de dos preserva instruccion imull
void test_opt_strength_reduction_non_power_of_two_preserves_imull(void) {
    const char *source =
        "int main() {\n"
        "    int x;\n"
        "    x = 7;\n"
        "    return x * 5;\n"
        "}\n";
    const char *asmOpt = compileToAssembly(source, OPTIMIZATION_STRENGTH);
    TEST_ASSERT_NOT_NULL(asmOpt);
    // Multiplicacion por 5 preserva instruccion imull
    TEST_ASSERT_TRUE(strstr(asmOpt, "imull") != NULL);
    assertSemanticEquivalence(source, OPTIMIZATION_STRENGTH, 35);
}

// ------------------------------------------------------------------------
// Grupo 8: Programas Canonicos y Combinacion Completa (OPTIMIZATION_ALL)
// ------------------------------------------------------------------------

// Factorial iterativo produce exactamente el mismo resultado con OPTIMIZATION_ALL
void test_opt_all_factorial_iterative(void) {
    const char *source =
        "int factorial(int n) {\n"
        "    int res;\n"
        "    res = 1;\n"
        "    while (n > 1) {\n"
        "        res = res * n;\n"
        "        n = n - 1;\n"
        "    }\n"
        "    return res;\n"
        "}\n"
        "int main() {\n"
        "    return factorial(5);\n"
        "}\n";
    // 5! = 120
    assertSemanticEquivalence(source, OPTIMIZATION_ALL, 120);
}

// Factorial recursivo produce exactamente el mismo resultado con OPTIMIZATION_ALL
void test_opt_all_factorial_recursive(void) {
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
    // 4! = 24
    assertSemanticEquivalence(source, OPTIMIZATION_ALL, 24);
}

// Fibonacci recursivo produce exactamente el mismo resultado con OPTIMIZATION_ALL
void test_opt_all_fibonacci_recursive(void) {
    const char *source =
        "int fib(int n) {\n"
        "    if (n == 0) {\n"
        "        return 0;\n"
        "    }\n"
        "    if (n == 1) {\n"
        "        return 1;\n"
        "    }\n"
        "    return fib(n - 1) + fib(n - 2);\n"
        "}\n"
        "int main() {\n"
        "    return fib(7);\n"
        "}\n";
    // fib(7) = 13
    assertSemanticEquivalence(source, OPTIMIZATION_ALL, 13);
}

// Bucles anidados y operaciones complejas conservan semantica
void test_opt_all_nested_loops_and_conditionals(void) {
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
    // 3 diagonales (3*2=6) + 6 fuera de diagonal (6*1=6) = 12
    assertSemanticEquivalence(source, OPTIMIZATION_ALL, 12);
}

// Expresion con multiples oportunidades converge a punto fijo reduciendo cuadruplas
void test_opt_all_fixed_point_reduction(void) {
    const char *source =
        "int main() {\n"
        "    int a;\n"
        "    int unused;\n"
        "    int res;\n"
        "    a = 1 + 2 + 3;\n"
        "    unused = 99;\n"
        "    if (10 > 5) {\n"
        "        res = a * 2;\n"
        "    } else {\n"
        "        res = 0;\n"
        "    }\n"
        "    return res;\n"
        "}\n";
    TACList *tacNoOpt = compileToOptimizedTAC(source, OPTIMIZATION_NONE);
    int countNoOpt = countInstructions(tacNoOpt);

    TACList *tacOpt = compileToOptimizedTAC(source, OPTIMIZATION_ALL);
    TEST_ASSERT_NOT_NULL(tacOpt);
    int countOpt = countInstructions(tacOpt);

    // Se redujo la cantidad total de instrucciones tras alcanzar punto fijo
    TEST_ASSERT_TRUE(countOpt < countNoOpt);
    // unused = 99 fue eliminada como dead store
    TEST_ASSERT_NULL(findAssignmentTo(tacOpt, "unused"));
    // El retorno devuelve directamente la constante 12
    const TACInstruction *retInst = NULL;
    for (const TACInstruction *inst = tacOpt->head; inst != NULL; inst = inst->next) {
        if (inst->operation == TAC_RETURN) retInst = inst;
    }
    TEST_ASSERT_NOT_NULL(retInst);
    TEST_ASSERT_EQUAL(OPERAND_CONSTANT, retInst->firstOperand.kind);
    TEST_ASSERT_EQUAL_INT(12, retInst->firstOperand.value.intValue);
    assertSemanticEquivalence(source, OPTIMIZATION_ALL, 12);
}

// Multiples variables globales y locales conservan su estado exacto tras optimizaciones
void test_opt_all_multi_variable_state_and_side_effects(void) {
    const char *source =
        "int g1;\n"
        "int g2;\n"
        "int g3;\n"
        "void mutate(int step) {\n"
        "    g1 = g1 + step;\n"
        "    g2 = g2 * step;\n"
        "}\n"
        "int main() {\n"
        "    int x;\n"
        "    int y;\n"
        "    g1 = 10;\n"
        "    g2 = 5;\n"
        "    g3 = 100;\n"
        "    mutate(2);\n"
        "    x = g1 + 3;\n"
        "    y = g2 - 2;\n"
        "    if (g1 == 12 && g2 == 10 && g3 == 100 && x == 15 && y == 8) {\n"
        "        return 1;\n"
        "    }\n"
        "    return 0;\n"
        "}\n";
    // Valida que el estado de 5 variables (g1, g2, g3, x, y) coincida exactamente tras OPTIMIZATION_ALL
    assertSemanticEquivalence(source, OPTIMIZATION_ALL, 1);
}

int main(void) {
    UNITY_BEGIN();

    // Grupo 1: Constant Folding
    RUN_TEST(test_opt_const_fold_arithmetic_equivalence);
    RUN_TEST(test_opt_const_fold_boolean_and_relational);
    RUN_TEST(test_opt_const_fold_unary_operators);
    RUN_TEST(test_opt_const_fold_reduces_arithmetic_quads);
    RUN_TEST(test_opt_const_fold_signed_overflow_does_not_crash);

    // Grupo 2: Constant Propagation
    RUN_TEST(test_opt_const_prop_linear_sequence);
    RUN_TEST(test_opt_const_prop_reassignment_preserves_latest);
    RUN_TEST(test_opt_const_prop_while_loop_boundary);
    RUN_TEST(test_opt_const_prop_function_call_invalidates_globals);
    RUN_TEST(test_opt_const_prop_function_call_preserves_local_variables);

    // Grupo 3: Dead Code Elimination
    RUN_TEST(test_opt_dead_code_after_return);
    RUN_TEST(test_opt_dead_code_both_branches_of_if_else);
    RUN_TEST(test_opt_dead_code_dead_store_elimination);
    RUN_TEST(test_opt_dead_code_reduces_instruction_count);
    RUN_TEST(test_opt_dead_code_many_labels_beyond_static_buffer);

    // Grupo 4: Constant Branch
    RUN_TEST(test_opt_const_branch_if_true);
    RUN_TEST(test_opt_const_branch_if_false);
    RUN_TEST(test_opt_const_branch_folded_condition);
    RUN_TEST(test_opt_const_branch_while_false);

    // Grupo 5: Self-Assignment
    RUN_TEST(test_opt_self_assign_local_and_global);
    RUN_TEST(test_opt_self_assign_parameter);
    RUN_TEST(test_opt_self_assign_shadowing_distinction);

    // Grupo 6: Peephole
    RUN_TEST(test_opt_peephole_redundant_jump_to_next_label);

    // Grupo 7: Strength Reduction
    RUN_TEST(test_opt_strength_reduction_powers_of_two);
    RUN_TEST(test_opt_strength_reduction_commutative);
    RUN_TEST(test_opt_strength_reduction_zero_and_one);
    RUN_TEST(test_opt_strength_reduction_negative_values);
    RUN_TEST(test_opt_strength_reduction_non_power_of_two_preserves_imull);

    // Grupo 8: Programas Canonicos y Combinacion Completa
    RUN_TEST(test_opt_all_factorial_iterative);
    RUN_TEST(test_opt_all_factorial_recursive);
    RUN_TEST(test_opt_all_fibonacci_recursive);
    RUN_TEST(test_opt_all_nested_loops_and_conditionals);
    RUN_TEST(test_opt_all_fixed_point_reduction);
    RUN_TEST(test_opt_all_multi_variable_state_and_side_effects);

    return UNITY_END();
}
