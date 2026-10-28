%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "symbol_table.h"
#include "semantic_analyzer.h"
#include "error_handler.h"
#include "string_builder.h"
#include "intermediate_code.h"
#include "object_code.h"
#include "cli.h"
extern int yylex(void);
extern char *yytext;
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
        NodeAST *decl = newNode(VARIABLE_DECLARATION_NODE, newSymbol($2, ID_SYMBOL, $1, NULL), NULL, NULL, NULL);
        decl->type = $1;
        $$ = initializeTemporaryList(decl, NULL); 
    }
    | Parameters ',' Type ID {
        NodeAST *decl = newNode(VARIABLE_DECLARATION_NODE, newSymbol($4, ID_SYMBOL, $3, NULL), NULL, NULL, NULL);
        decl->type = $3;
        $$ = appendToTemporaryList($1, decl);
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

/* Nombre del token para el volcado de -target scan. Devuelve NULL para los
 * tokens de un solo caracter, que el lexer retorna como su propio codigo. */
static const char *getTokenName(int token) {
    switch (token) {
        case VOID:        return "VOID";
        case INT:         return "INT";
        case FLOAT:       return "FLOAT";
        case BOOLEAN:     return "BOOLEAN";
        case IF:          return "IF";
        case ELSE:        return "ELSE";
        case WHILE:       return "WHILE";
        case RETURN:      return "RETURN";
        case EQ:          return "EQ";
        case AND:         return "AND";
        case OR:          return "OR";
        case NOT:         return "NOT";
        case ID:          return "ID";
        case NUMBER:      return "NUMBER";
        case BOOL_CONST:  return "BOOL_CONST";
        case FLOAT_CONST: return "FLOAT_CONST";
        default:          return NULL;
    }
}

/* Etapa scan: consume la entrada entera y vuelca la secuencia de tokens. */
static int runScan(const Options *options) {
    FILE *out = fopen(options->output, "w");
    if (out == NULL) {
        perror(options->output);
        return 1;
    }

    int token;
    while ((token = yylex()) != 0) {
        char singleChar[4];
        const char *name = getTokenName(token);

        if (name == NULL) {
            snprintf(singleChar, sizeof(singleChar), "'%c'", token);
            name = singleChar;
        }
        fprintf(out, "[Linea %d] %-12s %s\n", yylineno, name, yytext);
        if (options->debug) {
            printf("[Linea %d] %-12s %s\n", yylineno, name, yytext);
        }
    }
    fclose(out);

    if (lexical_errors > 0) {
        remove(options->output); /* una etapa que fallo no deja archivo */
        return 1;
    }
    return 0;
}

/* Etapas parse y sem: el producto es el AST, anotado o no segun la etapa. */
static int writeASTOutput(const Options *options) {
    FILE *out = fopen(options->output, "w");
    if (out == NULL) {
        perror(options->output);
        return 1;
    }

    fprintAST(out, raizAST, 0);
    fclose(out);

    if (options->debug) {
        printAST(raizAST, 0);
    }
    return 0;
}

/* Etapa codinter: el producto es el codigo de tres direcciones. */
static int writeIntermediateCodeOutput(const Options *options) {
    generateIntermediateCode(raizAST);
    printTACList(getIntermediateCode());

    FILE *out = fopen(options->output, "w");
    if (out == NULL) {
        perror(options->output);
        return 1;
    }

    fputs(getBuiltString(), out);
    fclose(out);

    if (options->debug) {
        printf("%s", getBuiltString());
    }
    return 0;
}

/* Las rutas se acotan a PATH_MAX, que es el limite del sistema de archivos, y
 * el comando de ensamblado necesita espacio para dos de ellas mas el resto. */
#define MAX_PATH_LENGTH 4096
#define MAX_COMMAND_LENGTH 8704

/* Escribe el contenido acumulado en el string_builder al archivo indicado. */
static int writeBuiltStringTo(const char *path) {
    FILE *out = fopen(path, "w");
    if (out == NULL) {
        perror(path);
        return 1;
    }

    fputs(getBuiltString(), out);
    fclose(out);

    return 0;
}

/*
 * Invoca al ensamblador del sistema para convertir el .ass en un ejecutable.
 *
 * Se usa gcc en vez de as + ld porque resuelve por su cuenta el runtime de C
 * (_start, la llamada a main y la salida del proceso), que es lo que permite
 * que el codigo de salida de main llegue al shell. -no-pie evita que el
 * enlazador exija codigo independiente de posicion, que es lo que asume el
 * acceso a globales por nombre(%rip) que emite el generador.
 */
static int assembleAndLink(const Options *options, const char *assemblyPath) {
    char command[MAX_COMMAND_LENGTH];

    /* -x assembler le dice explicitamente a gcc que el archivo es assembly, sin
     * depender de que deduzca el lenguaje por la extension. */
    int commandLength = snprintf(command, sizeof(command), "gcc -no-pie -x assembler '%s' -o '%s'",
                                 assemblyPath, options->output);

    /* snprintf trunca en silencio: si no se chequea, con rutas largas se
     * ejecutaria un comando cortado a la mitad y el error seria incomprensible. */
    if (commandLength < 0 || (size_t)commandLength >= sizeof(command)) {
        fprintf(stderr, "Error: las rutas son demasiado largas para construir el comando de ensamblado"
                        " (limite %zu caracteres).\n", sizeof(command) - 1);
        return 1;
    }

    if (options->debug) {
        printf("Ensamblando: %s\n", command);
    }

    int status = system(command);
    if (status != 0) {
        fprintf(stderr, "Error: fallo el ensamblado o el enlazado de '%s'.\n", assemblyPath);
        return 1;
    }

    return 0;
}

/* Etapa assembly: el producto es el .ass, o el ejecutable si no se pidio una
 * etapa explicita con -target. */
static int writeAssemblyOutput(const Options *options) {
    generateIntermediateCode(raizAST);
    generateAssembly(getIntermediateCode());

    /* Un error interno significa que el assembly emitido no es confiable.
     * Preferimos abortar antes que ensamblarlo y entregar un binario que
     * calcula mal o que falla recien en el enlazador. */
    if (getObjectCodeErrors() > 0) {
        fprintf(stderr, "\n\033[1;31mCompilacion abortada: %d errores internos en la generacion de codigo objeto.\033[0m\n",
                getObjectCodeErrors());
        return 1;
    }

    if (options->debug) {
        printf("%s", getBuiltString());
    }

    /* Con -target assembly el .ass es el entregable y la compilacion termina
     * ahi. Sin -target, el .ass es solo un paso intermedio hacia el ejecutable
     * y se borra al final. */
    if (!BUILDS_EXECUTABLE(options)) {
        return writeBuiltStringTo(options->output);
    }

    /*
     * El intermedio usa la extension .tmp.s y no .ass a proposito: si usara
     * .ass tomaria exactamente el nombre que produce `-target assembly`, de
     * modo que compilar al ejecutable pisaria y despues borraria un archivo
     * que el usuario pudo haber generado a mano para inspeccionarlo.
     */
    char assemblyPath[MAX_PATH_LENGTH];
    int pathLength = snprintf(assemblyPath, sizeof(assemblyPath), "%s.tmp.s", options->output);
    if (pathLength < 0 || (size_t)pathLength >= sizeof(assemblyPath)) {
        fprintf(stderr, "Error: la ruta del archivo intermedio excede los %zu caracteres.\n",
                sizeof(assemblyPath) - 1);
        return 1;
    }

    if (writeBuiltStringTo(assemblyPath) != 0) {
        return 1;
    }

    int result = assembleAndLink(options, assemblyPath);

    if (!options->debug) {
        remove(assemblyPath);
    } else {
        printf("Assembly intermedio conservado en %s\n", assemblyPath);
    }

    return result;
}

int main(int argc, char** argv) {
    Options options;

    if (parseArguments(argc, argv, &options) != 0) {
        return 1;
    }

    if (options.target > CURRENT_TARGET) {
        fprintf(stderr, "Error: la etapa '%s' todavia no esta implementada; la ultima es '%s'.\n",
                getTargetName(options.target), getTargetName(CURRENT_TARGET));
        freeOptions(&options);
        return 1;
    }

    if (options.opt != NULL) {
        fprintf(stderr, "Aviso: no hay optimizaciones implementadas todavia, se ignora -opt %s.\n",
                options.opt);
    }

    yyin = fopen(options.input, "r");
    if (yyin == NULL) {
        perror(options.input);
        freeOptions(&options);
        return 1;
    }

    if (options.debug) {
        printf("Entrada: %s\n", options.input);
        printf("Etapa:   %s\n", getTargetName(options.target));
        printf("Salida:  %s\n\n", options.output);
    }

    // Una sola salida al final, asi el archivo se cierra en todos los caminos
    // y cualquier fase que falle termina con codigo distinto de 0.
    int exit_code = 0;

    if (options.target == TARGET_SCAN) {
        exit_code = runScan(&options);
    } else if (yyparse() != 0) {
        // yyerror ya reporto el error sintactico por stderr
        exit_code = 1;
    } else if (lexical_errors > 0) {
        // El lexer descarta los simbolos invalidos, asi que el parseo puede
        // terminar bien sobre una entrada que igual era incorrecta.
        fprintf(stderr, "\n\033[1;31mCompilacion abortada: se encontraron %d errores lexicos.\033[0m\n", lexical_errors);
        exit_code = 1;
    } else if (options.target == TARGET_PARSE) {
        exit_code = writeASTOutput(&options);
    } else {
        analyzeSemantics(raizAST, NULL, TYPE_VOID);

        if (semantic_errors > 0) {
            fprintf(stderr, "\n\033[1;31mCompilacion abortada: se encontraron %d errores semanticos.\033[0m\n", semantic_errors);
            exit_code = 1;
        } else if (options.target == TARGET_SEM) {
            exit_code = writeASTOutput(&options);
        } else if (options.target == TARGET_CODINTER) {
            exit_code = writeIntermediateCodeOutput(&options);
        } else {
            exit_code = writeAssemblyOutput(&options);
        }
    }

    if (options.debug && exit_code == 0) {
        printf("\nSalida escrita en %s\n", options.output);
    }

    fclose(yyin);
    freeOptions(&options);
    return exit_code;
}
#endif
