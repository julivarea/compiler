#include "unity.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void setUp(void) {}
void tearDown(void) {}

typedef struct yy_buffer_state *YY_BUFFER_STATE;
extern YY_BUFFER_STATE yy_scan_string(const char * str);
extern void yy_delete_buffer(YY_BUFFER_STATE buffer);
extern int yylex(void);
extern int yylineno;

void test_lexer_output(const char* input, const char* expected) {
    fflush(stdout);

    FILE *temp = tmpfile();
    int old_stdout = dup(1);
    dup2(fileno(temp), 1);

    yylineno = 1;
    YY_BUFFER_STATE buffer = yy_scan_string(input);
    yylex();
    yy_delete_buffer(buffer);
    fflush(stdout);

    dup2(old_stdout, 1);
    close(old_stdout);

    fseek(temp, 0, SEEK_END);
    long size = ftell(temp);
    fseek(temp, 0, SEEK_SET);
    char *actual = malloc(size + 1);
    if (size > 0) fread(actual, 1, size, temp);
    actual[size] = 0;
    fclose(temp);

    TEST_ASSERT_EQUAL_STRING(expected, actual);
    free(actual);
}

void test_sequence_1(void) {
    const char* input = 
        "boolean else if int return true false void while float";
    const char* expected = 
        "[Linea 1] BOOLEAN: boolean\n"
        "[Linea 1] ELSE: else\n"
        "[Linea 1] IF: if\n"
        "[Linea 1] INT: int\n"
        "[Linea 1] RETURN: return\n"
        "[Linea 1] BOOLEAN_CONST: true\n"
        "[Linea 1] BOOLEAN_CONST: false\n"
        "[Linea 1] VOID: void\n"
        "[Linea 1] WHILE: while\n"
        "[Linea 1] FLOAT: float\n";
    test_lexer_output(input, expected);
}

void test_sequence_2(void) {
    const char* input = 
        "WHILE Int Return whiletrue cont Cont var1 x1 aB3";
    const char* expected = 
        "[Linea 1] ID: WHILE\n"
        "[Linea 1] ID: Int\n"
        "[Linea 1] ID: Return\n"
        "[Linea 1] ID: whiletrue\n"
        "[Linea 1] ID: cont\n"
        "[Linea 1] ID: Cont\n"
        "[Linea 1] ID: var1\n"
        "[Linea 1] ID: x1\n"
        "[Linea 1] ID: aB3\n";
    test_lexer_output(input, expected);
}

void test_sequence_3(void) {
    const char* input = 
        "0 7 123 2147483647\n"
        "3.14 0.5 100.001";
    const char* expected = 
        "[Linea 1] NUMBER: 0\n"
        "[Linea 1] NUMBER: 7\n"
        "[Linea 1] NUMBER: 123\n"
        "[Linea 1] NUMBER: 2147483647\n"
        "[Linea 2] FLOAT_CONST: 3.14\n"
        "[Linea 2] FLOAT_CONST: 0.5\n"
        "[Linea 2] FLOAT_CONST: 100.001\n";
    test_lexer_output(input, expected);
}

void test_sequence_4(void) {
    const char* input = 
        "a + b - c * d / e % f";
    const char* expected = 
        "[Linea 1] ID: a\n"
        "[Linea 1] OPERATOR: +\n"
        "[Linea 1] ID: b\n"
        "[Linea 1] OPERATOR: -\n"
        "[Linea 1] ID: c\n"
        "[Linea 1] OPERATOR: *\n"
        "[Linea 1] ID: d\n"
        "[Linea 1] OPERATOR: /\n"
        "[Linea 1] ID: e\n"
        "[Linea 1] OPERATOR: %\n"
        "[Linea 1] ID: f\n";
    test_lexer_output(input, expected);
}

void test_sequence_5(void) {
    const char* input = 
        "x = 5;\n"
        "x == 5;";
    const char* expected = 
        "[Linea 1] ID: x\n"
        "[Linea 1] OPERATOR: =\n"
        "[Linea 1] NUMBER: 5\n"
        "[Linea 1] DELIMITER: ;\n"
        "[Linea 2] ID: x\n"
        "[Linea 2] OPERATOR: ==\n"
        "[Linea 2] NUMBER: 5\n"
        "[Linea 2] DELIMITER: ;\n";
    test_lexer_output(input, expected);
}

void test_sequence_6(void) {
    const char* input = 
        "a < b\n"
        "a > b";
    const char* expected = 
        "[Linea 1] ID: a\n"
        "[Linea 1] OPERATOR: <\n"
        "[Linea 1] ID: b\n"
        "[Linea 2] ID: a\n"
        "[Linea 2] OPERATOR: >\n"
        "[Linea 2] ID: b\n";
    test_lexer_output(input, expected);
}

void test_sequence_7(void) {
    const char* input = 
        "a && b\n"
        "a || b\n"
        "!a";
    const char* expected = 
        "[Linea 1] ID: a\n"
        "[Linea 1] OPERATOR: &&\n"
        "[Linea 1] ID: b\n"
        "[Linea 2] ID: a\n"
        "[Linea 2] OPERATOR: ||\n"
        "[Linea 2] ID: b\n"
        "[Linea 3] OPERATOR: !\n"
        "[Linea 3] ID: a\n";
    test_lexer_output(input, expected);
}

void test_sequence_8(void) {
    const char* input = 
        "( ) { } ; ,";
    const char* expected = 
        "[Linea 1] DELIMITER: (\n"
        "[Linea 1] DELIMITER: )\n"
        "[Linea 1] DELIMITER: {\n"
        "[Linea 1] DELIMITER: }\n"
        "[Linea 1] DELIMITER: ;\n"
        "[Linea 1] DELIMITER: ,\n";
    test_lexer_output(input, expected);
}

void test_sequence_9(void) {
    const char* input = 
        "int inc(int x, int y) {\n"
        "    float pi;\n"
        "    pi = 3.14;\n"
        "    if (x == 1 && y > 0) {\n"
        "        return inc(x, y);\n"
        "    } else {\n"
        "        while (x < 10) {\n"
        "            x = x + 1;\n"
        "        }\n"
        "    }\n"
        "    return x;\n"
        "}";
    const char* expected = 
        "[Linea 1] INT: int\n"
        "[Linea 1] ID: inc\n"
        "[Linea 1] DELIMITER: (\n"
        "[Linea 1] INT: int\n"
        "[Linea 1] ID: x\n"
        "[Linea 1] DELIMITER: ,\n"
        "[Linea 1] INT: int\n"
        "[Linea 1] ID: y\n"
        "[Linea 1] DELIMITER: )\n"
        "[Linea 1] DELIMITER: {\n"
        "[Linea 2] FLOAT: float\n"
        "[Linea 2] ID: pi\n"
        "[Linea 2] DELIMITER: ;\n"
        "[Linea 3] ID: pi\n"
        "[Linea 3] OPERATOR: =\n"
        "[Linea 3] FLOAT_CONST: 3.14\n"
        "[Linea 3] DELIMITER: ;\n"
        "[Linea 4] IF: if\n"
        "[Linea 4] DELIMITER: (\n"
        "[Linea 4] ID: x\n"
        "[Linea 4] OPERATOR: ==\n"
        "[Linea 4] NUMBER: 1\n"
        "[Linea 4] OPERATOR: &&\n"
        "[Linea 4] ID: y\n"
        "[Linea 4] OPERATOR: >\n"
        "[Linea 4] NUMBER: 0\n"
        "[Linea 4] DELIMITER: )\n"
        "[Linea 4] DELIMITER: {\n"
        "[Linea 5] RETURN: return\n"
        "[Linea 5] ID: inc\n"
        "[Linea 5] DELIMITER: (\n"
        "[Linea 5] ID: x\n"
        "[Linea 5] DELIMITER: ,\n"
        "[Linea 5] ID: y\n"
        "[Linea 5] DELIMITER: )\n"
        "[Linea 5] DELIMITER: ;\n"
        "[Linea 6] DELIMITER: }\n"
        "[Linea 6] ELSE: else\n"
        "[Linea 6] DELIMITER: {\n"
        "[Linea 7] WHILE: while\n"
        "[Linea 7] DELIMITER: (\n"
        "[Linea 7] ID: x\n"
        "[Linea 7] OPERATOR: <\n"
        "[Linea 7] NUMBER: 10\n"
        "[Linea 7] DELIMITER: )\n"
        "[Linea 7] DELIMITER: {\n"
        "[Linea 8] ID: x\n"
        "[Linea 8] OPERATOR: =\n"
        "[Linea 8] ID: x\n"
        "[Linea 8] OPERATOR: +\n"
        "[Linea 8] NUMBER: 1\n"
        "[Linea 8] DELIMITER: ;\n"
        "[Linea 9] DELIMITER: }\n"
        "[Linea 10] DELIMITER: }\n"
        "[Linea 11] RETURN: return\n"
        "[Linea 11] ID: x\n"
        "[Linea 11] DELIMITER: ;\n"
        "[Linea 12] DELIMITER: }\n";
    test_lexer_output(input, expected);
}

void test_sequence_10(void) {
    const char* input = 
        "void main() {\n"
        "    int y;\n"
        "    y = 4;\n"
        "    // esto es un comentario de una linea\n"
        "    if (y == 1) {\n"
        "        return;\n"
        "    }\n"
        "}";
    const char* expected = 
        "[Linea 1] VOID: void\n"
        "[Linea 1] ID: main\n"
        "[Linea 1] DELIMITER: (\n"
        "[Linea 1] DELIMITER: )\n"
        "[Linea 1] DELIMITER: {\n"
        "[Linea 2] INT: int\n"
        "[Linea 2] ID: y\n"
        "[Linea 2] DELIMITER: ;\n"
        "[Linea 3] ID: y\n"
        "[Linea 3] OPERATOR: =\n"
        "[Linea 3] NUMBER: 4\n"
        "[Linea 3] DELIMITER: ;\n"
        "[Linea 4] SINGLE_LINE_COMMENT: // esto es un comentario de una linea\n"
        "[Linea 5] IF: if\n"
        "[Linea 5] DELIMITER: (\n"
        "[Linea 5] ID: y\n"
        "[Linea 5] OPERATOR: ==\n"
        "[Linea 5] NUMBER: 1\n"
        "[Linea 5] DELIMITER: )\n"
        "[Linea 5] DELIMITER: {\n"
        "[Linea 6] RETURN: return\n"
        "[Linea 6] DELIMITER: ;\n"
        "[Linea 7] DELIMITER: }\n"
        "[Linea 8] DELIMITER: }\n";
    test_lexer_output(input, expected);
}

void test_sequence_11(void) {
    const char* input = 
        "/* Comentario\n"
        "   multilinea\n"
        "   de varias lineas */\n"
        "/* comentario en una sola linea */\n"
        "/**/";
    const char* expected = 
        "[Linea 3] MULTI_LINE_COMMENT: /* Comentario\n"
        "   multilinea\n"
        "   de varias lineas */\n"
        "[Linea 4] MULTI_LINE_COMMENT: /* comentario en una sola linea */\n"
        "[Linea 5] MULTI_LINE_COMMENT: /**/\n";
    test_lexer_output(input, expected);
}

void test_sequence_12(void) {
    const char* input = 
        "$ # @ ` ?";
    const char* expected = 
        "[Linea 1] ERROR LEXICO: simbolo no permitido '$'\n"
        "[Linea 1] ERROR LEXICO: simbolo no permitido '#'\n"
        "[Linea 1] ERROR LEXICO: simbolo no permitido '@'\n"
        "[Linea 1] ERROR LEXICO: simbolo no permitido '`'\n"
        "[Linea 1] ERROR LEXICO: simbolo no permitido '?'\n";
    test_lexer_output(input, expected);
}

void test_sequence_13(void) {
    const char* input = 
        "5.\n"
        ".5\n"
        "7.x";
    const char* expected = 
        "[Linea 1] NUMBER: 5\n"
        "[Linea 1] ERROR LEXICO: simbolo no permitido '.'\n"
        "[Linea 2] ERROR LEXICO: simbolo no permitido '.'\n"
        "[Linea 2] NUMBER: 5\n"
        "[Linea 3] NUMBER: 7\n"
        "[Linea 3] ERROR LEXICO: simbolo no permitido '.'\n"
        "[Linea 3] ID: x\n";
    test_lexer_output(input, expected);
}

void test_sequence_14(void) {
    const char* input = 
        "/* este comentario \n"
        "nunca se cierra \n"
        "int nuncaSeEjecuta;";
    const char* expected = 
        "[Linea 1] OPERATOR: /\n"
        "[Linea 1] OPERATOR: *\n"
        "[Linea 1] ID: este\n"
        "[Linea 1] ID: comentario\n"
        "[Linea 2] ID: nunca\n"
        "[Linea 2] ID: se\n"
        "[Linea 2] ID: cierra\n"
        "[Linea 3] INT: int\n"
        "[Linea 3] ID: nuncaSeEjecuta\n"
        "[Linea 3] DELIMITER: ;\n";
    test_lexer_output(input, expected);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_sequence_1);
    RUN_TEST(test_sequence_2);
    RUN_TEST(test_sequence_3);
    RUN_TEST(test_sequence_4);
    RUN_TEST(test_sequence_5);
    RUN_TEST(test_sequence_6);
    RUN_TEST(test_sequence_7);
    RUN_TEST(test_sequence_8);
    RUN_TEST(test_sequence_9);
    RUN_TEST(test_sequence_10);
    RUN_TEST(test_sequence_11);
    RUN_TEST(test_sequence_12);
    RUN_TEST(test_sequence_13);
    RUN_TEST(test_sequence_14);
    return UNITY_END();
}
