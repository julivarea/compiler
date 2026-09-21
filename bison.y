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

%type <node> Program VariableDeclaration Statement Expression Type
%type <list> IdentifierList

%%
    Program
    : VariableDeclaration Statement { $$ = NULL; }
    ;

    Type
    : INT       { $$ = newNode(TYPE_NODE, newSymbol("int", NULL), NULL, NULL, NULL); $$->type = TYPE_INT; }
    | BOOLEAN   { $$ = newNode(TYPE_NODE, newSymbol("boolean", NULL), NULL, NULL, NULL); $$->type = TYPE_BOOL; }
    | FLOAT     { $$ = newNode(TYPE_NODE, newSymbol("float", NULL), NULL, NULL, NULL); $$->type = TYPE_FLOAT; }
    ;

    VariableDeclaration
    : Type IdentifierList ';' {
        NodeAST *decl = newNode(VARIABLE_DECLARATION_NODE, NULL, $1, NULL, NULL);
        attachChildren(decl, $2);
        $$ = decl;
    }
    ;

    IdentifierList
    : ID                { $$ = newNodeList(newNode(ID_NODE, newSymbol($1, NULL), NULL, NULL, NULL), NULL); }
    | IdentifierList ',' ID      {
        NodeList *l = $1;
        while (l->next) l = l->next;
        l->next = newNodeList(newNode(ID_NODE, newSymbol($3, NULL), NULL, NULL, NULL), NULL);
        $$ = $1;
    }
    ;

    Statement
    : ID '=' Expression ';' {
        $$ = newNode(ASSIGNMENT_NODE, newSymbol($1, NULL), newNode(ID_NODE, newSymbol($1, NULL), NULL, NULL, NULL), NULL, $3);
    }
    ;

    Statement
    : ID '=' Expression ';' { newNode(ASSIGNMENT_NODE, newSymbol($1, NULL), newNode(ID_NODE, newSymbol($1, NULL), NULL, NULL, NULL), NULL, $3)};
    | MethodCall ';' {$$ = $1; }
    | RETURN Expression ';' { newNode(RETURN_NODE, newSymbol($2, NULL), NULL, NULL, NULL); } // save a expression reference to return
    | Block { $$ = $1; }
    | WHILE Expression Block { newNode(WHILE_NODE, NULL, $2, NULL, $3) };
    | IF '(' Expression ')' Block ELSE Block { newNode(IF_ELSE_NODE, NULL, $1, $2, $3) };
    | ';' { }

    MethodCall
    : ID '(' ListArguments ')'

    ListArguments
    : Expression 
    | Expression ',' ListArguments
    ;

    Expression
    // during type checking, we verify that IdentifierList.size() == 1
    : IdentifierList
    | FLOAT_CONST { newLiteralNode(FLOAT_TYPE, getValueOf(FLOAT_CONST));}
    | BOOLEAN_CONST { newLiteralNode(BOOLEAN_CONST, getValueOf(BOOLEAN_CONST));}
    | NUMBER { newLiteralNode(NUMBER, getValueOf(NUMBER));}
    // during type cheking, we verify that this.isFloatConst() || this.isNumberConst() -> e.g = {-0.2, -2, ...}
    | '-' Expression
    // during type cheking, we verify that this.isBooleanConst()
    | '!' Expression
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
    : ReturnType ID '(' IdentifierList ')' Block

    ReturnType:
      Type
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
        // Aca en el futuro podremos inicializar el interprete o symbol table
    }

    if (argc > 1 && yyin) {
        fclose(yyin);
    }
    return 0;
}
#endif
