#include "unity.h"
#include "data_flow.h"
#include "tac_list.h"
#include "tac_instruction.h"
#include "tac_operand.h"
#include "tac_operation.h"
#include "ast.h"
#include "symbol_table.h"
#include "semantic_analyzer.h"
#include "intermediate_code.h"
#include "error_handler.h"
#include "string_builder.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

extern int yyparse(void);
typedef struct yy_buffer_state *YY_BUFFER_STATE;
extern YY_BUFFER_STATE yy_scan_string(const char *str);
extern void yy_delete_buffer(YY_BUFFER_STATE buffer);
extern int yylineno;
extern NodeAST *astRoot;
extern int semantic_errors;
extern int lexical_errors;

static const TACList *compileToTAC(const char *source) {
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

    return generateIntermediateCode(astRoot);
}

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
    if (astRoot != NULL) {
        freeAST(astRoot);
        astRoot = NULL;
    }
}

void test_instruction_data_flow_defs_and_uses(void) {
    /* t0 = a@1 + b@2 */
    TACOperand t0 = tacTemp("t0", TYPE_INT);
    TACOperand a = tacTemp("a@1", TYPE_INT);
    TACOperand b = tacTemp("b@2", TYPE_INT);

    TACInstruction *inst = createTACInstruction(TAC_ADD, t0, a, b);
    BlockDataFlow *block = createBlockDataFlow(inst, inst);

    TEST_ASSERT_NOT_NULL(block);
    TEST_ASSERT_EQUAL_size_t(1, block->count);

    const InstructionDataFlow *flow = blockGetFlow(block, inst);
    TEST_ASSERT_NOT_NULL(flow);
    TEST_ASSERT_EQUAL_size_t(1, flow->defCount);
    TEST_ASSERT_EQUAL_STRING("t0", flow->defs[0]->name);

    TEST_ASSERT_EQUAL_size_t(2, flow->useCount);
    TEST_ASSERT_TRUE(instructionFlowUses(flow, "a@1"));
    TEST_ASSERT_TRUE(instructionFlowUses(flow, "b@2"));
    TEST_ASSERT_FALSE(instructionFlowUses(flow, "c@3"));
    TEST_ASSERT_TRUE(instructionFlowDefines(flow, "t0"));

    freeBlockDataFlow(block);
    freeTACInstruction(inst);
}

void test_constants_and_labels_ignored_in_data_flow(void) {
    /* t0 = 10 + 20 */
    TACOperand t0 = tacTemp("t0", TYPE_INT);
    TACOperand c1 = tacConstantInt(10);
    TACOperand c2 = tacConstantInt(20);

    TACInstruction *inst = createTACInstruction(TAC_ADD, t0, c1, c2);
    BlockDataFlow *block = createBlockDataFlow(inst, inst);

    const InstructionDataFlow *flow = blockGetFlow(block, inst);
    TEST_ASSERT_NOT_NULL(flow);
    TEST_ASSERT_EQUAL_size_t(1, flow->defCount);
    TEST_ASSERT_EQUAL_size_t(0, flow->useCount);

    freeBlockDataFlow(block);
    freeTACInstruction(inst);
}

void test_block_find_next_use_and_def(void) {
    /*
     * i1: x@1 = 5
     * i2: y@1 = 10
     * i3: t0 = x@1 + 1
     * i4: x@1 = 20
     * i5: y@1 = 30
     */
    TACList *list = createTACList();
    TACOperand x = tacTemp("x@1", TYPE_INT);
    TACOperand y = tacTemp("y@1", TYPE_INT);
    TACOperand t0 = tacTemp("t0", TYPE_INT);
    TACOperand c5 = tacConstantInt(5);
    TACOperand c10 = tacConstantInt(10);
    TACOperand c1 = tacConstantInt(1);
    TACOperand c20 = tacConstantInt(20);
    TACOperand c30 = tacConstantInt(30);

    TACInstruction *i1 = createTACInstruction(TAC_ASSIGN, x, c5, tacNone());
    TACInstruction *i2 = createTACInstruction(TAC_ASSIGN, y, c10, tacNone());
    TACInstruction *i3 = createTACInstruction(TAC_ADD, t0, x, c1);
    TACInstruction *i4 = createTACInstruction(TAC_ASSIGN, x, c20, tacNone());
    TACInstruction *i5 = createTACInstruction(TAC_ASSIGN, y, c30, tacNone());

    i1->next = i2;
    i2->next = i3;
    i3->next = i4;
    i4->next = i5;
    list->head = i1;
    list->tail = i5;
    list->count = 5;

    BlockDataFlow *block = createBlockDataFlow(i1, i5);
    TEST_ASSERT_NOT_NULL(block);
    TEST_ASSERT_EQUAL_size_t(5, block->count);

    /* Buscar siguiente uso de x@1 a partir de i1 debe ser i3 */
    TACInstruction *nextUse = blockFindNextUse(block, i1, "x@1");
    TEST_ASSERT_EQUAL_PTR(i3, nextUse);

    /* Buscar siguiente uso de x@1 a partir de i3 debe ser NULL */
    TEST_ASSERT_NULL(blockFindNextUse(block, i3, "x@1"));

    /* Buscar siguiente def de x@1 a partir de i1 debe ser i4 */
    TACInstruction *nextDef = blockFindNextDef(block, i1, "x@1");
    TEST_ASSERT_EQUAL_PTR(i4, nextDef);

    /* Redefinición intermedia de x@1 entre i1 e i3 (no hay) */
    TEST_ASSERT_FALSE(blockIsRedefinedBetween(block, i1, i3, "x@1"));
    /* Redefinición intermedia de x@1 entre i2 e i5 (ocurre en i4) */
    TEST_ASSERT_TRUE(blockIsRedefinedBetween(block, i2, i5, "x@1"));

    freeBlockDataFlow(block);
    freeTACList(list);
}

void test_dead_definition_detection(void) {
    /*
     * i1: x@1 = 5       (Dead store: redefinido en i2 sin ser leído)
     * i2: x@1 = 10
     * i3: t0 = x@1 + 1  (Usa x@1 de i2)
     */
    TACList *list = createTACList();
    TACOperand x = tacTemp("x@1", TYPE_INT);
    TACOperand t0 = tacTemp("t0", TYPE_INT);
    TACOperand c5 = tacConstantInt(5);
    TACOperand c10 = tacConstantInt(10);
    TACOperand c1 = tacConstantInt(1);

    TACInstruction *i1 = createTACInstruction(TAC_ASSIGN, x, c5, tacNone());
    TACInstruction *i2 = createTACInstruction(TAC_ASSIGN, x, c10, tacNone());
    TACInstruction *i3 = createTACInstruction(TAC_ADD, t0, x, c1);

    i1->next = i2;
    i2->next = i3;
    list->head = i1;
    list->tail = i3;
    list->count = 3;

    BlockDataFlow *block = createBlockDataFlow(i1, i3);

    /* i1 es una definición muerta porque i2 la sobreescribe sin uso intermedio */
    TEST_ASSERT_TRUE(blockIsDeadDefinition(block, i1));

    /* i2 no es definición muerta porque i3 la lee */
    TEST_ASSERT_FALSE(blockIsDeadDefinition(block, i2));

    freeBlockDataFlow(block);
    freeTACList(list);
}

void test_unused_variable_returns_null_and_can_be_deleted(void) {
    /*
     * Programa simulado:
     *   int x;
     *   int y;
     *   x = 10;   (x jamás se usa más adelante)
     *   y = 20;   (y sí se usa en el return)
     *   return y;
     */
    TACList *list = createTACList();
    TACOperand x = tacTemp("x@1", TYPE_INT);
    TACOperand y = tacTemp("y@1", TYPE_INT);
    TACOperand c10 = tacConstantInt(10);
    TACOperand c20 = tacConstantInt(20);

    TACInstruction *instX = createTACInstruction(TAC_ASSIGN, x, c10, tacNone());
    TACInstruction *instY = createTACInstruction(TAC_ASSIGN, y, c20, tacNone());
    TACInstruction *instRet = createTACInstruction(TAC_RETURN, tacNone(), y, tacNone());

    instX->next = instY;
    instY->next = instRet;
    list->head = instX;
    list->tail = instRet;
    list->count = 3;

    /* Analizamos el bloque */
    BlockDataFlow *block = createBlockDataFlow(list->head, list->tail);
    TEST_ASSERT_NOT_NULL(block);

    /* 1. Preguntamos por x@1 si se usa más adelante: debe devolver NULL */
    TACInstruction *nextUseX = blockFindNextUse(block, instX, "x@1");
    TEST_ASSERT_NULL(nextUseX);

    /* 2. Preguntamos por y@1 si se usa más adelante: debe devolver instRet */
    TACInstruction *nextUseY = blockFindNextUse(block, instY, "y@1");
    TEST_ASSERT_NOT_NULL(nextUseY);
    TEST_ASSERT_EQUAL_PTR(instRet, nextUseY);

    /* 3. Como x@1 no se usa nunca más adelante (nextUseX == NULL), la eliminamos */
    if (nextUseX == NULL) {
        unlinkInstruction(list, NULL, instX);
    }

    /* 4. Verificamos que instX fue borrada y la lista conserva instY e instRet */
    TEST_ASSERT_EQUAL_INT(2, list->count);
    TEST_ASSERT_EQUAL_PTR(instY, list->head);
    TEST_ASSERT_EQUAL_PTR(instRet, list->head->next);
    TEST_ASSERT_EQUAL_PTR(instRet, list->tail);

    freeBlockDataFlow(block);
    freeTACList(list);
}

void test_compiled_program_unused_variable_detected_and_removed(void) {
    /*
     * Programa C-TDS completo:
     * 'unused' se asigna pero nunca se lee en el resto de la función.
     * 'used' sí se lee en el return.
     */
    const char *source =
        "int main() {\n"
        "    int unused;\n"
        "    int used;\n"
        "    unused = 100;\n"
        "    used = 200;\n"
        "    return used;\n"
        "}\n";

    TACList *list = (TACList *)compileToTAC(source);
    TEST_ASSERT_NOT_NULL(list);

    BlockDataFlow *block = createBlockDataFlow(list->head, list->tail);
    TEST_ASSERT_NOT_NULL(block);

    /* Buscamos la instrucción de asignación a unused (operación TAC_ASSIGN con result "unused@...") */
    TACInstruction *instUnused = NULL;
    TACInstruction *prevUnused = NULL;
    TACInstruction *instUsed = NULL;
    char unusedName[TAC_NAME_LENGTH] = "";
    char usedName[TAC_NAME_LENGTH] = "";

    TACInstruction *prev = NULL;
    for (TACInstruction *curr = list->head; curr != NULL; curr = curr->next) {
        if (curr->operation == TAC_ASSIGN && strstr(curr->result.name, "unused@") != NULL) {
            instUnused = curr;
            prevUnused = prev;
            snprintf(unusedName, sizeof(unusedName), "%s", curr->result.name);
        }
        if (curr->operation == TAC_ASSIGN && strstr(curr->result.name, "used@") != NULL) {
            instUsed = curr;
            snprintf(usedName, sizeof(usedName), "%s", curr->result.name);
        }
        prev = curr;
    }

    TEST_ASSERT_NOT_NULL(instUnused);
    TEST_ASSERT_NOT_NULL(instUsed);

    /* 1. Preguntamos si 'unused' se usa más adelante: debe ser NULL */
    TACInstruction *nextUseUnused = blockFindNextUse(block, instUnused, unusedName);
    TEST_ASSERT_NULL(nextUseUnused);

    /* 2. Preguntamos si 'used' se usa más adelante: NO debe ser NULL (se usa en return) */
    TACInstruction *nextUseUsed = blockFindNextUse(block, instUsed, usedName);
    TEST_ASSERT_NOT_NULL(nextUseUsed);

    /* 3. Borramos la asignación inútil a 'unused' */
    int countBefore = list->count;
    unlinkInstruction(list, prevUnused, instUnused);
    TEST_ASSERT_EQUAL_INT(countBefore - 1, list->count);

    /* 4. Validamos que la asignación a unused ya no está en la lista */
    for (TACInstruction *curr = list->head; curr != NULL; curr = curr->next) {
        bool isUnusedAssign = (curr->operation == TAC_ASSIGN && strcmp(curr->result.name, unusedName) == 0);
        TEST_ASSERT_FALSE(isUnusedAssign);
    }

    freeBlockDataFlow(block);
    freeTACList(list);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_instruction_data_flow_defs_and_uses);
    RUN_TEST(test_constants_and_labels_ignored_in_data_flow);
    RUN_TEST(test_block_find_next_use_and_def);
    RUN_TEST(test_dead_definition_detection);
    RUN_TEST(test_unused_variable_returns_null_and_can_be_deleted);
    RUN_TEST(test_compiled_program_unused_variable_detected_and_removed);
    return UNITY_END();
}
