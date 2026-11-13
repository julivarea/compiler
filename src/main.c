#include <stdio.h>
#include "ast.h"
#include "cli.h"
#include "driver.h"

NodeAST *astRoot = NULL;
extern FILE *yyin;

int main(int argc, char **argv) {
    Options options;
    if (parseArguments(argc, argv, &options) != CLI_SUCCESS){ 
        return 1;
    }

    yyin = fopen(options.input, "r");

    // se maneja la falla del sistema, e.g : no such file or directory
    if (yyin == NULL) {
        perror(options.input);
        freeOptions(&options);
        return 1;
    }

    int exitCode = compile(&options);
    fclose(yyin);
    freeOptions(&options);
    return exitCode;
}
