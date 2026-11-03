#ifndef _POSIX_C_SOURCE /* strdup, strtok_r */
#define _POSIX_C_SOURCE 200809L
#endif
#include "cli.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static const struct {
    const char *name;
    OptimizationSet flag;
} OPT_NAMES[] = {
    {"all", OPTIMIZATION_ALL},
    {"const-fold", OPTIMIZATION_CONST_FOLD},
    {"const-prop", OPTIMIZATION_CONST_PROP},
    {"dead-code", OPTIMIZATION_DEAD_CODE},
    {"const-branch", OPTIMIZATION_CONST_BRANCH},
    {"self-assign", OPTIMIZATION_SELF_ASSIGN},
    {"peephole", OPTIMIZATION_PEEPHOLE},
    {"strength-reduce", OPTIMIZATION_STRENGTH},
};

static const char *INPUT_EXTENSION = ".ctds";

static const struct {
    const char *name;
    Target target;
    const char *extension;
} STAGES[] = {
    {"scan", TARGET_SCAN, ".lex"},
    {"parse", TARGET_PARSE, ".sint"},
    {"sem", TARGET_SEM, ".sem"},
    {"codinter", TARGET_CODINTER, ".ci"},
    {"assembly", TARGET_ASSEMBLY, ".ass"},
    {"interp", TARGET_INTERP, ".interp"},
    {"interpret", TARGET_INTERP, ".interp"}, 
};

static void initDefaultOptions(Options *options);
static bool parsePositionalArgument(const char *argument, Options *options, const char *program);
static bool validateInputFile(const Options *options, const char *program);
static bool parseOutputOption(int *index, int argc, char **argv, const char **requestedOutput, const char *program);
static bool parseTargetOption(int *index, int argc, char **argv, Options *options, const char *program);
static bool parseOptimizationOption(int *index, int argc, char **argv, Options *options, const char *program);
static bool resolveOutputPath(Options *options, const char *requestedOutput);
static bool validateOutputNotInput(const Options *options);

static bool parseTarget(const char *value, Target *target);
static bool parseOptimization(const char *value, OptimizationSet *optimizations);
static bool parseSingleOptimization(const char *name, OptimizationSet *outFlag);
static bool hasInputExtension(const char *name);
static char *buildOutputName(const char *input, const char *extension);
static const char *getTargetExtension(const Options *options);
static void printUsage(const char *program);

int parseArguments(int argc, char **argv, Options *options) {
    const char *program = (argc > 0) ? argv[0] : "c-tds";
    const char *requestedOutput = NULL;

    initDefaultOptions(options);

    for (int i = 1; i < argc; i++) {
        char *argument = argv[i];

        if (strcmp(argument, "-o") == 0) {
            if (!parseOutputOption(&i, argc, argv, &requestedOutput, program)) {
                return CLI_ERROR;
            }
        } else if (strcmp(argument, "-target") == 0) {
            if (!parseTargetOption(&i, argc, argv, options, program)) {
                return CLI_ERROR;
            }
        } else if (strcmp(argument, "-opt") == 0) {
            if (!parseOptimizationOption(&i, argc, argv, options, program)) {
                return CLI_ERROR;
            }
        } else if (strcmp(argument, "-debug") == 0) {
            options->debug = 1;
        } else {
            if (!parsePositionalArgument(argument, options, program)) {
                return CLI_ERROR;
            }
        }
    }

    if (!validateInputFile(options, program)) {
        return CLI_ERROR;
    }

    if (!resolveOutputPath(options, requestedOutput)) {
        return CLI_ERROR;
    }

    return CLI_SUCCESS;
}

void freeOptions(Options *options) {
    if (options == NULL) return;

    free(options->output);
    options->output = NULL;
}

const char *getTargetName(Target target) {
    for (size_t i = 0; i < sizeof(STAGES) / sizeof(STAGES[0]); i++) {
        if (STAGES[i].target == target) return STAGES[i].name;
    }
    return "?";
}

static void initDefaultOptions(Options *options) {
    options->input = NULL;
    options->output = NULL;
    options->target = CURRENT_TARGET;
    options->explicitTarget = 0;
    options->debug = 0;
    options->optimizations = OPTIMIZATION_NONE;
}

static bool parsePositionalArgument(const char *argument, Options *options, const char *program) {
    bool isUnknownOption = (argument[0] == '-');
    if (isUnknownOption) {
        fprintf(stderr, "Error: opcion desconocida '%s'.\n", argument);
        printUsage(program);
        return false;
    }

    bool hasMultipleInputs = (options->input != NULL);
    if (hasMultipleInputs) {
        fprintf(stderr, "Error: se recibio mas de un archivo de entrada ('%s' y '%s').\n",
                options->input, argument);
        printUsage(program);
        return false;
    }

    options->input = argument;
    return true;
}

static bool validateInputFile(const Options *options, const char *program) {
    bool hasMissingInput = (options->input == NULL);
    if (hasMissingInput) {
        fprintf(stderr, "Error: falta el archivo de entrada.\n");
        printUsage(program);
        return false;
    }

    bool hasValidExtension = hasInputExtension(options->input);
    if (!hasValidExtension) {
        fprintf(stderr, "Error: el archivo de entrada debe tener extension %s ('%s').\n",
                INPUT_EXTENSION, options->input);
        return false;
    }

    return true;
}

static bool parseOutputOption(int *index, int argc, char **argv, const char **requestedOutput, const char *program) {
    bool hasMissingArgument = (++(*index) >= argc);
    if (hasMissingArgument || argv[*index][0] == '\0') {
        fprintf(stderr, "Error: -o necesita el nombre del archivo de salida.\n");
        printUsage(program);
        return false;
    }
    *requestedOutput = argv[*index];
    return true;
}

static bool parseTargetOption(int *index, int argc, char **argv, Options *options, const char *program) {
    bool hasMissingArgument = (++(*index) >= argc);
    if (hasMissingArgument) {
        fprintf(stderr, "Error: -target necesita una etapa.\n");
        printUsage(program);
        return false;
    }
    options->explicitTarget = 1;
    bool isTargetValid = parseTarget(argv[*index], &options->target);
    if (!isTargetValid) {
        fprintf(stderr, "Error: etapa desconocida '%s'.\n", argv[*index]);
        printUsage(program);
        return false;
    }
    return true;
}

static bool parseOptimizationOption(int *index, int argc, char **argv, Options *options, const char *program) {
    bool hasNextArgument = (*index + 1 < argc);
    bool nextIsOption = (hasNextArgument && argv[*index + 1][0] == '-');
    bool nextIsInputFile = (hasNextArgument && hasInputExtension(argv[*index + 1]));
    bool hasOptimizationName = (hasNextArgument && !nextIsOption && !nextIsInputFile);

    const char *optName = "all";
    if (hasOptimizationName) {
        optName = argv[++(*index)];
    }

    if (!parseOptimization(optName, &options->optimizations)) {
        fprintf(stderr, "Error: optimizacion desconocida '%s'. Soportadas:", optName);
        for (size_t i = 0; i < sizeof(OPT_NAMES) / sizeof(OPT_NAMES[0]); i++) {
            fprintf(stderr, "%s%s", i > 0 ? ", " : "", OPT_NAMES[i].name);
        }
        fprintf(stderr, ".\n");
        printUsage(program);
        return false;
    }
    return true;
}

static bool resolveOutputPath(Options *options, const char *requestedOutput) {
    bool hasCustomOutput = (requestedOutput != NULL);
    if (hasCustomOutput) {
        options->output = strdup(requestedOutput);
    } else {
        options->output = buildOutputName(options->input, getTargetExtension(options));
    }

    bool hasAllocationError = (options->output == NULL);
    if (hasAllocationError) {
        fprintf(stderr, "Error: no se pudo reservar memoria para el nombre de salida.\n");
        return false;
    }

    if (!validateOutputNotInput(options)) {
        free(options->output);
        options->output = NULL;
        return false;
    }

    return true;
}

static bool validateOutputNotInput(const Options *options) {
    bool isSameFile = (strcmp(options->output, options->input) == 0);
    if (!isSameFile) {
        struct stat statInput, statOutput;
        if (stat(options->input, &statInput) == 0 && stat(options->output, &statOutput) == 0) {
            isSameFile = (statInput.st_dev == statOutput.st_dev && statInput.st_ino == statOutput.st_ino);
        }
    }
    if (isSameFile) {
        fprintf(stderr, "Error: el archivo de salida no puede ser igual al de entrada ('%s').\n",
                options->input);
        return false;
    }
    return true;
}

static bool parseTarget(const char *value, Target *target) {
    for (size_t i = 0; i < sizeof(STAGES) / sizeof(STAGES[0]); i++) {
        if (strcmp(value, STAGES[i].name) == 0) {
            *target = STAGES[i].target;
            return true;
        }
    }
    return false;
}

static bool parseSingleOptimization(const char *name, OptimizationSet *outFlag) {
    for (size_t i = 0; i < sizeof(OPT_NAMES) / sizeof(OPT_NAMES[0]); i++) {
        if (strcmp(name, OPT_NAMES[i].name) == 0) {
            *outFlag = OPT_NAMES[i].flag;
            return true;
        }
    }
    return false;
}

static bool parseOptimization(const char *value, OptimizationSet *optimizations) {
    if (value == NULL) {
        return true;
    }

    bool hasComma = (strchr(value, ',') != NULL);
    if (!hasComma) {
        OptimizationSet flag = 0;
        bool isValid = parseSingleOptimization(value, &flag);
        if (!isValid) {
            return false;
        }
        *optimizations |= flag;
        return true;
    }

    char *copy = strdup(value);
    if (copy == NULL) {
        return false;
    }

    OptimizationSet accumulated = 0;
    char *savePointer = NULL;
    char *token = strtok_r(copy, ",", &savePointer);

    while (token != NULL) {
        OptimizationSet flag = 0;
        bool isValid = parseSingleOptimization(token, &flag);
        if (!isValid) {
            free(copy);
            return false;
        }

        accumulated |= flag;
        token = strtok_r(NULL, ",", &savePointer);
    }

    free(copy);
    *optimizations |= accumulated;
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

    bool hasOptimization = (options->optimizations != OPTIMIZATION_NONE);
    if (hasOptimization) return ".opt";

    for (size_t i = 0; i < sizeof(STAGES) / sizeof(STAGES[0]); i++) {
        if (STAGES[i].target == options->target) return STAGES[i].extension;
    }

    return ".out";
}

static void printUsage(const char *program) {
    fprintf(stderr, "Uso: %s [opciones] archivo%s\n", program, INPUT_EXTENSION);
    fprintf(stderr, "  -o <salida>      Nombre del archivo de salida\n");
    fprintf(stderr, "  -target <etapa>  Compila hasta esa etapa: ");
    bool first = true;
    for (size_t i = 0; i < sizeof(STAGES) / sizeof(STAGES[0]); i++) {
        /* imprime cada etapa una sola vez (omite alias como interpret) */
        if (i > 0 && STAGES[i].target == STAGES[i - 1].target) continue;
        fprintf(stderr, "%s%s", first ? "" : " | ", STAGES[i].name);
        first = false;
    }
    fprintf(stderr, "\n                   (por defecto: %s, la etapa mas avanzada implementada)\n",
            getTargetName(CURRENT_TARGET));
    fprintf(stderr, "  -opt [nombre]    Aplica optimizaciones; 'all' son todas. Por defecto ninguna\n");
    fprintf(stderr, "  -debug           Imprime informacion de debugging por stdout\n");
}
