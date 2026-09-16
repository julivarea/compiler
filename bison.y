%{
#include <stdio.h>
#include <stdlib.h>

extern int yylex(void);
extern FILE *yyin;
void yyerror(const char *s) {
    fprintf(stderr, "Error sintactico: %s\n", s);
}
%}

%union {
  int intval;
  float floatval;
  char* strval;
}

/* Tokens de una sola palabra clave */
%token MAIN VOID INT FLOAT BOOLEAN IF ELSE WHILE RETURN

/* Operadores relacionales y lógicos que ocupan más de un carácter */
%token EQ AND OR NOT

/* Tokens que traen un valor semántico asociado desde Flex */
%token <strval> ID
%token <intval> NUMBER BOOL_CONST
%token <floatval> FLOAT_CONST

/* Precedencia de menor a mayor */
%left OR           /* disyunción lógica || */
%left AND          /* conjunción lógica && */
%left EQ           /* igualdad == */
%left '<' '>'      /* relacionales (menor, mayor) */
%left '+' '-'      /* suma y resta */
%left '*' '/' '%'  /* multiplicación, división, resto */
%right NOT UMINUS  /* negación lógica ! y menos unario */

%%

    Program
    : error {
    printf("ADVERTENCIA: Gramatica no definida todavia.\n");
    }
    ;

%%

#ifndef UNITY_TESTING // notacion que ignora la compilacion del main, ya que tiene que correr el main de la suite de tests
int main(int argc, char** argv) {
    if (argc > 1) {
        yyin = fopen(argv[1], "r");
        if (!yyin) {
            perror("Error abriendo archivo");
            return 1;
        }
    } else {
        yyin = stdin;
    }

    if (yyparse() == 0) {
        printf("--- Analisis sintactico sin errores formales. ---\n");
        // Aca en el futuro podremos inicializar el interprete o symbol table
    }

    if (argc > 1 && yyin) {
        fclose(yyin);
    }
    return 0;
}
#endif
