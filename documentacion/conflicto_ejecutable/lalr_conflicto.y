%{
#include <stdio.h>
%}

%token INT VOID ID

%%

Program
    : VariableDeclarations MethodDeclarations
    ;

VariableDeclarations
    : /* empty */
    | VariableDeclaration VariableDeclarations
    ;

VariableDeclaration
    : Type ID ';'
    ;

MethodDeclarations
    : /* empty */
    | MethodDeclaration MethodDeclarations
    ;

MethodDeclaration
    : Type ID '(' ')' Block
    | VOID ID '(' ')' Block
    ;

Type
    : INT
    ;

Block
    : '{' '}'
    ;

%%