/**
 * @file test_cli.c
 * @brief Pruebas unitarias e integrales de la interfaz de linea de comandos (CLI) y driver.
 */

#define _GNU_SOURCE
#include "unity.h"
#include "cli.h"
#include "optimizer.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

static Options testOptions;

void setUp(void) {
    memset(&testOptions, 0, sizeof(Options));
}

void tearDown(void) {
    freeOptions(&testOptions);
}

// Helper para invocar parseArguments con un arreglo literal de argumentos
static int parseArgv(int count, const char **args, Options *opts) {
    char **argv = malloc((size_t)(count + 1) * sizeof(char *));
    for (int i = 0; i < count; i++) {
        argv[i] = (char *)args[i];
    }
    argv[count] = NULL;
    int rc = parseArguments(count, argv, opts);
    free(argv);
    return rc;
}

// Helper para ejecutar un comando y capturar codigo de salida y stdout
static int execCommand(const char *cmd, char *outBuf, size_t outSize) {
    if (outBuf != NULL && outSize > 0) {
        outBuf[0] = '\0';
    }
    FILE *fp = popen(cmd, "r");
    if (fp == NULL) return -1;
    if (outBuf != NULL && outSize > 0) {
        size_t total = 0;
        char chunk[256];
        while (fgets(chunk, sizeof(chunk), fp) != NULL) {
            size_t len = strlen(chunk);
            if (total + len < outSize) {
                strcat(outBuf, chunk);
                total += len;
            }
        }
    }
    int status = pclose(fp);
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return -1;
}

// Helper para crear un archivo temporal con contenido
static void createTempSourceFile(char *pathTemplate, const char *content) {
    int fd = mkstemps(pathTemplate, 5);
    TEST_ASSERT_TRUE(fd >= 0);
    if (content != NULL) {
        ssize_t written = write(fd, content, strlen(content));
        (void)written;
    }
    close(fd);
}

// Helper para verificar existencia de un archivo
static bool fileExists(const char *path) {
    struct stat st;
    return (stat(path, &st) == 0);
}

// Helper para leer el contenido de un archivo
static char *readFileContent(const char *path) {
    FILE *f = fopen(path, "r");
    if (f == NULL) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)size + 1);
    if (buf == NULL) {
        fclose(f);
        return NULL;
    }
    size_t readBytes = fread(buf, 1, (size_t)size, f);
    buf[readBytes] = '\0';
    fclose(f);
    return buf;
}

// Tests Unitarios de parseArguments

// Invocacion sin argumentos falla con codigo 1
void test_cli_no_arguments_fails(void) {
    const char *args[] = { "mi_compilador" };
    int rc = parseArgv(1, args, &testOptions);
    TEST_ASSERT_EQUAL_INT(1, rc);
    TEST_ASSERT_NULL(testOptions.output);
}

// Opcion -target sin archivo de entrada falla con codigo 1
void test_cli_missing_input_file_fails(void) {
    const char *args[] = { "mi_compilador", "-target", "scan" };
    int rc = parseArgv(3, args, &testOptions);
    TEST_ASSERT_EQUAL_INT(1, rc);
    TEST_ASSERT_NULL(testOptions.output);
}

// Archivo sin extension .ctds se rechaza con codigo 1
void test_cli_invalid_extension_fails(void) {
    const char *args1[] = { "mi_compilador", "programa.txt" };
    TEST_ASSERT_EQUAL_INT(1, parseArgv(2, args1, &testOptions));

    const char *args2[] = { "mi_compilador", "programa.c" };
    TEST_ASSERT_EQUAL_INT(1, parseArgv(2, args2, &testOptions));

    const char *args3[] = { "mi_compilador", "programa" };
    TEST_ASSERT_EQUAL_INT(1, parseArgv(2, args3, &testOptions));
}

// Archivo con nombre solo .ctds sin prefijo se rechaza
void test_cli_bare_extension_fails(void) {
    const char *args[] = { "mi_compilador", ".ctds" };
    int rc = parseArgv(2, args, &testOptions);
    TEST_ASSERT_EQUAL_INT(1, rc);
}

// Flag -o sin valor al final de argv falla con codigo 1
void test_cli_missing_output_value_fails(void) {
    const char *args[] = { "mi_compilador", "prog.ctds", "-o" };
    int rc = parseArgv(3, args, &testOptions);
    TEST_ASSERT_EQUAL_INT(1, rc);
}

// Flag -o con valor vacio falla con codigo 1
void test_cli_empty_output_value_fails(void) {
    const char *args[] = { "mi_compilador", "-o", "", "prog.ctds" };
    int rc = parseArgv(4, args, &testOptions);
    TEST_ASSERT_EQUAL_INT(1, rc);
}

// Flag -target sin valor al final de argv falla con codigo 1
void test_cli_missing_target_value_fails(void) {
    const char *args[] = { "mi_compilador", "prog.ctds", "-target" };
    int rc = parseArgv(3, args, &testOptions);
    TEST_ASSERT_EQUAL_INT(1, rc);
}

// Flag -target con etapa desconocida falla con codigo 1
void test_cli_unknown_target_fails(void) {
    const char *args[] = { "mi_compilador", "-target", "inexistente", "prog.ctds" };
    int rc = parseArgv(4, args, &testOptions);
    TEST_ASSERT_EQUAL_INT(1, rc);
}

// Opcion desconocida que empieza con guion falla con codigo 1
void test_cli_unknown_option_fails(void) {
    const char *args[] = { "mi_compilador", "-invalida", "prog.ctds" };
    int rc = parseArgv(3, args, &testOptions);
    TEST_ASSERT_EQUAL_INT(1, rc);
}

// Mas de un archivo de entrada se rechaza con codigo 1
void test_cli_multiple_inputs_fails(void) {
    const char *args[] = { "mi_compilador", "prog1.ctds", "prog2.ctds" };
    int rc = parseArgv(3, args, &testOptions);
    TEST_ASSERT_EQUAL_INT(1, rc);
}

// Nombre de archivo que empieza con guion se rechaza
void test_cli_input_starts_with_dash_fails(void) {
    const char *args[] = { "mi_compilador", "-prog.ctds" };
    int rc = parseArgv(2, args, &testOptions);
    TEST_ASSERT_EQUAL_INT(1, rc);
}

// Archivo de salida -o igual al archivo de entrada se rechaza para proteger el fuente
void test_cli_output_equals_input_fails(void) {
    const char *args1[] = { "mi_compilador", "-target", "parse", "-o", "prog.ctds", "prog.ctds" };
    int rc1 = parseArgv(6, args1, &testOptions);
    TEST_ASSERT_EQUAL_INT(1, rc1);
    TEST_ASSERT_NULL(testOptions.output);

    const char *args2[] = { "mi_compilador", "-o", "prog.ctds", "prog.ctds" };
    int rc2 = parseArgv(4, args2, &testOptions);
    TEST_ASSERT_EQUAL_INT(1, rc2);
    TEST_ASSERT_NULL(testOptions.output);
}

// Sin opciones compila por defecto con ejecutable .out
void test_cli_default_target_and_output(void) {
    const char *args[] = { "mi_compilador", "ejemplo.ctds" };
    int rc = parseArgv(2, args, &testOptions);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_STRING("ejemplo.ctds", testOptions.input);
    TEST_ASSERT_EQUAL_STRING("ejemplo.out", testOptions.output);
    TEST_ASSERT_EQUAL_INT(CURRENT_TARGET, testOptions.target);
    TEST_ASSERT_EQUAL_INT(0, testOptions.explicitTarget);
    TEST_ASSERT_EQUAL_INT(0, testOptions.debug);
    TEST_ASSERT_EQUAL_INT(OPTIMIZATION_NONE, testOptions.optimizations);
    TEST_ASSERT_TRUE(BUILDS_EXECUTABLE(&testOptions));
}

// -target scan produce extension .lex
void test_cli_explicit_target_scan(void) {
    const char *args[] = { "mi_compilador", "-target", "scan", "ejemplo.ctds" };
    int rc = parseArgv(4, args, &testOptions);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_INT(TARGET_SCAN, testOptions.target);
    TEST_ASSERT_EQUAL_INT(1, testOptions.explicitTarget);
    TEST_ASSERT_EQUAL_STRING("ejemplo.lex", testOptions.output);
    TEST_ASSERT_FALSE(BUILDS_EXECUTABLE(&testOptions));
}

// -target parse produce extension .sint
void test_cli_explicit_target_parse(void) {
    const char *args[] = { "mi_compilador", "-target", "parse", "ejemplo.ctds" };
    int rc = parseArgv(4, args, &testOptions);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_INT(TARGET_PARSE, testOptions.target);
    TEST_ASSERT_EQUAL_STRING("ejemplo.sint", testOptions.output);
}

// -target sem produce extension .sem
void test_cli_explicit_target_sem(void) {
    const char *args[] = { "mi_compilador", "-target", "sem", "ejemplo.ctds" };
    int rc = parseArgv(4, args, &testOptions);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_INT(TARGET_SEM, testOptions.target);
    TEST_ASSERT_EQUAL_STRING("ejemplo.sem", testOptions.output);
}

// -target codinter produce extension .ci
void test_cli_explicit_target_codinter(void) {
    const char *args[] = { "mi_compilador", "-target", "codinter", "ejemplo.ctds" };
    int rc = parseArgv(4, args, &testOptions);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_INT(TARGET_CODINTER, testOptions.target);
    TEST_ASSERT_EQUAL_STRING("ejemplo.ci", testOptions.output);
}

// -target assembly produce extension .ass
void test_cli_explicit_target_assembly(void) {
    const char *args[] = { "mi_compilador", "-target", "assembly", "ejemplo.ctds" };
    int rc = parseArgv(4, args, &testOptions);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_INT(TARGET_ASSEMBLY, testOptions.target);
    TEST_ASSERT_EQUAL_STRING("ejemplo.ass", testOptions.output);
    TEST_ASSERT_FALSE(BUILDS_EXECUTABLE(&testOptions));
}

// -target interp o interpret produce extension .interp
void test_cli_explicit_target_interp(void) {
    const char *args1[] = { "mi_compilador", "-target", "interp", "ejemplo.ctds" };
    int rc1 = parseArgv(4, args1, &testOptions);
    TEST_ASSERT_EQUAL_INT(0, rc1);
    TEST_ASSERT_EQUAL_INT(TARGET_INTERP, testOptions.target);
    TEST_ASSERT_EQUAL_STRING("ejemplo.interp", testOptions.output);
    freeOptions(&testOptions);

    const char *args2[] = { "mi_compilador", "-target", "interpret", "ejemplo.ctds" };
    int rc2 = parseArgv(4, args2, &testOptions);
    TEST_ASSERT_EQUAL_INT(0, rc2);
    TEST_ASSERT_EQUAL_INT(TARGET_INTERP, testOptions.target);
    TEST_ASSERT_EQUAL_STRING("ejemplo.interp", testOptions.output);
}

// -o personaliza el nombre del archivo de salida
void test_cli_custom_output_flag(void) {
    const char *args[] = { "mi_compilador", "-o", "mi_salida.bin", "ejemplo.ctds" };
    int rc = parseArgv(4, args, &testOptions);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_STRING("mi_salida.bin", testOptions.output);
}

// -debug activa el flag de depuracion
void test_cli_debug_flag(void) {
    const char *args[] = { "mi_compilador", "-debug", "ejemplo.ctds" };
    int rc = parseArgv(3, args, &testOptions);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_INT(1, testOptions.debug);
}

// -opt all y -opt sin argumento activan OPTIMIZATION_ALL
void test_cli_opt_all_and_default(void) {
    const char *args1[] = { "mi_compilador", "-opt", "all", "ejemplo.ctds" };
    int rc1 = parseArgv(4, args1, &testOptions);
    TEST_ASSERT_EQUAL_INT(0, rc1);
    TEST_ASSERT_EQUAL_INT(OPTIMIZATION_ALL, testOptions.optimizations);
    freeOptions(&testOptions);

    const char *args2[] = { "mi_compilador", "-opt", "ejemplo.ctds" };
    int rc2 = parseArgv(3, args2, &testOptions);
    TEST_ASSERT_EQUAL_INT(0, rc2);
    TEST_ASSERT_EQUAL_INT(OPTIMIZATION_ALL, testOptions.optimizations);
}

// -opt con nombres especificos configura el conjunto de optimizaciones correspondiente
void test_cli_opt_specific_passes(void) {
    struct {
        const char *name;
        OptimizationSet expected;
    } cases[] = {
        { "const-fold", OPTIMIZATION_CONST_FOLD },
        { "const-prop", OPTIMIZATION_CONST_PROP },
        { "dead-code", OPTIMIZATION_DEAD_CODE },
        { "const-branch", OPTIMIZATION_CONST_BRANCH },
        { "self-assign", OPTIMIZATION_SELF_ASSIGN },
        { "peephole", OPTIMIZATION_PEEPHOLE },
        { "strength-reduce", OPTIMIZATION_STRENGTH }
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        const char *args[] = { "mi_compilador", "-opt", cases[i].name, "ejemplo.ctds" };
        int rc = parseArgv(4, args, &testOptions);
        TEST_ASSERT_EQUAL_INT(0, rc);
        TEST_ASSERT_EQUAL_INT(cases[i].expected, testOptions.optimizations);
        freeOptions(&testOptions);
    }
}

// -opt con nombre desconocido falla con codigo 1
void test_cli_opt_unknown_fails(void) {
    const char *args[] = { "mi_compilador", "-opt", "invalido", "ejemplo.ctds" };
    int rc = parseArgv(4, args, &testOptions);
    TEST_ASSERT_EQUAL_INT(1, rc);
}

// Con -opt y -target explicito la extension por defecto es .opt
void test_cli_opt_with_explicit_target_extension(void) {
    const char *args[] = { "mi_compilador", "-opt", "-target", "codinter", "ejemplo.ctds" };
    int rc = parseArgv(5, args, &testOptions);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_STRING("ejemplo.opt", testOptions.output);
}

// freeOptions es seguro contra NULL e idempotente
void test_cli_free_options_null_safe(void) {
    freeOptions(NULL);
    testOptions.output = strdup("salida_temporal");
    freeOptions(&testOptions);
    TEST_ASSERT_NULL(testOptions.output);
    freeOptions(&testOptions);
}

// getTargetName retorna las cadenas esperadas para cada etapa
void test_cli_get_target_name(void) {
    TEST_ASSERT_EQUAL_STRING("scan", getTargetName(TARGET_SCAN));
    TEST_ASSERT_EQUAL_STRING("parse", getTargetName(TARGET_PARSE));
    TEST_ASSERT_EQUAL_STRING("sem", getTargetName(TARGET_SEM));
    TEST_ASSERT_EQUAL_STRING("codinter", getTargetName(TARGET_CODINTER));
    TEST_ASSERT_EQUAL_STRING("assembly", getTargetName(TARGET_ASSEMBLY));
    TEST_ASSERT_EQUAL_STRING("interp", getTargetName(TARGET_INTERP));
    TEST_ASSERT_EQUAL_STRING("?", getTargetName((Target)999));
}

// Tests Integrales (End-to-End con el binario mi_compilador)

// Ejecucion sin argumentos devuelve codigo 1 y modo de uso
void test_cli_exec_no_arguments_returns_1(void) {
    char out[512];
    int rc = execCommand("./mi_compilador 2>&1", out, sizeof(out));
    TEST_ASSERT_EQUAL_INT(1, rc);
    TEST_ASSERT_NOT_NULL(strstr(out, "Uso:"));
}

// Archivo inexistente retorna codigo 1
void test_cli_exec_nonexistent_file_returns_1(void) {
    char out[512];
    int rc = execCommand("./mi_compilador /tmp/archivo_inexistente_xyz_123.ctds 2>&1", out, sizeof(out));
    TEST_ASSERT_EQUAL_INT(1, rc);
}

// Compilacion normal genera ejecutable .out y ejecuta correctamente
void test_cli_exec_default_produces_out_and_runs(void) {
    char srcFile[] = "/tmp/test_ctds_exec_XXXXXX.ctds";
    createTempSourceFile(srcFile, "void main() {}\n");

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "./mi_compilador %s", srcFile);
    int rc = execCommand(cmd, NULL, 0);
    TEST_ASSERT_EQUAL_INT(0, rc);

    char outFile[512];
    snprintf(outFile, sizeof(outFile), "%.*s.out", (int)(strlen(srcFile) - 5), srcFile);
    TEST_ASSERT_TRUE(fileExists(outFile));

    snprintf(cmd, sizeof(cmd), "%s", outFile);
    int runRc = execCommand(cmd, NULL, 0);
    TEST_ASSERT_EQUAL_INT(0, runRc);

    unlink(srcFile);
    unlink(outFile);
}

// Opcion -o personaliza el ejecutable sin dejar el default .out
void test_cli_exec_custom_output_name(void) {
    char srcFile[] = "/tmp/test_ctds_exec_XXXXXX.ctds";
    createTempSourceFile(srcFile, "void main() {}\n");

    char customOut[] = "/tmp/test_custom_bin_XXXXXX";
    int fd = mkstemp(customOut);
    close(fd);
    unlink(customOut);

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "./mi_compilador -o %s %s", customOut, srcFile);
    int rc = execCommand(cmd, NULL, 0);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_TRUE(fileExists(customOut));

    char defaultOut[512];
    snprintf(defaultOut, sizeof(defaultOut), "%.*s.out", (int)(strlen(srcFile) - 5), srcFile);
    TEST_ASSERT_FALSE(fileExists(defaultOut));

    unlink(srcFile);
    unlink(customOut);
}

// -target scan produce unicamente archivo .lex
void test_cli_exec_target_scan_produces_only_lex(void) {
    char srcFile[] = "/tmp/test_ctds_exec_XXXXXX.ctds";
    createTempSourceFile(srcFile, "void main() {}\n");

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "./mi_compilador -target scan %s", srcFile);
    int rc = execCommand(cmd, NULL, 0);
    TEST_ASSERT_EQUAL_INT(0, rc);

    char lexFile[512];
    snprintf(lexFile, sizeof(lexFile), "%.*s.lex", (int)(strlen(srcFile) - 5), srcFile);
    TEST_ASSERT_TRUE(fileExists(lexFile));

    unlink(srcFile);
    unlink(lexFile);
}

// -target parse produce unicamente archivo .sint
void test_cli_exec_target_parse_produces_only_sint(void) {
    char srcFile[] = "/tmp/test_ctds_exec_XXXXXX.ctds";
    createTempSourceFile(srcFile, "void main() {}\n");

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "./mi_compilador -target parse %s", srcFile);
    int rc = execCommand(cmd, NULL, 0);
    TEST_ASSERT_EQUAL_INT(0, rc);

    char sintFile[512];
    snprintf(sintFile, sizeof(sintFile), "%.*s.sint", (int)(strlen(srcFile) - 5), srcFile);
    TEST_ASSERT_TRUE(fileExists(sintFile));

    unlink(srcFile);
    unlink(sintFile);
}

// -target sem produce unicamente archivo .sem
void test_cli_exec_target_sem_produces_only_sem(void) {
    char srcFile[] = "/tmp/test_ctds_exec_XXXXXX.ctds";
    createTempSourceFile(srcFile, "void main() {}\n");

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "./mi_compilador -target sem %s", srcFile);
    int rc = execCommand(cmd, NULL, 0);
    TEST_ASSERT_EQUAL_INT(0, rc);

    char semFile[512];
    snprintf(semFile, sizeof(semFile), "%.*s.sem", (int)(strlen(srcFile) - 5), srcFile);
    TEST_ASSERT_TRUE(fileExists(semFile));

    unlink(srcFile);
    unlink(semFile);
}

// -target codinter produce unicamente archivo .ci
void test_cli_exec_target_codinter_produces_only_ci(void) {
    char srcFile[] = "/tmp/test_ctds_exec_XXXXXX.ctds";
    createTempSourceFile(srcFile, "void main() {}\n");

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "./mi_compilador -target codinter %s", srcFile);
    int rc = execCommand(cmd, NULL, 0);
    TEST_ASSERT_EQUAL_INT(0, rc);

    char ciFile[512];
    snprintf(ciFile, sizeof(ciFile), "%.*s.ci", (int)(strlen(srcFile) - 5), srcFile);
    TEST_ASSERT_TRUE(fileExists(ciFile));

    unlink(srcFile);
    unlink(ciFile);
}

// -target assembly produce unicamente archivo .ass
void test_cli_exec_target_assembly_produces_only_ass(void) {
    char srcFile[] = "/tmp/test_ctds_exec_XXXXXX.ctds";
    createTempSourceFile(srcFile, "void main() {}\n");

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "./mi_compilador -target assembly %s", srcFile);
    int rc = execCommand(cmd, NULL, 0);
    TEST_ASSERT_EQUAL_INT(0, rc);

    char assFile[512];
    snprintf(assFile, sizeof(assFile), "%.*s.ass", (int)(strlen(srcFile) - 5), srcFile);
    TEST_ASSERT_TRUE(fileExists(assFile));

    unlink(srcFile);
    unlink(assFile);
}

// Error lexico en -target scan retorna 1 y no deja archivo de salida
void test_cli_exec_lexical_error_scan_returns_1_and_no_output(void) {
    char srcFile[] = "/tmp/test_ctds_exec_XXXXXX.ctds";
    createTempSourceFile(srcFile, "void main() { int $x; }\n");

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "./mi_compilador -target scan %s 2>/dev/null", srcFile);
    int rc = execCommand(cmd, NULL, 0);
    TEST_ASSERT_EQUAL_INT(1, rc);

    char lexFile[512];
    snprintf(lexFile, sizeof(lexFile), "%.*s.lex", (int)(strlen(srcFile) - 5), srcFile);
    TEST_ASSERT_FALSE(fileExists(lexFile));

    unlink(srcFile);
}

// Error sintactico en -target parse retorna 1 y no deja archivo de salida
void test_cli_exec_syntax_error_returns_1_and_no_output(void) {
    char srcFile[] = "/tmp/test_ctds_exec_XXXXXX.ctds";
    createTempSourceFile(srcFile, "void main( { }\n");

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "./mi_compilador -target parse %s 2>/dev/null", srcFile);
    int rc = execCommand(cmd, NULL, 0);
    TEST_ASSERT_EQUAL_INT(1, rc);

    char sintFile[512];
    snprintf(sintFile, sizeof(sintFile), "%.*s.sint", (int)(strlen(srcFile) - 5), srcFile);
    TEST_ASSERT_FALSE(fileExists(sintFile));

    unlink(srcFile);
}

// Error semantico en -target sem retorna 1 y no deja archivo de salida
void test_cli_exec_semantic_error_returns_1_and_no_output(void) {
    char srcFile[] = "/tmp/test_ctds_exec_XXXXXX.ctds";
    createTempSourceFile(srcFile, "void main() { int x; x = true; }\n");

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "./mi_compilador -target sem %s 2>/dev/null", srcFile);
    int rc = execCommand(cmd, NULL, 0);
    TEST_ASSERT_EQUAL_INT(1, rc);

    char semFile[512];
    snprintf(semFile, sizeof(semFile), "%.*s.sem", (int)(strlen(srcFile) - 5), srcFile);
    TEST_ASSERT_FALSE(fileExists(semFile));

    unlink(srcFile);
}

// -target parse corta antes de la etapa semantica y retorna 0 aun con error semantico
void test_cli_exec_early_cutoff_parse_ignores_semantic_error(void) {
    char srcFile[] = "/tmp/test_ctds_exec_XXXXXX.ctds";
    createTempSourceFile(srcFile, "void main() { int x; x = true; }\n");

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "./mi_compilador -target parse %s 2>/dev/null", srcFile);
    int rc = execCommand(cmd, NULL, 0);
    TEST_ASSERT_EQUAL_INT(0, rc);

    char sintFile[512];
    snprintf(sintFile, sizeof(sintFile), "%.*s.sint", (int)(strlen(srcFile) - 5), srcFile);
    TEST_ASSERT_TRUE(fileExists(sintFile));

    unlink(srcFile);
    unlink(sintFile);
}

// -o igual al archivo de entrada se rechaza y el codigo fuente permanece intacto
void test_cli_exec_output_equals_input_rejected_and_source_intact(void) {
    char srcFile[] = "/tmp/test_ctds_exec_XXXXXX.ctds";
    const char *originalSource = "void main() { int preservado; preservado = 42; }\n";
    createTempSourceFile(srcFile, originalSource);

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "./mi_compilador -target parse -o %s %s 2>/dev/null", srcFile, srcFile);
    int rc = execCommand(cmd, NULL, 0);
    TEST_ASSERT_EQUAL_INT(1, rc);

    char *currentSource = readFileContent(srcFile);
    TEST_ASSERT_NOT_NULL(currentSource);
    TEST_ASSERT_EQUAL_STRING(originalSource, currentSource);

    free(currentSource);
    unlink(srcFile);
}

// Ruta con comilla simple en -o compila y genera el ejecutable correctamente
void test_cli_exec_single_quote_in_output_path(void) {
    char srcFile[] = "/tmp/test_ctds_exec_XXXXXX.ctds";
    createTempSourceFile(srcFile, "void main() {}\n");

    char customOut[] = "/tmp/sal'ida_exec_test";

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "./mi_compilador -o \"%s\" %s", customOut, srcFile);
    int rc = execCommand(cmd, NULL, 0);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_TRUE(fileExists(customOut));

    unlink(srcFile);
    unlink(customOut);
}

// -opt all compila exitosamente
void test_cli_exec_opt_all_succeeds(void) {
    char srcFile[] = "/tmp/test_ctds_exec_XXXXXX.ctds";
    createTempSourceFile(srcFile, "void main() { int x; x = 2 + 3 * 4; }\n");

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "./mi_compilador -opt all %s", srcFile);
    int rc = execCommand(cmd, NULL, 0);
    TEST_ASSERT_EQUAL_INT(0, rc);

    char defaultOut[512];
    snprintf(defaultOut, sizeof(defaultOut), "%.*s.out", (int)(strlen(srcFile) - 5), srcFile);
    TEST_ASSERT_TRUE(fileExists(defaultOut));

    unlink(srcFile);
    unlink(defaultOut);
}

// Compilar dos veces seguidas regenera el ejecutable sin error
void test_cli_exec_recompile_twice_overwrites_cleanly(void) {
    char srcFile[] = "/tmp/test_ctds_exec_XXXXXX.ctds";
    createTempSourceFile(srcFile, "void main() {}\n");

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "./mi_compilador %s", srcFile);
    int rc1 = execCommand(cmd, NULL, 0);
    TEST_ASSERT_EQUAL_INT(0, rc1);

    int rc2 = execCommand(cmd, NULL, 0);
    TEST_ASSERT_EQUAL_INT(0, rc2);

    char defaultOut[512];
    snprintf(defaultOut, sizeof(defaultOut), "%.*s.out", (int)(strlen(srcFile) - 5), srcFile);
    TEST_ASSERT_TRUE(fileExists(defaultOut));

    unlink(srcFile);
    unlink(defaultOut);
}

int main(void) {
    UNITY_BEGIN();

    // Tests Unitarios de parseArguments
    RUN_TEST(test_cli_no_arguments_fails);
    RUN_TEST(test_cli_missing_input_file_fails);
    RUN_TEST(test_cli_invalid_extension_fails);
    RUN_TEST(test_cli_bare_extension_fails);
    RUN_TEST(test_cli_missing_output_value_fails);
    RUN_TEST(test_cli_empty_output_value_fails);
    RUN_TEST(test_cli_missing_target_value_fails);
    RUN_TEST(test_cli_unknown_target_fails);
    RUN_TEST(test_cli_unknown_option_fails);
    RUN_TEST(test_cli_multiple_inputs_fails);
    RUN_TEST(test_cli_input_starts_with_dash_fails);
    RUN_TEST(test_cli_output_equals_input_fails);
    RUN_TEST(test_cli_default_target_and_output);
    RUN_TEST(test_cli_explicit_target_scan);
    RUN_TEST(test_cli_explicit_target_parse);
    RUN_TEST(test_cli_explicit_target_sem);
    RUN_TEST(test_cli_explicit_target_codinter);
    RUN_TEST(test_cli_explicit_target_assembly);
    RUN_TEST(test_cli_explicit_target_interp);
    RUN_TEST(test_cli_custom_output_flag);
    RUN_TEST(test_cli_debug_flag);
    RUN_TEST(test_cli_opt_all_and_default);
    RUN_TEST(test_cli_opt_specific_passes);
    RUN_TEST(test_cli_opt_unknown_fails);
    RUN_TEST(test_cli_opt_with_explicit_target_extension);
    RUN_TEST(test_cli_free_options_null_safe);
    RUN_TEST(test_cli_get_target_name);

    // Tests Integrales con mi_compilador
    RUN_TEST(test_cli_exec_no_arguments_returns_1);
    RUN_TEST(test_cli_exec_nonexistent_file_returns_1);
    RUN_TEST(test_cli_exec_default_produces_out_and_runs);
    RUN_TEST(test_cli_exec_custom_output_name);
    RUN_TEST(test_cli_exec_target_scan_produces_only_lex);
    RUN_TEST(test_cli_exec_target_parse_produces_only_sint);
    RUN_TEST(test_cli_exec_target_sem_produces_only_sem);
    RUN_TEST(test_cli_exec_target_codinter_produces_only_ci);
    RUN_TEST(test_cli_exec_target_assembly_produces_only_ass);
    RUN_TEST(test_cli_exec_lexical_error_scan_returns_1_and_no_output);
    RUN_TEST(test_cli_exec_syntax_error_returns_1_and_no_output);
    RUN_TEST(test_cli_exec_semantic_error_returns_1_and_no_output);
    RUN_TEST(test_cli_exec_early_cutoff_parse_ignores_semantic_error);
    RUN_TEST(test_cli_exec_output_equals_input_rejected_and_source_intact);
    RUN_TEST(test_cli_exec_single_quote_in_output_path);
    RUN_TEST(test_cli_exec_opt_all_succeeds);
    RUN_TEST(test_cli_exec_recompile_twice_overwrites_cleanly);

    return UNITY_END();
}
