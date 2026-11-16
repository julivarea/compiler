%{
/**
 * @file bison.y
 * @brief Gramática LALR(1) y acciones de construcción del AST de C-TDS en Bison.
 */
#include "ast.h"
#include "parser_helpers.h"
#include "error_handler.h"
#include "string.h"
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

%token EQ AND OR NOT NEQ LEQ GEQ

%token ADD_ASSIGN SUB_ASSIGN MUL_ASSIGN DIV_ASSIGN

%token INC DEC

%token FOR

%token <strval> ID
%token <intval> NUMBER BOOL_CONST
%token <floatval> FLOAT_CONST

%left OR                /* disyunción lógica || */
%left AND               /* conjunción lógica && */
%left EQ NEQ            /* igualdad == */
%left '<' '>' GEQ LEQ   /* relacionales (menor, mayor) */
%left '+' '-'           /* suma y resta */
%left '*' '/' '%'       /* multiplicación, división, resto */
%precedence NOT UMINUS  /* negación lógica ! y menos unario */

%type <node> Program Statement Expression MethodCall Block MethodDeclaration ParameterList Assignment IfStatement
%type <dataType> Type
%type <array> VariableDeclaration IdentifierList ArgumentList VariableDeclarations Statements Parameters Declarations Declaration

%%

Program
    : Declarations                              { $$ = newBlockNode($1); astRoot = $$; }
    ;

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
    : ID                                        { $$ = appendNode(NULL, newIdentifierNode($1)); }
    | IdentifierList ',' ID                     { $$ = appendNode($1, newIdentifierNode($3)); }
    ;

MethodDeclaration
    : Type ID '(' ParameterList ')' Block       { $$ = newMethodDeclaration($1, $2, $4, $6); }
    | VOID ID '(' ParameterList ')' Block       { $$ = newMethodDeclaration(TYPE_VOID, $2, $4, $6); }
    ;

ParameterList
    : %empty                                    { $$ = newParametersNode(NULL); }
    | Parameters                                { $$ = newParametersNode($1); }
    ;

Parameters
    : Type ID                                   { $$ = appendNode(NULL, newParameterDeclaration($1, $2)); }
    | Parameters ',' Type ID                    { $$ = appendNode($1, newParameterDeclaration($3, $4)); }
    ;

Block
    : '{' VariableDeclarations Statements '}'   { $$ = newBlockNode(mergeNodeArrays($2, $3)); }
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
    : Assignment ';'                                                { $$ = $1; }
    | MethodCall ';'                                                { $$ = $1; }
    | RETURN Expression ';'                                         { $$ = newReturnNode($2); }
    | RETURN ';'                                                    { $$ = newReturnNode(NULL); }
    | Block                                                         { $$ = $1; }
    | WHILE '(' Expression ')' Block                                { $$ = newWhileNode($3, $5); }
    | FOR '(' Assignment ';' Expression ';' Assignment ')' Block    {
                                                                        NodeArray *initialization = appendNode(NULL, $3); // Creamos el NodeArray de la inicialización del For
                                                                        NodeArray *whileBodyItems = appendNode(appendNode(NULL, $9), $7); // Creamos el cuerpo del While: creamos el NodeArray del Block y le agregamos al final el Step
                                                                        NodeAST *whileBody = newBlockNode(whileBodyItems);
                                                                        NodeAST *whileNode = newWhileNode($5, whileBody);
                                                                        NodeArray *forItems = appendNode(initialization, whileNode); // Creamos el NodeArray del For: agregamos la inicialización y el While
                                                                        NodeAST *forStatement = newBlockNode(forItems);
                                                                        $$ = forStatement;
                                                                    }
    | IfStatement                                                   { $$ = $1; }
    | ';'                                                           { $$ = NULL; }
    ;

IfStatement
    : IF '(' Expression ')' Block                  { $$ = newIfElseNode($3, $5, NULL); }
    | IF '(' Expression ')' Block ELSE Block       { $$ = newIfElseNode($3, $5, $7); }
    | IF '(' Expression ')' Block ELSE IfStatement { $$ = newIfElseNode($3, $5, newBlockNode(appendNode(NULL, $7))); }
    ;

Assignment
    : ID '=' Expression                         { $$ = newAssignmentNode($1, $3); }
    | ID ADD_ASSIGN Expression                  { $$ = newAssignmentNode($1, newOperatorNode(OP_ADD, newIdentifierNode(strdup($1)), $3)); }
    | ID SUB_ASSIGN Expression                  { $$ = newAssignmentNode($1, newOperatorNode(OP_SUB, newIdentifierNode(strdup($1)), $3)); }
    | ID MUL_ASSIGN Expression                  { $$ = newAssignmentNode($1, newOperatorNode(OP_MUL, newIdentifierNode(strdup($1)), $3)); }
    | ID DIV_ASSIGN Expression                  { $$ = newAssignmentNode($1, newOperatorNode(OP_DIV, newIdentifierNode(strdup($1)), $3)); }
    | ID INC                                    { $$ = newAssignmentNode($1, newOperatorNode(OP_ADD, newIdentifierNode(strdup($1)), newIntLiteralNode(1))); }
    | ID DEC                                    { $$ = newAssignmentNode($1, newOperatorNode(OP_SUB, newIdentifierNode(strdup($1)), newIntLiteralNode(1))); }
    ;

MethodCall
    : ID '(' ')'                                { $$ = newMethodCallNode($1, NULL); }
    | ID '(' ArgumentList ')'                   { $$ = newMethodCallNode($1, $3); }
    ;

ArgumentList
    : Expression                                { $$ = appendNode(NULL, $1); }
    | ArgumentList ',' Expression               { $$ = appendNode($1, $3); }
    ;

Expression
    : ID                                        { $$ = newIdentifierNode($1); }
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
    | Expression LEQ Expression                 { $$ = newOperatorNode(OP_NOT, newOperatorNode(OP_GT, $1, $3), NULL); }
    | Expression GEQ Expression                 { $$ = newOperatorNode(OP_NOT, newOperatorNode(OP_LT, $1, $3), NULL); }
    | Expression '>' Expression                 { $$ = newOperatorNode(OP_GT, $1, $3); }
    | Expression NEQ Expression                 { $$ = newOperatorNode(OP_NOT, newOperatorNode(OP_EQ, $1, $3), NULL); }
    | Expression EQ Expression                  { $$ = newOperatorNode(OP_EQ, $1, $3); }
    | Expression AND Expression                 { $$ = newOperatorNode(OP_AND, $1, $3); }
    | Expression OR Expression                  { $$ = newOperatorNode(OP_OR, $1, $3); }
    ;

%%
