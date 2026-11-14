# Orquestación de las Etapas

**Archivos:** `include/driver.h`, `src/core/driver.c`

Coordina la ejecución secuencial de las fases del compilador según el valor de `-target`, detiene el proceso ante errores intermedios, vuelca las representaciones textuales a disco e invoca a `gcc` para ensamblar y enlazar el ejecutable final.

---

## 1. Propósito e interfaz pública

Las fases del compilador están desacopladas entre sí: el analizador léxico no invoca al sintáctico, ni el sintáctico al semántico. `driver.c` concentra el acoplamiento entre etapas, las ejecuta en orden y gestiona las operaciones de entrada/salida y subprocesos. Esto permite que cada módulo del pipeline se compile y pruebe de forma aislada en su propia suite unitaria.

Su interfaz pública expone una única función:

```c
int compile(const Options *options);
```

Retorna `0` si todas las etapas solicitadas finalizaron sin errores y `1` ante cualquier fallo (léxico, sintáctico, semántico, de generación o de sistema de archivos).

---

## 2. Despacho del pipeline según `-target`

El cuerpo de `compile` selecciona el flujo de ejecución mediante un `switch` sobre `options->target`:

```c
case TARGET_SCAN:
    return runScan(options);

case TARGET_PARSE:
    if (runParse() != 0) return 1;
    return writeASTTo(options);

case TARGET_SEM:
    if (runParse() != 0)    return 1;
    if (runSemantic() != 0) return 1;
    return writeASTTo(options);

case TARGET_CODINTER:
    if (runParse() != 0)    return 1;
    if (runSemantic() != 0) return 1;
    tacList = buildIntermediateCode(options);
    return runIntermediateCode(options, tacList);

case TARGET_ASSEMBLY:
    if (runParse() != 0)    return 1;
    if (runSemantic() != 0) return 1;
    tacList = buildIntermediateCode(options);
    return runAssembly(options, tacList);
```

- **Ejecución acumulativa:** cada objetivo ejecuta todas las fases previas necesarias para construir sus estructuras de entrada.
- **Independencia de `TARGET_SCAN`:** el escaneo consume tokens llamando directamente a `yylex()` en un bucle, sin invocar a `yyparse()` ni construir el AST (por lo que un programa con errores sintácticos pero tokens válidos supera `-target scan`).
- **Corte temprano:** si una fase retorna distinto de `0`, las etapas posteriores no se ejecutan y no se genera archivo de salida. De este modo, el generador de código intermedio y el backend pueden asumir que el AST recibido ya superó todas las validaciones sintácticas y semánticas.

### Estado global compartido con Flex y Bison

El driver se comunica con los analizadores generados por Flex y Bison a través de sus variables globales:

```c
extern NodeAST *astRoot;     /* asignado en bison.y, consumido por las fases siguientes */
extern int yyparse(void);
extern int yylex(void);
extern char *yytext;
extern FILE *yyin;
extern int yylineno;
```

---

## 3. Ejecución por etapa

### `runScan(options)`

```c
while ((token = yylex()) != 0) {
    const char *tokenName = getTokenName(token);
    if (tokenName == NULL) { /* token de un caracter: '+', ';', '(' ... */ }
    fprintf(outputFile, "[Linea %d] %-12s %s\n", yylineno, tokenName, yytext);
    debugPrintf(options->debug, "[SCAN] ...");
}
```

Invoca a `yylex()` hasta alcanzar `EOF` (`0`) y escribe una línea por token en el archivo `.lex` (ver [análisis léxico](../03-lexico-sintactico-y-ast/01-analisis-lexico.md)). `getTokenName` traduce el código numérico de Bison a su identificador simbólico, o retorna `NULL` cuando el token es un literal de un solo carácter cuyo código coincide con su valor ASCII.

### `runParse()`

```c
if (yyparse() != 0) return 1;
if (lexical_errors > 0) { compileAborted("lexicos", lexical_errors); return 1; }
```

El analizador léxico reporta caracteres inválidos e incrementa `lexical_errors` sin detener el escaneo, permitiendo recolectar todos los errores léxicos en una misma pasada. Por ello, una vez que `yyparse()` termina de consumir la entrada, `runParse` verifica el contador léxico antes de dar por válida la fase (teniendo en cuenta que un carácter inválido ignorado por el lexer puede provocar también un error sintáctico derivado en Bison).

### `runSemantic()` y `writeASTTo(options)`

```c
analyzeSemantics(astRoot, NULL);
if (semantic_errors > 0) { compileAborted("semanticos", semantic_errors); return 1; }
```

Invoca al analizador semántico pasando `NULL` como ámbito padre para indicar el alcance global (ver [análisis semántico](../04-semantico/03-analisis-semantico.md)). Las anotaciones de tipos y punteros a `Symbol` quedan registradas sobre los propios nodos de `astRoot`. Ni el árbol `Scope` devuelto ni el `astRoot` se liberan explícitamente al finalizar el proceso, delegando la recuperación de memoria al sistema operativo al terminar la ejecución (ver [AST](../03-lexico-sintactico-y-ast/04-ast.md) y [tabla de símbolos](../04-semantico/01-tabla-de-simbolos.md)).

Tanto `-target parse` como `-target sem` vuelcan el árbol a disco mediante `writeASTTo`, que invoca a `fprintAST`: en `parse` el árbol se imprime sin anotar (`.sint`) y en `sem` con los tipos y `uid` resueltos (`.sem`).

### `runIntermediateCode(options, tacList)`

```c
resetBuildString();
formatTACList(tacList);
if (hasStringBuilderError()) { ...; return 1; }
debugPrintf(options->debug, "%s", getBuiltString());
return writeBuiltStringTo(options->output);
```

Reinicia el acumulador de texto (`string_builder`), formatea la lista de cuádruplas TAC, verifica que no hayan ocurrido errores de asignación de memoria y escribe el resultado en el archivo `.ci` (o `.opt`). Si la escritura a disco falla parcialmente, `writeBuiltStringTo` elimina el archivo incompleto con `remove(options->output)`.

### `runAssembly(options, tacList)`

```c
resetBuildString();
generateAssembly(tacList, options->optimizations);

if (getObjectCodeErrors() > 0) { compileAborted("internos ...", ...); return 1; }
if (hasStringBuilderError())   { ...; return 1; }

if (!BUILDS_EXECUTABLE(options)) {
    return writeBuiltStringTo(options->output);     /* -target assembly: escribe .ass */
}

snprintf(assemblyPath, sizeof(assemblyPath), "%s.tmp.s", options->output);
writeBuiltStringTo(assemblyPath);
int result = assembleAndLink(options, assemblyPath);
if (!options->debug) remove(assemblyPath);
return result;
```

Evalúa la macro `BUILDS_EXECUTABLE` definida en [CLI y entrada](01-cli-y-entrada.md):

- Con `-target assembly`, vuelca el código ensamblador directamente en la ruta de salida (`.ass`).
- Sin `-target`, escribe un archivo temporal `<output>.tmp.s` (verificando previamente que la longitud de la ruta no supere `MAX_PATH_LENGTH` de 4096 bytes), invoca a `assembleAndLink` para generar el binario y elimina el archivo `.tmp.s` al terminar, salvo que se haya especificado `-debug`.

---

## 4. Integración del optimizador (`buildIntermediateCode`)

```c
const TACList *tacList = generateIntermediateCode(astRoot);
if (tacList == NULL) { ...; return NULL; }

if (options->debug) {
    LiveVariableAnalysis *analysis = createLiveVariableAnalysis(tacList);
    if (analysis != NULL) {
        printLiveVariableAnalysis(analysis, stdout);
        freeLiveVariableAnalysis(analysis);
    }
}

if (hasOptimizations) {
    setOptimizerDebug(options->debug);
    int changes = optimizeIntermediateCode((TACList *)tacList, enabled);
    debugPrintf(options->debug, "[OPT] Optimizaciones: %d instrucciones transformadas\n\n", changes);
}
return tacList;
```

- **Punto único de intercepción:** al aplicar las optimizaciones inmediatamente después de `generateIntermediateCode`, los tres flujos que consumen TAC (`-target codinter`, `-target assembly` y la generación del ejecutable) reciben la lista ya optimizada.
- **Ejecución condicional estricta:** si no se activó `-opt`, `optimizeIntermediateCode` no se invoca y la lista TAC permanece intacta.
- **Modificación *in-place*:** `generateIntermediateCode` retorna `const TACList *` para los consumidores de solo lectura, y `buildIntermediateCode` realiza un cast explícito a `(TACList *)` para que el optimizador reescriba la lista en el lugar (ver [generación de TAC](../05-codigo-intermedio/02-generacion-tac.md)).
- **Volcado de vivacidad en `-debug`:** cuando `-debug` está activo, el análisis de variables vivas y el CFG se construyen e imprimen sobre el TAC inicial incluso si no se solicitaron optimizaciones.

---

## 5. Ensamblado y enlazado (`assembleAndLink`)

Para producir el ejecutable final, el driver invoca a `gcc` mediante `fork`, `execvp` y `waitpid`:

```c
char *const assemblerArguments[] = {
    "gcc", "-no-pie", "-x", "assembler",
    (char *)assemblyPath, "-o", (char *)options->output, NULL
};

pid_t processId = fork();
if (processId < 0) { perror("fork"); return 1; }

if (processId == 0) {
    execvp(assemblerArguments[0], assemblerArguments);
    perror("gcc");
    _exit(127);
}

int status;
if (waitpid(processId, &status, 0) < 0) { perror("waitpid"); return 1; }
if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) { ...; return 1; }
```

### Uso de `fork` + `execvp` en lugar de `system()`

A diferencia de `system()`, que ejecuta el comando a través de un shell `/bin/sh -c` e interpreta metacaracteres, `execvp` recibe el arreglo de argumentos directamente y lo pasa al proceso hijo sin intervención del shell. Esto evita fallos o inyecciones de comandos cuando las rutas de entrada o salida contienen espacios, comillas simples, `;` o `$`.

### Opciones pasadas a `gcc` y control del proceso hijo

| Aspecto | Detalle técnico |
| --- | --- |
| `-x assembler` | Fuerza a `gcc` a tratar el archivo de entrada como código ensamblador independientemente de su extensión. |
| `-no-pie` | Deshabilita la generación de ejecutables independientes de posición (PIE), requerido por el esquema de direccionamiento emitido por el backend. |
| `_exit(127)` | Si `execvp` falla, el proceso hijo termina inmediatamente con `_exit` (código estándar `127` para comando no encontrado) en lugar de `exit`, evitando ejecutar manejadores `atexit` o vaciar por duplicado los buffers de E/S heredados del padre. |
| `WIFEXITED` y `WEXITSTATUS` | Verifica que el subproceso haya finalizado normalmente antes de inspeccionar su código de retorno. |

---

## 6. Conexión con otros módulos

| Origen / Destino | Interacción |
| --- | --- |
| **Recibe de** `cli.c` | Estructura `Options` y macro `BUILDS_EXECUTABLE` |
| **Recibe de** `main.c` | Descriptor `yyin` abierto sobre el archivo fuente |
| **Invoca en** `lexer.l` / `bison.y` | `yylex()` (en `TARGET_SCAN`) y `yyparse()` |
| **Invoca en** `semantic_analyzer.c` | `analyzeSemantics(astRoot, NULL)` |
| **Invoca en** `intermediate_code.c` | `generateIntermediateCode(astRoot)` |
| **Invoca en** `optimizer.c` / `live_variable_analysis.c` | `optimizeIntermediateCode` y volcado del CFG con `-debug` |
| **Invoca en** `object_code.c` / `string_builder.c` | `generateAssembly` y gestión del buffer de salida textual |
| **Invoca en** `error_handler.c` | Contadores de errores y resumen `compileAborted` |

---

## 7. Cómo se extiende

### Agregar una etapa a `-target`

Incorporar el `case` correspondiente en el `switch` de `compile` ejecutando primero las fases previas de las que dependa y luego su función de etapa, además de registrarla en `STAGES` dentro de [CLI y entrada](01-cli-y-entrada.md). Si un valor de `Target` no tiene `case` en `compile`, cae en la rama `default`, que retorna `1` sin emitir mensaje.

### Cambiar el ensamblador o enlazador

La invocación externa está encapsulada en el arreglo `assemblerArguments` dentro de `assembleAndLink`. Reemplazar `gcc` por invocaciones separadas a `as` y `ld` solo requiere actualizar dicho arreglo (o encadenar dos llamadas con el mismo esquema `fork`/`execvp`/`waitpid`).

### Soportar múltiples archivos de entrada

Actualmente `main` abre un único archivo y ejecuta `compile` una sola vez. Compilar y enlazar múltiples unidades requeriría mantener la tabla de símbolos global y acumular las listas TAC entre archivos antes de invocar al backend.

---

## 8. Validación

La suite `tests/test_cli.c` ejercita el driver verificando que cada valor de `-target` genere únicamente el archivo esperado, que los errores en cualquier fase retornen `1` sin dejar archivos residuales y que la compilación completa produzca un binario ejecutable funcional.

Comprobación manual de todas las etapas sobre un programa de ejemplo:

```bash
for etapa in scan parse sem codinter assembly; do
    ./mi_compilador -target $etapa examples/valido_completo.ctds || echo "FALLO en $etapa"
done
```

Para conservar e inspeccionar el archivo ensamblador intermedio `.tmp.s` generado al compilar un ejecutable:

```bash
./mi_compilador -debug examples/valido_factorial_recursivo.ctds
```

---

**Siguiente:** [Errores y Salida de Debug](03-errores-y-debug.md)
