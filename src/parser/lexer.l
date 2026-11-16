%{
/**
 * @file lexer.l
 * @brief Analizador léxico de C-TDS implementado en Flex.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "bison.tab.h"
#include "error_handler.h"

static int commentStartLine = 0;
%}

%option yylineno noyywrap nounput noinput
%x COMMENT

DIGIT       [0-9]
ALPHA       [a-zA-Z]
ALPHA_NUM   {ALPHA}|{DIGIT}|"_"
ID          {ALPHA}({ALPHA_NUM})*
NUMBER      {DIGIT}+
FLOAT       {DIGIT}+"."{DIGIT}+
HEX         0[xX][0-9a-fA-F]+
BIN         0[bB][01]+

%%

    /* KEYWORDS */
"boolean"                       { return BOOLEAN; }
"int"                           { return INT; }
"float"                         { return FLOAT; }
"void"                          { return VOID; }
"if"                            { return IF; }
"else"                          { return ELSE; }
"while"                         { return WHILE; }
"do"                            { return DO; }
"for"                           { return FOR; }
"break"                         { return BREAK; }
"continue"                      { return CONTINUE; }
"return"                        { return RETURN; }

    /* CONSTANTES y LITERALES */
"true"                          { yylval.intval = 1; return BOOL_CONST; }
"false"                         { yylval.intval = 0; return BOOL_CONST; }
{FLOAT}                         { yylval.floatval = atof(yytext); return FLOAT_CONST; }
{HEX}                           { yylval.intval = (int)strtol(yytext+2, NULL, 16); return NUMBER;}
{BIN}                           { yylval.intval = (int)strtol(yytext+2, NULL, 2); return NUMBER;}
{NUMBER}                        { yylval.intval = atoi(yytext); return NUMBER; }

    /* IDs */
{ID}                            { yylval.strval = strdup(yytext); return ID; }

    /* ASIGNACION */
"="                             { return '='; }

    /* ARITMETICOS */
"+="                            { return ADD_ASSIGN; }
"-="                            { return SUB_ASSIGN; }
"*="                            { return MUL_ASSIGN; }
"/="                            { return DIV_ASSIGN; }
"++"                            { return INC; }
"--"                            { return DEC; }
"+"|"-"|"*"|"/"|"%"             { return yytext[0]; }

    /* RELACIONALES */
"!="                            { return NEQ; }
"<="                            { return LEQ; }
">="                            { return GEQ; }
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

