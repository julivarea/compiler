/**
 * @file test_tac.c
 * @brief Pruebas unitarias de la generación de Código de Tres Direcciones (TAC).
 *
 * Valida la traducción completa del AST a instrucciones intermedias TAC:
 *  - Expresiones aritméticas y orden de emisión de subexpresiones.
 *  - Expresiones unarias (negación aritmética y lógica).
 *  - Operaciones relacionales y de igualdad.
 *  - Lógica de cortocircuito (&& y ||) con saltos condicionales y etiquetas.
 *  - Estructuras de control (if, if-else, while y bucles anidados).
 *  - Declaraciones de variables globales y locales en segmentos/marcos respectivos.
 *  - Sombreado de variables diferenciado por uids calificados (@uid).
 *  - Definición de métodos, lista de parámetros formales y retorno implícito/explícito.
 *  - Llamadas a métodos con pasaje ordenado de argumentos (param y call).
 *  - Rechazo de los tipos fuera del alcance de la etapa (`float`).
 *  - Instantáneas canónicas exactas de programas completos (.ci).
 *
 * Cada test arma a mano el AST anotado que entrega el análisis semántico: símbolos
 * resueltos con su uid, alcance global/local y tipo de cada expresión (ver
 * support/ast_builder.h). Así solo ejercita el generador de código intermedio; ni el
 * front-end ni el analizador semántico pueden hacer fallar esta suite. El comentario de
 * cada test muestra el programa C-TDS equivalente.
 */

#include "unity.h"
#include "ast.h"
#include "ast_builder.h"
#include "symbol_table.h"
#include "intermediate_code.h"
#include "tac.h"
#include "string_builder.h"
#include "types.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------------
 * Funciones auxiliares del arnés de pruebas
 * ------------------------------------------------------------------------ */

static NodeAST *program = NULL;

static const TACList *generate(NodeAST *root) {
    program = root;
    return generateIntermediateCode(root);
}

static const char *generateText(NodeAST *root) {
    const TACList *list = generate(root);
    if (list == NULL) {
        return NULL;
    }

    resetBuildString();
    formatTACList(list);
    return getBuiltString();
}

/* void main() con el uid dado; el cuerpo se completa con astAppend. */
static NodeAST *mainMethod(int uid) {
    return astMethodDeclaration(TYPE_VOID, "main", uid, astParameters(0));
}

/* void main() { int a; int b; <type> c; a = 1; b = 2; c = a <op> b; } */
static const TACList *generateBinaryInMain(OperationType op, DataType resultType) {
    NodeAST *main = mainMethod(1);
    NodeAST *a = astLocal(TYPE_INT, "a", 2);
    NodeAST *b = astLocal(TYPE_INT, "b", 3);
    NodeAST *c = astLocal(resultType, "c", 4);
    astAppend(main, 6, a, b, c,
              astStore(a, astInt(1)),
              astStore(b, astInt(2)),
              astStore(c, astBinary(op, astUse(a), astUse(b))));
    return generate(astProgram(1, main));
}

/* void main() { boolean a; boolean b; boolean r; a = true; b = false; r = a <op> b; } */
static const TACList *generateLogicalInMain(OperationType op) {
    NodeAST *main = mainMethod(1);
    NodeAST *a = astLocal(TYPE_BOOL, "a", 2);
    NodeAST *b = astLocal(TYPE_BOOL, "b", 3);
    NodeAST *r = astLocal(TYPE_BOOL, "r", 4);
    astAppend(main, 6, a, b, r,
              astStore(a, astBool(1)),
              astStore(b, astBool(0)),
              astStore(r, astBinary(op, astUse(a), astUse(b))));
    return generate(astProgram(1, main));
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

static bool precedes(const TACList *list, const TACInstruction *first, const TACInstruction *second) {
    for (const TACInstruction *inst = list->head; inst != NULL; inst = inst->next) {
        if (inst == first) return true;
        if (inst == second) return false;
    }
    return false;
}

void setUp(void) {
    resetSymbolUids();
    resetBuildString();
    program = NULL;
}

void tearDown(void) {
    freeAST(program);
    program = NULL;
}

/* ------------------------------------------------------------------------
 * Grupo 1: Expresiones Aritméticas, Unarias y Orden de Emisión
 * ------------------------------------------------------------------------ */

// Suma binaria entera genera instrucción TAC_ADD asignando el resultado a un nuevo temporal
// void main() { int a; int b; int c; a = 1; b = 2; c = a + b; }
void test_tac_binary_addition(void) {
    const TACList *list = generateBinaryInMain(OP_ADD, TYPE_INT);
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
// void main() { ... c = a - b; }
void test_tac_binary_subtraction(void) {
    const TACInstruction *subInst = findInstruction(generateBinaryInMain(OP_SUB, TYPE_INT), TAC_SUB, 0);
    TEST_ASSERT_NOT_NULL(subInst);
    TEST_ASSERT_EQUAL(OPERAND_TEMP, subInst->result.kind);
    TEST_ASSERT_EQUAL_STRING("t0", subInst->result.name);
}

// Multiplicación binaria genera instrucción TAC_MUL
// void main() { ... c = a * b; }
void test_tac_binary_multiplication(void) {
    const TACInstruction *mulInst = findInstruction(generateBinaryInMain(OP_MUL, TYPE_INT), TAC_MUL, 0);
    TEST_ASSERT_NOT_NULL(mulInst);
    TEST_ASSERT_EQUAL(OPERAND_TEMP, mulInst->result.kind);
    TEST_ASSERT_EQUAL_STRING("t0", mulInst->result.name);
}

// División binaria entera genera instrucción TAC_DIV
// void main() { ... c = a / b; }
void test_tac_binary_division(void) {
    const TACInstruction *divInst = findInstruction(generateBinaryInMain(OP_DIV, TYPE_INT), TAC_DIV, 0);
    TEST_ASSERT_NOT_NULL(divInst);
    TEST_ASSERT_EQUAL(OPERAND_TEMP, divInst->result.kind);
    TEST_ASSERT_EQUAL_STRING("t0", divInst->result.name);
}

// Operación módulo entera genera instrucción TAC_MOD
// void main() { ... c = a % b; }
void test_tac_binary_modulo(void) {
    const TACInstruction *modInst = findInstruction(generateBinaryInMain(OP_MOD, TYPE_INT), TAC_MOD, 0);
    TEST_ASSERT_NOT_NULL(modInst);
    TEST_ASSERT_EQUAL(OPERAND_TEMP, modInst->result.kind);
    TEST_ASSERT_EQUAL_STRING("t0", modInst->result.name);
}

// Negación aritmética unaria genera instrucción TAC_UMINUS con segundo operando nulo
// void main() { int a; int b; a = 5; b = -a; }
void test_tac_unary_minus(void) {
    NodeAST *main = mainMethod(1);
    NodeAST *a = astLocal(TYPE_INT, "a", 2);
    NodeAST *b = astLocal(TYPE_INT, "b", 3);
    astAppend(main, 4, a, b, astStore(a, astInt(5)), astStore(b, astUnary(OP_UMINUS, astUse(a))));
    const TACList *list = generate(astProgram(1, main));
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
// void main() { boolean a; boolean b; a = true; b = !a; }
void test_tac_unary_not(void) {
    NodeAST *main = mainMethod(1);
    NodeAST *a = astLocal(TYPE_BOOL, "a", 2);
    NodeAST *b = astLocal(TYPE_BOOL, "b", 3);
    astAppend(main, 4, a, b, astStore(a, astBool(1)), astStore(b, astUnary(OP_NOT, astUse(a))));
    const TACList *list = generate(astProgram(1, main));
    TEST_ASSERT_NOT_NULL(list);

    const TACInstruction *notInst = findInstruction(list, TAC_NOT, 0);
    TEST_ASSERT_NOT_NULL(notInst);
    TEST_ASSERT_EQUAL(OPERAND_TEMP, notInst->result.kind);
    TEST_ASSERT_EQUAL_STRING("t0", notInst->result.name);
    TEST_ASSERT_EQUAL(OPERAND_VARIABLE, notInst->firstOperand.kind);
    TEST_ASSERT_EQUAL_STRING("a@2", notInst->firstOperand.name);
    TEST_ASSERT_EQUAL(OPERAND_NONE, notInst->secondOperand.kind);
}

// Un subárbol derecho (*) se emite antes que el operador que lo contiene (+)
// void main() { int a; int b; int c; a = 1; b = 2; c = a + b * 3; }
void test_tac_operator_precedence_multiplication_before_addition(void) {
    NodeAST *main = mainMethod(1);
    NodeAST *a = astLocal(TYPE_INT, "a", 2);
    NodeAST *b = astLocal(TYPE_INT, "b", 3);
    NodeAST *c = astLocal(TYPE_INT, "c", 4);
    astAppend(main, 6, a, b, c,
              astStore(a, astInt(1)),
              astStore(b, astInt(2)),
              astStore(c, astBinary(OP_ADD, astUse(a), astBinary(OP_MUL, astUse(b), astInt(3)))));
    const TACList *list = generate(astProgram(1, main));
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
    TEST_ASSERT_TRUE(precedes(list, mulInst, addInst));
}

// Expresión compuesta con subárboles genera temporales secuenciales t0, t1 y t2
// void main() { int a; int b; int c; int d; int r; ... r = (a + b) * (c - d); }
void test_tac_complex_expression_temporaries_sequential(void) {
    NodeAST *main = mainMethod(1);
    NodeAST *a = astLocal(TYPE_INT, "a", 2);
    NodeAST *b = astLocal(TYPE_INT, "b", 3);
    NodeAST *c = astLocal(TYPE_INT, "c", 4);
    NodeAST *d = astLocal(TYPE_INT, "d", 5);
    NodeAST *r = astLocal(TYPE_INT, "r", 6);
    NodeAST *product = astBinary(OP_MUL,
                                 astBinary(OP_ADD, astUse(a), astUse(b)),
                                 astBinary(OP_SUB, astUse(c), astUse(d)));
    astAppend(main, 10, a, b, c, d, r,
              astStore(a, astInt(1)), astStore(b, astInt(2)), astStore(c, astInt(3)), astStore(d, astInt(4)),
              astStore(r, product));
    const TACList *list = generate(astProgram(1, main));
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
// void main() { int a; int b; boolean c; a = 1; b = 2; c = a < b; }
void test_tac_relational_less_than(void) {
    const TACInstruction *ltInst = findInstruction(generateBinaryInMain(OP_LT, TYPE_BOOL), TAC_LT, 0);
    TEST_ASSERT_NOT_NULL(ltInst);
    TEST_ASSERT_EQUAL(TYPE_BOOL, ltInst->result.type);
    TEST_ASSERT_EQUAL_STRING("t0", ltInst->result.name);
    TEST_ASSERT_EQUAL_STRING("a@2", ltInst->firstOperand.name);
    TEST_ASSERT_EQUAL_STRING("b@3", ltInst->secondOperand.name);
}

// Comparación mayor que genera instrucción TAC_GT con resultado booleano
// void main() { ... c = a > b; }
void test_tac_relational_greater_than(void) {
    const TACInstruction *gtInst = findInstruction(generateBinaryInMain(OP_GT, TYPE_BOOL), TAC_GT, 0);
    TEST_ASSERT_NOT_NULL(gtInst);
    TEST_ASSERT_EQUAL(TYPE_BOOL, gtInst->result.type);
    TEST_ASSERT_EQUAL_STRING("t0", gtInst->result.name);
}

// Comparación de igualdad genera instrucción TAC_EQ con resultado booleano
// void main() { ... c = a == b; }
void test_tac_relational_equality(void) {
    const TACInstruction *eqInst = findInstruction(generateBinaryInMain(OP_EQ, TYPE_BOOL), TAC_EQ, 0);
    TEST_ASSERT_NOT_NULL(eqInst);
    TEST_ASSERT_EQUAL(TYPE_BOOL, eqInst->result.type);
    TEST_ASSERT_EQUAL_STRING("t0", eqInst->result.name);
}

/* ------------------------------------------------------------------------
 * Grupo 3: Lógica de Cortocircuito (&&, ||)
 * ------------------------------------------------------------------------ */

// Operador lógico && genera salto condicional if_false hacia etiqueta de salida
// void main() { boolean a; boolean b; boolean r; a = true; b = false; r = a && b; }
void test_tac_short_circuit_and(void) {
    const TACList *list = generateLogicalInMain(OP_AND);
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
// void main() { boolean a; boolean b; boolean r; a = true; b = false; r = a || b; }
void test_tac_short_circuit_or(void) {
    const TACList *list = generateLogicalInMain(OP_OR);
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

/* boolean f() { return true; } */
static NodeAST *booleanMethodF(int uid) {
    NodeAST *f = astMethodDeclaration(TYPE_BOOL, "f", uid, astParameters(0));
    astAppend(f, 1, astReturn(astBool(1)));
    return f;
}

// Cortocircuito omite llamada a método derecho cuando el operando izquierdo determina el resultado
// boolean f() { return true; } void main() { boolean a; a = false && f(); }
void test_tac_short_circuit_skips_call_when_first_operand_determines_result(void) {
    NodeAST *f = booleanMethodF(1);
    NodeAST *main = mainMethod(2);
    NodeAST *a = astLocal(TYPE_BOOL, "a", 3);
    astAppend(main, 2, a, astStore(a, astBinary(OP_AND, astBool(0), astInvoke(f, 0))));
    const TACList *list = generate(astProgram(2, f, main));
    TEST_ASSERT_NOT_NULL(list);

    const TACInstruction *ifFalseInst = findInstruction(list, TAC_IF_FALSE, 0);
    const TACInstruction *callInst = findInstruction(list, TAC_CALL, 0);
    TEST_ASSERT_NOT_NULL(ifFalseInst);
    TEST_ASSERT_NOT_NULL(callInst);

    // ifFalse debe preceder en el código al call f
    TEST_ASSERT_TRUE(precedes(list, ifFalseInst, callInst));
}

// Operaciones de cortocircuito encadenadas generan etiquetas y saltos condicionales sucesivos
// void main() { boolean a; boolean b; boolean c; boolean r; ... r = a && b && c; }
void test_tac_short_circuit_chained(void) {
    NodeAST *main = mainMethod(1);
    NodeAST *a = astLocal(TYPE_BOOL, "a", 2);
    NodeAST *b = astLocal(TYPE_BOOL, "b", 3);
    NodeAST *c = astLocal(TYPE_BOOL, "c", 4);
    NodeAST *r = astLocal(TYPE_BOOL, "r", 5);
    NodeAST *chain = astBinary(OP_AND, astBinary(OP_AND, astUse(a), astUse(b)), astUse(c));
    astAppend(main, 8, a, b, c, r,
              astStore(a, astBool(1)), astStore(b, astBool(1)), astStore(c, astBool(0)),
              astStore(r, chain));
    const TACList *list = generate(astProgram(1, main));
    TEST_ASSERT_NOT_NULL(list);

    // Dos operaciones && encadenadas generan 2 saltos condicionales if_false y 2 etiquetas
    TEST_ASSERT_EQUAL_INT(2, countInstructions(list, TAC_IF_FALSE));
    TEST_ASSERT_EQUAL_INT(2, countInstructions(list, TAC_LABEL));
}

/* ------------------------------------------------------------------------
 * Grupo 4: Estructuras de Control (If, Else, While)
 * ------------------------------------------------------------------------ */

/* void main() { int x; x = 0; if (x < 1) { x = 2; } [else { x = 3; }] } */
static const TACList *generateIfInMain(bool withElse) {
    NodeAST *main = mainMethod(1);
    NodeAST *x = astLocal(TYPE_INT, "x", 2);
    NodeAST *elseBlock = withElse ? astBlock(1, astStore(x, astInt(3))) : NULL;
    astAppend(main, 3, x, astStore(x, astInt(0)),
              astIf(astBinary(OP_LT, astUse(x), astInt(1)), astBlock(1, astStore(x, astInt(2))), elseBlock));
    return generate(astProgram(1, main));
}

// Sentencia if simple emite condición, salto if_false y etiqueta de cierre sin goto
// void main() { int x; x = 0; if (x < 1) { x = 2; } }
void test_tac_if_without_else(void) {
    const TACList *list = generateIfInMain(false);
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
// void main() { int x; x = 0; if (x < 1) { x = 2; } else { x = 3; } }
void test_tac_if_with_else(void) {
    const TACList *list = generateIfInMain(true);
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

/* while (<var> < <limit>) { <var> = <var> + 1; } */
static NodeAST *countingLoop(NodeAST *var, int limit, NodeAST *bodyPrefix) {
    NodeAST *body = astBlock(2, bodyPrefix, astStore(var, astBinary(OP_ADD, astUse(var), astInt(1))));
    return astWhile(astBinary(OP_LT, astUse(var), astInt(limit)), body);
}

// Bucle while emite etiqueta de inicio, evaluación de condición, salto if_false y goto de retorno
// void main() { int i; i = 0; while (i < 5) { i = i + 1; } }
void test_tac_while_loop(void) {
    NodeAST *main = mainMethod(1);
    NodeAST *i = astLocal(TYPE_INT, "i", 2);
    astAppend(main, 3, i, astStore(i, astInt(0)), countingLoop(i, 5, NULL));
    const TACList *list = generate(astProgram(1, main));
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
// void main() { int i; int j; i = 0;
//               while (i < 3) { j = 0; while (j < 3) { j = j + 1; } i = i + 1; } }
void test_tac_nested_while_loops(void) {
    NodeAST *main = mainMethod(1);
    NodeAST *i = astLocal(TYPE_INT, "i", 2);
    NodeAST *j = astLocal(TYPE_INT, "j", 3);
    NodeAST *inner = astBlock(2, astStore(j, astInt(0)), countingLoop(j, 3, NULL));
    astAppend(main, 4, i, j, astStore(i, astInt(0)), countingLoop(i, 3, inner));
    const TACList *list = generate(astProgram(1, main));
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
// int g; boolean b; void main() { }
void test_tac_global_variable_declarations(void) {
    NodeAST *root = astProgram(3, astGlobal(TYPE_INT, "g", 1), astGlobal(TYPE_BOOL, "b", 2), mainMethod(3));
    const TACList *list = generate(root);
    TEST_ASSERT_NOT_NULL(list);

    TEST_ASSERT_EQUAL_INT(2, countInstructions(list, TAC_GLOBAL_DECL));

    const TACInstruction *g1 = findInstruction(list, TAC_GLOBAL_DECL, 0);
    const TACInstruction *g2 = findInstruction(list, TAC_GLOBAL_DECL, 1);

    TEST_ASSERT_EQUAL_STRING("g@1", g1->result.name);
    TEST_ASSERT_EQUAL(TYPE_INT, g1->result.type);

    TEST_ASSERT_EQUAL_STRING("b@2", g2->result.name);
    TEST_ASSERT_EQUAL(TYPE_BOOL, g2->result.type);
}

// Variables locales dentro del cuerpo de un método emiten instrucciones TAC_LOCAL_DECL
// void main() { int x; boolean z; }
void test_tac_local_variable_declarations(void) {
    NodeAST *main = mainMethod(1);
    astAppend(main, 2, astLocal(TYPE_INT, "x", 2), astLocal(TYPE_BOOL, "z", 3));
    const TACList *list = generate(astProgram(1, main));
    TEST_ASSERT_NOT_NULL(list);

    TEST_ASSERT_EQUAL_INT(2, countInstructions(list, TAC_LOCAL_DECL));

    const TACInstruction *l1 = findInstruction(list, TAC_LOCAL_DECL, 0);
    const TACInstruction *l2 = findInstruction(list, TAC_LOCAL_DECL, 1);

    TEST_ASSERT_EQUAL(TYPE_INT, l1->result.type);
    TEST_ASSERT_EQUAL(TYPE_BOOL, l2->result.type);
}

// Sombreado de variable local sobre global genera uids calificados distintos (@1 vs @3)
// int x; void main() { int x; x = 5; }
void test_tac_variable_shadowing_distinguishes_uids(void) {
    NodeAST *globalX = astGlobal(TYPE_INT, "x", 1);
    NodeAST *main = mainMethod(2);
    NodeAST *localX = astLocal(TYPE_INT, "x", 3);
    astAppend(main, 2, localX, astStore(localX, astInt(5)));
    const TACList *list = generate(astProgram(2, globalX, main));
    TEST_ASSERT_NOT_NULL(list);

    const TACInstruction *globalDecl = findInstruction(list, TAC_GLOBAL_DECL, 0);
    const TACInstruction *localDecl = findInstruction(list, TAC_LOCAL_DECL, 0);
    const TACInstruction *assignInst = findInstruction(list, TAC_ASSIGN, 0);

    TEST_ASSERT_NOT_NULL(globalDecl);
    TEST_ASSERT_NOT_NULL(localDecl);
    TEST_ASSERT_NOT_NULL(assignInst);

    // Global x@1 vs Local x@3
    TEST_ASSERT_EQUAL_STRING("x@1", globalDecl->result.name);
    TEST_ASSERT_EQUAL_STRING("x@3", localDecl->result.name);

    // La asignación interna debe modificar al local x@3
    TEST_ASSERT_EQUAL_STRING("x@3", assignInst->result.name);
}

// Asignación simple emite instrucción TAC_ASSIGN hacia el identificador calificado
// void main() { int x; x = 42; }
void test_tac_assignment_variable(void) {
    NodeAST *main = mainMethod(1);
    NodeAST *x = astLocal(TYPE_INT, "x", 2);
    astAppend(main, 2, x, astStore(x, astInt(42)));
    const TACList *list = generate(astProgram(1, main));
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
// void main() { }
void test_tac_method_declaration_and_implicit_void_return(void) {
    const TACList *list = generate(astProgram(1, mainMethod(1)));
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
// void main() { return; }
void test_tac_method_explicit_return_void_has_no_duplicate_ret(void) {
    NodeAST *main = mainMethod(1);
    astAppend(main, 1, astReturn(NULL));
    const TACList *list = generate(astProgram(1, main));
    TEST_ASSERT_NOT_NULL(list);

    // Solo debe haber un ret, no debe agregarse uno redundante
    TEST_ASSERT_EQUAL_INT(1, countInstructions(list, TAC_RETURN));
}

/* int <name>(int a, int b) { return a <op> b; } con uids name=1, a=2, b=3 */
static NodeAST *binaryIntMethod(const char *name, OperationType op) {
    NodeAST *a = astLocal(TYPE_INT, "a", 2);
    NodeAST *b = astLocal(TYPE_INT, "b", 3);
    NodeAST *method = astMethodDeclaration(TYPE_INT, name, 1, astParameters(2, a, b));
    astAppend(method, 1, astReturn(astBinary(op, astUse(a), astUse(b))));
    return method;
}

// Declaración de parámetros formales emite instrucciones TAC_PARAM_DECL en orden de recepción
// int add(int a, int b) { return a + b; } void main() { }
void test_tac_method_parameters(void) {
    const TACList *list = generate(astProgram(2, binaryIntMethod("add", OP_ADD), mainMethod(4)));
    TEST_ASSERT_NOT_NULL(list);

    const TACInstruction *beginFunc = findInstruction(list, TAC_BEGIN_FUNC, 0);
    TEST_ASSERT_NOT_NULL(beginFunc);
    TEST_ASSERT_EQUAL_STRING("add", beginFunc->result.name);
    TEST_ASSERT_EQUAL_STRING("2", beginFunc->firstOperand.name); // 2 parámetros

    TEST_ASSERT_EQUAL_INT(2, countInstructions(list, TAC_PARAM_DECL));
    const TACInstruction *p1 = findInstruction(list, TAC_PARAM_DECL, 0);
    const TACInstruction *p2 = findInstruction(list, TAC_PARAM_DECL, 1);

    TEST_ASSERT_EQUAL_STRING("a@2", p1->result.name);
    TEST_ASSERT_EQUAL_STRING("b@3", p2->result.name);
}

// Sentencia de llamada a método void emite paso de argumentos param y call sin temporal de destino
// void proc(int x) { } void main() { proc(10); }
void test_tac_method_call_statement_void(void) {
    NodeAST *proc = astMethodDeclaration(TYPE_VOID, "proc", 1, astParameters(1, astLocal(TYPE_INT, "x", 2)));
    NodeAST *main = mainMethod(3);
    astAppend(main, 1, astInvoke(proc, 1, astInt(10)));
    const TACList *list = generate(astProgram(2, proc, main));
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
// int f(int a) { return a + 1; } void main() { int x; x = f(2); }
void test_tac_method_call_expression_with_return_value(void) {
    NodeAST *a = astLocal(TYPE_INT, "a", 2);
    NodeAST *f = astMethodDeclaration(TYPE_INT, "f", 1, astParameters(1, a));
    astAppend(f, 1, astReturn(astBinary(OP_ADD, astUse(a), astInt(1))));
    NodeAST *main = mainMethod(3);
    NodeAST *x = astLocal(TYPE_INT, "x", 4);
    astAppend(main, 2, x, astStore(x, astInvoke(f, 1, astInt(2))));
    const TACList *list = generate(astProgram(2, f, main));
    TEST_ASSERT_NOT_NULL(list);

    const TACInstruction *callInst = findInstruction(list, TAC_CALL, 0);
    TEST_ASSERT_NOT_NULL(callInst);
    TEST_ASSERT_EQUAL(OPERAND_TEMP, callInst->result.kind);
    TEST_ASSERT_EQUAL_STRING("t0", callInst->result.name);
    TEST_ASSERT_EQUAL_STRING("f", callInst->firstOperand.name);

    // Asignación de x@4 = t0
    const TACInstruction *assignInst = findInstruction(list, TAC_ASSIGN, 0);
    TEST_ASSERT_NOT_NULL(assignInst);
    TEST_ASSERT_EQUAL_STRING("x@4", assignInst->result.name);
    TEST_ASSERT_EQUAL_STRING("t0", assignInst->firstOperand.name);
}

// Múltiples argumentos en una llamada a método se emiten en orden de izquierda a derecha
// int diff(int a, int b) { return a - b; } void main() { int x; x = diff(10, 3); }
void test_tac_method_call_arguments_left_to_right(void) {
    NodeAST *diff = binaryIntMethod("diff", OP_SUB);
    NodeAST *main = mainMethod(4);
    NodeAST *x = astLocal(TYPE_INT, "x", 5);
    astAppend(main, 2, x, astStore(x, astInvoke(diff, 2, astInt(10), astInt(3))));
    const TACList *list = generate(astProgram(2, diff, main));
    TEST_ASSERT_NOT_NULL(list);

    const TACInstruction *p1 = findInstruction(list, TAC_PARAM, 0);
    const TACInstruction *p2 = findInstruction(list, TAC_PARAM, 1);

    TEST_ASSERT_NOT_NULL(p1);
    TEST_ASSERT_NOT_NULL(p2);
    TEST_ASSERT_EQUAL_STRING("10", p1->firstOperand.name);
    TEST_ASSERT_EQUAL_STRING("3", p2->firstOperand.name);
}

/* ------------------------------------------------------------------------
 * Grupo 7: Instantáneas Canónicas de Programas Completos y Colisión de Nombres
 * ------------------------------------------------------------------------ */

// Programa factorial completo con globales, if/else recursivo y while valida salida .ci exacta
// int g;
// int fact(int n) { if (n < 2) { return 1; } else { return n * fact(n - 1); } }
// void main() { int i; i = 0; while (i < 3) { i = i + 1; } g = fact(i); }
void test_tac_canonical_factorial_program(void) {
    NodeAST *g = astGlobal(TYPE_INT, "g", 1);

    NodeAST *n = astLocal(TYPE_INT, "n", 3);
    NodeAST *fact = astMethodDeclaration(TYPE_INT, "fact", 2, astParameters(1, n));
    NodeAST *recursion = astBinary(OP_MUL, astUse(n), astInvoke(fact, 1, astBinary(OP_SUB, astUse(n), astInt(1))));
    astAppend(fact, 1, astIf(astBinary(OP_LT, astUse(n), astInt(2)),
                             astBlock(1, astReturn(astInt(1))),
                             astBlock(1, astReturn(recursion))));

    NodeAST *main = mainMethod(4);
    NodeAST *i = astLocal(TYPE_INT, "i", 5);
    astAppend(main, 4, i, astStore(i, astInt(0)), countingLoop(i, 3, NULL),
              astStore(g, astInvoke(fact, 1, astUse(i))));

    const char *expected =
        "global g@1, INT\n"
        "begin_func fact, 1\n"
        "param_decl n@3, INT\n"
        "t0 = n@3 < 2\n"
        "if t0 false goto L0\n"
        "ret 1\n"
        "goto L1\n"
        "L0:\n"
        "t2 = n@3 - 1\n"
        "param t2\n"
        "t3 = call fact, 1\n"
        "t1 = n@3 * t3\n"
        "ret t1\n"
        "L1:\n"
        "end_func fact\n"
        "begin_func main, 0\n"
        "local i@5, INT\n"
        "i@5 = 0\n"
        "L2:\n"
        "t0 = i@5 < 3\n"
        "if t0 false goto L3\n"
        "t1 = i@5 + 1\n"
        "i@5 = t1\n"
        "goto L2\n"
        "L3:\n"
        "param i@5\n"
        "t2 = call fact, 1\n"
        "g@1 = t2\n"
        "ret\n"
        "end_func main\n";

    const char *actual = generateText(astProgram(3, g, fact, main));
    TEST_ASSERT_NOT_NULL(actual);
    TEST_ASSERT_EQUAL_STRING(expected, actual);
}

// Programa con operadores mixtos, llamada y aritmética valida salida .ci exacta
// int half(int x) { return x / 2; } void main() { int r; r = half(7) + 1; }
void test_tac_canonical_half_and_mixed_operators(void) {
    NodeAST *x = astLocal(TYPE_INT, "x", 2);
    NodeAST *half = astMethodDeclaration(TYPE_INT, "half", 1, astParameters(1, x));
    astAppend(half, 1, astReturn(astBinary(OP_DIV, astUse(x), astInt(2))));

    NodeAST *main = mainMethod(3);
    NodeAST *r = astLocal(TYPE_INT, "r", 4);
    astAppend(main, 2, r, astStore(r, astBinary(OP_ADD, astInvoke(half, 1, astInt(7)), astInt(1))));

    const char *expected =
        "begin_func half, 1\n"
        "param_decl x@2, INT\n"
        "t0 = x@2 / 2\n"
        "ret t0\n"
        "end_func half\n"
        "begin_func main, 0\n"
        "local r@4, INT\n"
        "param 7\n"
        "t0 = call half, 1\n"
        "t1 = t0 + 1\n"
        "r@4 = t1\n"
        "ret\n"
        "end_func main\n";

    const char *actual = generateText(astProgram(2, half, main));
    TEST_ASSERT_NOT_NULL(actual);
    TEST_ASSERT_EQUAL_STRING(expected, actual);
}

// El enunciado acota la etapa a tipos enteros y logicos: un programa con float se rechaza
// void main() { float f; f = 1.5; }
void test_tac_rejects_float_program(void) {
    NodeAST *main = mainMethod(1);
    NodeAST *f = astLocal(TYPE_FLOAT, "f", 2);
    astAppend(main, 2, f, astStore(f, astFloat(1.5f)));
    TEST_ASSERT_NULL(generate(astProgram(1, main)));
    TEST_ASSERT_TRUE(hasUnsupportedTypeError());
}

// El rechazo alcanza al tipo de retorno de un metodo, no solo a las variables
// float f() { return 1; } void main() { }
void test_tac_rejects_float_return_type(void) {
    NodeAST *f = astMethodDeclaration(TYPE_FLOAT, "f", 1, astParameters(0));
    astAppend(f, 1, astReturn(astInt(1)));
    TEST_ASSERT_NULL(generate(astProgram(2, f, mainMethod(2))));
    TEST_ASSERT_TRUE(hasUnsupportedTypeError());
}

// Un programa sin float no activa la bandera de tipo no soportado
// void main() { int x; x = 1; }
void test_tac_integer_program_is_supported(void) {
    NodeAST *main = mainMethod(1);
    NodeAST *x = astLocal(TYPE_INT, "x", 2);
    astAppend(main, 2, x, astStore(x, astInt(1)));
    TEST_ASSERT_NOT_NULL(generate(astProgram(1, main)));
    TEST_ASSERT_FALSE(hasUnsupportedTypeError());
}

// Cadena booleana con cortocircuito a && f() || false valida salida .ci exacta
// boolean f() { return true; }
// void main() { boolean a; boolean b; a = true; b = a && f() || false; }
void test_tac_canonical_short_circuit_boolean_chain(void) {
    NodeAST *f = booleanMethodF(1);
    NodeAST *main = mainMethod(2);
    NodeAST *a = astLocal(TYPE_BOOL, "a", 3);
    NodeAST *b = astLocal(TYPE_BOOL, "b", 4);
    NodeAST *chain = astBinary(OP_OR, astBinary(OP_AND, astUse(a), astInvoke(f, 0)), astBool(0));
    astAppend(main, 4, a, b, astStore(a, astBool(1)), astStore(b, chain));

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

    const char *actual = generateText(astProgram(2, f, main));
    TEST_ASSERT_NOT_NULL(actual);
    TEST_ASSERT_EQUAL_STRING(expected, actual);
}

// Variables con nombres largos (> 64 caracteres) no deben colisionar en TAC por truncamiento
void test_tac_long_variable_name_collision(void) {
    NodeAST *main = mainMethod(1);
    NodeAST *first = astLocal(TYPE_INT, "variableConNombreExtremadamenteLargoParaProbarQueNoHayaColisionEnElBuffer1", 2);
    NodeAST *second = astLocal(TYPE_INT, "variableConNombreExtremadamenteLargoParaProbarQueNoHayaColisionEnElBuffer2", 3);
    astAppend(main, 4, first, second, astStore(first, astInt(1)), astStore(second, astInt(2)));
    const TACList *list = generate(astProgram(1, main));
    TEST_ASSERT_NOT_NULL(list);

    const TACInstruction *firstLocal = findInstruction(list, TAC_LOCAL_DECL, 0);
    const TACInstruction *secondLocal = findInstruction(list, TAC_LOCAL_DECL, 1);

    TEST_ASSERT_NOT_NULL(firstLocal);
    TEST_ASSERT_NOT_NULL(secondLocal);
    TEST_ASSERT_NOT_EQUAL_STRING(firstLocal->result.name, secondLocal->result.name);
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

    /* Grupo 1: Expresiones Aritméticas, Unarias y Orden de Emisión */
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

    /* Grupo 7: Instantáneas Canónicas de Programas Completos */
    RUN_TEST(test_tac_canonical_factorial_program);
    RUN_TEST(test_tac_canonical_half_and_mixed_operators);
    RUN_TEST(test_tac_rejects_float_program);
    RUN_TEST(test_tac_rejects_float_return_type);
    RUN_TEST(test_tac_integer_program_is_supported);
    RUN_TEST(test_tac_canonical_short_circuit_boolean_chain);
    RUN_TEST(test_tac_long_variable_name_collision);
    RUN_TEST(test_tac_null_ast_root_returns_null);
    RUN_TEST(test_tac_get_operand_name_null_and_none_return_empty_string);
    RUN_TEST(test_tac_format_list_null_safe);
    RUN_TEST(test_tac_helper_append_tac_instruction);

    return UNITY_END();
}
