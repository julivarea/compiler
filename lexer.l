%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int yywrap(void) {
    return 1;
}
%}

%option yylineno

DIGIT       [0-9]
ALPHA        [a-zA-Z]
ALPHA_NUM    {ALPHA}|{DIGIT}|"_"
ID          {ALPHA}({ALPHA_NUM})*
NUMBER      {DIGIT}+
FLOAT       {DIGIT}+"."{DIGIT}+

%%

"boolean"                       { printf("[Linea %d] BOOLEAN: %s\n", yylineno, yytext); }
"int"                           { printf("[Linea %d] INT: %s\n", yylineno, yytext); }
"float"                         { printf("[Linea %d] FLOAT: %s\n", yylineno, yytext); }
"void"                          { printf("[Linea %d] VOID: %s\n", yylineno, yytext); }
"if"                            { printf("[Linea %d] IF: %s\n", yylineno, yytext); }
"else"                          { printf("[Linea %d] ELSE: %s\n", yylineno, yytext); }
"while"                         { printf("[Linea %d] WHILE: %s\n", yylineno, yytext); }
"return"                        { printf("[Linea %d] RETURN: %s\n", yylineno, yytext); }
"main"                          { printf("[Linea %d] MAIN: %s\n", yylineno, yytext); }

"true"|"false"                  { printf("[Linea %d] BOOLEAN_CONST: %s\n", yylineno, yytext); }
{FLOAT}                         { printf("[Linea %d] FLOAT_CONST: %s\n", yylineno, yytext); }
{NUMBER}                        { printf("[Linea %d] NUMBER: %s\n", yylineno, yytext); }

{ID}                            { printf("[Linea %d] ID: %s\n", yylineno, yytext); }

"="                             { printf("[Linea %d] ASSIGN: %s\n", yylineno, yytext); }

"+"|"-"|"*"|"/"|"%"             { printf("[Linea %d] OPERATOR: %s\n", yylineno, yytext); }

"=="|"<"|">"                    { printf("[Linea %d] RELATIONAL_OP: %s\n", yylineno, yytext); }

"&&"|"||"                       { printf("[Linea %d] LOGIC_OP: %s\n", yylineno, yytext); }
"!"                             { printf("[Linea %d] NOT: %s\n", yylineno, yytext); }

"("|")"|"{"|"}"|";"|","         { printf("[Linea %d] DELIMITER: %s\n", yylineno, yytext); }

[ \t\r\n]+                      { /* eliminar blancos */ }
"//".*                          { printf("[Linea %d] SINGLE_LINE_COMMENT: %s\n", yylineno, yytext); }
"/*"([^*]|\*+[^*/])*\*+"/"      { printf("[Linea %d] MULTI_LINE_COMMENT: %s\n", yylineno, yytext); }

.                               { printf("[Linea %d] ERROR LEXICO: simbolo no permitido '%s'\n", yylineno, yytext); }

%%

int main(int argc, char **argv) {
    if (argc > 1) {
        yyin = fopen(argv[1], "r");
        if (!yyin) {
            perror("Error al abrir archivo");
            return 1;
        }
    } else {
        yyin = stdin;
    }

    yylex();

    if (argc > 1 && yyin) {
        fclose(yyin);
    }
    return 0;
}

