/**
 * @file test_tac.c
 * @brief Pruebas unitarias de la generación de Código de Tres Direcciones (TAC).
 *
 * Valida la traducción completa del AST a instrucciones intermedias TAC:
 *  - Expresiones aritméticas y precedencia de operadores (* antes que +).
 *  - Expresiones unarias (negación aritmética y lógica).
 *  - Operaciones relacionales y de igualdad.
 *  - Lógica de cortocircuito (&& y ||) con saltos condicionales y etiquetas.
 *  - Estructuras de control (if, if-else, while y bucles anidados).
 *  - Declaraciones de variables globales y locales en segmentos/marcos respectivos.
 *  - Sombreado de variables diferenciado por uids calificados (@uid).
 *  - Definición de métodos, lista de parámetros formales y retorno implícito/explícito.
 *  - Llamadas a métodos con pasaje ordenado de argumentos (param y call).
 *  - Coerciones de tipos implícitas (int -> float) en asignaciones, retornos, expresiones y llamadas.
 *  - Instantáneas canónicas exactas de programas completos (.ci).
 */

#include "unity.h"
#include "ast.h"
#include "symbol_table.h"
#include "semantic_analyzer.h"
#include "intermediate_code.h"
#include "tac_list.h"
#include "tac_instruction.h"
#include "tac_operand.h"
#include "tac_operation.h"
#include "string_builder.h"
#include "error_handler.h"
#include "types.h"
#include <stdbool.h>
#include <stdio.h>
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

/* ------------------------------------------------------------------------
 * Funciones auxiliares del arnés de pruebas
 * ------------------------------------------------------------------------ */

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

static const char *compileToTACText(const char *source) {
    const TACList *list = compileToTAC(source);
    if (list == NULL) {
        return NULL;
    }

    resetBuildString();
    formatTACList(list);
    return getBuiltString();
}

static const TACInstruction *findInstruction(const TACList *list, TACOperation op, int occurrence) {
    if (list == NULL) return NULL;
    int current = 0;
    for (const TACInstruction *inst = list->head; inst != NULL; inst = inst->next) {
        if (inst->operation == op) {
            if (current == occurrence) {
                return inst;
            }
            current++;
        }
    }
    return NULL;
}

static int countInstructions(const TACList *list, TACOperation op) {
    if (list == NULL) return 0;
    int count = 0;
    for (const TACInstruction *inst = list->head; inst != NULL; inst = inst->next) {
        if (inst->operation == op) {
            count++;
        }
    }
    return count;
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
    freeAST(astRoot);
    astRoot = NULL;
}

/* ------------------------------------------------------------------------
 * Grupo 1: Expresiones Aritméticas, Unarias y Precedencia
 * ------------------------------------------------------------------------ */

// Suma binaria entera genera instrucción TAC_ADD asignando el resultado a un nuevo temporal
void test_tac_binary_addition(void) {
    const char *source = "void main() { int a; int b; int c; a = 1; b = 2; c = a + b; }";
    const TACList *list = compileToTAC(source);
    TEST_ASSERT_NOT_NULL(list);

    const TACInstruction *addInst = findInstruction(list, TAC_ADD, 0);
    TEST_ASSERT_NOT_NULL(addInst);
    TEST_ASSERT_EQUAL(OPERAND_TEMP, addInst->result.kind);
    TEST_ASSERT_EQUAL_STRING("t0", addInst->result.name);
    TEST_ASSERT_EQUAL(OPERAND_VARIABLE, addInst->firstOperand.kind);
    TEST_ASSERT_EQUAL_STRING("a@2", addInst->firstOperand.name);
    TEST_ASSERT_EQUAL(OPERAND_VARIABLE, addInst->secondOperand.kind);
    TEST_ASSERT_EQUAL_STRING("b@3", addInst->secondOperand.name);
}

// Resta binaria entera genera instrucción TAC_SUB con temporal de destino
void test_tac_binary_subtraction(void) {
    const char *source = "void main() { int a; int b; int c; a = 5; b = 2; c = a - b; }";
    const TACList *list = compileToTAC(source);
    TEST_ASSERT_NOT_NULL(list);

    const TACInstruction *subInst = findInstruction(list, TAC_SUB, 0);
    TEST_ASSERT_NOT_NULL(subInst);
    TEST_ASSERT_EQUAL(OPERAND_TEMP, subInst->result.kind);
    TEST_ASSERT_EQUAL_STRING("t0", subInst->result.name);
}

// Multiplicación binaria genera instrucción TAC_MUL
void test_tac_binary_multiplication(void) {
    const char *source = "void main() { int a; int b; int c; a = 3; b = 4; c = a * b; }";
    const TACList *list = compileToTAC(source);
    TEST_ASSERT_NOT_NULL(list);

    const TACInstruction *mulInst = findInstruction(list, TAC_MUL, 0);
    TEST_ASSERT_NOT_NULL(mulInst);
    TEST_ASSERT_EQUAL(OPERAND_TEMP, mulInst->result.kind);
    TEST_ASSERT_EQUAL_STRING("t0", mulInst->result.name);
}

// División binaria entera genera instrucción TAC_DIV
void test_tac_binary_division(void) {
    const char *source = "void main() { int a; int b; int c; a = 10; b = 2; c = a / b; }";
    const TACList *list = compileToTAC(source);
    TEST_ASSERT_NOT_NULL(list);

    const TACInstruction *divInst = findInstruction(list, TAC_DIV, 0);
    TEST_ASSERT_NOT_NULL(divInst);
    TEST_ASSERT_EQUAL(OPERAND_TEMP, divInst->result.kind);
    TEST_ASSERT_EQUAL_STRING("t0", divInst->result.name);
}

// Operación módulo entera genera instrucción TAC_MOD
void test_tac_binary_modulo(void) {
    const char *source = "void main() { int a; int b; int c; a = 10; b = 3; c = a % b; }";
    const TACList *list = compileToTAC(source);
    TEST_ASSERT_NOT_NULL(list);

    const TACInstruction *modInst = findInstruction(list, TAC_MOD, 0);
    TEST_ASSERT_NOT_NULL(modInst);
    TEST_ASSERT_EQUAL(OPERAND_TEMP, modInst->result.kind);
    TEST_ASSERT_EQUAL_STRING("t0", modInst->result.name);
}

// Negación aritmética unaria genera instrucción TAC_UMINUS con segundo operando nulo
void test_tac_unary_minus(void) {
    const char *source = "void main() { int a; int b; a = 5; b = -a; }";
    const TACList *list = compileToTAC(source);
    TEST_ASSERT_NOT_NULL(list);

    const TACInstruction *negInst = findInstruction(list, TAC_UMINUS, 0);
    TEST_ASSERT_NOT_NULL(negInst);
    TEST_ASSERT_EQUAL(OPERAND_TEMP, negInst->result.kind);
    TEST_ASSERT_EQUAL_STRING("t0", negInst->result.name);
    TEST_ASSERT_EQUAL(OPERAND_VARIABLE, negInst->firstOperand.kind);
    TEST_ASSERT_EQUAL_STRING("a@2", negInst->firstOperand.name);
    TEST_ASSERT_EQUAL(OPERAND_NONE, negInst->secondOperand.kind);
}

// Negación lógica unaria genera instrucción TAC_NOT con operando booleano
void test_tac_unary_not(void) {
    const char *source = "void main() { boolean a; boolean b; a = true; b = !a; }";
    const TACList *list = compileToTAC(source);
    TEST_ASSERT_NOT_NULL(list);

    const TACInstruction *notInst = findInstruction(list, TAC_NOT, 0);
    TEST_ASSERT_NOT_NULL(notInst);
    TEST_ASSERT_EQUAL(OPERAND_TEMP, notInst->result.kind);
    TEST_ASSERT_EQUAL_STRING("t0", notInst->result.name);
    TEST_ASSERT_EQUAL(OPERAND_VARIABLE, notInst->firstOperand.kind);
    TEST_ASSERT_EQUAL_STRING("a@2", notInst->firstOperand.name);
    TEST_ASSERT_EQUAL(OPERAND_NONE, notInst->secondOperand.kind);
}

// Precedencia aritmética: multiplicación se emite antes que la suma
void test_tac_operator_precedence_multiplication_before_addition(void) {
    const char *source = "void main() { int a; int b; int c; a = 1; b = 2; c = a + b * 3; }";
    const TACList *list = compileToTAC(source);
    TEST_ASSERT_NOT_NULL(list);

    const TACInstruction *mulInst = findInstruction(list, TAC_MUL, 0);
    const TACInstruction *addInst = findInstruction(list, TAC_ADD, 0);
    TEST_ASSERT_NOT_NULL(mulInst);
    TEST_ASSERT_NOT_NULL(addInst);

    // En intermediate_code.c el temporal de + (t0) se reserva tras evaluar su hijo izquierdo (a),
    // y luego el subárbol derecho (*) reserva t1 y se emite primero: t1 = b@3 * 3
    TEST_ASSERT_EQUAL_STRING("t1", mulInst->result.name);
    TEST_ASSERT_EQUAL_STRING("b@3", mulInst->firstOperand.name);
    TEST_ASSERT_EQUAL_STRING("3", mulInst->secondOperand.name);

    // addInst calcula t0 = a@2 + t1
    TEST_ASSERT_EQUAL_STRING("t0", addInst->result.name);
    TEST_ASSERT_EQUAL_STRING("a@2", addInst->firstOperand.name);
    TEST_ASSERT_EQUAL_STRING("t1", addInst->secondOperand.name);

    // mulInst debe preceder en orden a addInst en la lista
    bool mulBeforeAdd = false;
    for (const TACInstruction *inst = list->head; inst != NULL; inst = inst->next) {
        if (inst == mulInst) mulBeforeAdd = true;
        if (inst == addInst) {
            TEST_ASSERT_TRUE(mulBeforeAdd);
            break;
        }
    }
}

// Expresión compuesta con subárboles genera temporales secuenciales t0, t1 y t2
void test_tac_complex_expression_temporaries_sequential(void) {
    const char *source = "void main() { int a; int b; int c; int d; int r; a = 1; b = 2; c = 3; d = 4; r = (a + b) * (c - d); }";
    const TACList *list = compileToTAC(source);
    TEST_ASSERT_NOT_NULL(list);

    const TACInstruction *addInst = findInstruction(list, TAC_ADD, 0);
    const TACInstruction *subInst = findInstruction(list, TAC_SUB, 0);
    const TACInstruction *mulInst = findInstruction(list, TAC_MUL, 0);

    TEST_ASSERT_NOT_NULL(addInst);
    TEST_ASSERT_NOT_NULL(subInst);
    TEST_ASSERT_NOT_NULL(mulInst);

    // t0 para la suma izquierda (a + b)
    TEST_ASSERT_EQUAL_STRING("t0", addInst->result.name);
    // t1 reservado para la multiplicacion raiz
    TEST_ASSERT_EQUAL_STRING("t1", mulInst->result.name);
    // t2 reservado para la resta derecha (c - d)
    TEST_ASSERT_EQUAL_STRING("t2", subInst->result.name);

    // La multiplicacion une los dos temporales
    TEST_ASSERT_EQUAL_STRING("t0", mulInst->firstOperand.name);
    TEST_ASSERT_EQUAL_STRING("t2", mulInst->secondOperand.name);
}

/* ------------------------------------------------------------------------
 * Grupo 2: Operaciones Relacionales y de Igualdad
 * ------------------------------------------------------------------------ */

// Comparación menor que genera instrucción TAC_LT con resultado booleano
void test_tac_relational_less_than(void) {
    const char *source = "void main() { int a; int b; boolean r; a = 1; b = 2; r = a < b; }";
    const TACList *list = compileToTAC(source);
    TEST_ASSERT_NOT_NULL(list);

    const TACInstruction *ltInst = findInstruction(list, TAC_LT, 0);
    TEST_ASSERT_NOT_NULL(ltInst);
    TEST_ASSERT_EQUAL(TYPE_BOOL, ltInst->result.type);
    TEST_ASSERT_EQUAL_STRING("t0", ltInst->result.name);
    TEST_ASSERT_EQUAL_STRING("a@2", ltInst->firstOperand.name);
    TEST_ASSERT_EQUAL_STRING("b@3", ltInst->secondOperand.name);
}

// Comparación mayor que genera instrucción TAC_GT con resultado booleano
void test_tac_relational_greater_than(void) {
    const char *source = "void main() { int a; int b; boolean r; a = 2; b = 1; r = a > b; }";
    const TACList *list = compileToTAC(source);
    TEST_ASSERT_NOT_NULL(list);

    const TACInstruction *gtInst = findInstruction(list, TAC_GT, 0);
    TEST_ASSERT_NOT_NULL(gtInst);
    TEST_ASSERT_EQUAL(TYPE_BOOL, gtInst->result.type);
    TEST_ASSERT_EQUAL_STRING("t0", gtInst->result.name);
}

// Comparación de igualdad genera instrucción TAC_EQ con resultado booleano
void test_tac_relational_equality(void) {
    const char *source = "void main() { int a; int b; boolean r; a = 1; b = 1; r = a == b; }";
    const TACList *list = compileToTAC(source);
    TEST_ASSERT_NOT_NULL(list);

    const TACInstruction *eqInst = findInstruction(list, TAC_EQ, 0);
    TEST_ASSERT_NOT_NULL(eqInst);
    TEST_ASSERT_EQUAL(TYPE_BOOL, eqInst->result.type);
    TEST_ASSERT_EQUAL_STRING("t0", eqInst->result.name);
}

/* ------------------------------------------------------------------------
 * Grupo 3: Lógica de Cortocircuito (&&, ||)
 * ------------------------------------------------------------------------ */

// Operador lógico && genera salto condicional if_false hacia etiqueta de salida
void test_tac_short_circuit_and(void) {
    const char *source = "void main() { boolean a; boolean b; boolean r; a = true; b = false; r = a && b; }";
    const TACList *list = compileToTAC(source);
    TEST_ASSERT_NOT_NULL(list);

    // a && b debe emitir:
    // t0 = a@2
    // if t0 false goto L0
    // t0 = b@3
    // L0:
    const TACInstruction *ifFalseInst = findInstruction(list, TAC_IF_FALSE, 0);
    TEST_ASSERT_NOT_NULL(ifFalseInst);
    TEST_ASSERT_EQUAL_STRING("t0", ifFalseInst->firstOperand.name);
    TEST_ASSERT_EQUAL_STRING("L0", ifFalseInst->secondOperand.name);

    const TACInstruction *labelInst = findInstruction(list, TAC_LABEL, 0);
    TEST_ASSERT_NOT_NULL(labelInst);
    TEST_ASSERT_EQUAL_STRING("L0", labelInst->result.name);
}

// Operador lógico || genera salto condicional if_true hacia etiqueta de salida
void test_tac_short_circuit_or(void) {
    const char *source = "void main() { boolean a; boolean b; boolean r; a = false; b = true; r = a || b; }";
    const TACList *list = compileToTAC(source);
    TEST_ASSERT_NOT_NULL(list);

    // a || b debe emitir:
    // t0 = a@2
    // if t0 true goto L0
    // t0 = b@3
    // L0:
    const TACInstruction *ifTrueInst = findInstruction(list, TAC_IF_TRUE, 0);
    TEST_ASSERT_NOT_NULL(ifTrueInst);
    TEST_ASSERT_EQUAL_STRING("t0", ifTrueInst->firstOperand.name);
    TEST_ASSERT_EQUAL_STRING("L0", ifTrueInst->secondOperand.name);

    const TACInstruction *labelInst = findInstruction(list, TAC_LABEL, 0);
    TEST_ASSERT_NOT_NULL(labelInst);
    TEST_ASSERT_EQUAL_STRING("L0", labelInst->result.name);
}

// Cortocircuito omite llamada a método derecho cuando el operando izquierdo determina el resultado
void test_tac_short_circuit_skips_call_when_first_operand_determines_result(void) {
    const char *source =
        "boolean f() { return true; }\n"
        "void main() { boolean a; a = false && f(); }\n";
    const TACList *list = compileToTAC(source);
    TEST_ASSERT_NOT_NULL(list);

    const TACInstruction *ifFalseInst = findInstruction(list, TAC_IF_FALSE, 0);
    const TACInstruction *callInst = findInstruction(list, TAC_CALL, 0);
    TEST_ASSERT_NOT_NULL(ifFalseInst);
    TEST_ASSERT_NOT_NULL(callInst);

    // ifFalse debe preceder en el código al call f
    bool jumpBeforeCall = false;
    for (const TACInstruction *inst = list->head; inst != NULL; inst = inst->next) {
        if (inst == ifFalseInst) jumpBeforeCall = true;
        if (inst == callInst) {
            TEST_ASSERT_TRUE(jumpBeforeCall);
            break;
        }
    }
}

// Operaciones de cortocircuito encadenadas generan etiquetas y saltos condicionales sucesivos
void test_tac_short_circuit_chained(void) {
    const char *source = "void main() { boolean a; boolean b; boolean c; boolean r; a = true; b = true; c = false; r = a && b && c; }";
    const TACList *list = compileToTAC(source);
    TEST_ASSERT_NOT_NULL(list);

    // Dos operaciones && encadenadas generan 2 saltos condicionales if_false y 2 etiquetas
    TEST_ASSERT_EQUAL_INT(2, countInstructions(list, TAC_IF_FALSE));
    TEST_ASSERT_EQUAL_INT(2, countInstructions(list, TAC_LABEL));
}

/* ------------------------------------------------------------------------
 * Grupo 4: Estructuras de Control (If, Else, While)
 * ------------------------------------------------------------------------ */

// Sentencia if simple emite condición, salto if_false y etiqueta de cierre sin goto
void test_tac_if_without_else(void) {
    const char *source = "void main() { int x; x = 0; if (x < 1) { x = 2; } }";
    const TACList *list = compileToTAC(source);
    TEST_ASSERT_NOT_NULL(list);

    // Estructura esperada:
    // t0 = x@2 < 1
    // if t0 false goto L0
    // x@2 = 2
    // L0:
    const TACInstruction *ifFalseInst = findInstruction(list, TAC_IF_FALSE, 0);
    TEST_ASSERT_NOT_NULL(ifFalseInst);
    TEST_ASSERT_EQUAL_STRING("t0", ifFalseInst->firstOperand.name);
    TEST_ASSERT_EQUAL_STRING("L0", ifFalseInst->secondOperand.name);

    const TACInstruction *labelInst = findInstruction(list, TAC_LABEL, 0);
    TEST_ASSERT_NOT_NULL(labelInst);
    TEST_ASSERT_EQUAL_STRING("L0", labelInst->result.name);

    // Sin else no debe haber TAC_GOTO
    TEST_ASSERT_EQUAL_INT(0, countInstructions(list, TAC_GOTO));
}

// Sentencia if-else emite bifurcación condicional, salto incondicional goto y etiquetas correspondientes
void test_tac_if_with_else(void) {
    const char *source = "void main() { int x; x = 0; if (x < 1) { x = 2; } else { x = 3; } }";
    const TACList *list = compileToTAC(source);
    TEST_ASSERT_NOT_NULL(list);

    // Estructura esperada:
    // t0 = x@2 < 1
    // if t0 false goto L0
    // x@2 = 2
    // goto L1
    // L0:
    // x@2 = 3
    // L1:
    const TACInstruction *ifFalseInst = findInstruction(list, TAC_IF_FALSE, 0);
    const TACInstruction *gotoInst = findInstruction(list, TAC_GOTO, 0);
    TEST_ASSERT_NOT_NULL(ifFalseInst);
    TEST_ASSERT_NOT_NULL(gotoInst);
    TEST_ASSERT_EQUAL_STRING("L0", ifFalseInst->secondOperand.name);
    TEST_ASSERT_EQUAL_STRING("L1", gotoInst->firstOperand.name);

    TEST_ASSERT_EQUAL_INT(2, countInstructions(list, TAC_LABEL));
}

// Bucle while emite etiqueta de inicio, evaluación de condición, salto if_false y goto de retorno
void test_tac_while_loop(void) {
    const char *source = "void main() { int i; i = 0; while (i < 5) { i = i + 1; } }";
    const TACList *list = compileToTAC(source);
    TEST_ASSERT_NOT_NULL(list);

    // Estructura esperada:
    // L0:
    // t0 = i@2 < 5
    // if t0 false goto L1
    // t1 = i@2 + 1
    // i@2 = t1
    // goto L0
    // L1:
    const TACInstruction *startLabel = findInstruction(list, TAC_LABEL, 0);
    const TACInstruction *endLabel = findInstruction(list, TAC_LABEL, 1);
    const TACInstruction *ifFalseInst = findInstruction(list, TAC_IF_FALSE, 0);
    const TACInstruction *gotoInst = findInstruction(list, TAC_GOTO, 0);

    TEST_ASSERT_NOT_NULL(startLabel);
    TEST_ASSERT_NOT_NULL(endLabel);
    TEST_ASSERT_NOT_NULL(ifFalseInst);
    TEST_ASSERT_NOT_NULL(gotoInst);

    TEST_ASSERT_EQUAL_STRING("L0", startLabel->result.name);
    TEST_ASSERT_EQUAL_STRING("L1", endLabel->result.name);
    TEST_ASSERT_EQUAL_STRING("L1", ifFalseInst->secondOperand.name);
    TEST_ASSERT_EQUAL_STRING("L0", gotoInst->firstOperand.name);
}

// Bucles while anidados generan pares de etiquetas independientes y saltos debidamente anidados
void test_tac_nested_while_loops(void) {
    const char *source =
        "void main() {\n"
        "  int i; int j;\n"
        "  i = 0;\n"
        "  while (i < 3) {\n"
        "    j = 0;\n"
        "    while (j < 3) { j = j + 1; }\n"
        "    i = i + 1;\n"
        "  }\n"
        "}\n";
    const TACList *list = compileToTAC(source);
    TEST_ASSERT_NOT_NULL(list);

    // Dos bucles while anidados generan 4 etiquetas (inicio y fin para cada uno)
    TEST_ASSERT_EQUAL_INT(4, countInstructions(list, TAC_LABEL));
    TEST_ASSERT_EQUAL_INT(2, countInstructions(list, TAC_GOTO));
    TEST_ASSERT_EQUAL_INT(2, countInstructions(list, TAC_IF_FALSE));
}

/* ------------------------------------------------------------------------
 * Grupo 5: Declaraciones y Ámbitos de Variables (Global, Local, Shadowing)
 * ------------------------------------------------------------------------ */

// Variables globales emiten instrucciones TAC_GLOBAL_DECL con sus tipos respectivos
void test_tac_global_variable_declarations(void) {
    const char *source = "int g; float h; boolean b; void main() { }";
    const TACList *list = compileToTAC(source);
    TEST_ASSERT_NOT_NULL(list);

    TEST_ASSERT_EQUAL_INT(3, countInstructions(list, TAC_GLOBAL_DECL));

    const TACInstruction *g1 = findInstruction(list, TAC_GLOBAL_DECL, 0);
    const TACInstruction *g2 = findInstruction(list, TAC_GLOBAL_DECL, 1);
    const TACInstruction *g3 = findInstruction(list, TAC_GLOBAL_DECL, 2);

    TEST_ASSERT_EQUAL_STRING("g@1", g1->firstOperand.name);
    TEST_ASSERT_EQUAL(TYPE_INT, g1->firstOperand.type);

    TEST_ASSERT_EQUAL_STRING("h@2", g2->firstOperand.name);
    TEST_ASSERT_EQUAL(TYPE_FLOAT, g2->firstOperand.type);

    TEST_ASSERT_EQUAL_STRING("b@3", g3->firstOperand.name);
    TEST_ASSERT_EQUAL(TYPE_BOOL, g3->firstOperand.type);
}

// Variables locales dentro del cuerpo de un método emiten instrucciones TAC_LOCAL_DECL
void test_tac_local_variable_declarations(void) {
    const char *source = "void main() { int x; float y; boolean z; }";
    const TACList *list = compileToTAC(source);
    TEST_ASSERT_NOT_NULL(list);

    TEST_ASSERT_EQUAL_INT(3, countInstructions(list, TAC_LOCAL_DECL));

    const TACInstruction *l1 = findInstruction(list, TAC_LOCAL_DECL, 0);
    const TACInstruction *l2 = findInstruction(list, TAC_LOCAL_DECL, 1);
    const TACInstruction *l3 = findInstruction(list, TAC_LOCAL_DECL, 2);

    TEST_ASSERT_EQUAL(TYPE_INT, l1->firstOperand.type);
    TEST_ASSERT_EQUAL(TYPE_FLOAT, l2->firstOperand.type);
    TEST_ASSERT_EQUAL(TYPE_BOOL, l3->firstOperand.type);
}

// Sombreado de variable local sobre global genera uids calificados distintos (@1 vs @3)
void test_tac_variable_shadowing_distinguishes_uids(void) {
    const char *source =
        "int x;\n"
        "void main() {\n"
        "  int x;\n"
        "  x = 5;\n"
        "}\n";
    const TACList *list = compileToTAC(source);
    TEST_ASSERT_NOT_NULL(list);

    const TACInstruction *globalDecl = findInstruction(list, TAC_GLOBAL_DECL, 0);
    const TACInstruction *localDecl = findInstruction(list, TAC_LOCAL_DECL, 0);
    const TACInstruction *assignInst = findInstruction(list, TAC_ASSIGN, 0);

    TEST_ASSERT_NOT_NULL(globalDecl);
    TEST_ASSERT_NOT_NULL(localDecl);
    TEST_ASSERT_NOT_NULL(assignInst);

    // Global x@1 vs Local x@3
    TEST_ASSERT_EQUAL_STRING("x@1", globalDecl->firstOperand.name);
    TEST_ASSERT_EQUAL_STRING("x@3", localDecl->firstOperand.name);

    // La asignación interna debe modificar al local x@3
    TEST_ASSERT_EQUAL_STRING("x@3", assignInst->result.name);
}

// Asignación simple emite instrucción TAC_ASSIGN hacia el identificador calificado
void test_tac_assignment_variable(void) {
    const char *source = "void main() { int x; x = 42; }";
    const TACList *list = compileToTAC(source);
    TEST_ASSERT_NOT_NULL(list);

    const TACInstruction *assignInst = findInstruction(list, TAC_ASSIGN, 0);
    TEST_ASSERT_NOT_NULL(assignInst);
    TEST_ASSERT_EQUAL(OPERAND_VARIABLE, assignInst->result.kind);
    TEST_ASSERT_EQUAL_STRING("x@2", assignInst->result.name);
    TEST_ASSERT_EQUAL(OPERAND_CONSTANT, assignInst->firstOperand.kind);
    TEST_ASSERT_EQUAL_STRING("42", assignInst->firstOperand.name);
    TEST_ASSERT_EQUAL_INT(42, assignInst->firstOperand.value.intValue);
}

/* ------------------------------------------------------------------------
 * Grupo 6: Métodos, Parámetros, Llamadas y Retornos
 * ------------------------------------------------------------------------ */

// Declaración de método void sin return explícito emite instrucción ret implícita
void test_tac_method_declaration_and_implicit_void_return(void) {
    const char *source = "void main() { }";
    const TACList *list = compileToTAC(source);
    TEST_ASSERT_NOT_NULL(list);

    const TACInstruction *beginFunc = findInstruction(list, TAC_BEGIN_FUNC, 0);
    const TACInstruction *retInst = findInstruction(list, TAC_RETURN, 0);
    const TACInstruction *endFunc = findInstruction(list, TAC_END_FUNC, 0);

    TEST_ASSERT_NOT_NULL(beginFunc);
    TEST_ASSERT_NOT_NULL(retInst);
    TEST_ASSERT_NOT_NULL(endFunc);

    TEST_ASSERT_EQUAL_STRING("main", beginFunc->result.name);
    TEST_ASSERT_EQUAL_STRING("0", beginFunc->firstOperand.name); // 0 parámetros
    TEST_ASSERT_EQUAL(OPERAND_NONE, retInst->firstOperand.kind); // ret void
    TEST_ASSERT_EQUAL_STRING("main", endFunc->result.name);
}

// Método void con return explícito no genera una instrucción ret implícita duplicada
void test_tac_method_explicit_return_void_has_no_duplicate_ret(void) {
    const char *source = "void main() { return; }";
    const TACList *list = compileToTAC(source);
    TEST_ASSERT_NOT_NULL(list);

    // Solo debe haber un ret, no debe agregarse uno redundante
    TEST_ASSERT_EQUAL_INT(1, countInstructions(list, TAC_RETURN));
}

// Declaración de parámetros formales emite instrucciones TAC_PARAM_DECL en orden de recepción
void test_tac_method_parameters(void) {
    const char *source = "int add(int a, int b) { return a + b; } void main() { }";
    const TACList *list = compileToTAC(source);
    TEST_ASSERT_NOT_NULL(list);

    const TACInstruction *beginFunc = findInstruction(list, TAC_BEGIN_FUNC, 0);
    TEST_ASSERT_NOT_NULL(beginFunc);
    TEST_ASSERT_EQUAL_STRING("add", beginFunc->result.name);
    TEST_ASSERT_EQUAL_STRING("2", beginFunc->firstOperand.name); // 2 parámetros

    TEST_ASSERT_EQUAL_INT(2, countInstructions(list, TAC_PARAM_DECL));
    const TACInstruction *p1 = findInstruction(list, TAC_PARAM_DECL, 0);
    const TACInstruction *p2 = findInstruction(list, TAC_PARAM_DECL, 1);

    TEST_ASSERT_EQUAL_STRING("a@2", p1->firstOperand.name);
    TEST_ASSERT_EQUAL_STRING("b@3", p2->firstOperand.name);
}

// Sentencia de llamada a método void emite paso de argumentos param y call sin temporal de destino
void test_tac_method_call_statement_void(void) {
    const char *source = "void proc(int x) { } void main() { proc(10); }";
    const TACList *list = compileToTAC(source);
    TEST_ASSERT_NOT_NULL(list);

    const TACInstruction *paramInst = findInstruction(list, TAC_PARAM, 0);
    const TACInstruction *callInst = findInstruction(list, TAC_CALL, 0);

    TEST_ASSERT_NOT_NULL(paramInst);
    TEST_ASSERT_NOT_NULL(callInst);

    TEST_ASSERT_EQUAL_STRING("10", paramInst->firstOperand.name);
    TEST_ASSERT_EQUAL(OPERAND_NONE, callInst->result.kind); // No asigna a temporal
    TEST_ASSERT_EQUAL_STRING("proc", callInst->firstOperand.name);
    TEST_ASSERT_EQUAL_STRING("1", callInst->secondOperand.name); // 1 argumento
}

// Llamada a método con retorno en expresión asigna el resultado de la llamada a un temporal
void test_tac_method_call_expression_with_return_value(void) {
    const char *source = "int f(int a) { return a + 1; } void main() { int x; x = f(2); }";
    const TACList *list = compileToTAC(source);
    TEST_ASSERT_NOT_NULL(list);

    const TACInstruction *callInst = findInstruction(list, TAC_CALL, 0);
    TEST_ASSERT_NOT_NULL(callInst);
    TEST_ASSERT_EQUAL(OPERAND_TEMP, callInst->result.kind);
    TEST_ASSERT_EQUAL_STRING("t1", callInst->result.name);
    TEST_ASSERT_EQUAL_STRING("f", callInst->firstOperand.name);

    // Asignación de x@4 = t1
    const TACInstruction *assignInst = findInstruction(list, TAC_ASSIGN, 0);
    TEST_ASSERT_NOT_NULL(assignInst);
    TEST_ASSERT_EQUAL_STRING("x@4", assignInst->result.name);
    TEST_ASSERT_EQUAL_STRING("t1", assignInst->firstOperand.name);
}

// Múltiples argumentos en una llamada a método se emiten en orden de izquierda a derecha
void test_tac_method_call_arguments_left_to_right(void) {
    const char *source = "int diff(int a, int b) { return a - b; } void main() { int x; x = diff(10, 3); }";
    const TACList *list = compileToTAC(source);
    TEST_ASSERT_NOT_NULL(list);

    const TACInstruction *p1 = findInstruction(list, TAC_PARAM, 0);
    const TACInstruction *p2 = findInstruction(list, TAC_PARAM, 1);

    TEST_ASSERT_NOT_NULL(p1);
    TEST_ASSERT_NOT_NULL(p2);
    TEST_ASSERT_EQUAL_STRING("10", p1->firstOperand.name);
    TEST_ASSERT_EQUAL_STRING("3", p2->firstOperand.name);
}

/* ------------------------------------------------------------------------
 * Grupo 7: Coerciones de Tipos (int -> float)
 * ------------------------------------------------------------------------ */

// Asignación de entero a variable real emite instrucción explícita int_to_float
void test_tac_coercion_on_assignment(void) {
    const char *source = "void main() { float f; f = 1; }";
    const TACList *list = compileToTAC(source);
    TEST_ASSERT_NOT_NULL(list);

    const TACInstruction *coerceInst = findInstruction(list, TAC_INT_TO_FLOAT, 0);
    TEST_ASSERT_NOT_NULL(coerceInst);
    TEST_ASSERT_EQUAL_STRING("t0", coerceInst->result.name);
    TEST_ASSERT_EQUAL_STRING("1", coerceInst->firstOperand.name);

    const TACInstruction *assignInst = findInstruction(list, TAC_ASSIGN, 0);
    TEST_ASSERT_NOT_NULL(assignInst);
    TEST_ASSERT_EQUAL_STRING("f@2", assignInst->result.name);
    TEST_ASSERT_EQUAL_STRING("t0", assignInst->firstOperand.name);
}

// Expresión binaria mixta con entero a la izquierda promociona dicho operando a real con int_to_float
void test_tac_coercion_in_binary_op_left(void) {
    const char *source = "void main() { float f; f = 1 + 2.5; }";
    const TACList *list = compileToTAC(source);
    TEST_ASSERT_NOT_NULL(list);

    const TACInstruction *coerceInst = findInstruction(list, TAC_INT_TO_FLOAT, 0);
    TEST_ASSERT_NOT_NULL(coerceInst);
    TEST_ASSERT_EQUAL_STRING("1", coerceInst->firstOperand.name);

    const TACInstruction *addInst = findInstruction(list, TAC_ADD, 0);
    TEST_ASSERT_NOT_NULL(addInst);
    TEST_ASSERT_EQUAL(TYPE_FLOAT, addInst->result.type);
}

// Expresión binaria mixta con entero a la derecha promociona dicho operando a real con int_to_float
void test_tac_coercion_in_binary_op_right(void) {
    const char *source = "void main() { float f; f = 2.5 + 1; }";
    const TACList *list = compileToTAC(source);
    TEST_ASSERT_NOT_NULL(list);

    const TACInstruction *coerceInst = findInstruction(list, TAC_INT_TO_FLOAT, 0);
    TEST_ASSERT_NOT_NULL(coerceInst);
    TEST_ASSERT_EQUAL_STRING("1", coerceInst->firstOperand.name);

    const TACInstruction *addInst = findInstruction(list, TAC_ADD, 0);
    TEST_ASSERT_NOT_NULL(addInst);
    TEST_ASSERT_EQUAL(TYPE_FLOAT, addInst->result.type);
}

// Retorno de valor entero en método con tipo de retorno float emite coerción int_to_float
void test_tac_coercion_on_return(void) {
    const char *source = "float f() { return 1; } void main() { }";
    const TACList *list = compileToTAC(source);
    TEST_ASSERT_NOT_NULL(list);

    const TACInstruction *coerceInst = findInstruction(list, TAC_INT_TO_FLOAT, 0);
    TEST_ASSERT_NOT_NULL(coerceInst);
    TEST_ASSERT_EQUAL_STRING("1", coerceInst->firstOperand.name);

    const TACInstruction *retInst = findInstruction(list, TAC_RETURN, 0);
    TEST_ASSERT_NOT_NULL(retInst);
    TEST_ASSERT_EQUAL_STRING(coerceInst->result.name, retInst->firstOperand.name);
}

// Paso de argumento entero en parámetro formal de tipo float genera coerción int_to_float previa
void test_tac_coercion_in_call_argument(void) {
    const char *source = "void show(float val) { } void main() { show(42); }";
    const TACList *list = compileToTAC(source);
    TEST_ASSERT_NOT_NULL(list);

    const TACInstruction *coerceInst = findInstruction(list, TAC_INT_TO_FLOAT, 0);
    TEST_ASSERT_NOT_NULL(coerceInst);
    TEST_ASSERT_EQUAL_STRING("42", coerceInst->firstOperand.name);

    const TACInstruction *paramInst = findInstruction(list, TAC_PARAM, 0);
    TEST_ASSERT_NOT_NULL(paramInst);
    TEST_ASSERT_EQUAL_STRING(coerceInst->result.name, paramInst->firstOperand.name);
}

/* ------------------------------------------------------------------------
 * Grupo 8: Instantáneas Canónicas de Programas Completos y Colisión de Nombres
 * ------------------------------------------------------------------------ */

// Programa factorial completo con globales, if/else recursivo y while valida salida .ci exacta
void test_tac_canonical_factorial_program(void) {
    const char *source =
        "int g;\n"
        "float h;\n"
        "int fact(int n) {\n"
        "  if (n < 2) { return 1; } else { return n * fact(n - 1); }\n"
        "}\n"
        "void main() {\n"
        "  int i;\n"
        "  i = 0;\n"
        "  while (i < 3) { i = i + 1; }\n"
        "  g = fact(i);\n"
        "}\n";

    const char *expected =
        "global g@1, INT\n"
        "global h@2, FLOAT\n"
        "begin_func fact, 1\n"
        "param_decl n@4\n"
        "t0 = n@4 < 2\n"
        "if t0 false goto L0\n"
        "ret 1\n"
        "goto L1\n"
        "L0:\n"
        "t2 = n@4 - 1\n"
        "param t2\n"
        "t3 = call fact, 1\n"
        "t1 = n@4 * t3\n"
        "ret t1\n"
        "L1:\n"
        "end_func fact\n"
        "begin_func main, 0\n"
        "local i@6, INT\n"
        "i@6 = 0\n"
        "L2:\n"
        "t4 = i@6 < 3\n"
        "if t4 false goto L3\n"
        "t5 = i@6 + 1\n"
        "i@6 = t5\n"
        "goto L2\n"
        "L3:\n"
        "param i@6\n"
        "t6 = call fact, 1\n"
        "g@1 = t6\n"
        "ret\n"
        "end_func main\n";

    const char *actual = compileToTACText(source);
    TEST_ASSERT_NOT_NULL(actual);
    TEST_ASSERT_EQUAL_STRING(expected, actual);
}

// Coerción en retorno y operaciones aritméticas mixtas valida salida .ci exacta
void test_tac_canonical_half_and_mixed_operators(void) {
    const char *source =
        "float half(int x) { return x / 2; }\n"
        "void main() {\n"
        "  float r;\n"
        "  r = half(7) + 1;\n"
        "}\n";

    const char *expected =
        "begin_func half, 1\n"
        "param_decl x@2\n"
        "t0 = x@2 / 2\n"
        "t1 = int_to_float t0\n"
        "ret t1\n"
        "end_func half\n"
        "begin_func main, 0\n"
        "local r@4, FLOAT\n"
        "param 7\n"
        "t2 = call half, 1\n"
        "t4 = int_to_float 1\n"
        "t3 = t2 + t4\n"
        "r@4 = t3\n"
        "ret\n"
        "end_func main\n";

    const char *actual = compileToTACText(source);
    TEST_ASSERT_NOT_NULL(actual);
    TEST_ASSERT_EQUAL_STRING(expected, actual);
}

// Cadena booleana con cortocircuito a && f() || false valida salida .ci exacta
void test_tac_canonical_short_circuit_boolean_chain(void) {
    const char *source =
        "boolean f() { return true; }\n"
        "void main() {\n"
        "  boolean a;\n"
        "  boolean b;\n"
        "  a = true;\n"
        "  b = a && f() || false;\n"
        "}\n";

    const char *expected =
        "begin_func f, 0\n"
        "ret true\n"
        "end_func f\n"
        "begin_func main, 0\n"
        "local a@3, BOOL\n"
        "local b@4, BOOL\n"
        "a@3 = true\n"
        "t0 = a@3\n"
        "if t0 false goto L0\n"
        "t1 = call f, 0\n"
        "t0 = t1\n"
        "L0:\n"
        "t2 = t0\n"
        "if t2 true goto L1\n"
        "t2 = false\n"
        "L1:\n"
        "b@4 = t2\n"
        "ret\n"
        "end_func main\n";

    const char *actual = compileToTACText(source);
    TEST_ASSERT_NOT_NULL(actual);
    TEST_ASSERT_EQUAL_STRING(expected, actual);
}

// Variables con nombres largos (> 64 caracteres) no deben colisionar en TAC por truncamiento
void test_tac_long_variable_name_collision(void) {
    const char *source =
        "void main() {\n"
        "  int variableConNombreExtremadamenteLargoParaProbarQueNoHayaColisionEnElBuffer1;\n"
        "  int variableConNombreExtremadamenteLargoParaProbarQueNoHayaColisionEnElBuffer2;\n"
        "  variableConNombreExtremadamenteLargoParaProbarQueNoHayaColisionEnElBuffer1 = 1;\n"
        "  variableConNombreExtremadamenteLargoParaProbarQueNoHayaColisionEnElBuffer2 = 2;\n"
        "}\n";

    const TACList *list = compileToTAC(source);
    TEST_ASSERT_NOT_NULL(list);

    const TACInstruction *firstLocal = findInstruction(list, TAC_LOCAL_DECL, 0);
    const TACInstruction *secondLocal = findInstruction(list, TAC_LOCAL_DECL, 1);

    TEST_ASSERT_NOT_NULL(firstLocal);
    TEST_ASSERT_NOT_NULL(secondLocal);
    TEST_ASSERT_NOT_EQUAL_STRING(firstLocal->firstOperand.name, secondLocal->firstOperand.name);
}

// Invocación de generación de código intermedio con raíz AST nula retorna NULL con seguridad
void test_tac_null_ast_root_returns_null(void) {
    const TACList *list = generateIntermediateCode(NULL);
    TEST_ASSERT_NULL(list);
}

// getOperandName retorna cadena vacía cuando el operando es NULL o OPERAND_NONE
void test_tac_get_operand_name_null_and_none_return_empty_string(void) {
    TEST_ASSERT_EQUAL_STRING("", getOperandName(NULL));

    TACOperand noneOperand = tacNone();
    TEST_ASSERT_EQUAL_STRING("", getOperandName(&noneOperand));

    TACOperand tempOperand = tacTemp("t0", TYPE_INT);
    TEST_ASSERT_EQUAL_STRING("t0", getOperandName(&tempOperand));
}

// formatTACList con lista nula no realiza acciones y es seguro contra punteros nulos
void test_tac_format_list_null_safe(void) {
    resetBuildString();
    formatTACList(NULL);
    TEST_ASSERT_EQUAL_STRING("", getBuiltString());
}

// appendTACInstruction agrega a TACList preservando conteo y enlaces O(1)
void test_tac_helper_append_tac_instruction(void) {
    TEST_ASSERT_NULL(appendTACInstruction(NULL, NULL));

    TACList *list = createTACList();
    TEST_ASSERT_NOT_NULL(list);
    TEST_ASSERT_NULL(appendTACInstruction(list, NULL));

    TACOperand cond = tacTemp("t0", TYPE_BOOL);
    TACOperand label = tacLabel("L0");

    TACInstruction *inst1 = createTACInstruction(TAC_IF_FALSE, tacNone(), cond, label);
    TACInstruction *inst2 = createTACInstruction(TAC_LABEL, label, tacNone(), tacNone());

    TEST_ASSERT_EQUAL_PTR(inst1, appendTACInstruction(list, inst1));
    TEST_ASSERT_EQUAL_INT(1, list->count);
    TEST_ASSERT_EQUAL_PTR(inst1, list->head);
    TEST_ASSERT_EQUAL_PTR(inst1, list->tail);

    TEST_ASSERT_EQUAL_PTR(inst2, appendTACInstruction(list, inst2));
    TEST_ASSERT_EQUAL_INT(2, list->count);
    TEST_ASSERT_EQUAL_PTR(inst1, list->head);
    TEST_ASSERT_EQUAL_PTR(inst2, list->tail);
    TEST_ASSERT_EQUAL_PTR(inst2, inst1->next);

    freeTACList(list);
}

/* ------------------------------------------------------------------------
 * Punto de entrada del ejecutable de pruebas
 * ------------------------------------------------------------------------ */

int main(void) {
    UNITY_BEGIN();

    /* Grupo 1: Expresiones Aritméticas, Unarias y Precedencia */
    RUN_TEST(test_tac_binary_addition);
    RUN_TEST(test_tac_binary_subtraction);
    RUN_TEST(test_tac_binary_multiplication);
    RUN_TEST(test_tac_binary_division);
    RUN_TEST(test_tac_binary_modulo);
    RUN_TEST(test_tac_unary_minus);
    RUN_TEST(test_tac_unary_not);
    RUN_TEST(test_tac_operator_precedence_multiplication_before_addition);
    RUN_TEST(test_tac_complex_expression_temporaries_sequential);

    /* Grupo 2: Operaciones Relacionales y de Igualdad */
    RUN_TEST(test_tac_relational_less_than);
    RUN_TEST(test_tac_relational_greater_than);
    RUN_TEST(test_tac_relational_equality);

    /* Grupo 3: Lógica de Cortocircuito */
    RUN_TEST(test_tac_short_circuit_and);
    RUN_TEST(test_tac_short_circuit_or);
    RUN_TEST(test_tac_short_circuit_skips_call_when_first_operand_determines_result);
    RUN_TEST(test_tac_short_circuit_chained);

    /* Grupo 4: Estructuras de Control */
    RUN_TEST(test_tac_if_without_else);
    RUN_TEST(test_tac_if_with_else);
    RUN_TEST(test_tac_while_loop);
    RUN_TEST(test_tac_nested_while_loops);

    /* Grupo 5: Declaraciones y Ámbitos de Variables */
    RUN_TEST(test_tac_global_variable_declarations);
    RUN_TEST(test_tac_local_variable_declarations);
    RUN_TEST(test_tac_variable_shadowing_distinguishes_uids);
    RUN_TEST(test_tac_assignment_variable);

    /* Grupo 6: Métodos, Parámetros, Llamadas y Retornos */
    RUN_TEST(test_tac_method_declaration_and_implicit_void_return);
    RUN_TEST(test_tac_method_explicit_return_void_has_no_duplicate_ret);
    RUN_TEST(test_tac_method_parameters);
    RUN_TEST(test_tac_method_call_statement_void);
    RUN_TEST(test_tac_method_call_expression_with_return_value);
    RUN_TEST(test_tac_method_call_arguments_left_to_right);

    /* Grupo 7: Coerciones de Tipos */
    RUN_TEST(test_tac_coercion_on_assignment);
    RUN_TEST(test_tac_coercion_in_binary_op_left);
    RUN_TEST(test_tac_coercion_in_binary_op_right);
    RUN_TEST(test_tac_coercion_on_return);
    RUN_TEST(test_tac_coercion_in_call_argument);

    /* Grupo 8: Instantáneas Canónicas de Programas Completos */
    RUN_TEST(test_tac_canonical_factorial_program);
    RUN_TEST(test_tac_canonical_half_and_mixed_operators);
    RUN_TEST(test_tac_canonical_short_circuit_boolean_chain);
    RUN_TEST(test_tac_long_variable_name_collision);
    RUN_TEST(test_tac_null_ast_root_returns_null);
    RUN_TEST(test_tac_get_operand_name_null_and_none_return_empty_string);
    RUN_TEST(test_tac_format_list_null_safe);
    RUN_TEST(test_tac_helper_append_tac_instruction);

    return UNITY_END();
}
