%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "bison.tab.h"
#include "error_handler.h"

static int commentStartLine = 0;

int yywrap(void) {
    return 1;
}
%}

%option yylineno
%x COMMENT

DIGIT       [0-9]
ALPHA       [a-zA-Z]
ALPHA_NUM   {ALPHA}|{DIGIT}|"_"
ID          {ALPHA}({ALPHA_NUM})*
NUMBER      {DIGIT}+
FLOAT       {DIGIT}+"."{DIGIT}+

%%

    /* KEYWORDS */
"boolean"                       { return BOOLEAN; }
"int"                           { return INT; }
"float"                         { return FLOAT; }
"void"                          { return VOID; }
"if"                            { return IF; }
"else"                          { return ELSE; }
"while"                         { return WHILE; }
"return"                        { return RETURN; }

    /* CONSTANTES y LITERALES */
"true"                          { yylval.intval = 1; return BOOL_CONST; }
"false"                         { yylval.intval = 0; return BOOL_CONST; }
{FLOAT}                         { yylval.floatval = atof(yytext); return FLOAT_CONST; }
{NUMBER}                        { yylval.intval = atoi(yytext); return NUMBER; }

    /* IDs */
{ID}                            { yylval.strval = strdup(yytext); return ID; }

    /* ASIGNACION */
"="                             { return '='; }

    /* ARITMETICOS */
"+"|"-"|"*"|"/"|"%"             { return yytext[0]; }

    /* RELACIONALES */
"=="                            { return EQ; }
"<"|">"                         { return yytext[0]; }

    /* LOGICOS */
"&&"                            { return AND; }
"||"                            { return OR; }
"!"                             { return NOT; }

    /* DELIMITADORES */
"("|")"|"{"|"}"|";"|","         { return yytext[0]; }

    /* COMENTARIOS Y ESPACIOS EN BLANCO */
[ \t\r\n]+                      { /* eliminar blancos */ }
"//".*                          { /* comentario simple linea */ }
"/*"                            { commentStartLine = yylineno; BEGIN(COMMENT); }
<COMMENT>"*/"                   { BEGIN(INITIAL); }
<COMMENT>[^*]+                  { /* ignorar contenido del comentario */ }
<COMMENT>"*"                    { /* ignorar asterisco suelto */ }
<COMMENT><<EOF>>                {
                                    lexicalError(commentStartLine, "comentario multilinea sin cerrar");
                                    BEGIN(INITIAL);
                                    return 0;
                                }

    /* ERRORES LEXICOS */
.                               { lexicalError(yylineno, "simbolo no permitido '%s'", yytext); }

%%

#ifdef STANDALONE_LEXER
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

    /* Bucle para probar el lexer de forma independiente imprimiendo lo que retorna */
    int token;
    while ((token = yylex())) {
        printf("Token: %d, Lexema: %s\n", token, yytext);
    }

    if (argc > 1 && yyin) {
        fclose(yyin);
    }
    return 0;
}
#endif
