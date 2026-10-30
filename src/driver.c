#include "driver.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "ast.h"
#include "bison.tab.h"
#include "cli.h"
#include "error_handler.h"
#include "intermediate_code.h"
#include "object_code.h"
#include "semantic_analyzer.h"
#include "string_builder.h"
/* 
    @brief ejecuta lo que le pide el cli y devuelve errores en caso de tenerlos
*/
#define ANSI_BOLD_RED "\033[1;31m"
#define ANSI_RESET    "\033[0m"
#define MAX_PATH_LENGTH 4096

extern NodeAST *raizAST;
extern int yyparse(void);
extern int yylex(void);
extern char *yytext;
extern FILE *yyin;
extern int yylineno;

static int runScan(const Options *options);
static int runParse(void);
static int runSemantic(void);
static int runIntermediateCode(const Options *options, const TACList *tacList);
static int runAssembly(const Options *options, const TACList *tacList);

static int writeASTTo(const Options *options);
static int writeBuiltStringTo(const char *path);
static FILE *openOutputFile(const char *path);
static int assembleAndLink(const Options *options, const char *assemblyPath);
static const char *getTokenName(int token);

int compile(const Options *options) {
    switch (options->target) {
        case TARGET_SCAN:
            return runScan(options);

        case TARGET_PARSE:
            if (runParse() != 0) {
                return 1;
            }
            return writeASTTo(options);

        case TARGET_SEM:
            if (runParse() != 0) { return 1; }
            if (runSemantic() != 0) { return 1; }
            return writeASTTo(options);

        case TARGET_CODINTER: {
            if (runParse() != 0) { return 1; }
            if (runSemantic() != 0) { return 1; }
            const TACList *tacList = generateIntermediateCode(raizAST);
            return runIntermediateCode(options, tacList);
        }

        case TARGET_ASSEMBLY: {
            if (runParse() != 0) { return 1; }
            if (runSemantic() != 0) { return 1; }
            const TACList *tacList = generateIntermediateCode(raizAST);
            return runAssembly(options, tacList);
        }

        default:
            return 1;
    }
}


static int runScan(const Options *options) {
    FILE *outputFile = openOutputFile(options->output);
    if (outputFile == NULL) {
        return 1;
    }

    int token;
    while ((token = yylex()) != 0) {
        char singleCharacter[4];
        const char *tokenName = getTokenName(token);

        if (tokenName == NULL) {
            snprintf(singleCharacter, sizeof(singleCharacter), "'%c'", token);
            tokenName = singleCharacter;
        }
        fprintf(outputFile, "[Linea %d] %-12s %s\n", yylineno, tokenName, yytext);
        if (options->debug) {
            printf("[Linea %d] %-12s %s\n", yylineno, tokenName, yytext);
        }
    }
    fclose(outputFile);

    if (lexical_errors > 0) {
        remove(options->output);
        return 1;
    }
    return 0;
}

static int runParse(void) {
    bool hasError = yyparse() != 0;
    if (hasError) { return 1; }

    if (lexical_errors > 0) {
        fprintf(stderr, "\n" ANSI_BOLD_RED "Compilacion abortada: se encontraron %d errores lexicos." ANSI_RESET "\n", lexical_errors);
        return 1;
    }

    return 0;
}

static int runSemantic(void) {
    analyzeSemantics(raizAST, NULL);

    if (semantic_errors > 0) {
        fprintf(stderr, "\n" ANSI_BOLD_RED "Compilacion abortada: se encontraron %d errores semanticos." ANSI_RESET "\n", semantic_errors);
        return 1;
    }

    return 0;
}

static int runIntermediateCode(const Options *options, const TACList *tacList) {
    resetBuildString();
    printTACList(tacList);

    if (options->debug) {
        printf("%s", getBuiltString());
    }

    return writeBuiltStringTo(options->output);
}

static int runAssembly(const Options *options, const TACList *tacList) {
    resetBuildString();
    generateAssembly(tacList);

    if (getObjectCodeErrors() > 0) {
        fprintf(stderr, "\n" ANSI_BOLD_RED "Compilacion abortada: %d errores internos en la generacion de codigo objeto." ANSI_RESET "\n",
                getObjectCodeErrors());
        return 1;
    }

    if (options->debug) {
        printf("%s", getBuiltString());
    }

    if (!BUILDS_EXECUTABLE(options)) {
        return writeBuiltStringTo(options->output);
    }

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

static int writeASTTo(const Options *options) {
    FILE *outputFile = openOutputFile(options->output);
    if (outputFile == NULL) {
        return 1;
    }

    fprintAST(outputFile, raizAST, 0);
    fclose(outputFile);

    if (options->debug) {
        printAST(raizAST, 0);
    }
    return 0;
}

static int writeBuiltStringTo(const char *path) {
    FILE *outputFile = openOutputFile(path);
    if (outputFile == NULL) {
        return 1;
    }

    fputs(getBuiltString(), outputFile);
    fclose(outputFile);

    return 0;
}

static FILE *openOutputFile(const char *path) {
    FILE *outputFile = fopen(path, "w");
    if (outputFile == NULL) {
        perror(path);
    }
    return outputFile;
}

static int assembleAndLink(const Options *options, const char *assemblyPath) {
    char *const assemblerArguments[] = {
        "gcc", "-no-pie", "-x", "assembler",
        (char *)assemblyPath, "-o", (char *)options->output, NULL
    };

    if (options->debug) {
        printf("Ensamblando: gcc -no-pie -x assembler %s -o %s\n",
               assemblyPath, options->output);
    }

    pid_t processId = fork();
    if (processId < 0) {
        perror("fork");
        return 1;
    }

    if (processId == 0) {
        execvp(assemblerArguments[0], assemblerArguments);
        perror("gcc");
        _exit(127);
    }

    int status;
    if (waitpid(processId, &status, 0) < 0) {
        perror("waitpid");
        return 1;
    }

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        fprintf(stderr, "Error: fallo el ensamblado o el enlazado de '%s'.\n", assemblyPath);
        return 1;
    }

    return 0;
}

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
