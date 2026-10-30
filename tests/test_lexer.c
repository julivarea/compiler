/**
 * @file test_lexer.c
 * @brief Pruebas unitarias para el analizador léxico (Flex) de C-TDS.
 */

#include <stdio.h>
#include <stdlib.h>
#include "unity.h"
#include "bison.tab.h"
#include "error_handler.h"

extern int yylex(void);
extern int yylineno;
extern YYSTYPE yylval;

typedef struct yy_buffer_state *YY_BUFFER_STATE;
extern YY_BUFFER_STATE yy_scan_string(const char *str);
extern void yy_delete_buffer(YY_BUFFER_STATE buffer);

#define ASSERT_ID(expected) do { \
    TEST_ASSERT_EQUAL_INT(ID, yylex()); \
    TEST_ASSERT_EQUAL_STRING((expected), yylval.strval); \
} while (0)

#define ASSERT_NUMBER(expected) do { \
    TEST_ASSERT_EQUAL_INT(NUMBER, yylex()); \
    TEST_ASSERT_EQUAL_INT((expected), yylval.intval); \
} while (0)

#define ASSERT_FLOAT(expected) do { \
    TEST_ASSERT_EQUAL_INT(FLOAT_CONST, yylex()); \
    TEST_ASSERT_FLOAT_WITHIN(0.001, (expected), yylval.floatval); \
} while (0)

#define ASSERT_BOOL(expected) do { \
    TEST_ASSERT_EQUAL_INT(BOOL_CONST, yylex()); \
    TEST_ASSERT_EQUAL_INT((expected), yylval.intval); \
} while (0)

/**
 * @brief Configuración previa a la ejecución de cada prueba unitaria.
 */
void setUp(void) {
    yylineno = 1;
    resetErrorHandler();
}

/**
 * @brief Limpieza posterior a la ejecución de cada prueba unitaria.
 */
void tearDown(void) {
}

// Las palabras reservadas devuelven su token; true y false dan BOOL_CONST 1 y 0.
void test_keywords_and_booleans(void) {
    const char *input = "boolean else if int return true false void while float";
    YY_BUFFER_STATE buffer = yy_scan_string(input);

    TEST_ASSERT_EQUAL_INT(BOOLEAN, yylex());
    TEST_ASSERT_EQUAL_INT(ELSE, yylex());
    TEST_ASSERT_EQUAL_INT(IF, yylex());
    TEST_ASSERT_EQUAL_INT(INT, yylex());
    TEST_ASSERT_EQUAL_INT(RETURN, yylex());
    ASSERT_BOOL(1);
    ASSERT_BOOL(0);
    TEST_ASSERT_EQUAL_INT(VOID, yylex());
    TEST_ASSERT_EQUAL_INT(WHILE, yylex());
    TEST_ASSERT_EQUAL_INT(FLOAT, yylex());

    TEST_ASSERT_EQUAL_INT(0, yylex());
    yy_delete_buffer(buffer);
}

// Identificadores válidos, incluidas palabras reservadas en mayúscula o combinadas.
void test_identifiers(void) {
    const char *input = "WHILE Int whiletrue cont";
    YY_BUFFER_STATE buffer = yy_scan_string(input);

    ASSERT_ID("WHILE");
    ASSERT_ID("Int");
    ASSERT_ID("whiletrue");
    ASSERT_ID("cont");

    TEST_ASSERT_EQUAL_INT(0, yylex());
    yy_delete_buffer(buffer);
}

// Enteros (incluido el máximo de 32 bits) y flotantes, con su valor respectivo.
void test_numbers(void) {
    const char *input = "0 2147483647 3.14 0.5";
    YY_BUFFER_STATE buffer = yy_scan_string(input);

    ASSERT_NUMBER(0);
    ASSERT_NUMBER(2147483647);
    ASSERT_FLOAT(3.14);
    ASSERT_FLOAT(0.5);

    TEST_ASSERT_EQUAL_INT(0, yylex());
    yy_delete_buffer(buffer);
}

// Cada operador y delimitador devuelve su token correspondiente.
void test_operators_and_delimiters(void) {
    const char *input = "+ - * / % = == < > && || ! ( ) { } ; ,";
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

    TEST_ASSERT_EQUAL_INT(0, yylex());
    yy_delete_buffer(buffer);
}

// Los comentarios de línea y multilínea no generan tokens.
void test_comments_are_ignored(void) {
    const char *input =
        "// esto es un comentario\n"
        "int x;\n"
        "/* comentario\n"
        " multilinea */\n"
        "x";
    YY_BUFFER_STATE buffer = yy_scan_string(input);

    TEST_ASSERT_EQUAL_INT(INT, yylex());
    ASSERT_ID("x");
    TEST_ASSERT_EQUAL_INT(';', yylex());
    ASSERT_ID("x");

    TEST_ASSERT_EQUAL_INT(0, yylex());
    yy_delete_buffer(buffer);
}

// Las palabras reservadas solo existen en minúscula.
void test_keywords_are_case_sensitive(void) {
    YY_BUFFER_STATE buffer = yy_scan_string("While TRUE False INT Void Return");

    ASSERT_ID("While");
    ASSERT_ID("TRUE");
    ASSERT_ID("False");
    ASSERT_ID("INT");
    ASSERT_ID("Void");
    ASSERT_ID("Return");

    TEST_ASSERT_EQUAL_INT(0, yylex());
    yy_delete_buffer(buffer);
}

// Maximal munch: una palabra reservada pegada a otras letras conforma un solo ID.
void test_keyword_prefix_is_identifier(void) {
    YY_BUFFER_STATE buffer = yy_scan_string("intx whiletrue ifelse truefalse voidmain floats main");

    ASSERT_ID("intx");
    ASSERT_ID("whiletrue");
    ASSERT_ID("ifelse");
    ASSERT_ID("truefalse");
    ASSERT_ID("voidmain");
    ASSERT_ID("floats");
    ASSERT_ID("main");

    TEST_ASSERT_EQUAL_INT(0, yylex());
    yy_delete_buffer(buffer);
}

// '_' vale después del primer carácter de un ID, pero no al principio.
void test_identifier_underscore_rules(void) {
    YY_BUFFER_STATE buffer = yy_scan_string("x_1 a__b _y");

    ASSERT_ID("x_1");
    ASSERT_ID("a__b");
    ASSERT_ID("y");

    TEST_ASSERT_EQUAL_INT(0, yylex());
    yy_delete_buffer(buffer);
}

// Un identificador no puede empezar con un dígito.
void test_identifier_cannot_start_with_digit(void) {
    YY_BUFFER_STATE buffer = yy_scan_string("1abc");

    ASSERT_NUMBER(1);
    ASSERT_ID("abc");

    TEST_ASSERT_EQUAL_INT(0, yylex());
    yy_delete_buffer(buffer);
}

// Los ceros a la izquierda son válidos en enteros y flotantes.
void test_leading_zeros(void) {
    YY_BUFFER_STATE buffer = yy_scan_string("007 00 0.0 00.50");

    ASSERT_NUMBER(7);
    ASSERT_NUMBER(0);
    ASSERT_FLOAT(0.0);
    ASSERT_FLOAT(0.5);

    TEST_ASSERT_EQUAL_INT(0, yylex());
    yy_delete_buffer(buffer);
}

// Un flotante necesita dígitos a ambos lados del punto; el '.' suelto es error léxico.
void test_incomplete_floats_are_not_float_const(void) {
    YY_BUFFER_STATE buffer = yy_scan_string("5. .5 1.2.3 3.e5");

    ASSERT_NUMBER(5);
    ASSERT_NUMBER(5);
    ASSERT_FLOAT(1.2);
    ASSERT_NUMBER(3);
    ASSERT_NUMBER(3);
    ASSERT_ID("e5");

    TEST_ASSERT_EQUAL_INT(0, yylex());
    yy_delete_buffer(buffer);
}

// <=, >=, != y === no existen en C-TDS: se parten en tokens válidos.
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

// '&' y '|' sueltos son error léxico y se descartan.
void test_single_ampersand_and_pipe_are_errors(void) {
    YY_BUFFER_STATE buffer = yy_scan_string("a & b | c &&& d");

    ASSERT_ID("a");
    ASSERT_ID("b");
    ASSERT_ID("c");
    TEST_ASSERT_EQUAL_INT(AND, yylex());
    ASSERT_ID("d");

    TEST_ASSERT_EQUAL_INT(0, yylex());
    yy_delete_buffer(buffer);
}

// Los símbolos no permitidos se reportan y se descartan.
void test_invalid_symbols_are_skipped(void) {
    YY_BUFFER_STATE buffer = yy_scan_string("a $ # @ ` ? : [ ] ~ ^ \" ' b");

    ASSERT_ID("a");
    ASSERT_ID("b");

    TEST_ASSERT_EQUAL_INT(0, yylex());
    yy_delete_buffer(buffer);
}

// Comentarios vacíos, con asteriscos y barras adentro, y con '//' o '*/' dentro de otro comentario.
void test_comment_edge_cases(void) {
    YY_BUFFER_STATE buffer = yy_scan_string(
        "/**/a /***/b /* * / ** */c /* x // y */d // z */ e\nf");

    ASSERT_ID("a");
    ASSERT_ID("b");
    ASSERT_ID("c");
    ASSERT_ID("d");
    ASSERT_ID("f");

    TEST_ASSERT_EQUAL_INT(0, yylex());
    yy_delete_buffer(buffer);
}

// Los comentarios no se anidan: cierran en el primer */.
void test_nested_comment_closes_at_first_end(void) {
    YY_BUFFER_STATE buffer = yy_scan_string("/* /* */ */q");

    TEST_ASSERT_EQUAL_INT('*', yylex());
    TEST_ASSERT_EQUAL_INT('/', yylex());
    ASSERT_ID("q");

    TEST_ASSERT_EQUAL_INT(0, yylex());
    yy_delete_buffer(buffer);
}

// Un comentario de línea al final del archivo, sin salto de línea.
void test_line_comment_at_eof_without_newline(void) {
    YY_BUFFER_STATE buffer = yy_scan_string("a // fin");

    ASSERT_ID("a");

    TEST_ASSERT_EQUAL_INT(0, yylex());
    yy_delete_buffer(buffer);
}

// Una barra sola es división, no el comienzo de un comentario.
void test_division_is_not_a_comment(void) {
    YY_BUFFER_STATE buffer = yy_scan_string("a/b a/*c*/b");

    ASSERT_ID("a");
    TEST_ASSERT_EQUAL_INT('/', yylex());
    ASSERT_ID("b");
    ASSERT_ID("a");
    ASSERT_ID("b");

    TEST_ASSERT_EQUAL_INT(0, yylex());
    yy_delete_buffer(buffer);
}

// Tabs y saltos de línea CRLF son espacio en blanco.
void test_whitespace_tabs_and_crlf(void) {
    YY_BUFFER_STATE buffer = yy_scan_string(" \t a\r\n\tb\r\n\r\nc \t");

    ASSERT_ID("a");
    ASSERT_ID("b");
    ASSERT_ID("c");

    TEST_ASSERT_EQUAL_INT(0, yylex());
    yy_delete_buffer(buffer);
}

// La entrada vacía devuelve fin de archivo.
void test_empty_input(void) {
    YY_BUFFER_STATE buffer = yy_scan_string("");

    TEST_ASSERT_EQUAL_INT(0, yylex());
    yy_delete_buffer(buffer);
}

// Un archivo con solo comentarios y blancos no genera tokens.
void test_only_comments_and_spaces(void) {
    YY_BUFFER_STATE buffer = yy_scan_string("  // a\n/* b\n c */\n\t");

    TEST_ASSERT_EQUAL_INT(0, yylex());
    yy_delete_buffer(buffer);
}

// yylineno cuenta bien las líneas, incluso dentro de comentarios y con CRLF.
void test_line_tracking(void) {
    YY_BUFFER_STATE buffer = yy_scan_string(
        "a\n"
        "b\r\n"
        "/* uno\n"
        "   dos */ c\n"
        "// comentario\n"
        "\n"
        "d");

    ASSERT_ID("a");
    TEST_ASSERT_EQUAL_INT(1, yylineno);

    ASSERT_ID("b");
    TEST_ASSERT_EQUAL_INT(2, yylineno);

    ASSERT_ID("c");
    TEST_ASSERT_EQUAL_INT(4, yylineno);

    ASSERT_ID("d");
    TEST_ASSERT_EQUAL_INT(7, yylineno);

    TEST_ASSERT_EQUAL_INT(0, yylex());
    yy_delete_buffer(buffer);
}

// Una expresión sin espacios se separa en los tokens correctos.
void test_no_spaces_between_tokens(void) {
    YY_BUFFER_STATE buffer = yy_scan_string("x=-a*(b+1.5)%2==!c&&d||e;");

    ASSERT_ID("x");
    TEST_ASSERT_EQUAL_INT('=', yylex());
    TEST_ASSERT_EQUAL_INT('-', yylex());
    ASSERT_ID("a");
    TEST_ASSERT_EQUAL_INT('*', yylex());
    TEST_ASSERT_EQUAL_INT('(', yylex());
    ASSERT_ID("b");
    TEST_ASSERT_EQUAL_INT('+', yylex());
    ASSERT_FLOAT(1.5);
    TEST_ASSERT_EQUAL_INT(')', yylex());
    TEST_ASSERT_EQUAL_INT('%', yylex());
    ASSERT_NUMBER(2);
    TEST_ASSERT_EQUAL_INT(EQ, yylex());
    TEST_ASSERT_EQUAL_INT(NOT, yylex());
    ASSERT_ID("c");
    TEST_ASSERT_EQUAL_INT(AND, yylex());
    ASSERT_ID("d");
    TEST_ASSERT_EQUAL_INT(OR, yylex());
    ASSERT_ID("e");
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
