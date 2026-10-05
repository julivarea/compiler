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

%code requires {
#include "ast.h"
}

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

%type <node> Program Statement Expression MethodCall Block MethodDeclaration
%type <dtype> Type 
%type <list> VariableDeclaration IdentifierList ListArguments VariableDeclarations Statements ParameterList Parameters Declarations Declaration

%%
    Program
    : Declarations { $$ = newNode(BLOCK_NODE, NULL, NULL, NULL, NULL); resolveTemporaryList($$, $1); raizAST = $$; }
    ;

    Declarations
    : /* empty */ { $$ = NULL; }
    | Declaration Declarations { $$ = mergeNodeLists($1, $2); } 
    ;

    Declaration
    : VariableDeclaration { $$ = $1; }  
    | MethodDeclaration { $$ = initializeTemporaryList($1, NULL); }
    ;


    VariableDeclaration
    : Type IdentifierList ';' {
        $$ = resolveVariableDefinition($1, $2);
    }
    ;
    MethodDeclaration
    : Type ID '(' ParameterList ')' Block { 
        NodeAST *parameters = newNode(PARAMETERS_NODE, NULL, NULL, NULL, NULL); 
        resolveTemporaryList(parameters, $4); 
        $$ = newMethodDeclaration($1, $2, parameters, $6); 
    }
    | VOID ID '(' ParameterList ')' Block { 
        NodeAST *parameters = newNode(PARAMETERS_NODE, NULL, NULL, NULL, NULL); 
        resolveTemporaryList(parameters, $4); 
        $$ = newMethodDeclaration(TYPE_VOID, $2, parameters, $6); 
    }
    ;

    ParameterList
    : /* empty */ { $$ = NULL; }
    | Parameters { $$ = $1; }
    ;

    Parameters
    : Type ID {
        $$ = initializeTemporaryList(newNode(VARIABLE_DECLARATION_NODE, newSymbol($2, ID_SYMBOL, $1, NULL), NULL, NULL, NULL), NULL); 
    }
    | Parameters ',' Type ID {
        $$ = appendToTemporaryList($1, newNode(VARIABLE_DECLARATION_NODE, newSymbol($4, ID_SYMBOL, $3, NULL), NULL, NULL, NULL));
    }
    ;
    
    IdentifierList
    : ID { 
        $$ = initializeTemporaryList(newNode(ID_NODE, newSymbol($1, ID_SYMBOL, TYPE_VOID, NULL), NULL, NULL, NULL), NULL); 
    }
    | IdentifierList ',' ID { 
        $$ = appendToTemporaryList($1, newNode(ID_NODE, newSymbol($3, ID_SYMBOL, TYPE_VOID, NULL), NULL, NULL, NULL)); 
    }
    ;
    
    Type 
    : INT       { $$ = TYPE_INT; }
    | BOOLEAN   { $$ = TYPE_BOOL; }
    | FLOAT     { $$ = TYPE_FLOAT; }
    ;

    Statements
    : /* empty */                  { $$ = NULL; }
    | Statement Statements         { $$ = initializeTemporaryList($1, $2); }
    ;

    Statement
    : ID '=' Expression ';' { $$ = newNode(ASSIGNMENT_NODE, newSymbol($1, ID_SYMBOL, TYPE_VOID, NULL), newNode(ID_NODE, newSymbol($1, ID_SYMBOL, TYPE_VOID, NULL), NULL, NULL, NULL), NULL, $3); }
    | MethodCall ';' {$$ = $1; }
    | RETURN Expression ';' { $$ = newNode(RETURN_NODE, NULL, $2, NULL, NULL); }
    | RETURN ';' { $$ = newNode(RETURN_NODE, NULL, NULL, NULL, NULL); }
    | Block { $$ = $1; }
    | WHILE '(' Expression ')' Block { $$ = newNode(WHILE_NODE, NULL, $3, NULL, $5); }
    | IF '(' Expression ')' Block { $$ = newNode(IF_ELSE_NODE, NULL, $3, $5, NULL); }
    | IF '(' Expression ')' Block ELSE Block { $$ = newNode(IF_ELSE_NODE, NULL, $3, $5, $7); }
    | ';' { $$ = NULL; }
    ;

    MethodCall
    : ID '(' ')' { $$ = newNode(METHOD_CALL_NODE, newSymbol($1, METHOD_SYMBOL, TYPE_VOID, NULL), NULL, NULL, NULL); }
    | ID '(' ListArguments ')' {
        NodeAST *call = newNode(METHOD_CALL_NODE, newSymbol($1, METHOD_SYMBOL, TYPE_VOID, NULL), NULL, NULL, NULL);
        resolveTemporaryList(call, $3);
        $$ = call;
    }
    ;

    ListArguments
    : Expression { $$ = initializeTemporaryList($1, NULL); }
    | ListArguments ',' Expression { $$ = appendToTemporaryList($1, $3); }
    ;

    Expression
    : ID { $$ = newNode(ID_NODE, newSymbol($1, ID_SYMBOL, TYPE_VOID, NULL), NULL, NULL, NULL) ; }
    | MethodCall { $$ = $1; }
    | FLOAT_CONST  { $$ = newFloatLiteralNode($1); } 
    | BOOL_CONST   { $$ = newBoolLiteralNode($1); }
    | NUMBER       { $$ = newIntLiteralNode($1); }
    | '-' Expression %prec UMINUS { $$ = newBinaryOperatorNode(OP_NEGATIVE, $2, NULL); }
    | NOT Expression { $$ = newBinaryOperatorNode(OP_NEGATION, $2, NULL); }
    | '(' Expression ')' { $$ = $2; }
    | Expression '+' Expression { $$ = newBinaryOperatorNode(OP_ADD, $1, $3); }
    | Expression '-' Expression { $$ = newBinaryOperatorNode(OP_SUB, $1, $3); }
    | Expression '*' Expression { $$ = newBinaryOperatorNode(OP_MUL, $1, $3); }
    | Expression '/' Expression { $$ = newBinaryOperatorNode(OP_DIV, $1, $3); }
    | Expression '%' Expression { $$ = newBinaryOperatorNode(OP_MOD, $1, $3); }
    | Expression '<' Expression { $$ = newBinaryOperatorNode(OP_LT, $1, $3); }
    | Expression '>' Expression { $$ = newBinaryOperatorNode(OP_GT, $1, $3); }
    | Expression EQ Expression { $$ = newBinaryOperatorNode(OP_EQ, $1, $3); }
    | Expression AND Expression { $$ = newBinaryOperatorNode(OP_AND, $1, $3); }
    | Expression OR Expression { $$ = newBinaryOperatorNode(OP_OR, $1, $3); }
    ;

    Block
    : '{' VariableDeclarations Statements '}'
        {
            NodeAST *block = newNode(BLOCK_NODE, NULL, NULL, NULL, NULL);
            resolveTemporaryList(block, mergeNodeLists($2, $3));
            $$ = block;
        }
    ;


    VariableDeclarations
    : /* empty */                                { $$ = NULL; } 
    | VariableDeclaration VariableDeclarations   { $$ = mergeNodeLists($1, $2); } 
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
