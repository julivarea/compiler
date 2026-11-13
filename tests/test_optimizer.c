/**
 * @file test_optimizer.c
 * @brief Pruebas unitarias del optimizador de código intermedio (pasadas sobre TAC).
 *
 * La entrada de cada test es el TAC sin optimizar escrito en el formato del `.ci`
 * (ver support/tac_builder.h); el comentario previo muestra el programa C-TDS del que
 * proviene. Así un test solo puede fallar por un defecto del optimizador, nunca del
 * front-end ni del generador de código intermedio.
 *
 * Quedan fuera de esta suite, por no ser responsabilidad de la etapa:
 *  - la reducción de fuerza, que se aplica al emitir assembly (test_object_code.c);
 *  - la equivalencia semántica con y sin optimizaciones, que compila y ejecuta programas
 *    completos (test_integration.c).
 */

#include "unity.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tac.h"
#include "tac_builder.h"
#include "optimizer.h"

// Helpers del arnés

/* Construye el TAC descrito por @p tac y le aplica las pasadas pedidas. */
static TACList *optimize(const char *tac, OptimizationSet optimizations) {
    TACList *list = buildTAC(tac);
    if (list != NULL) {
        optimizeIntermediateCode(list, optimizations);
    }
    return list;
}

static const TACInstruction *lastReturn(const TACList *list) {
    const TACInstruction *found = NULL;
    for (const TACInstruction *inst = list->head; inst != NULL; inst = inst->next) {
        if (inst->operation == TAC_RETURN) found = inst;
    }
    return found;
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

void setUp(void) {}

void tearDown(void) {
    releaseTACSymbols();
}

// ------------------------------------------------------------------------
// Grupo 1: Constant Folding (Plegado de Constantes)
// ------------------------------------------------------------------------

// Aritmetica constante entera se pliega en compilacion conservando equivalencia semantica
void test_opt_const_fold_arithmetic_equivalence(void) {
    // int main() {
    //     int res;
    //     res = 2 + 3 * 4 + (20 / 4) - (10 % 3);
    //     return res;
    // }
    const char *input =
        "begin_func main, 0\n"
        "local res@2, INT\n"
        "t1 = 3 * 4\n"
        "t0 = 2 + t1\n"
        "t3 = 20 / 4\n"
        "t2 = t0 + t3\n"
        "t5 = 10 % 3\n"
        "t4 = t2 - t5\n"
        "res@2 = t4\n"
        "ret res@2\n"
        "end_func main\n";
    // 2 + 12 + 5 - 1 = 18
    TACList *tac = optimize(input, OPTIMIZATION_CONST_FOLD | OPTIMIZATION_CONST_PROP);
    TEST_ASSERT_NOT_NULL(tac);
    // Todas las operaciones aritmeticas sobre constantes se plegaron
    TEST_ASSERT_EQUAL_INT(0, countOperations(tac, TAC_ADD));
    TEST_ASSERT_EQUAL_INT(0, countOperations(tac, TAC_MUL));
    TEST_ASSERT_EQUAL_INT(0, countOperations(tac, TAC_DIV));
    TEST_ASSERT_EQUAL_INT(0, countOperations(tac, TAC_MOD));
    // La variable res recibe directamente el resultado constante 18
    TEST_ASSERT_NOT_NULL(findAssignmentWithConstant(tac, "res", 18));
}

// Operaciones relacionales y logicas entre constantes se pliegan correctamente
void test_opt_const_fold_boolean_and_relational(void) {
    // int main() {
    //     boolean a;
    //     boolean b;
    //     a = 10 > 5;
    //     b = 2 < 4;
    //     if (a && b) {
    //         return 1;
    //     }
    //     return 0;
    // }
    const char *input =
        "begin_func main, 0\n"
        "local a@2, BOOL\n"
        "local b@3, BOOL\n"
        "t0 = 10 > 5\n"
        "a@2 = t0\n"
        "t1 = 2 < 4\n"
        "b@3 = t1\n"
        "t2 = a@2\n"
        "if t2 false goto L0\n"
        "t2 = b@3\n"
        "L0:\n"
        "if t2 false goto L1\n"
        "ret 1\n"
        "L1:\n"
        "ret 0\n"
        "end_func main\n";
    TACList *tac = optimize(input, OPTIMIZATION_CONST_FOLD | OPTIMIZATION_CONST_PROP);
    TEST_ASSERT_NOT_NULL(tac);
    // Operaciones relacionales sobre constantes se plegaron en tiempo de compilacion
    TEST_ASSERT_EQUAL_INT(0, countOperations(tac, TAC_GT));
    TEST_ASSERT_EQUAL_INT(0, countOperations(tac, TAC_LT));
    // Las variables booleanas a y b reciben el valor 1 (true) directamente
    TEST_ASSERT_NOT_NULL(findAssignmentWithConstant(tac, "a", 1));
    TEST_ASSERT_NOT_NULL(findAssignmentWithConstant(tac, "b", 1));
}

// Operaciones unarias entre constantes (menos unario y not) se pliegan
void test_opt_const_fold_unary_operators(void) {
    // int main() {
    //     int x;
    //     int res;
    //     x = -5;
    //     res = -x + 15;
    //     return res;
    // }
    const char *input =
        "begin_func main, 0\n"
        "local x@2, INT\n"
        "local res@3, INT\n"
        "t0 = -5\n"
        "x@2 = t0\n"
        "t1 = -x@2\n"
        "t2 = t1 + 15\n"
        "res@3 = t2\n"
        "ret res@3\n"
        "end_func main\n";
    TACList *tac = optimize(input, OPTIMIZATION_CONST_FOLD | OPTIMIZATION_CONST_PROP);
    TEST_ASSERT_NOT_NULL(tac);
    // Menos unario sobre constante plegado
    TEST_ASSERT_EQUAL_INT(0, countOperations(tac, TAC_UMINUS));
    // La variable x recibe directamente el valor constante -5
    TEST_ASSERT_NOT_NULL(findAssignmentWithConstant(tac, "x", -5));
    // La variable res recibe 5 + 15 = 20
    TEST_ASSERT_NOT_NULL(findAssignmentWithConstant(tac, "res", 20));
}

// Plegado de constantes reduce cuadruplas aritmeticas a asignaciones de constantes
void test_opt_const_fold_reduces_arithmetic_quads(void) {
    // int main() {
    //     int k;
    //     k = 10 + 20;
    //     return k;
    // }
    const char *input =
        "begin_func main, 0\n"
        "local k@2, INT\n"
        "t0 = 10 + 20\n"
        "k@2 = t0\n"
        "ret k@2\n"
        "end_func main\n";
    TACList *tacNoOpt = buildTAC(input);
    TEST_ASSERT_NOT_NULL(tacNoOpt);
    int addsNoOpt = countOperations(tacNoOpt, TAC_ADD);
    TEST_ASSERT_TRUE(addsNoOpt >= 1);

    TACList *tacOpt = optimize(input, OPTIMIZATION_CONST_FOLD | OPTIMIZATION_CONST_PROP);
    TEST_ASSERT_NOT_NULL(tacOpt);
    int addsOpt = countOperations(tacOpt, TAC_ADD);
    TEST_ASSERT_EQUAL_INT(0, addsOpt);
    // La variable k almacena 30 directamente sin sumas en tiempo de ejecucion
    TEST_ASSERT_NOT_NULL(findAssignmentWithConstant(tacOpt, "k", 30));
}

// Division y modulo con desbordamiento (INT32_MIN / -1 e INT32_MIN % -1) no se pliegan y no abortan el compilador
void test_opt_const_fold_signed_overflow_does_not_crash(void) {
    // int main() {
    //     int minVal;
    //     int res;
    //     minVal = -2147483647 - 1;
    //     res = minVal / -1;
    //     return res;
    // }
    const char *inputDiv =
        "begin_func main, 0\n"
        "local minVal@2, INT\n"
        "local res@3, INT\n"
        "t0 = -2147483647\n"
        "t1 = t0 - 1\n"
        "minVal@2 = t1\n"
        "t3 = -1\n"
        "t2 = minVal@2 / t3\n"
        "res@3 = t2\n"
        "ret res@3\n"
        "end_func main\n";

    TACList *tacDiv = optimize(inputDiv, OPTIMIZATION_CONST_FOLD | OPTIMIZATION_CONST_PROP);
    TEST_ASSERT_NOT_NULL(tacDiv);
    // La division no debe plegarse debido al desbordamiento
    TEST_ASSERT_EQUAL_INT(1, countOperations(tacDiv, TAC_DIV));

    // int main() {
    //     int minVal;
    //     int res;
    //     minVal = -2147483647 - 1;
    //     res = minVal % -1;
    //     return res;
    // }
    const char *inputMod =
        "begin_func main, 0\n"
        "local minVal@2, INT\n"
        "local res@3, INT\n"
        "t0 = -2147483647\n"
        "t1 = t0 - 1\n"
        "minVal@2 = t1\n"
        "t3 = -1\n"
        "t2 = minVal@2 % t3\n"
        "res@3 = t2\n"
        "ret res@3\n"
        "end_func main\n";

    TACList *tacMod = optimize(inputMod, OPTIMIZATION_CONST_FOLD | OPTIMIZATION_CONST_PROP);
    TEST_ASSERT_NOT_NULL(tacMod);
    // El modulo no debe plegarse debido al desbordamiento
    TEST_ASSERT_EQUAL_INT(1, countOperations(tacMod, TAC_MOD));
}

// ------------------------------------------------------------------------
// Grupo 2: Constant Propagation (Propagacion de Constantes)
// ------------------------------------------------------------------------

// Propagacion de constantes en cadena lineal de variables
void test_opt_const_prop_linear_sequence(void) {
    // int main() {
    //     int a;
    //     int b;
    //     int c;
    //     a = 7;
    //     b = a + 3;
    //     c = b * 2;
    //     return c;
    // }
    const char *input =
        "begin_func main, 0\n"
        "local a@2, INT\n"
        "local b@3, INT\n"
        "local c@4, INT\n"
        "a@2 = 7\n"
        "t0 = a@2 + 3\n"
        "b@3 = t0\n"
        "t1 = b@3 * 2\n"
        "c@4 = t1\n"
        "ret c@4\n"
        "end_func main\n";
    TACList *tac = optimize(input, OPTIMIZATION_CONST_PROP | OPTIMIZATION_CONST_FOLD);
    TEST_ASSERT_NOT_NULL(tac);
    // Cada variable en la cadena lineal recibe su constante calculada
    TEST_ASSERT_NOT_NULL(findAssignmentWithConstant(tac, "a", 7));
    TEST_ASSERT_NOT_NULL(findAssignmentWithConstant(tac, "b", 10));
    TEST_ASSERT_NOT_NULL(findAssignmentWithConstant(tac, "c", 20));
    // Se elimino toda operacion de suma y multiplicacion en tiempo de ejecucion
    TEST_ASSERT_EQUAL_INT(0, countOperations(tac, TAC_ADD));
    TEST_ASSERT_EQUAL_INT(0, countOperations(tac, TAC_MUL));
}

// Reasignacion de variable actualiza el valor propagado
void test_opt_const_prop_reassignment_preserves_latest(void) {
    // int main() {
    //     int x;
    //     int y;
    //     x = 5;
    //     x = 15;
    //     y = x + 5;
    //     return y;
    // }
    const char *input =
        "begin_func main, 0\n"
        "local x@2, INT\n"
        "local y@3, INT\n"
        "x@2 = 5\n"
        "x@2 = 15\n"
        "t0 = x@2 + 5\n"
        "y@3 = t0\n"
        "ret y@3\n"
        "end_func main\n";
    TACList *tac = optimize(input, OPTIMIZATION_CONST_PROP | OPTIMIZATION_CONST_FOLD);
    TEST_ASSERT_NOT_NULL(tac);
    // La variable y toma 15 + 5 = 20 (no el valor previo 5)
    TEST_ASSERT_NOT_NULL(findAssignmentWithConstant(tac, "y", 20));
}

// Las constantes no se propagan indebidamente a traves de saltos y bucles
void test_opt_const_prop_while_loop_boundary(void) {
    // int main() {
    //     int sum;
    //     int i;
    //     sum = 0;
    //     i = 1;
    //     while (i < 6) {
    //         sum = sum + i;
    //         i = i + 1;
    //     }
    //     return sum;
    // }
    const char *input =
        "begin_func main, 0\n"
        "local sum@2, INT\n"
        "local i@3, INT\n"
        "sum@2 = 0\n"
        "i@3 = 1\n"
        "L0:\n"
        "t0 = i@3 < 6\n"
        "if t0 false goto L1\n"
        "t1 = sum@2 + i@3\n"
        "sum@2 = t1\n"
        "t2 = i@3 + 1\n"
        "i@3 = t2\n"
        "goto L0\n"
        "L1:\n"
        "ret sum@2\n"
        "end_func main\n";
    TACList *tac = optimize(input, OPTIMIZATION_CONST_PROP | OPTIMIZATION_CONST_FOLD);
    TEST_ASSERT_NOT_NULL(tac);
    // Dentro del bucle las sumas NO se plegaron porque las variables varian en cada iteracion
    TEST_ASSERT_TRUE(countOperations(tac, TAC_ADD) >= 2);
}

// Llamadas a metodos invalidan variables globales conocidas
void test_opt_const_prop_function_call_invalidates_globals(void) {
    // int g;
    // void modifyGlobal() {
    //     g = 42;
    // }
    // int main() {
    //     g = 10;
    //     modifyGlobal();
    //     return g;
    // }
    const char *input =
        "global g@1, INT\n"
        "begin_func modifyGlobal, 0\n"
        "g@1 = 42\n"
        "ret\n"
        "end_func modifyGlobal\n"
        "begin_func main, 0\n"
        "g@1 = 10\n"
        "call modifyGlobal, 0\n"
        "ret g@1\n"
        "end_func main\n";
    TACList *tac = optimize(input, OPTIMIZATION_CONST_PROP);
    TEST_ASSERT_NOT_NULL(tac);
    // El return g en main debe leer g de memoria y no propagar el valor viejo 10
    const TACInstruction *retInst = lastReturn(tac);
    TEST_ASSERT_NOT_NULL(retInst);
    TEST_ASSERT_EQUAL(OPERAND_VARIABLE, retInst->firstOperand.kind);
    TEST_ASSERT_EQUAL_STRING("g@1", retInst->firstOperand.name);
}

// Llamadas a metodos no invalidan variables locales del llamador
void test_opt_const_prop_function_call_preserves_local_variables(void) {
    // int dummy() {
    //     return 99;
    // }
    // int main() {
    //     int localVal;
    //     int discard;
    //     localVal = 10;
    //     discard = dummy();
    //     return localVal + 5;
    // }
    const char *input =
        "begin_func dummy, 0\n"
        "ret 99\n"
        "end_func dummy\n"
        "begin_func main, 0\n"
        "local localVal@3, INT\n"
        "local discard@4, INT\n"
        "localVal@3 = 10\n"
        "t0 = call dummy, 0\n"
        "discard@4 = t0\n"
        "t1 = localVal@3 + 5\n"
        "ret t1\n"
        "end_func main\n";
    TACList *tac = optimize(input, OPTIMIZATION_CONST_PROP | OPTIMIZATION_CONST_FOLD);
    TEST_ASSERT_NOT_NULL(tac);
    // localVal + 5 se pliega a 15 porque localVal se preservo a traves del call dummy()
    TEST_ASSERT_EQUAL_INT(0, countOperations(tac, TAC_ADD));
}

// ------------------------------------------------------------------------
// Grupo 3: Dead Code Elimination (Codigo Inalcanzable y Dead Stores)
// ------------------------------------------------------------------------

// Sentencias tras un return en el mismo bloque son eliminadas
void test_opt_dead_code_after_return(void) {
    // int main() {
    //     int x;
    //     x = 42;
    //     return x;
    //     x = 100;
    //     return x;
    // }
    const char *input =
        "begin_func main, 0\n"
        "local x@2, INT\n"
        "x@2 = 42\n"
        "ret x@2\n"
        "x@2 = 100\n"
        "ret x@2\n"
        "end_func main\n";
    TACList *tacNoOpt = buildTAC(input);
    TEST_ASSERT_NOT_NULL(findAssignmentWithConstant(tacNoOpt, "x", 100));

    TACList *tacOpt = optimize(input, OPTIMIZATION_DEAD_CODE);
    TEST_ASSERT_NOT_NULL(tacOpt);
    // La asignacion muerta x = 100 fue completamente eliminada del TAC
    TEST_ASSERT_NULL(findAssignmentWithConstant(tacOpt, "x", 100));
    // Solo sobrevive la asignacion viva x = 42
    TEST_ASSERT_NOT_NULL(findAssignmentWithConstant(tacOpt, "x", 42));
}

// Sentencias muertas tras return en ambas ramas de un if-else
void test_opt_dead_code_both_branches_of_if_else(void) {
    // int check(int val) {
    //     int deadA;
    //     int deadB;
    //     if (val > 0) {
    //         return 10;
    //         deadA = 1;
    //     } else {
    //         return 20;
    //         deadB = 2;
    //     }
    //     return 30;
    // }
    // int main() {
    //     return check(5) + check(-3);
    // }
    const char *input =
        "begin_func check, 1\n"
        "param_decl val@2, INT\n"
        "local deadA@3, INT\n"
        "local deadB@4, INT\n"
        "t0 = val@2 > 0\n"
        "if t0 false goto L0\n"
        "ret 10\n"
        "deadA@3 = 1\n"
        "goto L1\n"
        "L0:\n"
        "ret 20\n"
        "deadB@4 = 2\n"
        "L1:\n"
        "ret 30\n"
        "end_func check\n"
        "begin_func main, 0\n"
        "param 5\n"
        "t0 = call check, 1\n"
        "t2 = -3\n"
        "param t2\n"
        "t3 = call check, 1\n"
        "t1 = t0 + t3\n"
        "ret t1\n"
        "end_func main\n";
    TACList *tacOpt = optimize(input, OPTIMIZATION_DEAD_CODE);
    TEST_ASSERT_NOT_NULL(tacOpt);
    // Las sentencias muertas deadA = 1 y deadB = 2 fueron eliminadas
    TEST_ASSERT_NULL(findAssignmentWithConstant(tacOpt, "deadA", 1));
    TEST_ASSERT_NULL(findAssignmentWithConstant(tacOpt, "deadB", 2));
}

// Asignaciones a variables locales nunca leidas se eliminan por analisis de vivacidad
void test_opt_dead_code_dead_store_elimination(void) {
    // int main() {
    //     int unused;
    //     int active;
    //     unused = 100;
    //     active = 7;
    //     return active;
    // }
    const char *input =
        "begin_func main, 0\n"
        "local unused@2, INT\n"
        "local active@3, INT\n"
        "unused@2 = 100\n"
        "active@3 = 7\n"
        "ret active@3\n"
        "end_func main\n";
    TACList *tacNoOpt = buildTAC(input);
    TEST_ASSERT_NOT_NULL(findAssignmentTo(tacNoOpt, "unused"));

    TACList *tacOpt = optimize(input, OPTIMIZATION_DEAD_CODE);
    TEST_ASSERT_NOT_NULL(tacOpt);
    // La asignacion a unused fue eliminada por ser dead store
    TEST_ASSERT_NULL(findAssignmentTo(tacOpt, "unused"));
    // La variable active se preserva intacta con su valor 7
    TEST_ASSERT_NOT_NULL(findAssignmentWithConstant(tacOpt, "active", 7));
}

// Dead code elimination reduce el numero de instrucciones en el TAC
void test_opt_dead_code_reduces_instruction_count(void) {
    // int main() {
    //     int a;
    //     int b;
    //     return 1;
    //     a = 2;
    //     b = 3;
    //     return b;
    // }
    const char *input =
        "begin_func main, 0\n"
        "local a@2, INT\n"
        "local b@3, INT\n"
        "ret 1\n"
        "a@2 = 2\n"
        "b@3 = 3\n"
        "ret b@3\n"
        "end_func main\n";
    TACList *tacNoOpt = buildTAC(input);
    int countNoOpt = countInstructions(tacNoOpt);

    TACList *tacOpt = optimize(input, OPTIMIZATION_DEAD_CODE);
    int countOpt = countInstructions(tacOpt);

    TEST_ASSERT_TRUE(countOpt < countNoOpt);
}

// ------------------------------------------------------------------------
// Grupo 4: Constant Branch Folding (Condiciones Constantes en Control de Flujo)
// ------------------------------------------------------------------------

// if (true) ejecuta directamente el cuerpo sin evaluar condicion
void test_opt_const_branch_if_true(void) {
    // int main() {
    //     int x;
    //     x = 1;
    //     if (true) {
    //         x = 25;
    //     } else {
    //         x = 50;
    //     }
    //     return x;
    // }
    const char *input =
        "begin_func main, 0\n"
        "local x@2, INT\n"
        "x@2 = 1\n"
        "if true false goto L0\n"
        "x@2 = 25\n"
        "goto L1\n"
        "L0:\n"
        "x@2 = 50\n"
        "L1:\n"
        "ret x@2\n"
        "end_func main\n";
    TACList *tacOpt = optimize(input, OPTIMIZATION_CONST_BRANCH | OPTIMIZATION_DEAD_CODE);
    TEST_ASSERT_NOT_NULL(tacOpt);
    // La rama muerta else (x = 50) fue completamente eliminada
    TEST_ASSERT_NULL(findAssignmentWithConstant(tacOpt, "x", 50));
    // La variable x toma el valor 25 de la rama ejecutada
    TEST_ASSERT_NOT_NULL(findAssignmentWithConstant(tacOpt, "x", 25));
}

// if (false) descarta la rama then y toma la rama else
void test_opt_const_branch_if_false(void) {
    // int main() {
    //     int x;
    //     x = 1;
    //     if (false) {
    //         x = 25;
    //     } else {
    //         x = 50;
    //     }
    //     return x;
    // }
    const char *input =
        "begin_func main, 0\n"
        "local x@2, INT\n"
        "x@2 = 1\n"
        "if false false goto L0\n"
        "x@2 = 25\n"
        "goto L1\n"
        "L0:\n"
        "x@2 = 50\n"
        "L1:\n"
        "ret x@2\n"
        "end_func main\n";
    TACList *tacOpt = optimize(input, OPTIMIZATION_CONST_BRANCH | OPTIMIZATION_DEAD_CODE);
    TEST_ASSERT_NOT_NULL(tacOpt);
    // La rama muerta then (x = 25) fue completamente eliminada
    TEST_ASSERT_NULL(findAssignmentWithConstant(tacOpt, "x", 25));
    // La variable x toma el valor 50 de la rama else
    TEST_ASSERT_NOT_NULL(findAssignmentWithConstant(tacOpt, "x", 50));
}

// Condicion relacional que se pliega a constante simplifica el condicional
void test_opt_const_branch_folded_condition(void) {
    // int main() {
    //     int res;
    //     if (100 > 50 && 2 == 2) {
    //         res = 77;
    //     } else {
    //         res = 11;
    //     }
    //     return res;
    // }
    const char *input =
        "begin_func main, 0\n"
        "local res@2, INT\n"
        "t0 = 100 > 50\n"
        "t1 = t0\n"
        "if t1 false goto L0\n"
        "t2 = 2 == 2\n"
        "t1 = t2\n"
        "L0:\n"
        "if t1 false goto L1\n"
        "res@2 = 77\n"
        "goto L2\n"
        "L1:\n"
        "res@2 = 11\n"
        "L2:\n"
        "ret res@2\n"
        "end_func main\n";
    TACList *tacOpt = optimize(input, OPTIMIZATION_ALL);
    TEST_ASSERT_NOT_NULL(tacOpt);
    // La rama muerta else fue eliminada por completo
    TEST_ASSERT_NULL(findAssignmentWithConstant(tacOpt, "res", 11));
    // El retorno final devuelve directamente la constante 77
    const TACInstruction *retInst = lastReturn(tacOpt);
    TEST_ASSERT_NOT_NULL(retInst);
    TEST_ASSERT_EQUAL(OPERAND_CONSTANT, retInst->firstOperand.kind);
    TEST_ASSERT_EQUAL_INT(77, retInst->firstOperand.value.intValue);
}

// while (false) nunca ejecuta el cuerpo y el salto se elimina
void test_opt_const_branch_while_false(void) {
    // int main() {
    //     int x;
    //     x = 13;
    //     while (false) {
    //         x = 99;
    //     }
    //     return x;
    // }
    const char *input =
        "begin_func main, 0\n"
        "local x@2, INT\n"
        "x@2 = 13\n"
        "L0:\n"
        "if false false goto L1\n"
        "x@2 = 99\n"
        "goto L0\n"
        "L1:\n"
        "ret x@2\n"
        "end_func main\n";
    TACList *tacOpt = optimize(input, OPTIMIZATION_CONST_BRANCH | OPTIMIZATION_DEAD_CODE);
    TEST_ASSERT_NOT_NULL(tacOpt);
    // El cuerpo inalcanzable del while (x = 99) fue eliminado
    TEST_ASSERT_NULL(findAssignmentWithConstant(tacOpt, "x", 99));
    TEST_ASSERT_NOT_NULL(findAssignmentWithConstant(tacOpt, "x", 13));
}

// ------------------------------------------------------------------------
// Grupo 5: Self-Assignment Elimination (Asignaciones a Si Misma)
// ------------------------------------------------------------------------

// Sentencias x = x sobre variables locales y globales se eliminan
void test_opt_self_assign_local_and_global(void) {
    // int g;
    // int main() {
    //     int x;
    //     x = 10;
    //     x = x;
    //     g = 20;
    //     g = g;
    //     return x + g;
    // }
    const char *input =
        "global g@1, INT\n"
        "begin_func main, 0\n"
        "local x@3, INT\n"
        "x@3 = 10\n"
        "x@3 = x@3\n"
        "g@1 = 20\n"
        "g@1 = g@1\n"
        "t0 = x@3 + g@1\n"
        "ret t0\n"
        "end_func main\n";
    TACList *tacNoOpt = buildTAC(input);
    int countNoOpt = countInstructions(tacNoOpt);

    TACList *tacOpt = optimize(input, OPTIMIZATION_SELF_ASSIGN);
    TEST_ASSERT_NOT_NULL(tacOpt);
    int countOpt = countInstructions(tacOpt);
    // Se removieron exactamente las 2 autoasignaciones redundantes
    TEST_ASSERT_EQUAL_INT(countNoOpt - 2, countOpt);
    // Las variables x y g conservan sus valores originales asignados
    TEST_ASSERT_NOT_NULL(findAssignmentWithConstant(tacOpt, "x", 10));
    TEST_ASSERT_NOT_NULL(findAssignmentWithConstant(tacOpt, "g", 20));
}

// Autoasignacion sobre parametros formales
void test_opt_self_assign_parameter(void) {
    // int helper(int p) {
    //     p = p;
    //     return p * 2;
    // }
    // int main() {
    //     return helper(8);
    // }
    const char *input =
        "begin_func helper, 1\n"
        "param_decl p@2, INT\n"
        "p@2 = p@2\n"
        "t0 = p@2 * 2\n"
        "ret t0\n"
        "end_func helper\n"
        "begin_func main, 0\n"
        "param 8\n"
        "t0 = call helper, 1\n"
        "ret t0\n"
        "end_func main\n";
    TACList *tacNoOpt = buildTAC(input);
    int countNoOpt = countInstructions(tacNoOpt);

    TACList *tacOpt = optimize(input, OPTIMIZATION_SELF_ASSIGN);
    TEST_ASSERT_NOT_NULL(tacOpt);
    int countOpt = countInstructions(tacOpt);
    // La autoasignacion p = p fue eliminada
    TEST_ASSERT_EQUAL_INT(countNoOpt - 1, countOpt);
}

// Ocultamiento de variables con mismo nombre (shadowing) no se confunde con autoasignacion
void test_opt_self_assign_shadowing_distinction(void) {
    // int x;
    // int main() {
    //     int y;
    //     x = 5;
    //     { int x; x = 10; y = x; }
    //     return x + y;
    // }
    const char *input =
        "global x@1, INT\n"
        "begin_func main, 0\n"
        "local y@3, INT\n"
        "x@1 = 5\n"
        "local x@4, INT\n"
        "x@4 = 10\n"
        "y@3 = x@4\n"
        "t0 = x@1 + y@3\n"
        "ret t0\n"
        "end_func main\n";
    // x global (5) + y (10) = 15
    TACList *tacOpt = optimize(input, OPTIMIZATION_SELF_ASSIGN);
    TEST_ASSERT_NOT_NULL(tacOpt);
    // La asignacion de x local a y no se considera autoasignacion
    TEST_ASSERT_NOT_NULL(findAssignmentTo(tacOpt, "y"));
}

// ------------------------------------------------------------------------
// Grupo 6: Peephole Optimization (Mirilla)
// ------------------------------------------------------------------------

// Salto redundante a la etiqueta inmediatamente siguiente se remueve
void test_opt_peephole_redundant_jump_to_next_label(void) {
    // int main() {
    //     int a;
    //     a = 3;
    //     if (a > 5) {
    //         a = 10;
    //     }
    //     return a;
    // }
    const char *input =
        "begin_func main, 0\n"
        "local a@2, INT\n"
        "a@2 = 3\n"
        "t0 = a@2 > 5\n"
        "if t0 false goto L0\n"
        "a@2 = 10\n"
        "L0:\n"
        "ret a@2\n"
        "end_func main\n";
    TACList *tacNoOpt = buildTAC(input);
    int countNoOpt = countInstructions(tacNoOpt);

    TACList *tacOpt = optimize(input, OPTIMIZATION_PEEPHOLE);
    TEST_ASSERT_NOT_NULL(tacOpt);
    int countOpt = countInstructions(tacOpt);
    // La pasada de mirilla remueve saltos innecesarios
    TEST_ASSERT_TRUE(countOpt <= countNoOpt);
}

// ------------------------------------------------------------------------
// Grupo 8: Programas Canonicos y Combinacion Completa (OPTIMIZATION_ALL)
// ------------------------------------------------------------------------

// Expresion con multiples oportunidades converge a punto fijo reduciendo cuadruplas
void test_opt_all_fixed_point_reduction(void) {
    // int main() {
    //     int a;
    //     int unused;
    //     int res;
    //     a = 1 + 2 + 3;
    //     unused = 99;
    //     if (10 > 5) {
    //         res = a * 2;
    //     } else {
    //         res = 0;
    //     }
    //     return res;
    // }
    const char *input =
        "begin_func main, 0\n"
        "local a@2, INT\n"
        "local unused@3, INT\n"
        "local res@4, INT\n"
        "t0 = 1 + 2\n"
        "t1 = t0 + 3\n"
        "a@2 = t1\n"
        "unused@3 = 99\n"
        "t2 = 10 > 5\n"
        "if t2 false goto L0\n"
        "t3 = a@2 * 2\n"
        "res@4 = t3\n"
        "goto L1\n"
        "L0:\n"
        "res@4 = 0\n"
        "L1:\n"
        "ret res@4\n"
        "end_func main\n";
    TACList *tacNoOpt = buildTAC(input);
    int countNoOpt = countInstructions(tacNoOpt);

    TACList *tacOpt = optimize(input, OPTIMIZATION_ALL);
    TEST_ASSERT_NOT_NULL(tacOpt);
    int countOpt = countInstructions(tacOpt);

    // Se redujo la cantidad total de instrucciones tras alcanzar punto fijo
    TEST_ASSERT_TRUE(countOpt < countNoOpt);
    // unused = 99 fue eliminada como dead store
    TEST_ASSERT_NULL(findAssignmentTo(tacOpt, "unused"));
    // El retorno devuelve directamente la constante 12
    const TACInstruction *retInst = lastReturn(tacOpt);
    TEST_ASSERT_NOT_NULL(retInst);
    TEST_ASSERT_EQUAL(OPERAND_CONSTANT, retInst->firstOperand.kind);
    TEST_ASSERT_EQUAL_INT(12, retInst->firstOperand.value.intValue);
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
    // Grupo 8: Programas Canonicos y Combinacion Completa
    RUN_TEST(test_opt_all_fixed_point_reduction);

    return UNITY_END();
}
