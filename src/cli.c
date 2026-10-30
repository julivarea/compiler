#include "cli.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *INPUT_EXTENSION = ".ctds";

static void printUsage(const char *program);
static bool parseTarget(const char *value, Target *target);
static bool hasInputExtension(const char *name);
static char *buildOutputName(const char *input, const char *extension);
static const char *getTargetExtension(const Options *options);

int parseArguments(int argc, char **argv, Options *options) {
    const char *program = (argc > 0) ? argv[0] : "c-tds";
    const char *requestedOutput = NULL;

    options->input = NULL;
    options->output = NULL;
    options->target = CURRENT_TARGET;
    options->explicitTarget = 0;
    options->debug = 0;
    options->opt = NULL;

    for (int i = 1; i < argc; i++) {
        char *argument = argv[i];

        if (strcmp(argument, "-o") == 0) {
            bool hasMissingArgument = (++i >= argc);
            if (hasMissingArgument) {
                fprintf(stderr, "Error: -o necesita el nombre del archivo de salida.\n");
                printUsage(program);
                return 1;
            }
            requestedOutput = argv[i];

        } else if (strcmp(argument, "-target") == 0) {
            bool hasMissingArgument = (++i >= argc);
            if (hasMissingArgument) {
                fprintf(stderr, "Error: -target necesita una etapa.\n");
                printUsage(program);
                return 1;
            }
            options->explicitTarget = 1;
            bool isTargetValid = parseTarget(argv[i], &options->target);
            if (!isTargetValid) {
                fprintf(stderr, "Error: etapa desconocida '%s'.\n", argv[i]);
                printUsage(program);
                return 1;
            }

        } else if (strcmp(argument, "-opt") == 0) {
            bool hasNextArgument = (i + 1 < argc);
            bool nextIsOption = (hasNextArgument && argv[i + 1][0] == '-');
            bool nextIsInputFile = (hasNextArgument && hasInputExtension(argv[i + 1]));
            bool hasOptimizationName = (hasNextArgument && !nextIsOption && !nextIsInputFile);

            if (hasOptimizationName) {
                options->opt = argv[++i];
            } else {
                options->opt = "all";
            }

        } else if (strcmp(argument, "-debug") == 0) {
            options->debug = 1;

        } else {
            bool isUnknownOption = (argument[0] == '-');
            if (isUnknownOption) {
                fprintf(stderr, "Error: opcion desconocida '%s'.\n", argument);
                printUsage(program);
                return 1;
            }

            bool hasMultipleInputs = (options->input != NULL);
            if (hasMultipleInputs) {
                fprintf(stderr, "Error: se recibio mas de un archivo de entrada ('%s' y '%s').\n",
                        options->input, argument);
                printUsage(program);
                return 1;
            }

            options->input = argument;
        }
    }

    bool hasMissingInput = (options->input == NULL);
    if (hasMissingInput) {
        fprintf(stderr, "Error: falta el archivo de entrada.\n");
        printUsage(program);
        return 1;
    }

    bool hasValidExtension = hasInputExtension(options->input);
    if (!hasValidExtension) {
        fprintf(stderr, "Error: el archivo de entrada debe tener extension %s ('%s').\n",
                INPUT_EXTENSION, options->input);
        return 1;
    }

    bool hasCustomOutput = (requestedOutput != NULL);
    if (hasCustomOutput) {
        options->output = malloc(strlen(requestedOutput) + 1);
        if (options->output != NULL) {
            strcpy(options->output, requestedOutput);
        }
    } else {
        options->output = buildOutputName(options->input, getTargetExtension(options));
    }

    bool hasAllocationError = (options->output == NULL);
    if (hasAllocationError) {
        fprintf(stderr, "Error: no se pudo reservar memoria para el nombre de salida.\n");
        return 1;
    }

    return 0;
}

void freeOptions(Options *options) {
    if (options == NULL) return;

    free(options->output);
    options->output = NULL;
}

static void printUsage(const char *program) {
    fprintf(stderr,
        "Uso: %s [opciones] archivo%s\n"
        "\n"
        "  -o <salida>      Nombre del archivo de salida\n"
        "  -target <etapa>  Compila hasta esa etapa: scan | parse | sem | codinter | assembly\n"
        "                   (por defecto: %s, la etapa mas avanzada implementada)\n"
        "  -opt [nombre]    Aplica optimizaciones; 'all' son todas. Por defecto ninguna\n"
        "  -debug           Imprime informacion de debugging por stdout\n",
        program, INPUT_EXTENSION, getTargetName(CURRENT_TARGET));
}

static bool parseTarget(const char *value, Target *target) {
    if (strcmp(value, "scan") == 0) {
        *target = TARGET_SCAN;
    } else if (strcmp(value, "parse") == 0) {
        *target = TARGET_PARSE;
    } else if (strcmp(value, "sem") == 0) {
        *target = TARGET_SEM;
    } else if (strcmp(value, "codinter") == 0) {
        *target = TARGET_CODINTER;
    } else if (strcmp(value, "assembly") == 0) {
        *target = TARGET_ASSEMBLY;
    } else {
        return false;
    }

    return true;
}

static bool hasInputExtension(const char *name) {
    size_t nameLength = strlen(name);
    size_t extensionLength = strlen(INPUT_EXTENSION);

    bool isTooShort = (nameLength <= extensionLength);
    if (isTooShort) return false;

    return strcmp(name + nameLength - extensionLength, INPUT_EXTENSION) == 0;
}

static char *buildOutputName(const char *input, const char *extension) {
    size_t baseLength = strlen(input) - strlen(INPUT_EXTENSION);
    size_t extensionLength = strlen(extension);

    char *output = malloc(baseLength + extensionLength + 1);
    bool hasAllocationError = (output == NULL);
    if (hasAllocationError) return NULL;

    memcpy(output, input, baseLength);
    memcpy(output + baseLength, extension, extensionLength + 1);

    return output;
}

static const char *getTargetExtension(const Options *options) {
    bool producesExecutable = BUILDS_EXECUTABLE(options);
    if (producesExecutable) return ".out";

    bool hasOptimization = (options->opt != NULL);
    if (hasOptimization) return ".opt";

    switch (options->target) {
        case TARGET_SCAN:     return ".lex";
        case TARGET_PARSE:    return ".sint";
        case TARGET_SEM:      return ".sem";
        case TARGET_CODINTER: return ".ci";
        case TARGET_ASSEMBLY: return ".ass";
    }

    return ".out";
}

const char *getTargetName(Target target) {
    switch (target) {
        case TARGET_SCAN:     return "scan";
        case TARGET_PARSE:    return "parse";
        case TARGET_SEM:      return "sem";
        case TARGET_CODINTER: return "codinter";
        case TARGET_ASSEMBLY: return "assembly";
    }

    return "?";
}
