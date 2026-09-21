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

%type <node> Program VariableDeclaration Statement Expression Type MethodCall Block
%type <list> IdentifierList ListArguments

%%
    Program
    : VariableDeclarations MethodDeclarations
    ;

    VariableDeclarations
    : /* empty */
    | VariableDeclaration VariableDeclarations
    ;

    MethodDeclarations
    : /* empty */
    | MethodDeclaration MethodDeclarations
    ;
    
    Type
    : INT       /* { $$ = newNode(TYPE_NODE, newSymbol("int", NULL), NULL, NULL, NULL); $$->type = TYPE_INT; } */
    | BOOLEAN   /* { $$ = newNode(TYPE_NODE, newSymbol("boolean", NULL), NULL, NULL, NULL); $$->type = TYPE_BOOL; } */
    | FLOAT     /* { $$ = newNode(TYPE_NODE, newSymbol("float", NULL), NULL, NULL, NULL); $$->type = TYPE_FLOAT; } */
    ;

    VariableDeclaration
    : Type IdentifierList ';' /* {
        NodeAST *decl = newNode(VARIABLE_DECLARATION_NODE, NULL, $1, NULL, NULL);
        attachChildren(decl, $2);
        $$ = decl;
    } */
    ;

    Statements:
    |
    Statement Statements
    ;
    IdentifierList
    : ID                /* { $$ = newNodeList(newNode(ID_NODE, newSymbol($1, NULL), NULL, NULL, NULL), NULL); } */
    | IdentifierList ',' ID      /* {
        NodeList *l = $1;
        while (l->next) l = l->next;
        l->next = newNodeList(newNode(ID_NODE, newSymbol($3, NULL), NULL, NULL, NULL), NULL);
        $$ = $1;
    } */
    ;

    Statement
    : ID '=' Expression ';' /* { $$ = newNode(ASSIGNMENT_NODE, newSymbol($1, NULL), newNode(ID_NODE, newSymbol($1, NULL), NULL, NULL, NULL), NULL, $3); } */
    | MethodCall ';' /* {$$ = $1; } */
    | RETURN Expression ';' /* { $$ = newNode(RETURN_NODE, NULL, $2, NULL, NULL); } */
    | RETURN ';'
    | Block /* { $$ = $1; } */
    | WHILE Expression Block /* { $$ = newNode(WHILE_NODE, NULL, $2, NULL, $3); } */
    | IF '(' Expression ')' Block
    | IF '(' Expression ')' Block ELSE Block /* { $$ = newNode(IF_ELSE_NODE, NULL, $3, $5, $7); } */
    | ';' /* { } */
    ;

    MethodCall
    : ID '(' ')'
    | ID '(' ListArguments ')' /* {
        NodeAST *call = newNode(METHOD_CALL_NODE, newSymbol($1, NULL), NULL, NULL, NULL);
        attachChildren(call, $3);
        $$ = call;
    } */
    ;

    ListArguments
    : Expression /* { $$ = newNodeList($1, NULL); } */
    | ListArguments ',' Expression /* {
        NodeList *l = $1;
        while (l->next) l = l->next;
        l->next = newNodeList($3, NULL);
        $$ = $1;
    } */
    ;

    Expression
    : ID
    | MethodCall
    | FLOAT_CONST /* { newLiteralNode(TYPE_FLOAT, NULL);} */
    | BOOL_CONST /* { newLiteralNode(BOOL_CONST, NULL);} */
    | NUMBER /* { newLiteralNode(NUMBER, NULL);} */
    | '-' Expression
    | NOT Expression
    | '(' Expression ')'
    | Expression '+' Expression
    | Expression '-' Expression
    | Expression '*' Expression
    | Expression '/' Expression
    | Expression '%' Expression
    | Expression '<' Expression
    | Expression '>' Expression
    | Expression EQ Expression
    | Expression AND Expression
    | Expression OR Expression
    ;

    MethodDeclaration
    : ReturnType ID '(' ParameterList ')' Block
    ;

    ParameterList
    : /* empty */
    | Parameters
    ;

    Parameters
    : Type ID
    | Parameters ',' Type ID
    ;

    Block
    : '{' VariableDeclarations Statements '}' /* { $$ = newNode(BLOCK_NODE, NULL, NULL, NULL, NULL); } */
    ;

    ReturnType
    : Type
    | VOID
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
    }

    if (argc > 1 && yyin) {
        fclose(yyin);
    }
    return 0;
}
#endif
