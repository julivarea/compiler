/**
 * @file test_object_code.c
 * @brief Pruebas unitarias del generador de código objeto (assembly x86-64).
 *
 * La entrada de cada test es TAC: instrucciones construidas a mano o un listado en el
 * formato del `.ci` (ver support/tac_builder.h), y las aserciones inspeccionan el
 * assembly emitido sin ensamblarlo. Así un test solo puede fallar por un defecto del
 * generador de código objeto. La ejecución de programas completos está en
 * test_integration.c.
 */

#include "unity.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "symbol_table.h"
#include "tac.h"
#include "tac_builder.h"
#include "object_code.h"
#include "function_frame.h"
#include "string_builder.h"
#include "types.h"

/* ========================================================================
 * Helpers del arnés de pruebas
 * ======================================================================== */

/* Emite el assembly del TAC descrito por @p tac (formato .ci) y lo devuelve. */
static const char *emitAssembly(const char *tac, OptimizationSet optimizations) {
    TACList *list = buildTAC(tac);
    if (list == NULL) {
        return NULL;
    }
    resetBuildString();
    generateAssembly(list, optimizations);
    freeTACList(list);
    return getBuiltString();
}

void setUp(void) {
    resetBuildString();
}

void tearDown(void) {
    releaseTACSymbols();
}

/* ========================================================================
 * Grupo 1: Pruebas estructurales de marco y backend (In-Memory, sin GCC)
 * ======================================================================== */

// Creacion de FunctionFrame, asignacion de ranuras, alineacion a 16 bytes y manejo de nulos
void test_asm_frame_allocation_alignment_and_null_safety(void) {
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

    /* Manejo seguro de punteros nulos */
    FunctionFrame *nullFrame = createFrame(NULL);
    TEST_ASSERT_NOT_NULL(nullFrame);
    freeFrame(nullFrame);
    freeFrame(NULL);
    TEST_ASSERT_EQUAL_INT(0, addSlot(NULL, &localOperand));
    TEST_ASSERT_EQUAL_INT(0, addAnonymousSlot(NULL));
}

// Emision de seccion .note.GNU-stack
void test_asm_sections_gnu_stack(void) {
    // int main() { return 0; }
    const char *tac =
        "begin_func main, 0\n"
        "ret 0\n"
        "end_func main\n";
    const char *assembly = emitAssembly(tac, OPTIMIZATION_NONE);
    TEST_ASSERT_NOT_NULL(assembly);
    TEST_ASSERT_NOT_NULL(strstr(assembly, ".section .note.GNU-stack,\"\",@progbits"));

    /* Lista TAC nula produce cadena vacia de forma segura */
    resetBuildString();
    generateAssembly(NULL, OPTIMIZATION_NONE);
    TEST_ASSERT_EQUAL_STRING("", getBuiltString());
}

// Emision de prologo, epilogo y garantia de codigo de salida 0 en void main
void test_asm_emit_prologue_and_epilogue(void) {
    FunctionFrame *frame = createFrame("main");
    frame->returnType = TYPE_VOID;
    frame->frameSize = 32;

    resetBuildString();
    emitPrologue(frame);
    const char *prologueOutput = getBuiltString();

    TEST_ASSERT_NOT_NULL(strstr(prologueOutput, ".globl main"));
    TEST_ASSERT_NOT_NULL(strstr(prologueOutput, "pushq %rbp"));
    TEST_ASSERT_NOT_NULL(strstr(prologueOutput, "movq %rsp, %rbp"));
    TEST_ASSERT_NOT_NULL(strstr(prologueOutput, "subq $32, %rsp"));

    resetBuildString();
    emitEpilogue(frame);
    const char *epilogueOutput = getBuiltString();

    TEST_ASSERT_NOT_NULL(strstr(epilogueOutput, ".L_main_epilogue:"));
    TEST_ASSERT_NOT_NULL(strstr(epilogueOutput, "movl $0, %eax"));
    TEST_ASSERT_NOT_NULL(strstr(epilogueOutput, "leave"));
    TEST_ASSERT_NOT_NULL(strstr(epilogueOutput, "ret"));

    freeFrame(frame);
}

// Reduccion de fuerza reemplaza multiplicacion entera por potencia de 2 por sall
void test_asm_strength_reduction_multiplication_by_power_of_two(void) {
    // int main() { int x; x = 5; return x * 4; }
    const char *tac =
        "begin_func main, 0\n"
        "local x@2, INT\n"
        "x@2 = 5\n"
        "t0 = x@2 * 4\n"
        "ret t0\n"
        "end_func main\n";
    const char *assembly = emitAssembly(tac, OPTIMIZATION_STRENGTH);
    TEST_ASSERT_NOT_NULL(assembly);
    TEST_ASSERT_NOT_NULL(strstr(assembly, "sall $2, %eax"));
}

// Fusion de condicion relacional y salto condicional (Peephole) emite cmpl + jcc directo
void test_asm_fused_relational_branch_peephole(void) {
    // int main() { int a; a = 10; if (a < 20) { return 1; } return 0; }
    const char *tac =
        "begin_func main, 0\n"
        "local a@2, INT\n"
        "a@2 = 10\n"
        "t0 = a@2 < 20\n"
        "if t0 false goto L0\n"
        "ret 1\n"
        "L0:\n"
        "ret 0\n"
        "end_func main\n";
    const char *assembly = emitAssembly(tac, OPTIMIZATION_PEEPHOLE);
    TEST_ASSERT_NOT_NULL(assembly);
    TEST_ASSERT_NOT_NULL(strstr(assembly, "cmpl %edx, %eax"));
    TEST_ASSERT_NOT_NULL(strstr(assembly, "jge ."));
    TEST_ASSERT_NULL(strstr(assembly, "setl %al"));
}

// Multiplicacion por potencias de dos se reduce a desplazamientos aritmeticos
void test_asm_strength_reduction_powers_of_two(void) {
    // int main() {
    const char *tac =
        "begin_func main, 0\n"
        "local x@2, INT\n"
        "x@2 = 3\n"
        "t0 = x@2 * 2\n"
        "t2 = x@2 * 4\n"
        "t1 = t0 + t2\n"
        "t4 = x@2 * 8\n"
        "t3 = t1 + t4\n"
        "ret t3\n"
        "end_func main\n";
    const char *assembly = emitAssembly(tac, OPTIMIZATION_NONE);
    TEST_ASSERT_NOT_NULL(assembly);
    TEST_ASSERT_TRUE(strstr(assembly, "imull") != NULL);

    assembly = emitAssembly(tac, OPTIMIZATION_STRENGTH);
    TEST_ASSERT_NOT_NULL(assembly);
    // En assembly se emitieron desplazamientos sall/shll
    TEST_ASSERT_TRUE(strstr(assembly, "sall") != NULL || strstr(assembly, "shll") != NULL);
    // No permanecen imull con multiplicadores potencia de 2
    TEST_ASSERT_NULL(strstr(assembly, "imull $2"));
    TEST_ASSERT_NULL(strstr(assembly, "imull $4"));
    TEST_ASSERT_NULL(strstr(assembly, "imull $8"));
}

// Multiplicacion conmutativa (constante a izquierda o derecha) se reduce por igual
void test_asm_strength_reduction_commutative(void) {
    // int main() {
    const char *tac =
        "begin_func main, 0\n"
        "local x@2, INT\n"
        "x@2 = 5\n"
        "t0 = 2 * x@2\n"
        "t2 = 4 * x@2\n"
        "t1 = t0 + t2\n"
        "ret t1\n"
        "end_func main\n";
    const char *assembly = emitAssembly(tac, OPTIMIZATION_STRENGTH);
    TEST_ASSERT_NOT_NULL(assembly);
    TEST_ASSERT_TRUE(strstr(assembly, "sall") != NULL || strstr(assembly, "shll") != NULL);
}

// Multiplicacion por cero y por uno se reducen a xor o asignacion
void test_asm_strength_reduction_zero_and_one(void) {
    // int main() {
    const char *tac =
        "begin_func main, 0\n"
        "local x@2, INT\n"
        "x@2 = 33\n"
        "t0 = x@2 * 1\n"
        "t2 = x@2 * 0\n"
        "t1 = t0 + t2\n"
        "ret t1\n"
        "end_func main\n";
    const char *assembly = emitAssembly(tac, OPTIMIZATION_STRENGTH);
    TEST_ASSERT_NOT_NULL(assembly);
    // Multiplicacion por 0 se reduce a xorl
    TEST_ASSERT_TRUE(strstr(assembly, "xorl") != NULL);
}

// Multiplicacion por factores que no son potencia de dos preserva instruccion imull
void test_asm_strength_reduction_non_power_of_two_preserves_imull(void) {
    // int main() {
    const char *tac =
        "begin_func main, 0\n"
        "local x@2, INT\n"
        "x@2 = 7\n"
        "t0 = x@2 * 5\n"
        "ret t0\n"
        "end_func main\n";
    const char *assembly = emitAssembly(tac, OPTIMIZATION_STRENGTH);
    TEST_ASSERT_NOT_NULL(assembly);
    // Multiplicacion por 5 preserva instruccion imull
    TEST_ASSERT_TRUE(strstr(assembly, "imull") != NULL);
}

/* ========================================================================
 * Grupo 2: Pruebas unitarias de traduccion aislada TAC -> Assembly (generateInstruction)
 * ======================================================================== */

/* --- Subgrupo 2.1: Asignacion y Aritmetica Entera --- */

// TAC_ASSIGN entero emite movl desde inmediato hacia registro y ranura de destino
void test_asm_unit_tac_assign_int(void) {
    FunctionFrame *frame = createFrame("test_func");
    TACOperand res = tacTemp("t0", TYPE_INT);
    TACOperand op1 = tacConstantInt(42);
    addSlot(frame, &res);

    TACInstruction inst = {
        .operation = TAC_ASSIGN,
        .result = res,
        .firstOperand = op1,
        .secondOperand = tacNone(),
        .next = NULL
    };

    resetBuildString();
    generateInstruction(&inst, frame);
    const char *asmOutput = getBuiltString();

    TEST_ASSERT_NOT_NULL(strstr(asmOutput, "movl $42, %eax"));
    TEST_ASSERT_NOT_NULL(strstr(asmOutput, "movl %eax, -4(%rbp)"));
    freeFrame(frame);
}

// TAC_ADD entero emite addl entre operandos y almacena en el resultado
void test_asm_unit_tac_add_int(void) {
    FunctionFrame *frame = createFrame("test_func");
    TACOperand res = tacTemp("t0", TYPE_INT);
    TACOperand op1 = tacTemp("t1", TYPE_INT);
    TACOperand op2 = tacTemp("t2", TYPE_INT);
    addSlot(frame, &res);
    addSlot(frame, &op1);
    addSlot(frame, &op2);

    TACInstruction inst = {
        .operation = TAC_ADD,
        .result = res,
        .firstOperand = op1,
        .secondOperand = op2,
        .next = NULL
    };

    resetBuildString();
    generateInstruction(&inst, frame);
    const char *asmOutput = getBuiltString();

    TEST_ASSERT_NOT_NULL(strstr(asmOutput, "addl %edx, %eax"));
    TEST_ASSERT_NOT_NULL(strstr(asmOutput, "movl %eax, -4(%rbp)"));
    freeFrame(frame);
}

// TAC_SUB entero emite subl entre operandos y almacena en el resultado
void test_asm_unit_tac_sub_int(void) {
    FunctionFrame *frame = createFrame("test_func");
    TACOperand res = tacTemp("t0", TYPE_INT);
    TACOperand op1 = tacTemp("t1", TYPE_INT);
    TACOperand op2 = tacTemp("t2", TYPE_INT);
    addSlot(frame, &res);
    addSlot(frame, &op1);
    addSlot(frame, &op2);

    TACInstruction inst = {
        .operation = TAC_SUB,
        .result = res,
        .firstOperand = op1,
        .secondOperand = op2,
        .next = NULL
    };

    resetBuildString();
    generateInstruction(&inst, frame);
    const char *asmOutput = getBuiltString();

    TEST_ASSERT_NOT_NULL(strstr(asmOutput, "subl %edx, %eax"));
    TEST_ASSERT_NOT_NULL(strstr(asmOutput, "movl %eax, -4(%rbp)"));
    freeFrame(frame);
}

// TAC_MUL entero emite imull entre operandos cuando no aplica reduccion
void test_asm_unit_tac_mul_int(void) {
    FunctionFrame *frame = createFrame("test_func");
    TACOperand res = tacTemp("t0", TYPE_INT);
    TACOperand op1 = tacTemp("t1", TYPE_INT);
    TACOperand op2 = tacTemp("t2", TYPE_INT);
    addSlot(frame, &res);
    addSlot(frame, &op1);
    addSlot(frame, &op2);

    TACInstruction inst = {
        .operation = TAC_MUL,
        .result = res,
        .firstOperand = op1,
        .secondOperand = op2,
        .next = NULL
    };

    resetBuildString();
    generateInstruction(&inst, frame);
    const char *asmOutput = getBuiltString();

    TEST_ASSERT_NOT_NULL(strstr(asmOutput, "imull %edx, %eax"));
    TEST_ASSERT_NOT_NULL(strstr(asmOutput, "movl %eax, -4(%rbp)"));
    freeFrame(frame);
}

// TAC_DIV entero extiende con cltd, divide con idivl %ecx y almacena %eax
void test_asm_unit_tac_div_int(void) {
    FunctionFrame *frame = createFrame("test_func");
    TACOperand res = tacTemp("t0", TYPE_INT);
    TACOperand op1 = tacTemp("t1", TYPE_INT);
    TACOperand op2 = tacTemp("t2", TYPE_INT);
    addSlot(frame, &res);
    addSlot(frame, &op1);
    addSlot(frame, &op2);

    TACInstruction inst = {
        .operation = TAC_DIV,
        .result = res,
        .firstOperand = op1,
        .secondOperand = op2,
        .next = NULL
    };

    resetBuildString();
    generateInstruction(&inst, frame);
    const char *asmOutput = getBuiltString();

    TEST_ASSERT_NOT_NULL(strstr(asmOutput, "cltd"));
    TEST_ASSERT_NOT_NULL(strstr(asmOutput, "idivl %ecx"));
    TEST_ASSERT_NOT_NULL(strstr(asmOutput, "movl %eax, -4(%rbp)"));
    freeFrame(frame);
}

// TAC_MOD entero divide con idivl %ecx y almacena el resto desde %edx
void test_asm_unit_tac_mod_int(void) {
    FunctionFrame *frame = createFrame("test_func");
    TACOperand res = tacTemp("t0", TYPE_INT);
    TACOperand op1 = tacTemp("t1", TYPE_INT);
    TACOperand op2 = tacTemp("t2", TYPE_INT);
    addSlot(frame, &res);
    addSlot(frame, &op1);
    addSlot(frame, &op2);

    TACInstruction inst = {
        .operation = TAC_MOD,
        .result = res,
        .firstOperand = op1,
        .secondOperand = op2,
        .next = NULL
    };

    resetBuildString();
    generateInstruction(&inst, frame);
    const char *asmOutput = getBuiltString();

    TEST_ASSERT_NOT_NULL(strstr(asmOutput, "cltd"));
    TEST_ASSERT_NOT_NULL(strstr(asmOutput, "idivl %ecx"));
    TEST_ASSERT_NOT_NULL(strstr(asmOutput, "movl %edx, -4(%rbp)"));
    freeFrame(frame);
}

// TAC_UMINUS entero emite negl %eax
void test_asm_unit_tac_uminus_int(void) {
    FunctionFrame *frame = createFrame("test_func");
    TACOperand res = tacTemp("t0", TYPE_INT);
    TACOperand op1 = tacTemp("t1", TYPE_INT);
    addSlot(frame, &res);
    addSlot(frame, &op1);

    TACInstruction inst = {
        .operation = TAC_UMINUS,
        .result = res,
        .firstOperand = op1,
        .secondOperand = tacNone(),
        .next = NULL
    };

    resetBuildString();
    generateInstruction(&inst, frame);
    const char *asmOutput = getBuiltString();

    TEST_ASSERT_NOT_NULL(strstr(asmOutput, "negl %eax"));
    TEST_ASSERT_NOT_NULL(strstr(asmOutput, "movl %eax, -4(%rbp)"));
    freeFrame(frame);
}

/* --- Subgrupo 2.2: Operaciones Booleanas y Relacionales Enteras --- */

// TAC_NOT booleano emite xorl $1 sobre %eax
void test_asm_unit_tac_not(void) {
    FunctionFrame *frame = createFrame("test_func");
    TACOperand res = tacTemp("t0", TYPE_BOOL);
    TACOperand op1 = tacTemp("t1", TYPE_BOOL);
    addSlot(frame, &res);
    addSlot(frame, &op1);

    TACInstruction inst = {
        .operation = TAC_NOT,
        .result = res,
        .firstOperand = op1,
        .secondOperand = tacNone(),
        .next = NULL
    };

    resetBuildString();
    generateInstruction(&inst, frame);
    const char *asmOutput = getBuiltString();

    TEST_ASSERT_NOT_NULL(strstr(asmOutput, "xorl $1, %eax"));
    TEST_ASSERT_NOT_NULL(strstr(asmOutput, "movl %eax, -4(%rbp)"));
    freeFrame(frame);
}

// TAC_LT entero emite cmpl con setl y extension con movzbl
void test_asm_unit_tac_lt_int(void) {
    FunctionFrame *frame = createFrame("test_func");
    TACOperand res = tacTemp("t0", TYPE_BOOL);
    TACOperand op1 = tacTemp("t1", TYPE_INT);
    TACOperand op2 = tacTemp("t2", TYPE_INT);
    addSlot(frame, &res);
    addSlot(frame, &op1);
    addSlot(frame, &op2);

    TACInstruction inst = {
        .operation = TAC_LT,
        .result = res,
        .firstOperand = op1,
        .secondOperand = op2,
        .next = NULL
    };

    resetBuildString();
    generateInstruction(&inst, frame);
    const char *asmOutput = getBuiltString();

    TEST_ASSERT_NOT_NULL(strstr(asmOutput, "cmpl %edx, %eax"));
    TEST_ASSERT_NOT_NULL(strstr(asmOutput, "setl %al"));
    TEST_ASSERT_NOT_NULL(strstr(asmOutput, "movzbl %al, %eax"));
    TEST_ASSERT_NOT_NULL(strstr(asmOutput, "movl %eax, -4(%rbp)"));
    freeFrame(frame);
}

// TAC_GT entero emite cmpl con setg y extension con movzbl
void test_asm_unit_tac_gt_int(void) {
    FunctionFrame *frame = createFrame("test_func");
    TACOperand res = tacTemp("t0", TYPE_BOOL);
    TACOperand op1 = tacTemp("t1", TYPE_INT);
    TACOperand op2 = tacTemp("t2", TYPE_INT);
    addSlot(frame, &res);
    addSlot(frame, &op1);
    addSlot(frame, &op2);

    TACInstruction inst = {
        .operation = TAC_GT,
        .result = res,
        .firstOperand = op1,
        .secondOperand = op2,
        .next = NULL
    };

    resetBuildString();
    generateInstruction(&inst, frame);
    const char *asmOutput = getBuiltString();

    TEST_ASSERT_NOT_NULL(strstr(asmOutput, "cmpl %edx, %eax"));
    TEST_ASSERT_NOT_NULL(strstr(asmOutput, "setg %al"));
    TEST_ASSERT_NOT_NULL(strstr(asmOutput, "movzbl %al, %eax"));
    freeFrame(frame);
}

// TAC_EQ entero emite cmpl con sete y extension con movzbl
void test_asm_unit_tac_eq_int(void) {
    FunctionFrame *frame = createFrame("test_func");
    TACOperand res = tacTemp("t0", TYPE_BOOL);
    TACOperand op1 = tacTemp("t1", TYPE_INT);
    TACOperand op2 = tacTemp("t2", TYPE_INT);
    addSlot(frame, &res);
    addSlot(frame, &op1);
    addSlot(frame, &op2);

    TACInstruction inst = {
        .operation = TAC_EQ,
        .result = res,
        .firstOperand = op1,
        .secondOperand = op2,
        .next = NULL
    };

    resetBuildString();
    generateInstruction(&inst, frame);
    const char *asmOutput = getBuiltString();

    TEST_ASSERT_NOT_NULL(strstr(asmOutput, "cmpl %edx, %eax"));
    TEST_ASSERT_NOT_NULL(strstr(asmOutput, "sete %al"));
    TEST_ASSERT_NOT_NULL(strstr(asmOutput, "movzbl %al, %eax"));
    freeFrame(frame);
}

/* --- Subgrupo 2.3: Control de Flujo y Saltos --- */

// TAC_LABEL emite definicion de etiqueta local con punto delante
void test_asm_unit_tac_label(void) {
    FunctionFrame *frame = createFrame("test_func");
    TACOperand labelOp = tacLabel("L10");

    TACInstruction inst = {
        .operation = TAC_LABEL,
        .result = labelOp,
        .firstOperand = tacNone(),
        .secondOperand = tacNone(),
        .next = NULL
    };

    resetBuildString();
    generateInstruction(&inst, frame);
    const char *asmOutput = getBuiltString();

    TEST_ASSERT_NOT_NULL(strstr(asmOutput, ".L10:\n"));
    freeFrame(frame);
}

// TAC_GOTO emite salto incondicional jmp a etiqueta local
void test_asm_unit_tac_goto(void) {
    FunctionFrame *frame = createFrame("test_func");
    TACOperand labelOp = tacLabel("L10");

    TACInstruction inst = {
        .operation = TAC_GOTO,
        .result = tacNone(),
        .firstOperand = labelOp,
        .secondOperand = tacNone(),
        .next = NULL
    };

    resetBuildString();
    generateInstruction(&inst, frame);
    const char *asmOutput = getBuiltString();

    TEST_ASSERT_NOT_NULL(strstr(asmOutput, "jmp .L10\n"));
    freeFrame(frame);
}

// TAC_IF_FALSE compara condicion con cero y salta con je si es falso
void test_asm_unit_tac_if_false(void) {
    FunctionFrame *frame = createFrame("test_func");
    TACOperand cond = tacTemp("t0", TYPE_BOOL);
    addSlot(frame, &cond);
    TACOperand target = tacLabel("L20");

    TACInstruction inst = {
        .operation = TAC_IF_FALSE,
        .result = tacNone(),
        .firstOperand = cond,
        .secondOperand = target,
        .next = NULL
    };

    resetBuildString();
    generateInstruction(&inst, frame);
    const char *asmOutput = getBuiltString();

    TEST_ASSERT_NOT_NULL(strstr(asmOutput, "cmpl $0, %eax"));
    TEST_ASSERT_NOT_NULL(strstr(asmOutput, "je .L20\n"));
    freeFrame(frame);
}

// TAC_IF_TRUE compara condicion con cero y salta con jne si es verdadero
void test_asm_unit_tac_if_true(void) {
    FunctionFrame *frame = createFrame("test_func");
    TACOperand cond = tacTemp("t0", TYPE_BOOL);
    addSlot(frame, &cond);
    TACOperand target = tacLabel("L20");

    TACInstruction inst = {
        .operation = TAC_IF_TRUE,
        .result = tacNone(),
        .firstOperand = cond,
        .secondOperand = target,
        .next = NULL
    };

    resetBuildString();
    generateInstruction(&inst, frame);
    const char *asmOutput = getBuiltString();

    TEST_ASSERT_NOT_NULL(strstr(asmOutput, "cmpl $0, %eax"));
    TEST_ASSERT_NOT_NULL(strstr(asmOutput, "jne .L20\n"));
    freeFrame(frame);
}

/* --- Subgrupo 2.4: Funciones, Parametros y Retorno --- */

// TAC_PARAM_DECL copia el primer argumento entero desde %edi a su ranura en la pila
void test_asm_unit_tac_param_decl_register(void) {
    FunctionFrame *frame = createFrame("test_func");
    TACOperand param = tacTemp("p1", TYPE_INT);
    addSlot(frame, &param);

    TACInstruction inst = {
        .operation = TAC_PARAM_DECL,
        .result = param,
        .firstOperand = tacNone(),
        .secondOperand = tacNone(),
        .next = NULL
    };

    resetBuildString();
    generateInstruction(&inst, frame);
    const char *asmOutput = getBuiltString();

    TEST_ASSERT_NOT_NULL(strstr(asmOutput, "movl %edi, -4(%rbp)"));
    TEST_ASSERT_EQUAL_INT(1, frame->integerParamCount);
    freeFrame(frame);
}

// TAC_PARAM_DECL para el 7mo parametro entero lee desde 16(%rbp) en el marco del llamador
void test_asm_unit_tac_param_decl_stack(void) {
    FunctionFrame *frame = createFrame("test_func");
    frame->integerParamCount = 6;

    TACOperand param = tacTemp("p7", TYPE_INT);
    addSlot(frame, &param);

    TACInstruction inst = {
        .operation = TAC_PARAM_DECL,
        .result = param,
        .firstOperand = tacNone(),
        .secondOperand = tacNone(),
        .next = NULL
    };

    resetBuildString();
    generateInstruction(&inst, frame);
    const char *asmOutput = getBuiltString();

    TEST_ASSERT_NOT_NULL(strstr(asmOutput, "movl 16(%rbp), %eax"));
    TEST_ASSERT_NOT_NULL(strstr(asmOutput, "movl %eax, -4(%rbp)"));
    TEST_ASSERT_EQUAL_INT(1, frame->stackParamCount);
    freeFrame(frame);
}

// TAC_PARAM y TAC_CALL gestionan argumento en ranura pendiente y emiten call
void test_asm_unit_tac_param_and_call(void) {
    FunctionFrame *frame = createFrame("caller_func");
    frame->pendingParamSlots[0] = addAnonymousSlot(frame);
    frame->maxPendingParams = 1;

    TACOperand arg = tacConstantInt(42);
    TACInstruction paramInst = {
        .operation = TAC_PARAM,
        .result = tacNone(),
        .firstOperand = arg,
        .secondOperand = tacNone(),
        .next = NULL
    };

    resetBuildString();
    generateInstruction(&paramInst, frame);
    const char *paramOutput = getBuiltString();
    TEST_ASSERT_NOT_NULL(strstr(paramOutput, "movl $42, %eax"));
    TEST_ASSERT_NOT_NULL(strstr(paramOutput, "movl %eax, -4(%rbp)"));
    TEST_ASSERT_EQUAL_INT(1, frame->pendingParamCount);

    TACOperand res = tacTemp("t0", TYPE_INT);
    addSlot(frame, &res);
    TACOperand callee = tacMethod("target_func", TYPE_INT);
    TACOperand arity = tacConstantInt(1);

    TACInstruction callInst = {
        .operation = TAC_CALL,
        .result = res,
        .firstOperand = callee,
        .secondOperand = arity,
        .next = NULL
    };

    resetBuildString();
    generateInstruction(&callInst, frame);
    const char *callOutput = getBuiltString();

    TEST_ASSERT_NOT_NULL(strstr(callOutput, "movl -4(%rbp), %edi"));
    TEST_ASSERT_NOT_NULL(strstr(callOutput, "call target_func"));
    TEST_ASSERT_NOT_NULL(strstr(callOutput, "movl %eax, -8(%rbp)"));
    freeFrame(frame);
}

// TAC_RETURN con expresion carga %eax y salta al epilogo de la funcion
void test_asm_unit_tac_return_with_expression(void) {
    FunctionFrame *frame = createFrame("my_func");
    TACOperand retVal = tacTemp("t0", TYPE_INT);
    addSlot(frame, &retVal);

    TACInstruction inst = {
        .operation = TAC_RETURN,
        .result = tacNone(),
        .firstOperand = retVal,
        .secondOperand = tacNone(),
        .next = NULL
    };

    resetBuildString();
    generateInstruction(&inst, frame);
    const char *asmOutput = getBuiltString();

    TEST_ASSERT_NOT_NULL(strstr(asmOutput, "movl -4(%rbp), %eax"));
    TEST_ASSERT_NOT_NULL(strstr(asmOutput, "jmp .L_my_func_epilogue"));
    freeFrame(frame);
}

// TAC_RETURN sin expresion salta directamente al epilogo de la funcion
void test_asm_unit_tac_return_void(void) {
    FunctionFrame *frame = createFrame("void_func");

    TACInstruction inst = {
        .operation = TAC_RETURN,
        .result = tacNone(),
        .firstOperand = tacNone(),
        .secondOperand = tacNone(),
        .next = NULL
    };

    resetBuildString();
    generateInstruction(&inst, frame);
    const char *asmOutput = getBuiltString();

    TEST_ASSERT_NOT_NULL(strstr(asmOutput, "jmp .L_void_func_epilogue"));
    freeFrame(frame);
}

/* ========================================================================
 * Grupo 3: Pruebas unitarias de emision de flujo con TAC estructurado (In-Memory)
 * ======================================================================== */

// Emision completa de control de flujo con saltos condicionales y etiquetas
void test_asm_flow_short_circuit_and_labels(void) {
    TACList *list = createTACList();
    TACOperand mainFunc = tacMethod("main", TYPE_INT);
    appendInstruction(list, TAC_BEGIN_FUNC, mainFunc, tacConstantInt(0), tacNone());

    TACOperand t0 = tacTemp("t0", TYPE_BOOL);
    appendInstruction(list, TAC_LOCAL_DECL, t0, tacNone(), tacNone());
    appendInstruction(list, TAC_ASSIGN, t0, tacConstantBool(0), tacNone());

    TACOperand labelL0 = tacLabel("L0");
    appendInstruction(list, TAC_IF_FALSE, tacNone(), t0, labelL0);

    TACOperand labelL1 = tacLabel("L1");
    appendInstruction(list, TAC_GOTO, tacNone(), labelL1, tacNone());

    appendInstruction(list, TAC_LABEL, labelL0, tacNone(), tacNone());
    appendInstruction(list, TAC_RETURN, tacNone(), tacConstantInt(42), tacNone());

    appendInstruction(list, TAC_LABEL, labelL1, tacNone(), tacNone());
    appendInstruction(list, TAC_RETURN, tacNone(), tacConstantInt(0), tacNone());

    appendInstruction(list, TAC_END_FUNC, mainFunc, tacNone(), tacNone());

    resetBuildString();
    generateAssembly(list, OPTIMIZATION_NONE);
    const char *output = getBuiltString();

    TEST_ASSERT_NOT_NULL(strstr(output, "main:"));
    TEST_ASSERT_NOT_NULL(strstr(output, "pushq %rbp"));
    TEST_ASSERT_NOT_NULL(strstr(output, "cmpl $0, %eax"));
    TEST_ASSERT_NOT_NULL(strstr(output, "je .L0"));
    TEST_ASSERT_NOT_NULL(strstr(output, "jmp .L1"));
    TEST_ASSERT_NOT_NULL(strstr(output, ".L0:"));
    TEST_ASSERT_NOT_NULL(strstr(output, ".L1:"));
    TEST_ASSERT_NOT_NULL(strstr(output, ".L_main_epilogue:"));

    freeTACList(list);
}

// Emision completa de bucle while con salto hacia atras
void test_asm_flow_while_loop(void) {
    TACList *list = createTACList();
    TACOperand mainFunc = tacMethod("main", TYPE_INT);
    appendInstruction(list, TAC_BEGIN_FUNC, mainFunc, tacConstantInt(0), tacNone());

    TACOperand i = tacTemp("t0", TYPE_INT);
    appendInstruction(list, TAC_LOCAL_DECL, i, tacNone(), tacNone());
    appendInstruction(list, TAC_ASSIGN, i, tacConstantInt(0), tacNone());

    TACOperand loopStart = tacLabel("L_loop");
    TACOperand loopEnd = tacLabel("L_end");
    appendInstruction(list, TAC_LABEL, loopStart, tacNone(), tacNone());

    TACOperand cmp = tacTemp("t1", TYPE_BOOL);
    appendInstruction(list, TAC_LOCAL_DECL, cmp, tacNone(), tacNone());
    appendInstruction(list, TAC_LT, cmp, i, tacConstantInt(10));
    appendInstruction(list, TAC_IF_FALSE, tacNone(), cmp, loopEnd);

    appendInstruction(list, TAC_ADD, i, i, tacConstantInt(1));
    appendInstruction(list, TAC_GOTO, tacNone(), loopStart, tacNone());

    appendInstruction(list, TAC_LABEL, loopEnd, tacNone(), tacNone());
    appendInstruction(list, TAC_RETURN, tacNone(), i, tacNone());
    appendInstruction(list, TAC_END_FUNC, mainFunc, tacNone(), tacNone());

    resetBuildString();
    generateAssembly(list, OPTIMIZATION_NONE);
    const char *output = getBuiltString();

    TEST_ASSERT_NOT_NULL(strstr(output, ".L_loop:"));
    TEST_ASSERT_NOT_NULL(strstr(output, "cmpl $0, %eax"));
    TEST_ASSERT_NOT_NULL(strstr(output, "je .L_end"));
    TEST_ASSERT_NOT_NULL(strstr(output, "jmp .L_loop"));
    TEST_ASSERT_NOT_NULL(strstr(output, ".L_end:"));

    freeTACList(list);
}

// Emision de llamada con 7 parametros genera relleno de pila subq $8 y pushq
void test_asm_flow_caller_stack_padding(void) {
    TACList *list = createTACList();
    TACOperand mainFunc = tacMethod("main", TYPE_INT);
    appendInstruction(list, TAC_BEGIN_FUNC, mainFunc, tacConstantInt(0), tacNone());

    for (int p = 1; p <= 7; p++) {
        appendInstruction(list, TAC_PARAM, tacNone(), tacConstantInt(p), tacNone());
    }

    TACOperand callee = tacMethod("target_func", TYPE_INT);
    TACOperand res = tacTemp("t0", TYPE_INT);
    appendInstruction(list, TAC_LOCAL_DECL, res, tacNone(), tacNone());
    appendInstruction(list, TAC_CALL, res, callee, tacConstantInt(7));

    appendInstruction(list, TAC_RETURN, tacNone(), res, tacNone());
    appendInstruction(list, TAC_END_FUNC, mainFunc, tacNone(), tacNone());

    resetBuildString();
    generateAssembly(list, OPTIMIZATION_NONE);
    const char *output = getBuiltString();

    TEST_ASSERT_NOT_NULL(strstr(output, "subq $8, %rsp"));
    TEST_ASSERT_NOT_NULL(strstr(output, "pushq %rax"));
    TEST_ASSERT_NOT_NULL(strstr(output, "call target_func"));
    TEST_ASSERT_NOT_NULL(strstr(output, "addq $16, %rsp"));

    freeTACList(list);
}

/* ========================================================================
 * Runner principal de la suite
 * ======================================================================== */

int main(void) {
    UNITY_BEGIN();

    /* Grupo 1: Pruebas estructurales de marco y backend (In-Memory) */
    RUN_TEST(test_asm_frame_allocation_alignment_and_null_safety);
    RUN_TEST(test_asm_sections_gnu_stack);
    RUN_TEST(test_asm_emit_prologue_and_epilogue);
    RUN_TEST(test_asm_strength_reduction_multiplication_by_power_of_two);
    RUN_TEST(test_asm_fused_relational_branch_peephole);
    RUN_TEST(test_asm_strength_reduction_powers_of_two);
    RUN_TEST(test_asm_strength_reduction_commutative);
    RUN_TEST(test_asm_strength_reduction_zero_and_one);
    RUN_TEST(test_asm_strength_reduction_non_power_of_two_preserves_imull);

    /* Grupo 2: Pruebas unitarias de traduccion aislada TAC -> Assembly (generateInstruction) */
    /* Subgrupo 2.1: Asignacion y Aritmetica Entera */
    RUN_TEST(test_asm_unit_tac_assign_int);
    RUN_TEST(test_asm_unit_tac_add_int);
    RUN_TEST(test_asm_unit_tac_sub_int);
    RUN_TEST(test_asm_unit_tac_mul_int);
    RUN_TEST(test_asm_unit_tac_div_int);
    RUN_TEST(test_asm_unit_tac_mod_int);
    RUN_TEST(test_asm_unit_tac_uminus_int);

    /* Subgrupo 2.2: Operaciones Booleanas y Relacionales Enteras */
    RUN_TEST(test_asm_unit_tac_not);
    RUN_TEST(test_asm_unit_tac_lt_int);
    RUN_TEST(test_asm_unit_tac_gt_int);
    RUN_TEST(test_asm_unit_tac_eq_int);

    /* Subgrupo 2.3: Control de Flujo y Saltos */
    RUN_TEST(test_asm_unit_tac_label);
    RUN_TEST(test_asm_unit_tac_goto);
    RUN_TEST(test_asm_unit_tac_if_false);
    RUN_TEST(test_asm_unit_tac_if_true);

    /* Subgrupo 2.4: Funciones, Parametros y Retorno */
    RUN_TEST(test_asm_unit_tac_param_decl_register);
    RUN_TEST(test_asm_unit_tac_param_decl_stack);
    RUN_TEST(test_asm_unit_tac_param_and_call);
    RUN_TEST(test_asm_unit_tac_return_with_expression);
    RUN_TEST(test_asm_unit_tac_return_void);

    /* Grupo 3: Pruebas unitarias de emision de flujo con TAC estructurado (In-Memory) */
    RUN_TEST(test_asm_flow_short_circuit_and_labels);
    RUN_TEST(test_asm_flow_while_loop);
    RUN_TEST(test_asm_flow_caller_stack_padding);

    return UNITY_END();
}
