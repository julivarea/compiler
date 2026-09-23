#include "unity.h"
#include <stdio.h>
#include <stdlib.h>

// Declaraciones de Flex/Bison
extern int yyparse(void);
typedef struct yy_buffer_state *YY_BUFFER_STATE;
extern YY_BUFFER_STATE yy_scan_string(const char * str);
extern void yy_delete_buffer(YY_BUFFER_STATE buffer);
extern int yylineno;

// Configuración de Unity
void setUp(void) {
    yylineno = 1; // cada test arranca a contar líneas desde 1
}
void tearDown(void) {}

// Programa mínimo válido
void test_parser_programa_minimo_valido(void) {
    const char* input = "void main() { }";
    YY_BUFFER_STATE buffer = yy_scan_string(input);
    
    int result = yyparse(); // Retorna 0 si es válido
    
    yy_delete_buffer(buffer);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, result, "El parser deberia aceptar un main vacio.");
}

// Declaraciones locales de los tres tipos
void test_parser_declaraciones_variables(void) {
    const char* input = "void main() { int x; float y; boolean z; }";
    YY_BUFFER_STATE buffer = yy_scan_string(input);
    
    int result = yyparse();
    
    yy_delete_buffer(buffer);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, result, "El parser deberia aceptar declaraciones de variables de todos los tipos.");
}

// Declaración sin punto y coma
void test_parser_error_sintactico_falta_punto_y_coma(void) {
    const char* input = "void main() { int x }";
    YY_BUFFER_STATE buffer = yy_scan_string(input);
    
    int result = yyparse(); // Retorna 1 si hay error de sintaxis
    
    yy_delete_buffer(buffer);
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(0, result, "El parser deberia rechazar declaraciones sin punto y coma.");
}

// Expresión con todos los operadores aritméticos
void test_parser_expresiones_matematicas(void) {
    const char* input = "void main() { int x; x = 1 + 2 * 3 / 4 % 5 - 6; }";
    YY_BUFFER_STATE buffer = yy_scan_string(input);
    
    int result = yyparse();
    
    yy_delete_buffer(buffer);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, result, "El parser deberia reconocer expresiones matematicas complejas.");
}

// Variables y métodos globales intercalados (conflicto shift/reduce resuelto)
void test_parser_intercalado_funciones_variables(void) {
    const char* input = 
        "int a;"
        "void f1() {}"
        "float b;"
        "int f2(int x) { return x; }"
        "void main() {}";
    YY_BUFFER_STATE buffer = yy_scan_string(input);
    
    int result = yyparse();
    
    yy_delete_buffer(buffer);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, result, "El parser deberia permitir declaraciones de variables y funciones globales intercaladas (Shift/Reduce fix).");
}

static int parse(const char *input) {
    YY_BUFFER_STATE buffer = yy_scan_string(input);
    int result = yyparse();
    yy_delete_buffer(buffer);
    return result;
}

static char msg[512];

// Reciben la línea del caso para que un FAIL apunte a ella y no al helper
static void accepts_at(int line, const char *input) {
    snprintf(msg, sizeof msg, "deberia ACEPTAR: %s", input);
    if (parse(input) != 0) UNITY_TEST_FAIL(line, msg);
}

static void rejects_at(int line, const char *input) {
    snprintf(msg, sizeof msg, "deberia RECHAZAR: %s", input);
    if (parse(input) == 0) UNITY_TEST_FAIL(line, msg);
}

#define accepts(input) accepts_at(__LINE__, (input))
#define rejects(input) rejects_at(__LINE__, (input))

// Programa vacío, con solo comentarios o con comentarios entre cada token
void test_programa_vacio_y_solo_comentarios(void) {
    accepts("");
    accepts("// nada\n/* nada */");
    accepts("/*c*/void/*c*/main/*c*/(/*c*/)/*c*/{/*c*/}//c");
}

// Declaraciones globales de variables y métodos en cualquier orden
void test_declaraciones_globales(void) {
    accepts("int a,b,c; float f; boolean q; void main(){}");
    accepts("int a;int f(){return 1;}float b;void g(){}boolean c;void main(){}");
    accepts("int f(int a){ return a; } int b; void main(){ b = f(1); }");
    accepts("int main; void main(){}"); // chequeo de nombres es semántico
}

// Métodos con cada tipo de retorno y con parámetros
void test_declaraciones_de_metodos(void) {
    accepts("void f(){}");
    accepts("int f(int a, float b, boolean c){ return a; }");
    accepts("float f(){ return 1.5; }");
    accepts("boolean f(){ return true; }");
    accepts("void f(){} void f(){}"); // duplicados: semántico
}

// El programa de ejemplo test.tds, en varias líneas y con recursión
void test_ejemplo_factorial(void) {
    accepts(
        "int a,b;\n"
        "int factorial(int a){\n"
        "    if(a == 0) { return 1; }\n"
        "    return a * factorial(a-1);\n"
        "}\n"
        "void main(){\n"
        "    a = 2;\n"
        "    factorial(a);\n"
        "}");
}

// Bloques vacíos, anidados y con declaraciones antes de las sentencias
void test_bloques(void) {
    accepts("void main(){ ; ; ; }");
    accepts("void main(){ {} { {} } }");
    accepts("void main(){ { int x; x = 1; } }");
    accepts("void main(){ int x; { int x; } }");
    accepts("void main(){ int a; float b; boolean c; a = 1; b = 2.0; }");
}

// if, if-else y while, simples y anidados
void test_sentencias_de_control(void) {
    accepts("void main(){ if (a) { } }");
    accepts("void main(){ if (a) { } else { } }");
    accepts("void main(){ if (a) { if (b) { x = 1; } else { x = 2; } } }");
    accepts("void main(){ if (a) { } else { if (b) { } else { } } }");
    accepts("void main(){ if (x) {} if (y) {} else {} }");
    accepts("void main(){ while (x) { } }");
    accepts("void main(){ while (x < 3) { x = x + 1; } }");
    accepts("void main(){ while ((a)) { } }");
    accepts("void main(){ while (a) { while (b) { } } }");
}

// return con y sin expresión
void test_return(void) {
    accepts("void main(){ return; }");
    accepts("int f(){ return 1; }");
    accepts("int f(){ return f(x) + 1; }");
    accepts("int f(){ return; }"); // tipo de retorno: semántico
}

// Llamadas como sentencia y como expresión, anidadas y con varios argumentos
void test_llamadas_a_metodo(void) {
    accepts("void main(){ f(); g(1); h(1, 2, 3); }");
    accepts("void main(){ x = f(g(h()), 1+2, !a); }");
    accepts("void main(){ x = f() + g(1) * h(); }");
}

// Literales, paréntesis y combinaciones de todos los operadores
void test_expresiones_validas(void) {
    accepts("void main(){ x = true; y = false; z = 3.5; w = 0; }");
    accepts("void main(){ x = ((((1)))); }");
    accepts("void main(){ x = -a*b+c==d&&e||!f; }");
    accepts("void main(){ x = a<b==c>d; }");
    accepts("void main(){ x = a < b < c; }");
    accepts("void main(){ x = a == b == c; }");
    accepts("void main(){ x = !!!a; }");
    accepts("void main(){ x = - - -a; }");
    accepts("void main(){ x = a--b; }");
    accepts("void main(){ x = -(-(1)); }");
    accepts("void main(){ x = 1 % 2 / 3 * 4; }");
    accepts("void main(){ x = (a || b) && (c || d); }");
}

// Falta el ';' al final de distintas construcciones
void test_error_falta_punto_y_coma(void) {
    rejects("void main(){ x = 1 }");
    rejects("int a");
    rejects("int f(){ return 1 }");
    rejects("void main(){ f() }");
}

// El cuerpo de if, else, while y de un método siempre lleva llaves
void test_error_cuerpo_sin_llaves(void) {
    rejects("void main(){ if (x) x = 1; }");
    rejects("void main(){ while (x) x = 1; }");
    rejects("void main(){ if (x) { } else x = 1; }");
    rejects("void main(){ if (x) { } else if (y) { } }"); // else exige bloque
    rejects("void f() return;");
}

// Condiciones faltantes o sin paréntesis, y else sin if
void test_error_if_while_mal_formados(void) {
    rejects("void main(){ if x { } }");
    rejects("void main(){ if () { } }");
    rejects("void main(){ while { } }");
    rejects("void main(){ while () { } }");
    rejects("void main(){ while x < 3 { } }"); // la spec omite los paréntesis por error
    rejects("void main(){ while (a) + b { } }");
    rejects("void main(){ else { } }");
}

// Declaraciones mal formadas y sentencias sueltas a nivel global
void test_error_declaraciones(void) {
    rejects("int a = 5;");
    rejects("void main(){ int a = 5; }");
    rejects("void x;");
    rejects("void main(){ void y; }");
    rejects("int int;");
    rejects("int if;");
    rejects("int a, ;");
    rejects("int ,a;");
    rejects("int a b;");
    rejects("int;");
    rejects("x = 1;");       // sentencia a nivel global
    rejects("f();");
    rejects("{ }");
    rejects("int f(){} ;");  // ';' suelto a nivel global
}

// Dentro de un bloque las declaraciones van antes que las sentencias
void test_error_declaracion_despues_de_sentencia_en_bloque(void) {
    rejects("void main(){ x = 1; int y; }");
    rejects("void main(){ { } int y; }");
}

// Firmas de método mal formadas
void test_error_firmas_de_metodo(void) {
    rejects("main(){}");
    rejects("void main()");
    rejects("void main();");
    rejects("void f(int a,){}");
    rejects("void f(,int a){}");
    rejects("void f(int){}");
    rejects("void f(a){}");
    rejects("void f(void){}");
    rejects("void f(int a, b){}");
    rejects("int f(int a)(int b){}");
}

// Construcciones que no son sentencias de C-TDS
void test_error_sentencias_invalidas(void) {
    rejects("void main(){ 1 + 2; }");
    rejects("void main(){ x; }");
    rejects("void main(){ (f()); }");
    rejects("void main(){ x == 1; }");
    rejects("void main(){ x = y = 1; }");
    rejects("void main(){ true = 1; }");
    rejects("void main(){ 1 = x; }");
    rejects("void main(){ return return; }");
    rejects("void main(){ x++; }");
    rejects("void main(){ x += 1; }");
}

// Expresiones incompletas o con operadores que no existen en C-TDS
void test_error_expresiones_invalidas(void) {
    rejects("void main(){ x = ; }");
    rejects("void main(){ x = 1 +; }");
    rejects("void main(){ x = * 2; }");
    rejects("void main(){ x = +2; }");   // no hay + unario
    rejects("void main(){ x = a b; }");
    rejects("void main(){ x = a <= b; }");
    rejects("void main(){ x = a != b; }");
    rejects("void main(){ x = a < = b; }");
    rejects("void main(){ x = !; }");
    rejects("void main(){ x = ( ); }");
}

// Llamadas a método mal formadas
void test_error_llamadas_invalidas(void) {
    rejects("void main(){ f(1,); }");
    rejects("void main(){ f(,1); }");
    rejects("void main(){ f(1 2); }");
    rejects("void main(){ f()(); }");
    rejects("void main(){ x = f(1)(2); }");
}

// Llaves y paréntesis desbalanceados, y fin de archivo inesperado
void test_error_desbalanceados_y_eof(void) {
    rejects("void main(){ ");
    rejects("void main() }");
    rejects("void main(){ } }");
    rejects("void main(){ x = (a; }");
    rejects("void main(){ x = a); }");
    rejects("void main(){ x = ((a); }");
    rejects("void main(){ x = 1");
    rejects("void main(){ /* sin cerrar }");
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_parser_programa_minimo_valido);
    RUN_TEST(test_parser_declaraciones_variables);
    RUN_TEST(test_parser_error_sintactico_falta_punto_y_coma);
    RUN_TEST(test_parser_expresiones_matematicas);
    RUN_TEST(test_parser_intercalado_funciones_variables);

    RUN_TEST(test_programa_vacio_y_solo_comentarios);
    RUN_TEST(test_declaraciones_globales);
    RUN_TEST(test_declaraciones_de_metodos);
    RUN_TEST(test_ejemplo_factorial);
    RUN_TEST(test_bloques);
    RUN_TEST(test_sentencias_de_control);
    RUN_TEST(test_return);
    RUN_TEST(test_llamadas_a_metodo);
    RUN_TEST(test_expresiones_validas);

    RUN_TEST(test_error_falta_punto_y_coma);
    RUN_TEST(test_error_cuerpo_sin_llaves);
    RUN_TEST(test_error_if_while_mal_formados);
    RUN_TEST(test_error_declaraciones);
    RUN_TEST(test_error_declaracion_despues_de_sentencia_en_bloque);
    RUN_TEST(test_error_firmas_de_metodo);
    RUN_TEST(test_error_sentencias_invalidas);
    RUN_TEST(test_error_expresiones_invalidas);
    RUN_TEST(test_error_llamadas_invalidas);
    RUN_TEST(test_error_desbalanceados_y_eof);

    return UNITY_END();
}
