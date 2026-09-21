%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"

extern int yylex(void);
extern FILE *yyin;
extern int yylineno;
void yyerror(const char *s) {
    fprintf(stderr, "[Linea %d] Error sintactico: %s\n", yylineno, s);
}

NodeAST *raizAST = NULL;
%}

%union {
  int intval;
  float floatval;
  char* strval;
  struct NodeAST *node;
  struct NodeList *list;
}

/* Tokens de una sola palabra clave */
%token VOID INT FLOAT BOOLEAN IF ELSE WHILE RETURN

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

%type <node> Program VarDecl Statement Expr Type
%type <list> IdList

%%
    Program
    : VarDecl Statement { $$ = NULL; }
    ;

    Type
    : INT       { $$ = newNode(DEFINITION_NODE, newSymbol("int", NULL), NULL, NULL); $$->type = TYPE_INT; }
    | BOOLEAN   { $$ = newNode(DEFINITION_NODE, newSymbol("boolean", NULL), NULL, NULL); $$->type = TYPE_BOOL; }
    | FLOAT     { $$ = newNode(DEFINITION_NODE, newSymbol("float", NULL), NULL, NULL); $$->type = TYPE_FLOAT; }
    ;

    VarDecl
    : Type IdList ';' {
        NodeAST *decl = newNode(VAR_DECL_NODE, NULL, $1, NULL);
        attachChildren(decl, $2);
        $$ = decl;
    }
    ;

    IdList
    : ID                { $$ = newNodeList(newNode(ID_NODE, newSymbol($1, NULL), NULL, NULL), NULL); }
    | IdList ',' ID      {
        NodeList *l = $1;
        while (l->next) l = l->next;
        l->next = newNodeList(newNode(ID_NODE, newSymbol($3, NULL), NULL, NULL), NULL);
        $$ = $1;
    }
    ;

    Statement
    : ID '=' Expr ';' {
        $$ = newNode(ASSIGNMENT_NODE, newSymbol($1, NULL), newNode(ID_NODE, newSymbol($1, NULL), NULL, NULL), $3);
    }
    ;

    Expr
    : NUMBER {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d", $1);
        $$ = newNode(CONSTANT_NODE, newSymbol(buf, buf), NULL, NULL);
        $$->type = TYPE_INT;
    }
    ;

%%

#ifndef UNITY_TESTING
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
