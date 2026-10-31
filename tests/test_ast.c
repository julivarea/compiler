#include "unity.h"
#include "ast.h"
#include "error_handler.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

// Declaraciones de Flex/Bison
extern int yyparse(void);
typedef struct yy_buffer_state *YY_BUFFER_STATE;
extern YY_BUFFER_STATE yy_scan_string(const char *str);
extern void yy_delete_buffer(YY_BUFFER_STATE buffer);
extern int yylineno;
extern NodeAST *astRoot;

void setUp(void) {
    yylineno = 1;
    astRoot = NULL;
    resetErrorHandler();
}
void tearDown(void) {
    freeAST(astRoot);
    astRoot = NULL;
}

/* ------------------------------------------------------------------------
 * Utilidades para parsear y mirar el árbol (no hacen ninguna aserción)
 * ------------------------------------------------------------------------ */

// Parsea un programa y devuelve la raíz del AST; NULL si no parsea
static NodeAST *parseProgram(const char *input) {
    freeAST(astRoot);
    astRoot = NULL;
    YY_BUFFER_STATE buffer = yy_scan_string(input);
    yyparse();
    yy_delete_buffer(buffer);
    return astRoot;
}

// Devuelve el cuerpo (bloque) del primer método del programa; NULL si no hay
static NodeAST *firstMethodBody(NodeAST *root) {
    if (root == NULL || root->childCount == 0) return NULL;
    NodeAST *method = root->children[0];
    return method->nodeType == METHOD_DECLARATION_NODE ? method->children[1] : NULL;
}

static void append(char *out, size_t size, const char *format, ...) {
    size_t used = strlen(out);
    va_list args;
    va_start(args, format);
    vsnprintf(out + used, size - used, format, args);
    va_end(args);
}

static const char *operatorSymbol(OperationType operation) {
    switch (operation) {
        case OP_ADD:      return "+";
        case OP_SUB:      return "-";
        case OP_MUL:      return "*";
        case OP_DIV:      return "/";
        case OP_MOD:      return "%";
        case OP_LT:       return "<";
        case OP_GT:       return ">";
        case OP_EQ:       return "==";
        case OP_AND:      return "&&";
        case OP_OR:       return "||";
        case OP_UMINUS:   return "neg";
        case OP_NOT:      return "!";
    }
    return "?";
}

// Escribe una expresión en notación prefija con paréntesis: "1 + 2 * 3" -> "(+ 1 (* 2 3))"
static void renderExpression(const NodeAST *node, char *out, size_t size) {
    switch (node->nodeType) {
        case ID_NODE:
            append(out, size, "%s", node->symbol->id);
            return;
        case CONSTANT_NODE:
            if (node->symbol->type == TYPE_INT)        append(out, size, "%d", node->symbol->value.intValue);
            else if (node->symbol->type == TYPE_FLOAT) append(out, size, "%g", node->symbol->value.floatValue);
            else                                       append(out, size, "%s", node->symbol->value.intValue ? "true" : "false");
            return;
        case OPERATOR_NODE:
            append(out, size, "(%s", operatorSymbol(node->operationType));
            break;
        case METHOD_CALL_NODE:
            append(out, size, "(call %s", node->symbol->id);
            break;
        default:
            append(out, size, "?");
            return;
    }
    for (int i = 0; i < node->childCount; i++) {
        append(out, size, " ");
        renderExpression(node->children[i], out, size);
    }
    append(out, size, ")");
}

// Parsea "void main() { x = <expression>; }" y devuelve la expresión como texto prefijo
static const char *parseExpression(const char *expression) {
    static char program[512];
    static char rendered[512];
    snprintf(program, sizeof program, "void main() { x = %s; }", expression);
    rendered[0] = '\0';

    NodeAST *body = firstMethodBody(parseProgram(program));
    if (body == NULL || body->childCount == 0) return "<no parsea>";
    renderExpression(body->children[0]->children[0], rendered, sizeof rendered);
    return rendered;
}

/* ------------------------------------------------------------------------
 * Precedencia y asociatividad de los operadores, según la tabla de la especificación:
 * - (menos unario), !, * / %, + -, < >, ==, &&, || de mayor a menor precedencia;
 * los operadores de igual precedencia se evalúan de izquierda a derecha.
 * ------------------------------------------------------------------------ */

// * / % ligan más fuerte que + -
void test_multiplicative_binds_tighter_than_additive(void) {
    TEST_ASSERT_EQUAL_STRING("(+ 1 (* 2 3))", parseExpression("1 + 2 * 3"));
    TEST_ASSERT_EQUAL_STRING("(+ (* 1 2) 3)", parseExpression("1 * 2 + 3"));
    TEST_ASSERT_EQUAL_STRING("(- 1 (/ 2 3))", parseExpression("1 - 2 / 3"));
    TEST_ASSERT_EQUAL_STRING("(+ 1 (% 2 3))", parseExpression("1 + 2 % 3"));
}

// + - ligan más fuerte que < >
void test_additive_binds_tighter_than_relational(void) {
    TEST_ASSERT_EQUAL_STRING("(< (+ a 1) (- b 2))", parseExpression("a + 1 < b - 2"));
    TEST_ASSERT_EQUAL_STRING("(> a (* b 2))", parseExpression("a > b * 2"));
}

// < > ligan más fuerte que ==
void test_relational_binds_tighter_than_equality(void) {
    TEST_ASSERT_EQUAL_STRING("(== (< a b) (> c d))", parseExpression("a < b == c > d"));
}

// == liga más fuerte que &&
void test_equality_binds_tighter_than_conjunction(void) {
    TEST_ASSERT_EQUAL_STRING("(&& (== a b) (== c d))", parseExpression("a == b && c == d"));
}

// && liga más fuerte que ||
void test_conjunction_binds_tighter_than_disjunction(void) {
    TEST_ASSERT_EQUAL_STRING("(|| a (&& b c))", parseExpression("a || b && c"));
    TEST_ASSERT_EQUAL_STRING("(|| (&& a b) c)", parseExpression("a && b || c"));
}

// El menos unario y ! ligan más fuerte que cualquier operador binario
void test_unary_operators_bind_tighter_than_binary(void) {
    TEST_ASSERT_EQUAL_STRING("(* (neg a) b)", parseExpression("-a * b"));
    TEST_ASSERT_EQUAL_STRING("(+ (neg a) b)", parseExpression("-a + b"));
    TEST_ASSERT_EQUAL_STRING("(== (! a) b)", parseExpression("!a == b"));
    TEST_ASSERT_EQUAL_STRING("(&& (! a) b)", parseExpression("!a && b"));
}

// Los operadores unarios se pueden apilar
void test_unary_operators_nest(void) {
    TEST_ASSERT_EQUAL_STRING("(neg (neg a))", parseExpression("- - a"));
    TEST_ASSERT_EQUAL_STRING("(! (! a))", parseExpression("!!a"));
    TEST_ASSERT_EQUAL_STRING("(neg (! a))", parseExpression("-!a"));
    TEST_ASSERT_EQUAL_STRING("(! (neg a))", parseExpression("!-a"));
}

// Operadores de igual precedencia: de izquierda a derecha
void test_same_precedence_is_left_associative(void) {
    TEST_ASSERT_EQUAL_STRING("(- (- 8 3) 2)", parseExpression("8 - 3 - 2"));
    TEST_ASSERT_EQUAL_STRING("(/ (/ 8 4) 2)", parseExpression("8 / 4 / 2"));
    TEST_ASSERT_EQUAL_STRING("(% (* a b) c)", parseExpression("a * b % c"));
    TEST_ASSERT_EQUAL_STRING("(+ (- a b) c)", parseExpression("a - b + c"));
    TEST_ASSERT_EQUAL_STRING("(> (< a b) c)", parseExpression("a < b > c"));
    TEST_ASSERT_EQUAL_STRING("(== (== a b) c)", parseExpression("a == b == c"));
    TEST_ASSERT_EQUAL_STRING("(&& (&& a b) c)", parseExpression("a && b && c"));
    TEST_ASSERT_EQUAL_STRING("(|| (|| a b) c)", parseExpression("a || b || c"));
}

// Los paréntesis modifican la precedencia y no dejan ningún nodo en el árbol
void test_parentheses_override_precedence(void) {
    TEST_ASSERT_EQUAL_STRING("(* (+ 1 2) 3)", parseExpression("(1 + 2) * 3"));
    TEST_ASSERT_EQUAL_STRING("(* (* a (+ b c)) d)", parseExpression("a * (b + c) * d"));
    TEST_ASSERT_EQUAL_STRING("(neg (+ a b))", parseExpression("-(a + b)"));
    TEST_ASSERT_EQUAL_STRING("(! (|| a b))", parseExpression("!(a || b)"));
    TEST_ASSERT_EQUAL_STRING("(- a (- b c))", parseExpression("a - (b - c)"));
    TEST_ASSERT_EQUAL_STRING("1", parseExpression("((1))"));
}

/* ------------------------------------------------------------------------
 * Operandos de las expresiones
 * ------------------------------------------------------------------------ */

// Identificadores y literales de los tres tipos
void test_expression_operands(void) {
    TEST_ASSERT_EQUAL_STRING("x", parseExpression("x"));
    TEST_ASSERT_EQUAL_STRING("42", parseExpression("42"));
    TEST_ASSERT_EQUAL_STRING("2.5", parseExpression("2.5"));
    TEST_ASSERT_EQUAL_STRING("true", parseExpression("true"));
    TEST_ASSERT_EQUAL_STRING("false", parseExpression("false"));
}

// Una llamada a método es una expresión más, con sus argumentos en orden
void test_method_calls_in_expressions(void) {
    TEST_ASSERT_EQUAL_STRING("(call f)", parseExpression("f()"));
    TEST_ASSERT_EQUAL_STRING("(call f 1 a)", parseExpression("f(1, a)"));
    TEST_ASSERT_EQUAL_STRING("(call f (call g 1) (+ 2 3))", parseExpression("f(g(1), 2 + 3)"));
    TEST_ASSERT_EQUAL_STRING("(+ (call f 1) (* 2 (call g)))", parseExpression("f(1) + 2 * g()"));
}

// Un literal es un nodo constante con su tipo y su valor
void test_constant_nodes_have_type_and_value(void) {
    NodeAST *body = firstMethodBody(parseProgram("void main() { x = 7; y = 2.5; z = true; w = false; }"));
    TEST_ASSERT_NOT_NULL(body);
    TEST_ASSERT_EQUAL_INT(4, body->childCount);

    NodeAST *integer = body->children[0]->children[0];
    TEST_ASSERT_EQUAL_INT(CONSTANT_NODE, integer->nodeType);
    TEST_ASSERT_EQUAL_INT(TYPE_INT, integer->symbol->type);
    TEST_ASSERT_EQUAL_INT(7, integer->symbol->value.intValue);

    NodeAST *real = body->children[1]->children[0];
    TEST_ASSERT_EQUAL_INT(TYPE_FLOAT, real->symbol->type);
    TEST_ASSERT_EQUAL_FLOAT(2.5f, real->symbol->value.floatValue);

    NodeAST *yes = body->children[2]->children[0];
    TEST_ASSERT_EQUAL_INT(TYPE_BOOL, yes->symbol->type);
    TEST_ASSERT_EQUAL_INT(1, yes->symbol->value.intValue);

    NodeAST *no = body->children[3]->children[0];
    TEST_ASSERT_EQUAL_INT(TYPE_BOOL, no->symbol->type);
    TEST_ASSERT_EQUAL_INT(0, no->symbol->value.intValue);
}

/* ------------------------------------------------------------------------
 * Estructura del programa: declaraciones globales y métodos
 * ------------------------------------------------------------------------ */

// "int x, y;" se desarma en una declaración por variable, con el mismo tipo
void test_global_declaration_list_is_split(void) {
    NodeAST *root = parseProgram("int x, y; float z; boolean b;");
    TEST_ASSERT_NOT_NULL(root);
    TEST_ASSERT_EQUAL_INT(BLOCK_NODE, root->nodeType);
    TEST_ASSERT_EQUAL_INT(4, root->childCount);

    const char *names[] = { "x", "y", "z", "b" };
    DataType types[] = { TYPE_INT, TYPE_INT, TYPE_FLOAT, TYPE_BOOL };
    for (int i = 0; i < 4; i++) {
        TEST_ASSERT_EQUAL_INT(VARIABLE_DECLARATION_NODE, root->children[i]->nodeType);
        TEST_ASSERT_EQUAL_STRING(names[i], root->children[i]->symbol->id);
        TEST_ASSERT_EQUAL_INT(types[i], root->children[i]->type);
    }
}

// Variables globales y métodos intercalados quedan en el orden del código fuente
void test_globals_and_methods_keep_source_order(void) {
    NodeAST *root = parseProgram("int a; void f() {} float b; void main() {}");
    TEST_ASSERT_NOT_NULL(root);
    TEST_ASSERT_EQUAL_INT(4, root->childCount);
    TEST_ASSERT_EQUAL_INT(VARIABLE_DECLARATION_NODE, root->children[0]->nodeType);
    TEST_ASSERT_EQUAL_INT(METHOD_DECLARATION_NODE, root->children[1]->nodeType);
    TEST_ASSERT_EQUAL_INT(VARIABLE_DECLARATION_NODE, root->children[2]->nodeType);
    TEST_ASSERT_EQUAL_INT(METHOD_DECLARATION_NODE, root->children[3]->nodeType);
    TEST_ASSERT_EQUAL_STRING("f", root->children[1]->symbol->id);
    TEST_ASSERT_EQUAL_STRING("main", root->children[3]->symbol->id);
}

// Un método guarda su nombre, su tipo de retorno, sus parámetros en orden y su cuerpo
void test_method_declaration_structure(void) {
    NodeAST *root = parseProgram("float f(int a, boolean b) { }");
    TEST_ASSERT_NOT_NULL(root);
    NodeAST *method = root->children[0];
    TEST_ASSERT_EQUAL_INT(METHOD_DECLARATION_NODE, method->nodeType);
    TEST_ASSERT_EQUAL_STRING("f", method->symbol->id);
    TEST_ASSERT_EQUAL_INT(TYPE_FLOAT, method->type);

    NodeAST *parameters = method->children[0];
    TEST_ASSERT_EQUAL_INT(PARAMETERS_NODE, parameters->nodeType);
    TEST_ASSERT_EQUAL_INT(2, parameters->childCount);
    TEST_ASSERT_EQUAL_STRING("a", parameters->children[0]->symbol->id);
    TEST_ASSERT_EQUAL_INT(TYPE_INT, parameters->children[0]->type);
    TEST_ASSERT_EQUAL_STRING("b", parameters->children[1]->symbol->id);
    TEST_ASSERT_EQUAL_INT(TYPE_BOOL, parameters->children[1]->type);

    TEST_ASSERT_EQUAL_INT(BLOCK_NODE, method->children[1]->nodeType);
    TEST_ASSERT_EQUAL_INT(0, method->children[1]->childCount);
}

// Un método void y sin parámetros
void test_void_method_without_parameters(void) {
    NodeAST *root = parseProgram("void main() { }");
    TEST_ASSERT_NOT_NULL(root);
    NodeAST *method = root->children[0];
    TEST_ASSERT_EQUAL_INT(TYPE_VOID, method->type);
    TEST_ASSERT_EQUAL_INT(PARAMETERS_NODE, method->children[0]->nodeType);
    TEST_ASSERT_EQUAL_INT(0, method->children[0]->childCount);
}

/* ------------------------------------------------------------------------
 * Estructura de los bloques y las sentencias
 * ------------------------------------------------------------------------ */

// En un bloque, las declaraciones locales van primero y las sentencias después, en orden
void test_block_keeps_declarations_then_statements_in_order(void) {
    NodeAST *body = firstMethodBody(parseProgram("void main() { int a; float b; a = 1; b = 2.0; }"));
    TEST_ASSERT_NOT_NULL(body);
    TEST_ASSERT_EQUAL_INT(4, body->childCount);
    TEST_ASSERT_EQUAL_INT(VARIABLE_DECLARATION_NODE, body->children[0]->nodeType);
    TEST_ASSERT_EQUAL_STRING("a", body->children[0]->symbol->id);
    TEST_ASSERT_EQUAL_INT(VARIABLE_DECLARATION_NODE, body->children[1]->nodeType);
    TEST_ASSERT_EQUAL_STRING("b", body->children[1]->symbol->id);
    TEST_ASSERT_EQUAL_INT(ASSIGNMENT_NODE, body->children[2]->nodeType);
    TEST_ASSERT_EQUAL_STRING("a", body->children[2]->symbol->id);
    TEST_ASSERT_EQUAL_INT(ASSIGNMENT_NODE, body->children[3]->nodeType);
    TEST_ASSERT_EQUAL_STRING("b", body->children[3]->symbol->id);
}

// Un bloque dentro de otro es una sentencia más, con sus propias declaraciones
void test_nested_block_is_a_statement(void) {
    NodeAST *body = firstMethodBody(parseProgram("void main() { { int x; x = 1; } }"));
    TEST_ASSERT_NOT_NULL(body);
    TEST_ASSERT_EQUAL_INT(1, body->childCount);
    NodeAST *inner = body->children[0];
    TEST_ASSERT_EQUAL_INT(BLOCK_NODE, inner->nodeType);
    TEST_ASSERT_EQUAL_INT(2, inner->childCount);
    TEST_ASSERT_EQUAL_INT(VARIABLE_DECLARATION_NODE, inner->children[0]->nodeType);
    TEST_ASSERT_EQUAL_INT(ASSIGNMENT_NODE, inner->children[1]->nodeType);
}

// Una asignación guarda la variable destino y tiene como único hijo la expresión
void test_assignment_structure(void) {
    NodeAST *body = firstMethodBody(parseProgram("void main() { x = 1 + 2; }"));
    TEST_ASSERT_NOT_NULL(body);
    NodeAST *assignment = body->children[0];
    TEST_ASSERT_EQUAL_INT(ASSIGNMENT_NODE, assignment->nodeType);
    TEST_ASSERT_EQUAL_STRING("x", assignment->symbol->id);
    TEST_ASSERT_EQUAL_INT(1, assignment->childCount);
    TEST_ASSERT_EQUAL_INT(OPERATOR_NODE, assignment->children[0]->nodeType);
}

// if sin else tiene condición y bloque; con else suma el bloque del else
void test_if_structure(void) {
    NodeAST *body = firstMethodBody(parseProgram("void main() { if (a) { } if (b) { x = 1; } else { } }"));
    TEST_ASSERT_NOT_NULL(body);
    TEST_ASSERT_EQUAL_INT(2, body->childCount);

    NodeAST *withoutElse = body->children[0];
    TEST_ASSERT_EQUAL_INT(IF_ELSE_NODE, withoutElse->nodeType);
    TEST_ASSERT_EQUAL_INT(2, withoutElse->childCount);
    TEST_ASSERT_EQUAL_INT(ID_NODE, withoutElse->children[0]->nodeType);
    TEST_ASSERT_EQUAL_INT(BLOCK_NODE, withoutElse->children[1]->nodeType);

    NodeAST *withElse = body->children[1];
    TEST_ASSERT_EQUAL_INT(3, withElse->childCount);
    TEST_ASSERT_EQUAL_INT(1, withElse->children[1]->childCount);
    TEST_ASSERT_EQUAL_INT(BLOCK_NODE, withElse->children[2]->nodeType);
    TEST_ASSERT_EQUAL_INT(0, withElse->children[2]->childCount);
}

// while tiene la condición y el bloque del cuerpo
void test_while_structure(void) {
    NodeAST *body = firstMethodBody(parseProgram("void main() { while (x < 3) { x = x + 1; } }"));
    TEST_ASSERT_NOT_NULL(body);
    NodeAST *loop = body->children[0];
    TEST_ASSERT_EQUAL_INT(WHILE_NODE, loop->nodeType);
    TEST_ASSERT_EQUAL_INT(2, loop->childCount);
    TEST_ASSERT_EQUAL_INT(OPERATOR_NODE, loop->children[0]->nodeType);
    TEST_ASSERT_EQUAL_INT(BLOCK_NODE, loop->children[1]->nodeType);
    TEST_ASSERT_EQUAL_INT(1, loop->children[1]->childCount);
}

// return tiene un hijo con la expresión, o ninguno si es un return vacío
void test_return_structure(void) {
    NodeAST *body = firstMethodBody(parseProgram("int f() { return 1; return; }"));
    TEST_ASSERT_NOT_NULL(body);
    TEST_ASSERT_EQUAL_INT(2, body->childCount);
    TEST_ASSERT_EQUAL_INT(RETURN_NODE, body->children[0]->nodeType);
    TEST_ASSERT_EQUAL_INT(1, body->children[0]->childCount);
    TEST_ASSERT_EQUAL_INT(RETURN_NODE, body->children[1]->nodeType);
    TEST_ASSERT_EQUAL_INT(0, body->children[1]->childCount);
}

// Una llamada como sentencia guarda el nombre del método y sus argumentos
void test_method_call_statement_structure(void) {
    NodeAST *body = firstMethodBody(parseProgram("void main() { f(); g(1, x); }"));
    TEST_ASSERT_NOT_NULL(body);
    TEST_ASSERT_EQUAL_INT(2, body->childCount);
    TEST_ASSERT_EQUAL_INT(METHOD_CALL_NODE, body->children[0]->nodeType);
    TEST_ASSERT_EQUAL_STRING("f", body->children[0]->symbol->id);
    TEST_ASSERT_EQUAL_INT(0, body->children[0]->childCount);
    TEST_ASSERT_EQUAL_STRING("g", body->children[1]->symbol->id);
    TEST_ASSERT_EQUAL_INT(2, body->children[1]->childCount);
}

/* ------------------------------------------------------------------------
 * Línea de cada nodo: la línea donde empieza la construcción en el código fuente
 * ------------------------------------------------------------------------ */

static const char *LINES_PROGRAM =
    "void main() {\n"      // 1
    "  int x;\n"           // 2
    "  x = 1;\n"           // 3
    "  if (x == 1) {\n"    // 4
    "    x = 2;\n"         // 5
    "  }\n"                // 6
    "  while (x < 3) {\n"  // 7
    "    x = x + 1;\n"     // 8
    "  }\n"                // 9
    "  return;\n"          // 10
    "}\n";                 // 11

// Una declaración y una asignación llevan la línea donde están escritas
void test_declaration_and_assignment_lines(void) {
    NodeAST *body = firstMethodBody(parseProgram(LINES_PROGRAM));
    TEST_ASSERT_NOT_NULL(body);
    TEST_ASSERT_EQUAL_INT(2, body->children[0]->line);
    TEST_ASSERT_EQUAL_INT(3, body->children[1]->line);
}

// Un return lleva la línea donde está escrito
void test_return_line(void) {
    NodeAST *body = firstMethodBody(parseProgram(LINES_PROGRAM));
    TEST_ASSERT_NOT_NULL(body);
    TEST_ASSERT_EQUAL_INT(10, body->children[4]->line);
}

// Un if sin else lleva la línea de su if, no la del token que viene después
void test_if_line(void) {
    NodeAST *body = firstMethodBody(parseProgram(LINES_PROGRAM));
    TEST_ASSERT_NOT_NULL(body);
    TEST_ASSERT_EQUAL_INT(4, body->children[2]->line);
}

// Un while lleva la línea de su while, no la del token que viene después
void test_while_line(void) {
    NodeAST *body = firstMethodBody(parseProgram(LINES_PROGRAM));
    TEST_ASSERT_NOT_NULL(body);
    TEST_ASSERT_EQUAL_INT(7, body->children[3]->line);
}

// Un método lleva la línea de su encabezado, no la de su llave de cierre
void test_method_line(void) {
    NodeAST *root = parseProgram(LINES_PROGRAM);
    TEST_ASSERT_NOT_NULL(root);
    TEST_ASSERT_EQUAL_INT(1, root->children[0]->line);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_multiplicative_binds_tighter_than_additive);
    RUN_TEST(test_additive_binds_tighter_than_relational);
    RUN_TEST(test_relational_binds_tighter_than_equality);
    RUN_TEST(test_equality_binds_tighter_than_conjunction);
    RUN_TEST(test_conjunction_binds_tighter_than_disjunction);
    RUN_TEST(test_unary_operators_bind_tighter_than_binary);
    RUN_TEST(test_unary_operators_nest);
    RUN_TEST(test_same_precedence_is_left_associative);
    RUN_TEST(test_parentheses_override_precedence);

    RUN_TEST(test_expression_operands);
    RUN_TEST(test_method_calls_in_expressions);
    RUN_TEST(test_constant_nodes_have_type_and_value);

    RUN_TEST(test_global_declaration_list_is_split);
    RUN_TEST(test_globals_and_methods_keep_source_order);
    RUN_TEST(test_method_declaration_structure);
    RUN_TEST(test_void_method_without_parameters);

    RUN_TEST(test_block_keeps_declarations_then_statements_in_order);
    RUN_TEST(test_nested_block_is_a_statement);
    RUN_TEST(test_assignment_structure);
    RUN_TEST(test_if_structure);
    RUN_TEST(test_while_structure);
    RUN_TEST(test_return_structure);
    RUN_TEST(test_method_call_statement_structure);

    RUN_TEST(test_declaration_and_assignment_lines);
    RUN_TEST(test_return_line);
    RUN_TEST(test_if_line);
    RUN_TEST(test_while_line);
    RUN_TEST(test_method_line);

    return UNITY_END();
}
