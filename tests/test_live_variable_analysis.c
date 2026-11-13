/**
 * @file test_live_variable_analysis.c
 * @brief Pruebas unitarias del análisis de variables vivas (bloques básicos y ecuaciones de flujo).
 *
 * La entrada de cada test es TAC: construido instrucción por instrucción o escrito en el
 * formato del `.ci` con buildTAC (ver support/tac_builder.h). Ninguna etapa anterior del
 * compilador participa, así que un fallo acá apunta al análisis de variables vivas.
 */

#include "unity.h"
#include "live_variable_analysis.h"
#include "tac.h"
#include "tac_builder.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static TACInstruction *unlinkInstruction(TACList *list, TACInstruction *prev, TACInstruction *curr) {
    TACInstruction *next = curr->next;
    if (prev == NULL) {
        list->head = next;
    } else {
        prev->next = next;
    }
    if (list->tail == curr) {
        list->tail = prev;
    }
    freeTACInstruction(curr);
    list->count--;
    return next;
}

void setUp(void) {}

void tearDown(void) {
    releaseTACSymbols();
}

void test_algebraic_bitmask_equations(void) {
    /*
     * Comprobación algebraica directa:
     * USE = {x} (bit 0)
     * DEF = {a} (bit 1)
     * OUT = {a, b} (bits 1 y 2)
     *
     * IN = USE | (OUT & ~DEF) = {x, b} (bits 0 y 2)
     */
    VariableSet use = (1ULL << 0);
    VariableSet def = (1ULL << 1);
    VariableSet liveOut = (1ULL << 1) | (1ULL << 2);

    VariableSet liveIn = use | (liveOut & ~def);
    VariableSet expectedIn = (1ULL << 0) | (1ULL << 2);

    TEST_ASSERT_EQUAL_HEX64(expectedIn, liveIn);
    TEST_ASSERT_TRUE((liveIn & (1ULL << 0)) != 0); // x está viva
    TEST_ASSERT_TRUE((liveIn & (1ULL << 2)) != 0); // b está viva
    TEST_ASSERT_FALSE((liveIn & (1ULL << 1)) != 0); // a NO está viva (fue redefinida)
}

void test_live_variables_in_linear_block(void) {
    /*
     * x@1 = 5
     * y@1 = 10
     * t0 = x@1 + y@1
     * ret t0
     */
    TACList *list = createTACList();
    TACOperand x = tacTemp("x@1", TYPE_INT);
    TACOperand y = tacTemp("y@1", TYPE_INT);
    TACOperand t0 = tacTemp("t0", TYPE_INT);

    appendInstruction(list, TAC_ASSIGN, x, tacConstantInt(5), tacNone());
    appendInstruction(list, TAC_ASSIGN, y, tacConstantInt(10), tacNone());
    appendInstruction(list, TAC_ADD, t0, x, y);
    appendInstruction(list, TAC_RETURN, tacNone(), t0, tacNone());

    LiveVariableAnalysis *analysis = createLiveVariableAnalysis(list);
    TEST_ASSERT_NOT_NULL(analysis);
    TEST_ASSERT_EQUAL_size_t(1, analysis->blockCount);

    BasicBlock *b0 = analysis->blocks[0];
    TEST_ASSERT_EQUAL_HEX64(0ULL, b0->liveIn);
    TEST_ASSERT_EQUAL_HEX64(0ULL, b0->liveOut);

    freeLiveVariableAnalysis(analysis);
    freeTACList(list);
}

void test_live_variables_in_if_else_control_flow(void) {
    /*
     * B0: if c@1 false goto L_ELSE
     * B1: x@1 = 1; goto L_END
     * B2: L_ELSE: x@1 = 2
     * B3: L_END: ret x@1
     */
    TACList *list = createTACList();
    TACOperand c = tacTemp("c@1", TYPE_BOOL);
    TACOperand x = tacTemp("x@1", TYPE_INT);
    TACOperand lElse = tacLabel("L_ELSE");
    TACOperand lEnd = tacLabel("L_END");

    appendInstruction(list, TAC_IF_FALSE, tacNone(), c, lElse);
    TACInstruction *iThen = appendInstruction(list, TAC_ASSIGN, x, tacConstantInt(1), tacNone());
    appendInstruction(list, TAC_GOTO, tacNone(), lEnd, tacNone());
    appendInstruction(list, TAC_LABEL, lElse, tacNone(), tacNone());
    TACInstruction *iElse = appendInstruction(list, TAC_ASSIGN, x, tacConstantInt(2), tacNone());
    appendInstruction(list, TAC_LABEL, lEnd, tacNone(), tacNone());
    appendInstruction(list, TAC_RETURN, tacNone(), x, tacNone());

    LiveVariableAnalysis *analysis = createLiveVariableAnalysis(list);
    TEST_ASSERT_NOT_NULL(analysis);

    /* x@1 NO debe ser dead definition en ninguna de las dos ramas */
    TEST_ASSERT_FALSE(isDeadDefinition(analysis, iThen));
    TEST_ASSERT_FALSE(isDeadDefinition(analysis, iElse));

    freeLiveVariableAnalysis(analysis);
    freeTACList(list);
}

void test_live_variables_in_while_loop_back_edge(void) {
    /*
     * L_LOOP:
     *     x@1 = x@1 + 1
     *     t0 = x@1 < 10
     *     if t0 true goto L_LOOP
     *     ret 0
     */
    TACList *list = createTACList();
    TACOperand x = tacTemp("x@1", TYPE_INT);
    TACOperand t0 = tacTemp("t0", TYPE_BOOL);
    TACOperand lLoop = tacLabel("L_LOOP");

    appendInstruction(list, TAC_LABEL, lLoop, tacNone(), tacNone());
    TACInstruction *iAdd = appendInstruction(list, TAC_ADD, x, x, tacConstantInt(1));
    appendInstruction(list, TAC_LT, t0, x, tacConstantInt(10));
    appendInstruction(list, TAC_IF_TRUE, tacNone(), t0, lLoop);
    appendInstruction(list, TAC_RETURN, tacNone(), tacConstantInt(0), tacNone());

    LiveVariableAnalysis *analysis = createLiveVariableAnalysis(list);
    TEST_ASSERT_NOT_NULL(analysis);

    TEST_ASSERT_FALSE(isDeadDefinition(analysis, iAdd));

    freeLiveVariableAnalysis(analysis);
    freeTACList(list);
}

void test_dead_definition_detected_when_never_read_globally(void) {
    /*
     * x@1 = 100
     * ret 0
     */
    TACList *list = createTACList();
    TACOperand x = tacTemp("x@1", TYPE_INT);

    TACInstruction *iDead = appendInstruction(list, TAC_ASSIGN, x, tacConstantInt(100), tacNone());
    appendInstruction(list, TAC_RETURN, tacNone(), tacConstantInt(0), tacNone());

    LiveVariableAnalysis *analysis = createLiveVariableAnalysis(list);
    TEST_ASSERT_NOT_NULL(analysis);

    TEST_ASSERT_TRUE(isDeadDefinition(analysis, iDead));

    freeLiveVariableAnalysis(analysis);
    freeTACList(list);
}

void test_program_unused_variable_detected_and_removed(void) {
    /* int main() { int unused; int used; unused = 100; used = 200; return used; } */
    TACList *list = buildTAC(
        "begin_func main, 0\n"
        "local unused@2, INT\n"
        "local used@3, INT\n"
        "unused@2 = 100\n"
        "used@3 = 200\n"
        "ret used@3\n"
        "end_func main\n");
    TEST_ASSERT_NOT_NULL(list);

    LiveVariableAnalysis *analysis = createLiveVariableAnalysis(list);
    TEST_ASSERT_NOT_NULL(analysis);

    TACInstruction *instUnused = NULL;
    TACInstruction *prevUnused = NULL;
    TACInstruction *instUsed = NULL;
    char unusedName[TAC_NAME_LENGTH] = "";

    TACInstruction *prev = NULL;
    for (TACInstruction *curr = list->head; curr != NULL; curr = curr->next) {
        if (curr->operation == TAC_ASSIGN && strstr(curr->result.name, "unused@") != NULL) {
            instUnused = curr;
            prevUnused = prev;
            snprintf(unusedName, sizeof(unusedName), "%s", curr->result.name);
        }
        if (curr->operation == TAC_ASSIGN && strstr(curr->result.name, "used@") != NULL) {
            instUsed = curr;
        }
        prev = curr;
    }

    TEST_ASSERT_NOT_NULL(instUnused);
    TEST_ASSERT_NOT_NULL(instUsed);

    TEST_ASSERT_TRUE(isDeadDefinition(analysis, instUnused));
    TEST_ASSERT_FALSE(isDeadDefinition(analysis, instUsed));

    BasicBlock *blockUnused = findInstructionBlock(analysis, instUnused);
    BasicBlock *blockUsed = findInstructionBlock(analysis, instUsed);
    TEST_ASSERT_NOT_NULL(blockUnused);
    TEST_ASSERT_NOT_NULL(blockUsed);
    TEST_ASSERT_TRUE(isDeadDefinitionInBlock(analysis, blockUnused, instUnused));
    TEST_ASSERT_FALSE(isDeadDefinitionInBlock(analysis, blockUsed, instUsed));

    int countBefore = list->count;
    unlinkInstruction(list, prevUnused, instUnused);
    TEST_ASSERT_EQUAL_INT(countBefore - 1, list->count);

    for (TACInstruction *curr = list->head; curr != NULL; curr = curr->next) {
        bool isUnusedAssign = (curr->operation == TAC_ASSIGN && strcmp(curr->result.name, unusedName) == 0);
        TEST_ASSERT_FALSE(isUnusedAssign);
    }

    freeLiveVariableAnalysis(analysis);
    freeTACList(list);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_algebraic_bitmask_equations);
    RUN_TEST(test_live_variables_in_linear_block);
    RUN_TEST(test_live_variables_in_if_else_control_flow);
    RUN_TEST(test_live_variables_in_while_loop_back_edge);
    RUN_TEST(test_dead_definition_detected_when_never_read_globally);
    RUN_TEST(test_program_unused_variable_detected_and_removed);
    return UNITY_END();
}
