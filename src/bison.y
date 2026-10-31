%{
#include "ast.h"
#include "parser_helpers.h"
#include "symbol_table.h"
#include "error_handler.h"
extern int yylex(void);
extern int yylineno;
static void yyerror(const char *s) {
    (void)s; /* el mensaje de Bison esta en ingles y no aporta nada mas que la linea */
    syntaxError(yylineno, "error de sintaxis");
}

/* En la build normal la definicion vive en main.c. En los tests unitarios
 * (UNITY_TESTING) no se linkea main.c, por lo que se define aqui. */
#ifdef UNITY_TESTING
NodeAST *astRoot = NULL;
#else
extern NodeAST *astRoot;
#endif
%}

%code requires {
#include "ast.h"
#include "parser_helpers.h"
}

%union {
    int intval;
    float floatval;
    char *strval;
    struct NodeAST *node;
    struct NodeArray *array;
    DataType dataType;
}

%token VOID INT FLOAT BOOLEAN IF ELSE WHILE RETURN

%token EQ AND OR NOT

%token <strval> ID
%token <intval> NUMBER BOOL_CONST
%token <floatval> FLOAT_CONST

%left OR                /* disyunción lógica || */
%left AND               /* conjunción lógica && */
%left EQ                /* igualdad == */
%left '<' '>'           /* relacionales (menor, mayor) */
%left '+' '-'           /* suma y resta */
%left '*' '/' '%'       /* multiplicación, división, resto */
%precedence NOT UMINUS  /* negación lógica ! y menos unario */

%type <node> Program Statement Expression MethodCall Block MethodDeclaration ParameterList
%type <dataType> Type
%type <array> VariableDeclaration IdentifierList ArgumentList VariableDeclarations Statements Parameters Declarations Declaration

%%

Program
    : Declarations                              { $$ = newNode(BLOCK_NODE, NULL, NULL, NULL, NULL); resolveNodeArray($$, $1); astRoot = $$; }
    ;

/* Variables globales y metodos se unifican bajo Declaration para permitir
 * su declaracion intercalada y evitar el conflicto shift/reduce LR(1)
 * sobre el prefijo comun "Type ID". */
Declarations
    : %empty                                    { $$ = NULL; }
    | Declarations Declaration                  { $$ = mergeNodeArrays($1, $2); }
    ;

Declaration
    : VariableDeclaration                       { $$ = $1; }
    | MethodDeclaration                         { $$ = appendNode(NULL, $1); }
    ;

VariableDeclaration
    : Type IdentifierList ';'                   { $$ = resolveVariableDefinition($1, $2); }
    ;

Type
    : INT                                       { $$ = TYPE_INT; }
    | BOOLEAN                                   { $$ = TYPE_BOOL; }
    | FLOAT                                     { $$ = TYPE_FLOAT; }
    ;

IdentifierList
    : ID                                        { $$ = appendNode(NULL, newNode(ID_NODE, newSymbol($1, VARIABLE_SYMBOL, TYPE_VOID, NULL), NULL, NULL, NULL)); }
    | IdentifierList ',' ID                     { $$ = appendNode($1, newNode(ID_NODE, newSymbol($3, VARIABLE_SYMBOL, TYPE_VOID, NULL), NULL, NULL, NULL)); }
    ;

MethodDeclaration
    : Type ID '(' ParameterList ')' Block       { $$ = newMethodDeclaration($1, $2, $4, $6); }
    | VOID ID '(' ParameterList ')' Block       { $$ = newMethodDeclaration(TYPE_VOID, $2, $4, $6); }
    ;

ParameterList
    : %empty                                    { $$ = newNode(PARAMETERS_NODE, NULL, NULL, NULL, NULL); }
    | Parameters {
        NodeAST *parameters = newNode(PARAMETERS_NODE, NULL, NULL, NULL, NULL);
        resolveNodeArray(parameters, $1);
        $$ = parameters;
    }
    ;

Parameters
    : Type ID                                   { $$ = appendNode(NULL, newParameterDeclaration($1, $2)); }
    | Parameters ',' Type ID                    { $$ = appendNode($1, newParameterDeclaration($3, $4)); }
    ;

Block
    : '{' VariableDeclarations Statements '}' {
        NodeAST *block = newNode(BLOCK_NODE, NULL, NULL, NULL, NULL);
        resolveNodeArray(block, mergeNodeArrays($2, $3));
        $$ = block;
    }
    ;

VariableDeclarations
    : %empty                                    { $$ = NULL; }
    | VariableDeclarations VariableDeclaration  { $$ = mergeNodeArrays($1, $2); }
    ;

Statements
    : %empty                                    { $$ = NULL; }
    | Statements Statement                      { $$ = appendNode($1, $2); }
    ;

Statement
    : ID '=' Expression ';'                     { $$ = newNode(ASSIGNMENT_NODE, newSymbol($1, VARIABLE_SYMBOL, TYPE_VOID, NULL), $3, NULL, NULL); }
    | MethodCall ';'                            { $$ = $1; }
    | RETURN Expression ';'                     { $$ = newNode(RETURN_NODE, NULL, $2, NULL, NULL); }
    | RETURN ';'                                { $$ = newNode(RETURN_NODE, NULL, NULL, NULL, NULL); }
    | Block                                     { $$ = $1; }
    | WHILE '(' Expression ')' Block            { $$ = newNode(WHILE_NODE, NULL, $3, NULL, $5); }
    | IF '(' Expression ')' Block               { $$ = newNode(IF_ELSE_NODE, NULL, $3, $5, NULL); }
    | IF '(' Expression ')' Block ELSE Block    { $$ = newNode(IF_ELSE_NODE, NULL, $3, $5, $7); }
    | ';'                                       { $$ = NULL; }
    ;

MethodCall
    : ID '(' ')'                                { $$ = newNode(METHOD_CALL_NODE, newSymbol($1, METHOD_SYMBOL, TYPE_VOID, NULL), NULL, NULL, NULL); }
    | ID '(' ArgumentList ')' {
        NodeAST *call = newNode(METHOD_CALL_NODE, newSymbol($1, METHOD_SYMBOL, TYPE_VOID, NULL), NULL, NULL, NULL);
        resolveNodeArray(call, $3);
        $$ = call;
    }
    ;

ArgumentList
    : Expression                                { $$ = appendNode(NULL, $1); }
    | ArgumentList ',' Expression               { $$ = appendNode($1, $3); }
    ;

Expression
    : ID                                        { $$ = newNode(ID_NODE, newSymbol($1, VARIABLE_SYMBOL, TYPE_VOID, NULL), NULL, NULL, NULL); }
    | MethodCall                                { $$ = $1; }
    | FLOAT_CONST                               { $$ = newFloatLiteralNode($1); }
    | BOOL_CONST                                { $$ = newBoolLiteralNode($1); }
    | NUMBER                                    { $$ = newIntLiteralNode($1); }
    | '-' Expression %prec UMINUS               { $$ = newOperatorNode(OP_UMINUS, $2, NULL); }
    | NOT Expression                            { $$ = newOperatorNode(OP_NOT, $2, NULL); }
    | '(' Expression ')'                        { $$ = $2; }
    | Expression '+' Expression                 { $$ = newOperatorNode(OP_ADD, $1, $3); }
    | Expression '-' Expression                 { $$ = newOperatorNode(OP_SUB, $1, $3); }
    | Expression '*' Expression                 { $$ = newOperatorNode(OP_MUL, $1, $3); }
    | Expression '/' Expression                 { $$ = newOperatorNode(OP_DIV, $1, $3); }
    | Expression '%' Expression                 { $$ = newOperatorNode(OP_MOD, $1, $3); }
    | Expression '<' Expression                 { $$ = newOperatorNode(OP_LT, $1, $3); }
    | Expression '>' Expression                 { $$ = newOperatorNode(OP_GT, $1, $3); }
    | Expression EQ Expression                  { $$ = newOperatorNode(OP_EQ, $1, $3); }
    | Expression AND Expression                 { $$ = newOperatorNode(OP_AND, $1, $3); }
    | Expression OR Expression                  { $$ = newOperatorNode(OP_OR, $1, $3); }
    ;

%%
