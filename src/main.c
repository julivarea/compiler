#include <stdio.h>
#include <stdlib.h>
#include "bison.tab.h"
#include "ast.h"
#include "cli.h"
#include "error_handler.h"
#include "driver.h"

NodeAST *raizAST = NULL;
extern FILE *yyin;

int main(int argc, char **argv) {
    Options options;
    bool hasError = parseArguments(argc, argv, &options) != 0;
    if (hasError) {
        return 1;
    }

    yyin = fopen(options.input, "r");
    if (yyin == NULL) {
        perror(options.input);
        freeOptions(&options);
        return 1;
    }
    setErrorSource(options.input);

    int exitCode = compile(&options);

    if (options.debug && exitCode == 0) {
        printf("\nSalida escrita en %s\n", options.output);
    }

    fclose(yyin);
    freeOptions(&options);
    return exitCode;
}
