/**
 * @file lexer_stub.c
 * @brief Variable del lexer que necesitan los módulos del AST en los tests que no lo enlazan.
 *
 * ast.c lee yylineno para fechar cada nodo con su línea. Las suites que arman el AST a mano
 * (análisis semántico, código intermedio) no enlazan lex.yy.c, así que la definen acá.
 */

int yylineno = 1;
