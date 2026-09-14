%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int yywrap(void) {
    return 1;
}
%}

DIGIT       [0-9]
CHAR        [a-zA-Z]
ID          {CHAR}({CHAR}|{DIGIT})*
NUMBER      {DIGIT}+

%%

"boolean"                       { printf("BOOLEAN: %s\n", yytext); }
"else"                          { printf("ELSE: %s\n", yytext); }
"if"                            { printf("IF: %s\n", yytext); }
"int"                           { printf("INT: %s\n", yytext); }
"return"                        { printf("RETURN: %s\n", yytext); }
"true"|"false"                  { printf("BOOLEAN_CONST: %s\n", yytext); }
"void"                          { printf("VOID: %s\n", yytext); }
"while"                         { printf("WHILE: %s\n", yytext); }
"float"                         { printf("FLOAT: %s\n", yytext); }
{ID}                            { printf("ID: %s\n", yytext); }
{NUMBER}                        { printf("NUMBER: %s\n", yytext); }
"+"|"-"|"*"|"="                 { printf("OPERATOR: %s\n", yytext); }
"("|")"|"{"|"}"|";"             { printf("DELIMITER: %s\n", yytext); }
[ \t\r\n]+                      { /* eliminar blancos */ }
"//".*                          { printf("SINGLE_LINE_COMMENT: %s\n", yytext); }
"/*"([^*]|\*+[^*/])*\*+"/"      { printf("MULTI_LINE_COMMENT: %s\n", yytext); }
.                               { printf("Token inválido: %s\n", yytext); }

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

