#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "bison.tab.h"
#include "ast.h"
#include "cli.h"
#include "debug.h"
#include "error_handler.h"
#include "driver.h"

NodeAST *astRoot = NULL;
extern FILE *yyin;

int main(int argc, char **argv) {
    Options options;
    if (parseArguments(argc, argv, &options) != CLI_SUCCESS){ 
        return 1;
    }

    yyin = fopen(options.input, "r");
    if (yyin == NULL) {
        perror(options.input);
        freeOptions(&options);
        return 1;
    }

    int exitCode = compile(&options);

    debugPrintf(options.debug && exitCode == 0, "\n[DRIVER] Salida escrita en %s\n", options.output);

    fclose(yyin);
    freeOptions(&options);
    return exitCode;
}
