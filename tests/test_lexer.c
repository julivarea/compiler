#include "unity.h"
#include "bison.tab.h"
#include <stdio.h>
#include <stdlib.h>

extern int yylex(void);
extern int yylineno;
extern YYSTYPE yylval;

typedef struct yy_buffer_state *YY_BUFFER_STATE;
extern YY_BUFFER_STATE yy_scan_string(const char * str);
extern void yy_delete_buffer(YY_BUFFER_STATE buffer);

void setUp(void) {
    yylineno = 1; // Reiniciamos la línea antes de cada test
}
void tearDown(void) {}

// Las 10 palabras reservadas devuelven su token; true y false dan BOOL_CONST 1 y 0
void test_keywords_and_booleans(void) {
    const char* input = "boolean else if int return true false void while float";
    YY_BUFFER_STATE buffer = yy_scan_string(input);
    
    TEST_ASSERT_EQUAL_INT(BOOLEAN, yylex());
    TEST_ASSERT_EQUAL_INT(ELSE, yylex());
    TEST_ASSERT_EQUAL_INT(IF, yylex());
    TEST_ASSERT_EQUAL_INT(INT, yylex());
    TEST_ASSERT_EQUAL_INT(RETURN, yylex());
    
    // Verificamos constantes booleanas
    TEST_ASSERT_EQUAL_INT(BOOL_CONST, yylex());
    TEST_ASSERT_EQUAL_INT(1, yylval.intval);
    
    TEST_ASSERT_EQUAL_INT(BOOL_CONST, yylex());
    TEST_ASSERT_EQUAL_INT(0, yylval.intval);
    
    TEST_ASSERT_EQUAL_INT(VOID, yylex());
    TEST_ASSERT_EQUAL_INT(WHILE, yylex());
    TEST_ASSERT_EQUAL_INT(FLOAT, yylex());
    
    TEST_ASSERT_EQUAL_INT(0, yylex()); // 0 significa End of File
    yy_delete_buffer(buffer);
}

// Identificadores válidos, incluidas palabras reservadas en mayúscula o pegadas a otras
void test_identifiers(void) {
    const char* input = "WHILE Int whiletrue cont";
    YY_BUFFER_STATE buffer = yy_scan_string(input);
    
    TEST_ASSERT_EQUAL_INT(ID, yylex());
    TEST_ASSERT_EQUAL_STRING("WHILE", yylval.strval);
    
    TEST_ASSERT_EQUAL_INT(ID, yylex());
    TEST_ASSERT_EQUAL_STRING("Int", yylval.strval);
    
    TEST_ASSERT_EQUAL_INT(ID, yylex());
    TEST_ASSERT_EQUAL_STRING("whiletrue", yylval.strval);
    
    TEST_ASSERT_EQUAL_INT(ID, yylex());
    TEST_ASSERT_EQUAL_STRING("cont", yylval.strval);
    
    yy_delete_buffer(buffer);
}

// Enteros (incluido el máximo de 32 bits) y floats, con su valor
void test_numbers(void) {
    const char* input = "0 2147483647 3.14 0.5";
    YY_BUFFER_STATE buffer = yy_scan_string(input);
    
    TEST_ASSERT_EQUAL_INT(NUMBER, yylex());
    TEST_ASSERT_EQUAL_INT(0, yylval.intval);
    
    TEST_ASSERT_EQUAL_INT(NUMBER, yylex());
    TEST_ASSERT_EQUAL_INT(2147483647, yylval.intval);
    
    TEST_ASSERT_EQUAL_INT(FLOAT_CONST, yylex());
    TEST_ASSERT_FLOAT_WITHIN(0.001, 3.14, yylval.floatval);
    
    TEST_ASSERT_EQUAL_INT(FLOAT_CONST, yylex());
    TEST_ASSERT_FLOAT_WITHIN(0.001, 0.5, yylval.floatval);
    
    yy_delete_buffer(buffer);
}

// Cada operador y delimitador devuelve su token
void test_operators_and_delimiters(void) {
    const char* input = "+ - * / % = == < > && || ! ( ) { } ; ,";
    YY_BUFFER_STATE buffer = yy_scan_string(input);
    
    // Aritméticos y asignación (devuelven su propio código ASCII)
    TEST_ASSERT_EQUAL_INT('+', yylex());
    TEST_ASSERT_EQUAL_INT('-', yylex());
    TEST_ASSERT_EQUAL_INT('*', yylex());
    TEST_ASSERT_EQUAL_INT('/', yylex());
    TEST_ASSERT_EQUAL_INT('%', yylex());
    TEST_ASSERT_EQUAL_INT('=', yylex());
    
    // Relacionales y Lógicos (devuelven tokens de Bison)
    TEST_ASSERT_EQUAL_INT(EQ, yylex());
    TEST_ASSERT_EQUAL_INT('<', yylex());
    TEST_ASSERT_EQUAL_INT('>', yylex());
    TEST_ASSERT_EQUAL_INT(AND, yylex());
    TEST_ASSERT_EQUAL_INT(OR, yylex());
    TEST_ASSERT_EQUAL_INT(NOT, yylex());
    
    // Delimitadores
    TEST_ASSERT_EQUAL_INT('(', yylex());
    TEST_ASSERT_EQUAL_INT(')', yylex());
    TEST_ASSERT_EQUAL_INT('{', yylex());
    TEST_ASSERT_EQUAL_INT('}', yylex());
    TEST_ASSERT_EQUAL_INT(';', yylex());
    TEST_ASSERT_EQUAL_INT(',', yylex());
    
    yy_delete_buffer(buffer);
}

// Los comentarios de línea y multilínea no generan tokens
void test_comments_are_ignored(void) {
    const char* input = 
        "// esto es un comentario\n"
        "int x;\n"
        "/* comentario\n"
        " multilinea */\n"
        "x";
    YY_BUFFER_STATE buffer = yy_scan_string(input);
    
    // El lexer debe ignorar completamente los comentarios.
    // El primer token real es 'int'.
    TEST_ASSERT_EQUAL_INT(INT, yylex());
    TEST_ASSERT_EQUAL_INT(ID, yylex());
    TEST_ASSERT_EQUAL_INT(';', yylex());
    
    // Ignora el multilínea y sigue
    TEST_ASSERT_EQUAL_INT(ID, yylex());
    TEST_ASSERT_EQUAL_STRING("x", yylval.strval);
    
    yy_delete_buffer(buffer);
}

static void expect_id(const char *name) {
    TEST_ASSERT_EQUAL_INT(ID, yylex());
    TEST_ASSERT_EQUAL_STRING(name, yylval.strval);
}

// Las palabras reservadas solo existen en minúscula
void test_keywords_are_case_sensitive(void) {
    YY_BUFFER_STATE buffer = yy_scan_string("While TRUE False INT Void Return");
    expect_id("While");
    expect_id("TRUE");
    expect_id("False");
    expect_id("INT");
    expect_id("Void");
    expect_id("Return");
    TEST_ASSERT_EQUAL_INT(0, yylex());
    yy_delete_buffer(buffer);
}

// Maximal munch: una palabra reservada pegada a otras letras es un solo ID
void test_keyword_prefix_is_identifier(void) {
    YY_BUFFER_STATE buffer = yy_scan_string("intx whiletrue ifelse truefalse voidmain floats main");
    expect_id("intx");
    expect_id("whiletrue");
    expect_id("ifelse");
    expect_id("truefalse");
    expect_id("voidmain");
    expect_id("floats");
    expect_id("main");
    TEST_ASSERT_EQUAL_INT(0, yylex());
    yy_delete_buffer(buffer);
}

// '_' vale después del primer carácter de un ID, pero no al principio
void test_identifier_underscore_rules(void) {
    YY_BUFFER_STATE buffer = yy_scan_string("x_1 a__b _y");
    expect_id("x_1");
    expect_id("a__b");
    expect_id("y");
    TEST_ASSERT_EQUAL_INT(0, yylex());
    yy_delete_buffer(buffer);
}

// Un identificador no puede empezar con un dígito
void test_identifier_cannot_start_with_digit(void) {
    YY_BUFFER_STATE buffer = yy_scan_string("1abc");
    TEST_ASSERT_EQUAL_INT(NUMBER, yylex());
    TEST_ASSERT_EQUAL_INT(1, yylval.intval);
    expect_id("abc");
    yy_delete_buffer(buffer);
}

// Los ceros a la izquierda son válidos en enteros y floats
void test_leading_zeros(void) {
    YY_BUFFER_STATE buffer = yy_scan_string("007 00 0.0 00.50");
    TEST_ASSERT_EQUAL_INT(NUMBER, yylex());
    TEST_ASSERT_EQUAL_INT(7, yylval.intval);
    TEST_ASSERT_EQUAL_INT(NUMBER, yylex());
    TEST_ASSERT_EQUAL_INT(0, yylval.intval);
    TEST_ASSERT_EQUAL_INT(FLOAT_CONST, yylex());
    TEST_ASSERT_FLOAT_WITHIN(0.001, 0.0, yylval.floatval);
    TEST_ASSERT_EQUAL_INT(FLOAT_CONST, yylex());
    TEST_ASSERT_FLOAT_WITHIN(0.001, 0.5, yylval.floatval);
    yy_delete_buffer(buffer);
}

// Un float necesita dígitos a ambos lados del punto; el '.' suelto es error léxico
void test_incomplete_floats_are_not_float_const(void) {
    YY_BUFFER_STATE buffer = yy_scan_string("5. .5 1.2.3 3.e5");
    TEST_ASSERT_EQUAL_INT(NUMBER, yylex());
    TEST_ASSERT_EQUAL_INT(5, yylval.intval);
    TEST_ASSERT_EQUAL_INT(NUMBER, yylex());
    TEST_ASSERT_EQUAL_INT(5, yylval.intval);
    TEST_ASSERT_EQUAL_INT(FLOAT_CONST, yylex());
    TEST_ASSERT_FLOAT_WITHIN(0.001, 1.2, yylval.floatval);
    TEST_ASSERT_EQUAL_INT(NUMBER, yylex());
    TEST_ASSERT_EQUAL_INT(3, yylval.intval);
    TEST_ASSERT_EQUAL_INT(NUMBER, yylex());
    TEST_ASSERT_EQUAL_INT(3, yylval.intval);
    expect_id("e5");
    TEST_ASSERT_EQUAL_INT(0, yylex());
    yy_delete_buffer(buffer);
}

// <=, >=, != y === no existen en C-TDS: se parten en tokens válidos
void test_operators_not_in_language_split(void) {
    YY_BUFFER_STATE buffer = yy_scan_string("<= >= != === !!");
    TEST_ASSERT_EQUAL_INT('<', yylex());
    TEST_ASSERT_EQUAL_INT('=', yylex());
    TEST_ASSERT_EQUAL_INT('>', yylex());
    TEST_ASSERT_EQUAL_INT('=', yylex());
    TEST_ASSERT_EQUAL_INT(NOT, yylex());
    TEST_ASSERT_EQUAL_INT('=', yylex());
    TEST_ASSERT_EQUAL_INT(EQ, yylex());
    TEST_ASSERT_EQUAL_INT('=', yylex());
    TEST_ASSERT_EQUAL_INT(NOT, yylex());
    TEST_ASSERT_EQUAL_INT(NOT, yylex());
    TEST_ASSERT_EQUAL_INT(0, yylex());
    yy_delete_buffer(buffer);
}

// '&' y '|' sueltos son error léxico y se descartan
void test_single_ampersand_and_pipe_are_errors(void) {
    YY_BUFFER_STATE buffer = yy_scan_string("a & b | c &&& d");
    expect_id("a");
    expect_id("b");
    expect_id("c");
    TEST_ASSERT_EQUAL_INT(AND, yylex());
    expect_id("d");
    TEST_ASSERT_EQUAL_INT(0, yylex());
    yy_delete_buffer(buffer);
}

// Los símbolos no permitidos se reportan y se descartan
void test_invalid_symbols_are_skipped(void) {
    YY_BUFFER_STATE buffer = yy_scan_string("a $ # @ ` ? : [ ] ~ ^ \" ' b");
    expect_id("a");
    expect_id("b");
    TEST_ASSERT_EQUAL_INT(0, yylex());
    yy_delete_buffer(buffer);
}

// Comentarios vacíos, con asteriscos y barras adentro, y con '//' o '*/' dentro de otro comentario
void test_comment_edge_cases(void) {
    YY_BUFFER_STATE buffer = yy_scan_string(
        "/**/a /***/b /* * / ** */c /* x // y */d // z */ e\nf");
    expect_id("a");
    expect_id("b");
    expect_id("c");
    expect_id("d");
    expect_id("f");
    TEST_ASSERT_EQUAL_INT(0, yylex());
    yy_delete_buffer(buffer);
}

// Los comentarios no se anidan (como en C): cierran en el primer */
void test_nested_comment_closes_at_first_end(void) {
    YY_BUFFER_STATE buffer = yy_scan_string("/* /* */ */q");
    TEST_ASSERT_EQUAL_INT('*', yylex());
    TEST_ASSERT_EQUAL_INT('/', yylex());
    expect_id("q");
    yy_delete_buffer(buffer);
}

// Un comentario de línea al final del archivo, sin salto de línea
void test_line_comment_at_eof_without_newline(void) {
    YY_BUFFER_STATE buffer = yy_scan_string("a // fin");
    expect_id("a");
    TEST_ASSERT_EQUAL_INT(0, yylex());
    yy_delete_buffer(buffer);
}

// Una barra sola es división, no el comienzo de un comentario
void test_division_is_not_a_comment(void) {
    YY_BUFFER_STATE buffer = yy_scan_string("a/b a/*c*/b");
    expect_id("a");
    TEST_ASSERT_EQUAL_INT('/', yylex());
    expect_id("b");
    expect_id("a");
    expect_id("b");
    TEST_ASSERT_EQUAL_INT(0, yylex());
    yy_delete_buffer(buffer);
}

// Tabs y saltos de línea CRLF son espacio en blanco
void test_whitespace_tabs_and_crlf(void) {
    YY_BUFFER_STATE buffer = yy_scan_string(" \t a\r\n\tb\r\n\r\nc \t");
    expect_id("a");
    expect_id("b");
    expect_id("c");
    TEST_ASSERT_EQUAL_INT(0, yylex());
    yy_delete_buffer(buffer);
}

// La entrada vacía devuelve fin de archivo
void test_empty_input(void) {
    YY_BUFFER_STATE buffer = yy_scan_string("");
    TEST_ASSERT_EQUAL_INT(0, yylex());
    yy_delete_buffer(buffer);
}

// Un archivo con solo comentarios y blancos no genera tokens
void test_only_comments_and_spaces(void) {
    YY_BUFFER_STATE buffer = yy_scan_string("  // a\n/* b\n c */\n\t");
    TEST_ASSERT_EQUAL_INT(0, yylex());
    yy_delete_buffer(buffer);
}

// yylineno cuenta bien las líneas, incluso dentro de comentarios y con CRLF
void test_line_tracking(void) {
    YY_BUFFER_STATE buffer = yy_scan_string(
        "a\n"
        "b\r\n"
        "/* uno\n"
        "   dos */ c\n"
        "// comentario\n"
        "\n"
        "d");
    expect_id("a");
    TEST_ASSERT_EQUAL_INT(1, yylineno);
    expect_id("b");
    TEST_ASSERT_EQUAL_INT(2, yylineno);
    expect_id("c");
    TEST_ASSERT_EQUAL_INT(4, yylineno);
    expect_id("d");
    TEST_ASSERT_EQUAL_INT(7, yylineno);
    yy_delete_buffer(buffer);
}

// Una expresión sin espacios se separa en los tokens correctos
void test_no_spaces_between_tokens(void) {
    YY_BUFFER_STATE buffer = yy_scan_string("x=-a*(b+1.5)%2==!c&&d||e;");
    expect_id("x");
    TEST_ASSERT_EQUAL_INT('=', yylex());
    TEST_ASSERT_EQUAL_INT('-', yylex());
    expect_id("a");
    TEST_ASSERT_EQUAL_INT('*', yylex());
    TEST_ASSERT_EQUAL_INT('(', yylex());
    expect_id("b");
    TEST_ASSERT_EQUAL_INT('+', yylex());
    TEST_ASSERT_EQUAL_INT(FLOAT_CONST, yylex());
    TEST_ASSERT_EQUAL_INT(')', yylex());
    TEST_ASSERT_EQUAL_INT('%', yylex());
    TEST_ASSERT_EQUAL_INT(NUMBER, yylex());
    TEST_ASSERT_EQUAL_INT(EQ, yylex());
    TEST_ASSERT_EQUAL_INT(NOT, yylex());
    expect_id("c");
    TEST_ASSERT_EQUAL_INT(AND, yylex());
    expect_id("d");
    TEST_ASSERT_EQUAL_INT(OR, yylex());
    expect_id("e");
    TEST_ASSERT_EQUAL_INT(';', yylex());
    TEST_ASSERT_EQUAL_INT(0, yylex());
    yy_delete_buffer(buffer);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_keywords_and_booleans);
    RUN_TEST(test_identifiers);
    RUN_TEST(test_numbers);
    RUN_TEST(test_operators_and_delimiters);
    RUN_TEST(test_comments_are_ignored);
    RUN_TEST(test_keywords_are_case_sensitive);
    RUN_TEST(test_keyword_prefix_is_identifier);
    RUN_TEST(test_identifier_underscore_rules);
    RUN_TEST(test_identifier_cannot_start_with_digit);
    RUN_TEST(test_leading_zeros);
    RUN_TEST(test_incomplete_floats_are_not_float_const);
    RUN_TEST(test_operators_not_in_language_split);
    RUN_TEST(test_single_ampersand_and_pipe_are_errors);
    RUN_TEST(test_invalid_symbols_are_skipped);
    RUN_TEST(test_comment_edge_cases);
    RUN_TEST(test_nested_comment_closes_at_first_end);
    RUN_TEST(test_line_comment_at_eof_without_newline);
    RUN_TEST(test_division_is_not_a_comment);
    RUN_TEST(test_whitespace_tabs_and_crlf);
    RUN_TEST(test_empty_input);
    RUN_TEST(test_only_comments_and_spaces);
    RUN_TEST(test_line_tracking);
    RUN_TEST(test_no_spaces_between_tokens);
    return UNITY_END();
}
