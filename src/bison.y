%{
#include "ast.h"
#include "parser_helpers.h"
#include "symbol_table.h"
#include "error_handler.h"
extern int yylex(void);
extern int yylineno;
void yyerror(const char *s) {
    (void)s; /* el mensaje de Bison esta en ingles y no aporta nada mas que la linea */
    syntaxError(yylineno, "error de sintaxis");
}

/* En la build normal la definicion vive en main.c. En los tests unitarios
 * (UNITY_TESTING) no se linkea main.c, por lo que se define aqui. */
#ifdef UNITY_TESTING
NodeAST *raizAST = NULL;
#else
extern NodeAST *raizAST;
#endif
%}

%code requires {
#include "ast.h"
#include "parser_helpers.h"
}

%union {
  int intval;
  float floatval;
  char* strval;
  struct NodeAST *node;
  struct NodeArray *array;
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

%type <node> Program Statement Expression MethodCall Block MethodDeclaration ParameterList
%type <dtype> Type 
%type <array> VariableDeclaration IdentifierList ListArguments VariableDeclarations Statements Parameters Declarations Declaration

%%
    Program
    : Declarations { $$ = newNode(BLOCK_NODE, NULL, NULL, NULL, NULL); resolveNodeArray($$, $1); raizAST = $$; }
    ;

    Declarations
    : /* empty */ { $$ = NULL; }
    | Declaration Declarations { $$ = mergeNodeArrays($1, $2); } 
    ;

    Declaration
    : VariableDeclaration { $$ = $1; }  
    | MethodDeclaration { $$ = appendNode(NULL, $1); }
    ;


    VariableDeclaration
    : Type IdentifierList ';' {
        $$ = resolveVariableDefinition($1, $2);
    }
    ;
    MethodDeclaration
    : Type ID '(' ParameterList ')' Block { 
        $$ = newMethodDeclaration($1, $2, $4, $6); 
        if ($4 != NULL) $$->line = $4->line;
    }
    | VOID ID '(' ParameterList ')' Block { 
        $$ = newMethodDeclaration(TYPE_VOID, $2, $4, $6); 
        if ($4 != NULL) $$->line = $4->line;
    }
    ;

    ParameterList
    : /* empty */ {
        $$ = newNode(PARAMETERS_NODE, NULL, NULL, NULL, NULL);
    }
    | Parameters {
        NodeAST *parameters = newNode(PARAMETERS_NODE, NULL, NULL, NULL, NULL);
        resolveNodeArray(parameters, $1);
        $$ = parameters;
    }
    ;

    Parameters
    : Type ID {
        NodeAST *decl = newNode(VARIABLE_DECLARATION_NODE, newSymbol($2, ID_SYMBOL, $1, NULL), NULL, NULL, NULL);
        decl->type = $1;
        $$ = appendNode(NULL, decl); 
    }
    | Parameters ',' Type ID {
        NodeAST *decl = newNode(VARIABLE_DECLARATION_NODE, newSymbol($4, ID_SYMBOL, $3, NULL), NULL, NULL, NULL);
        decl->type = $3;
        $$ = appendNode($1, decl);
    }
    ;
    
    IdentifierList
    : ID { 
        $$ = appendNode(NULL, newNode(ID_NODE, newSymbol($1, ID_SYMBOL, TYPE_VOID, NULL), NULL, NULL, NULL)); 
    }
    | IdentifierList ',' ID { 
        $$ = appendNode($1, newNode(ID_NODE, newSymbol($3, ID_SYMBOL, TYPE_VOID, NULL), NULL, NULL, NULL)); 
    }
    ;
    
    Type 
    : INT       { $$ = TYPE_INT; }
    | BOOLEAN   { $$ = TYPE_BOOL; }
    | FLOAT     { $$ = TYPE_FLOAT; }
    ;

    Statements
    : /* empty */                  { $$ = NULL; }
    | Statement Statements         { $$ = prependNode($2, $1); }
    ;

    Statement
    : ID '=' Expression ';' { $$ = newNode(ASSIGNMENT_NODE, newSymbol($1, ID_SYMBOL, TYPE_VOID, NULL), $3, NULL, NULL); }
    | MethodCall ';' {$$ = $1; }
    | RETURN Expression ';' { $$ = newNode(RETURN_NODE, NULL, $2, NULL, NULL); }
    | RETURN ';' { $$ = newNode(RETURN_NODE, NULL, NULL, NULL, NULL); }
    | Block { $$ = $1; }
    | WHILE '(' Expression ')' Block {
        $$ = newNode(WHILE_NODE, NULL, $3, NULL, $5);
        if ($3 != NULL) $$->line = $3->line;
    }
    | IF '(' Expression ')' Block {
        $$ = newNode(IF_ELSE_NODE, NULL, $3, $5, NULL);
        if ($3 != NULL) $$->line = $3->line;
    }
    | IF '(' Expression ')' Block ELSE Block {
        $$ = newNode(IF_ELSE_NODE, NULL, $3, $5, $7);
        if ($3 != NULL) $$->line = $3->line;
    }
    | ';' { $$ = NULL; }
    ;

    MethodCall
    : ID '(' ')' { $$ = newNode(METHOD_CALL_NODE, newSymbol($1, METHOD_SYMBOL, TYPE_VOID, NULL), NULL, NULL, NULL); }
    | ID '(' ListArguments ')' {
        NodeAST *call = newNode(METHOD_CALL_NODE, newSymbol($1, METHOD_SYMBOL, TYPE_VOID, NULL), NULL, NULL, NULL);
        resolveNodeArray(call, $3);
        $$ = call;
    }
    ;

    ListArguments
    : Expression { $$ = appendNode(NULL, $1); }
    | ListArguments ',' Expression { $$ = appendNode($1, $3); }
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
            resolveNodeArray(block, mergeNodeArrays($2, $3));
            $$ = block;
        }
    ;


    VariableDeclarations
    : /* empty */                                { $$ = NULL; } 
    | VariableDeclaration VariableDeclarations   { $$ = mergeNodeArrays($1, $2); } 
    ;


%%
