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
  DataType dtype;
}

%token VOID INT FLOAT BOOLEAN IF ELSE WHILE RETURN

%token EQ AND OR NOT

%token <strval> ID
%token <intval> NUMBER BOOL_CONST
%token <floatval> FLOAT_CONST

%left OR           /* disyunción lógica || */
%left AND          /* conjunción lógica && */
%left EQ           /* igualdad == */
%left '<' '>'      /* relacionales (menor, mayor) */
%left '+' '-'      /* suma y resta */
%left '*' '/' '%'  /* multiplicación, división, resto */
%right NOT UMINUS  /* negación lógica ! y menos unario */

%type <node> Program Statement Expression MethodCall Block
%type <dtype> Type 
%type <list> VariableDeclaration IdentifierList ListArguments VariableDeclarations Statements

%%
    Program
    : Declarations { $$ = $1; } 
    ;

    Declarations
    : /* empty */ { $$ = NULL; }
    | Declaration Declarations { $$ = initializeTemporaryList($1, $2); } 
    ;

    Declaration
    : VariableDeclaration { $$ = $1; }  
    | MethodDeclaration { $$ = $1; }
    ;


    VariableDeclaration
    : Type IdentifierList ';' {
        $$ = resolveVariableDefinition($1, $2);
    }
    ;

    // int sumar(int a, int b){}
    MethodDeclaration
    : Type ID '(' ParameterList ')' Block { $$ = newNodeMethod($1, $2, $3, $4) }
    | VOID ID '(' ParameterList ')' Block { $$ = newNodeMethod(TYPE_VOID, $2, $3, $4) }
    ;

    ParameterList
    | Parameter { $$ =  $1; }
    ;

    Parameter
    : Type ID {
        $$ = initializeTemporaryList(newNode(VARIABLE_DECLARATION_NODE, newSymbol($2, NULL), NULL, NULL, NULL), NULL); 
    }
    | Parameters ',' Type ID {
        $$ = appendToTemporaryList($1, newNode(VARIABLE_DECLARATION_NODE, newSymbol($4, NULL), NULL, NULL, NULL));
    }
    ;

    IdentifierList
    : ID { 
        $$ = initializeTemporaryList(newNode(ID_NODE, newSymbol($1, NULL), NULL, NULL, NULL), NULL); 
    }
    | IdentifierList ',' ID { 
        $$ = appendToTemporaryList($1, newNode(ID_NODE, newSymbol($3, NULL), NULL, NULL, NULL)); 
    }
    ;
    
    Type 
    : INT       { $$ = TYPE_INT; }
    | BOOLEAN   { $$ = TYPE_BOOL; }
    | FLOAT     { $$ = TYPE_FLOAT; }
    ;

    Statements
    : /* empty */                  /* { $$ = NULL; } */
    | Statement Statements         /* { $$ = initializeTemporaryList($1, $2); } */
    ;

    Statement
    : ID '=' Expression ';' /* { $$ = newNode(ASSIGNMENT_NODE, newSymbol($1, NULL), newNode(ID_NODE, newSymbol($1, NULL), NULL, NULL, NULL), NULL, $3); } */
    | MethodCall ';' /* {$$ = $1; } */
    | RETURN Expression ';' /* { $$ = newNode(RETURN_NODE, NULL, $2, NULL, NULL); } */
    | RETURN ';'
    | Block /* { $$ = $1; } */
    | WHILE '(' Expression ')' Block /* { $$ = newNode(WHILE_NODE, NULL, $3, NULL, $5); } */
    | IF '(' Expression ')' Block
    | IF '(' Expression ')' Block ELSE Block /* { $$ = newNode(IF_ELSE_NODE, NULL, $3, $5, $7); } */
    | ';' /* { } */
    ;

    MethodCall
    : ID '(' ')'
    | ID '(' ListArguments ')' /* {
        NodeAST *call = newNode(METHOD_CALL_NODE, newSymbol($1, NULL), NULL, NULL, NULL);
        resolveTemporaryList(call, $3);
        $$ = call;
    } */
    ;

    ListArguments
    : Expression /* { $$ = initializeTemporaryList($1, NULL); } */
    | ListArguments ',' Expression /* {
        NodeList *l = $1;
        while (l->next) l = l->next;
        l->next = initializeTemporaryList($3, NULL);
        $$ = $1;
    } */
    ;

    Expression
    : ID
    | MethodCall
    | FLOAT_CONST /* { $$ = newLiteralNode(TYPE_FLOAT, NULL); } */
    | BOOL_CONST  /* { $$ = newLiteralNode(TYPE_BOOL, NULL); } */
    | NUMBER      /* { $$ = newLiteralNode(TYPE_INT, NULL); } */
    | '-' Expression %prec UMINUS
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

    Block
    : '{' VariableDeclarations Statements '}'
        /* {
        NodeAST *block = newNode(BLOCK_NODE, NULL, NULL, NULL, NULL);
        resolveTemporaryList(block, mergeNodeLists($2, $3));
        $$ = block;
} */
    ;


    VariableDeclarations
    : /* empty */                               /* { $$ = NULL; } */
    | VariableDeclaration VariableDeclarations  /* { $$ = mergeNodeLists($1, $2); } */
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
