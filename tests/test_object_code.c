/**
 * @file test_object_code.c
 * @brief Suite de pruebas unitarias y de integracion para codigo objeto (Assembly x86-64).
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

// Emision de secciones .note.GNU-stack y tabla de literales reales en .rodata
void test_asm_sections_gnu_stack_and_float_pool(void) {
    const char *source = "int main() { float f; f = 3.14; return 0; }";
    const char *assembly = compileToAssembly(source, OPTIMIZATION_NONE);
    TEST_ASSERT_NOT_NULL(assembly);
    TEST_ASSERT_NOT_NULL(strstr(assembly, ".section .note.GNU-stack,\"\",@progbits"));
    TEST_ASSERT_NOT_NULL(strstr(assembly, ".section .rodata"));
    TEST_ASSERT_NOT_NULL(strstr(assembly, ".long "));

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
    const char *source = "int main() { int x; x = 5; return x * 4; }";
    const char *assembly = compileToAssembly(source, OPTIMIZATION_STRENGTH);
    TEST_ASSERT_NOT_NULL(assembly);
    TEST_ASSERT_NOT_NULL(strstr(assembly, "sall $2, %eax"));
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

// TAC_ASSIGN float emite movss entre ranuras de memoria XMM
void test_asm_unit_tac_assign_float(void) {
    FunctionFrame *frame = createFrame("test_func");
    TACOperand res = tacTemp("f0", TYPE_FLOAT);
    TACOperand op1 = tacTemp("f1", TYPE_FLOAT);
    addSlot(frame, &res);
    addSlot(frame, &op1);

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

    TEST_ASSERT_NOT_NULL(strstr(asmOutput, "movss -8(%rbp), %xmm0"));
    TEST_ASSERT_NOT_NULL(strstr(asmOutput, "movss %xmm0, -4(%rbp)"));
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
        .result = tacNone(),
        .firstOperand = param,
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
        .result = tacNone(),
        .firstOperand = param,
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

/* --- Subgrupo 2.5: Operaciones de Punto Flotante y Conversion SSE --- */

// TAC_INT_TO_FLOAT emite cvtsi2ss %eax, %xmm0 y almacena en slot flotante
void test_asm_unit_tac_int_to_float(void) {
    FunctionFrame *frame = createFrame("test_func");
    TACOperand res = tacTemp("f0", TYPE_FLOAT);
    TACOperand op1 = tacTemp("t0", TYPE_INT);
    addSlot(frame, &res);
    addSlot(frame, &op1);

    TACInstruction inst = {
        .operation = TAC_INT_TO_FLOAT,
        .result = res,
        .firstOperand = op1,
        .secondOperand = tacNone(),
        .next = NULL
    };

    resetBuildString();
    generateInstruction(&inst, frame);
    const char *asmOutput = getBuiltString();

    TEST_ASSERT_NOT_NULL(strstr(asmOutput, "cvtsi2ss %eax, %xmm0"));
    TEST_ASSERT_NOT_NULL(strstr(asmOutput, "movss %xmm0, -4(%rbp)"));
    freeFrame(frame);
}

// TAC_ADD float emite addss sobre registros XMM
void test_asm_unit_tac_add_float(void) {
    FunctionFrame *frame = createFrame("test_func");
    TACOperand res = tacTemp("f0", TYPE_FLOAT);
    TACOperand op1 = tacTemp("f1", TYPE_FLOAT);
    TACOperand op2 = tacTemp("f2", TYPE_FLOAT);
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

    TEST_ASSERT_NOT_NULL(strstr(asmOutput, "movss -8(%rbp), %xmm0"));
    TEST_ASSERT_NOT_NULL(strstr(asmOutput, "movss -12(%rbp), %xmm1"));
    TEST_ASSERT_NOT_NULL(strstr(asmOutput, "addss %xmm1, %xmm0"));
    TEST_ASSERT_NOT_NULL(strstr(asmOutput, "movss %xmm0, -4(%rbp)"));
    freeFrame(frame);
}

// TAC_SUB float emite subss sobre registros XMM
void test_asm_unit_tac_sub_float(void) {
    FunctionFrame *frame = createFrame("test_func");
    TACOperand res = tacTemp("f0", TYPE_FLOAT);
    TACOperand op1 = tacTemp("f1", TYPE_FLOAT);
    TACOperand op2 = tacTemp("f2", TYPE_FLOAT);
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

    TEST_ASSERT_NOT_NULL(strstr(asmOutput, "subss %xmm1, %xmm0"));
    freeFrame(frame);
}

// TAC_MUL float emite mulss sobre registros XMM
void test_asm_unit_tac_mul_float(void) {
    FunctionFrame *frame = createFrame("test_func");
    TACOperand res = tacTemp("f0", TYPE_FLOAT);
    TACOperand op1 = tacTemp("f1", TYPE_FLOAT);
    TACOperand op2 = tacTemp("f2", TYPE_FLOAT);
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

    TEST_ASSERT_NOT_NULL(strstr(asmOutput, "mulss %xmm1, %xmm0"));
    freeFrame(frame);
}

// TAC_DIV float emite divss sobre registros XMM
void test_asm_unit_tac_div_float(void) {
    FunctionFrame *frame = createFrame("test_func");
    TACOperand res = tacTemp("f0", TYPE_FLOAT);
    TACOperand op1 = tacTemp("f1", TYPE_FLOAT);
    TACOperand op2 = tacTemp("f2", TYPE_FLOAT);
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

    TEST_ASSERT_NOT_NULL(strstr(asmOutput, "divss %xmm1, %xmm0"));
    freeFrame(frame);
}

// TAC_UMINUS float emite pxor y subss
void test_asm_unit_tac_uminus_float(void) {
    FunctionFrame *frame = createFrame("test_func");
    TACOperand res = tacTemp("f0", TYPE_FLOAT);
    TACOperand op1 = tacTemp("f1", TYPE_FLOAT);
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

    TEST_ASSERT_NOT_NULL(strstr(asmOutput, "pxor %xmm0, %xmm0"));
    TEST_ASSERT_NOT_NULL(strstr(asmOutput, "subss %xmm1, %xmm0"));
    freeFrame(frame);
}

// TAC_LT float emite comiss con condicion setb sin signo
void test_asm_unit_tac_lt_float(void) {
    FunctionFrame *frame = createFrame("test_func");
    TACOperand res = tacTemp("b0", TYPE_BOOL);
    TACOperand op1 = tacTemp("f1", TYPE_FLOAT);
    TACOperand op2 = tacTemp("f2", TYPE_FLOAT);
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

    TEST_ASSERT_NOT_NULL(strstr(asmOutput, "comiss %xmm1, %xmm0"));
    TEST_ASSERT_NOT_NULL(strstr(asmOutput, "setb %al"));
    TEST_ASSERT_NOT_NULL(strstr(asmOutput, "movzbl %al, %eax"));
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
    appendInstruction(list, TAC_LOCAL_DECL, tacNone(), t0, tacNone());
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
    appendInstruction(list, TAC_LOCAL_DECL, tacNone(), i, tacNone());
    appendInstruction(list, TAC_ASSIGN, i, tacConstantInt(0), tacNone());

    TACOperand loopStart = tacLabel("L_loop");
    TACOperand loopEnd = tacLabel("L_end");
    appendInstruction(list, TAC_LABEL, loopStart, tacNone(), tacNone());

    TACOperand cmp = tacTemp("t1", TYPE_BOOL);
    appendInstruction(list, TAC_LOCAL_DECL, tacNone(), cmp, tacNone());
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
    appendInstruction(list, TAC_LOCAL_DECL, tacNone(), res, tacNone());
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
 * Grupo 4: Suite de integracion y ejecucion con GCC (Minima y Rigurosa)
 * ======================================================================== */

// Compilacion y ejecucion completa de los programas canonicos examples/factorial y programa_sin_error
void test_asm_exec_canonical_programs(void) {
    TEST_ASSERT_EQUAL_INT(0, compileAndRunFile("examples/factorial.ctds"));
    TEST_ASSERT_EQUAL_INT(0, compileAndRunFile("examples/programa_sin_error.ctds"));
}

// Convencion de llamada y llamadas anidadas exhaustivas (reproduccion y cierre de Bug 1.2 callArgIndex)
void test_asm_exec_calling_convention_and_nested_calls(void) {
    const char *source = "int doble(int x) { return x * 2; }\n"
                         "int triple(int x) { return x * 3; }\n"
                         "int sumar(int a, int b) { return a + b; }\n"
                         "int main() {\n"
                         "    int r1, r2, r3, r4;\n"
                         "    r1 = sumar(3, doble(2));\n"
                         "    r2 = sumar(doble(2), 3);\n"
                         "    r3 = sumar(doble(2), triple(3));\n"
                         "    r4 = doble(doble(doble(2)));\n"
                         "    if (r1 == 7 && r2 == 7 && r3 == 13 && r4 == 16) { return 42; }\n"
                         "    return 0;\n"
                         "}";
    TEST_ASSERT_EQUAL_INT(42, compileAndRun(source));
}

// Pasaje de parametros por pila segun System V AMD64 para enteros (>6) y flotantes (>8)
void test_asm_exec_system_v_stack_parameters(void) {
    const char *intSource = "int s(int a, int b, int c, int d, int e, int f, int g, int h) {\n"
                            "    return a + b + c + d + e + f + g + h;\n"
                            "}\n"
                            "int main() {\n"
                            "    return s(1, 2, 3, 4, 5, 6, 7, 8); /* 36 */\n"
                            "}";
    TEST_ASSERT_EQUAL_INT(36, compileAndRun(intSource));

    const char *floatSource = "float f(float a, float b, float c, float d, float e, float f1, float g, float h, float i, float j) {\n"
                              "    return a + b + c + d + e + f1 + g + h + i + j;\n"
                              "}\n"
                              "int main() {\n"
                              "    float r;\n"
                              "    r = f(1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0);\n"
                              "    if (r == 55.0) { return 1; }\n"
                              "    return 0;\n"
                              "}";
    TEST_ASSERT_EQUAL_INT(1, compileAndRun(floatSource));
}

// Parametros mezclados de int y float consumen registros de sus respectivas clases
void test_asm_exec_mixed_int_and_float_parameters(void) {
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

// Sombreado de variables y resolucion de ambitos en bloques anidados y parametros
void test_asm_exec_variable_shadowing_and_scopes(void) {
    const char *source = "int g;\n"
                         "int f(int g) { return g + 1; }\n"
                         "int main() {\n"
                         "    int r1, a, x;\n"
                         "    g = 100;\n"
                         "    r1 = f(4);\n"
                         "    a = 1;\n"
                         "    { int b; b = 2; { int c; c = 3; a = a + b + c; } }\n"
                         "    x = 1;\n"
                         "    { int x; x = 2; }\n"
                         "    if (r1 == 5 && a == 6 && x == 1 && g == 100) { return 1; }\n"
                         "    return 0;\n"
                         "}";
    TEST_ASSERT_EQUAL_INT(1, compileAndRun(source));
}

// Division y modulo con signo respetan truncamiento a cero y signo de resto con idivl
void test_asm_exec_runtime_signed_division_and_modulo(void) {
    const char *source = "int main() {\n"
                         "    int d1, m1, d2, m2;\n"
                         "    d1 = -7 / 2;  /* -3 */\n"
                         "    m1 = -7 % 3;  /* -1 */\n"
                         "    d2 = 7 / -2;  /* -3 */\n"
                         "    m2 = 7 % -3;  /* 1 */\n"
                         "    if (d1 == -3 && m1 == -1 && d2 == -3 && m2 == 1) { return 1; }\n"
                         "    return 0;\n"
                         "}";
    TEST_ASSERT_EQUAL_INT(1, compileAndRun(source));
}

// Cortocircuito booleano no ejecuta efectos colaterales y respeta evaluacion izquierda a derecha
void test_asm_exec_runtime_short_circuit_and_eval_order(void) {
    const char *source = "int c;\n"
                         "boolean inc() { c = c + 1; return true; }\n"
                         "int next() { c = c + 1; return c; }\n"
                         "int sub(int a, int b) { return a - b; }\n"
                         "int main() {\n"
                         "    boolean b1, b2, b3, b4;\n"
                         "    int ord;\n"
                         "    c = 0;\n"
                         "    b1 = false && inc();\n"
                         "    b2 = true || inc();\n"
                         "    b3 = true && inc();\n"
                         "    b4 = false || inc();\n"
                         "    ord = sub(next(), next());\n"
                         "    if (c == 4 && ord == -1 && !b1 && b2 && b3 && b4) { return 1; }\n"
                         "    return 0;\n"
                         "}";
    TEST_ASSERT_EQUAL_INT(1, compileAndRun(source));
}

// Aritmetica flotante con SSE, negacion y coercion int a float
void test_asm_exec_runtime_float_sse_and_coercion(void) {
    const char *source = "float half(float x) { return x / 2; }\n"
                         "int two() { return 2; }\n"
                         "int main() {\n"
                         "    float f, r1, r2;\n"
                         "    boolean b;\n"
                         "    f = -2.5;\n"
                         "    f = -f;\n"
                         "    r1 = half(7);\n"
                         "    r2 = two() + 0.5;\n"
                         "    b = (1 < 2.5) && (3.5 > 2);\n"
                         "    if (f == 2.5 && r1 == 3.5 && r2 == 2.5 && b) { return 1; }\n"
                         "    return 0;\n"
                         "}";
    TEST_ASSERT_EQUAL_INT(1, compileAndRun(source));
}

// Doble recursion de fibonacci valida anidamiento de marcos y puntero de pila en ejecucion
void test_asm_exec_recursion_fibonacci(void) {
    const char *source = "int fib(int n) { if (n < 2) { return n; } else { return fib(n - 1) + fib(n - 2); } }\n"
                         "int main() { return fib(10); }";
    TEST_ASSERT_EQUAL_INT(55, compileAndRun(source));
}

// Bucle while con if/else anidados y salida anticipada por return
void test_asm_exec_while_with_nested_if_and_return(void) {
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

// E21: 70 argumentos salientes acumulados con pasaje por pila
void test_asm_exec_limit_70_arguments(void) {
    char *source = generate70ParamsProgram();
    int result = compileAndRun(source);
    free(source);
    TEST_ASSERT_EQUAL_INT(70, result);
}

// E22: 300 constantes reales distintas conservan su valor en .rodata
void test_asm_exec_limit_300_float_constants(void) {
    char *source = generate300FloatConstantsProgram();
    int result = compileAndRun(source);
    free(source);
    TEST_ASSERT_EQUAL_INT(1, result);
}

// E23: 300 variables globales con almacenamiento estatico
void test_asm_exec_limit_300_global_variables(void) {
    char *source = generate300GlobalVariablesProgram();
    int result = compileAndRun(source);
    free(source);
    TEST_ASSERT_EQUAL_INT(7, result);
}

// E24: Comparacion de igualdad entre reales con NaN debe ser falsa segun IEEE
void test_asm_exec_float_nan_equality(void) {
    const char *source = "int main() { float z; float n; z = 0.0; n = z / z; if (n == n) { return 1; } return 0; }";
    TEST_ASSERT_EQUAL_INT(0, compileAndRun(source));
}

// E41: Comparacion menor con NaN en reales debe ser falsa segun IEEE
void test_asm_exec_float_nan_less_than(void) {
    const char *source = "int main() { float z; float n; z = 0.0; n = z / z; if (n < 1.0) { return 1; } return 0; }";
    TEST_ASSERT_EQUAL_INT(0, compileAndRun(source));
}

// E42: Comparacion mayor con NaN en reales es falsa segun IEEE
void test_asm_exec_float_nan_greater_than(void) {
    const char *source = "int main() { float z; float n; z = 0.0; n = z / z; if (n > 1.0) { return 1; } return 0; }";
    TEST_ASSERT_EQUAL_INT(0, compileAndRun(source));
}

/* ========================================================================
 * Runner principal de la suite
 * ======================================================================== */

int main(void) {
    UNITY_BEGIN();

    /* Grupo 1: Pruebas estructurales de marco y backend (In-Memory) */
    RUN_TEST(test_asm_frame_allocation_alignment_and_null_safety);
    RUN_TEST(test_asm_sections_gnu_stack_and_float_pool);
    RUN_TEST(test_asm_emit_prologue_and_epilogue);
    RUN_TEST(test_asm_strength_reduction_multiplication_by_power_of_two);

    /* Grupo 2: Pruebas unitarias de traduccion aislada TAC -> Assembly (generateInstruction) */
    /* Subgrupo 2.1: Asignacion y Aritmetica Entera */
    RUN_TEST(test_asm_unit_tac_assign_int);
    RUN_TEST(test_asm_unit_tac_assign_float);
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

    /* Subgrupo 2.5: Operaciones de Punto Flotante y Conversion SSE */
    RUN_TEST(test_asm_unit_tac_int_to_float);
    RUN_TEST(test_asm_unit_tac_add_float);
    RUN_TEST(test_asm_unit_tac_sub_float);
    RUN_TEST(test_asm_unit_tac_mul_float);
    RUN_TEST(test_asm_unit_tac_div_float);
    RUN_TEST(test_asm_unit_tac_uminus_float);
    RUN_TEST(test_asm_unit_tac_lt_float);

    /* Grupo 3: Pruebas unitarias de emision de flujo con TAC estructurado (In-Memory) */
    RUN_TEST(test_asm_flow_short_circuit_and_labels);
    RUN_TEST(test_asm_flow_while_loop);
    RUN_TEST(test_asm_flow_caller_stack_padding);

    /* Grupo 4: Suite de integracion y ejecucion con GCC (Minima y Rigurosa) */
    RUN_TEST(test_asm_exec_canonical_programs);
    RUN_TEST(test_asm_exec_calling_convention_and_nested_calls);
    RUN_TEST(test_asm_exec_system_v_stack_parameters);
    RUN_TEST(test_asm_exec_mixed_int_and_float_parameters);
    RUN_TEST(test_asm_exec_variable_shadowing_and_scopes);
    RUN_TEST(test_asm_exec_runtime_signed_division_and_modulo);
    RUN_TEST(test_asm_exec_runtime_short_circuit_and_eval_order);
    RUN_TEST(test_asm_exec_runtime_float_sse_and_coercion);
    RUN_TEST(test_asm_exec_recursion_fibonacci);
    RUN_TEST(test_asm_exec_while_with_nested_if_and_return);
    RUN_TEST(test_asm_exec_limit_70_arguments);
    RUN_TEST(test_asm_exec_limit_300_float_constants);
    RUN_TEST(test_asm_exec_limit_300_global_variables);
    RUN_TEST(test_asm_exec_float_nan_equality);
    RUN_TEST(test_asm_exec_float_nan_less_than);
    RUN_TEST(test_asm_exec_float_nan_greater_than);

    return UNITY_END();
}
