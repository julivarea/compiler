# Orquestación de las Etapas

**Archivos:** `include/driver.h`, `src/core/driver.c` (349 líneas)

Decide qué fases correr según `-target`, corta ante el primer error de una fase,
escribe el archivo de salida y, si corresponde, llama a `gcc` para ensamblar.

---

## 1. Qué problema resuelve

Las fases del compilador no se conocen entre sí: el lexer no llama al parser, el
parser no llama al semántico. **Alguien tiene que llamarlas en orden**, decidir
dónde parar, y manejar lo que no es traducción: abrir archivos, volcar la salida,
invocar el ensamblador.

Es el único módulo que incluye a **todas** las fases, y por eso es también el
mapa de cómo se arma el pipeline. Si hay un documento que contesta *"¿qué pasa
exactamente cuando corrés `./mi_compilador programa.ctds`?"*, es este.

---

## 2. Interfaz pública

Una sola función:

```c
int compile(const Options *options);
```

Devuelve **0 si todo salió bien y 1 si falló cualquier fase**, y ese valor es el
que `main` devuelve al sistema operativo. No hay más API: todo lo demás son
once funciones `static`.

Esa estrechez es a propósito. El driver es el único que sabe el orden de las
fases, y nadie necesita preguntarle nada más.

---

## 3. El `switch` sobre `-target`

El cuerpo entero de `compile` es un `switch` con seis casos, y leerlo es ver el
pipeline:

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

Tres cosas que se leen de esa estructura:

**El prefijo se repite y es acumulativo.** Cada etapa corre todas las anteriores.
No hay forma de pedir el semántico sin parsear, y eso es lo que hace que
`-target` signifique "hasta acá" y no "solo esto".

**`TARGET_SCAN` es el único que no llama a `runParse`.** El escaneo se hace
directamente con `yylex()` en bucle, sin el parser. Es la única etapa que se
puede correr sin construir el AST.

**Cada `if (... != 0) return 1;` es un corte.** Si una fase falla, las siguientes
no corren y no se escribe ningún archivo de salida. Esa es la garantía que
permite que el generador de código no revalide nada: **si llega a ejecutarse, el
programa es válido**.

### Las variables globales que el driver conecta

```c
extern NodeAST *astRoot;     /* lo llena bison.y, lo leen todas las fases */
extern int yyparse(void);
extern int yylex(void);
extern char *yytext;
extern FILE *yyin;
extern int yylineno;
```

El driver no recibe el AST por parámetro: lo lee de la global que el parser
dejó. Es la consecuencia de que Bison y Flex se comuniquen por globales, y el
driver es donde eso se vuelve visible.

---

## 4. Las cinco funciones de etapa

### `runScan(options)` — la única que no usa el parser

```c
while ((token = yylex()) != 0) {
    const char *tokenName = getTokenName(token);
    if (tokenName == NULL) { /* es un caracter suelto: '+' , ';' ... */ }
    fprintf(outputFile, "[Linea %d] %-12s %s\n", yylineno, tokenName, yytext);
    debugPrintf(options->debug, "[SCAN] ...");
}
```

Llama a `yylex()` hasta que devuelva 0 e imprime una línea por token con su
número de línea y su lexema. **Es el único consumidor de `yylex` que no es el
parser**, y por eso el léxico tiene dos clientes (ver la
[análisis léxico](../03-lexico-sintactico-y-ast/01-analisis-lexico.md)).

`getTokenName` traduce el número de token a su nombre. Devuelve `NULL` para los
tokens que son **un carácter** —`'+'`, `';'`, `'('`— porque esos no tienen
constante con nombre: su número *es* su código ASCII. En ese caso el driver
formatea el carácter entre comillas.

### `runParse()` — parsea y chequea los errores léxicos

```c
if (yyparse() != 0) return 1;
if (lexical_errors > 0) { compileAborted("lexicos", lexical_errors); return 1; }
```

Dos chequeos, y el orden tiene sentido: `yyparse` consume todos los tokens, así
que para cuando termina, el contador de errores léxicos ya está completo.

**Un error léxico no aborta el parseo.** El lexer reporta y sigue (ver [análisis léxico](../03-lexico-sintactico-y-ast/01-analisis-lexico.md)), así
que el parser puede completar su trabajo; recién al final se mira el contador. Eso
permite reportar todos los símbolos inválidos de una pasada en lugar de uno por
corrida.

### `runSemantic()`

```c
analyzeSemantics(astRoot, NULL);
if (semantic_errors > 0) { compileAborted("semanticos", semantic_errors); return 1; }
```

El `NULL` del segundo parámetro es lo que le dice al análisis semántico "este es
el ámbito global" (ver la [análisis semántico](../04-semantico/03-analisis-semantico.md)). **Se descarta el
`Scope` que devuelve**: el resultado útil quedó anotado en el AST.

### `runIntermediateCode(options, tacList)`

```c
resetBuildString();
formatTACList(tacList);
if (hasStringBuilderError()) { ...; return 1; }
debugPrintf(options->debug, "%s", getBuiltString());
return writeBuiltStringTo(options->output);
```

El patrón que usan las dos etapas de salida textual: resetear el acumulador,
llenarlo, chequear que no haya fallado por memoria, volcarlo con `-debug` y
escribirlo al archivo.

### `runAssembly(options, tacList)` — la que bifurca

```c
resetBuildString();
generateAssembly(tacList, options->optimizations);

if (getObjectCodeErrors() > 0) { compileAborted("internos ...", ...); return 1; }
if (hasStringBuilderError())   { ...; return 1; }

if (!BUILDS_EXECUTABLE(options)) {
    return writeBuiltStringTo(options->output);     /* -target assembly: queda el .ass */
}

/* sin -target: hay que ensamblar */
snprintf(assemblyPath, sizeof(assemblyPath), "%s.tmp.s", options->output);
writeBuiltStringTo(assemblyPath);
int result = assembleAndLink(options, assemblyPath);
if (!options->debug) remove(assemblyPath);
return result;
```

Acá se materializa la decisión de `BUILDS_EXECUTABLE` de la
[CLI y entrada](01-cli-y-entrada.md): con `-target assembly` el `.ass` **es** la salida; sin
`-target`, el assembly es un **paso intermedio** que se escribe a
`<salida>.tmp.s`, se ensambla y se borra.

**Con `-debug` el intermedio se conserva**, y se informa dónde quedó. Es la forma
de ver el assembly exacto que produjo un ejecutable.

El sufijo `.tmp.s` no es decorativo: el nombre se deriva de la **salida** y no de
la entrada, para que dos compilaciones simultáneas con distintos `-o` no se pisen
el archivo temporal. Y hay un chequeo de truncamiento del `snprintf` contra
`MAX_PATH_LENGTH` (4096), porque una ruta larga truncada escribiría a un archivo
equivocado.

---

## 5. `buildIntermediateCode`: donde se enchufa el optimizador

```c
const TACList *tacList = generateIntermediateCode(astRoot);
if (tacList == NULL) { ...; return NULL; }

if (options->debug) {                                    /* el CFG y la vivacidad */
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

Tres cosas que vale señalar:

**El optimizador se enchufa acá, entre la generación del TAC y su consumo.** Por
eso lo aprovechan los tres consumidores: `-target codinter` escribe el `.opt`,
`-target assembly` traduce el TAC optimizado, y el ejecutable también. Una sola
línea cubre los tres casos.

**Sin `-opt` el optimizador no se llama en absoluto**, así que la lista queda
exactamente como la emitió el generador. Es la garantía de que `-opt` sea
opcional en el sentido fuerte: no es que no cambie nada, es que no corre.

**El cast `(TACList *)` descarta el `const`** con el que
`generateIntermediateCode` devolvió la lista, porque el optimizador la modifica
en el lugar. Es deliberado y está en la línea 69; la
[generación de TAC](../05-codigo-intermedio/02-generacion-tac.md) lo explica desde el otro lado.

**El análisis de variables vivas se imprime solo con `-debug`, y se imprime
siempre que haya `-debug`, incluso sin `-opt`.** O sea que se puede ver el CFG de
un programa sin optimizar nada. Y se construye y se libera solo para imprimir: el
que usa el optimizador es otro, construido adentro de `removeDeadStores`.

---

## 6. `assembleAndLink`: `fork` + `execvp`, no `system`

Es la parte más interesante del módulo y la que más consecuencias tiene.

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

### Por qué no `system()`

`system("gcc -x assembler ... -o ...")` habría sido una línea. El problema es que
`system` pasa el comando **a un shell**, y el shell interpreta espacios, comillas,
`;`, `$`, `&&`… Un nombre de archivo con cualquiera de esos caracteres rompería
el comando o, peor, ejecutaría algo distinto.

Con `fork` + `execvp` los argumentos se pasan **como un arreglo** y no hay shell
en el medio: cada elemento llega al programa tal cual, sin interpretación.

Verificado con cuatro nombres que un `system()` habría roto:

| Nombre de salida | Resultado |
| --- | --- |
| `con espacios.out` | compila y ejecuta |
| `con'apostrofe.out` | compila y ejecuta |
| `con;punto_y_coma.out` | compila y ejecuta |
| `con$dolar.out` | compila y ejecuta |

Los cuatro producen un ejecutable correcto. Es la clase de robustez que no se nota
hasta que alguien compila en una carpeta con un espacio en el nombre.

### Los dos flags de `gcc`

| Flag | Por qué |
| --- | --- |
| `-x assembler` | Le dice a `gcc` **explícitamente** que el archivo es assembly. Sin esto, `gcc` decide por la extensión, y un `.s` mayúscula/minúscula o un nombre inesperado lo haría tratar el archivo como otra cosa |
| `-no-pie` | Desactiva el ejecutable independiente de posición. El código generado usa direccionamiento `símbolo(%rip)` pero no es PIC completo, así que con PIE activado el enlazado falla |

### El manejo del proceso hijo

- `execvp` **solo retorna si falló**, así que el `perror("gcc")` y el `_exit(127)`
  que vienen después son el camino de error. El 127 es la convención de "comando
  no encontrado".
- `_exit` y no `exit`: en el hijo no hay que correr los `atexit` ni vaciar los
  buffers heredados del padre, porque eso duplicaría la salida.
- `WIFEXITED` antes de `WEXITSTATUS`: un proceso que murió por señal no tiene
  código de salida válido, y preguntarlo sin chequear daría basura.

---

## 7. Las funciones de escritura

### `openOutputFile(path)`

Un `fopen` en modo `"w"` con el `perror` correspondiente si falla. Centralizado
para que las tres etapas que escriben archivos reporten el error igual.

### `writeBuiltStringTo(path)`

Vuelca el acumulador de `string_builder` a un archivo. Lo usan el código
intermedio y el assembly.

### `writeASTTo(options)`

Escribe el AST con `fprintAST`, y lo usan `-target parse` y `-target sem`. **Son
la misma función con el árbol en dos estados distintos**: sin anotar y anotado.
Comparar el `.sint` con el `.sem` del mismo programa es exactamente ver qué
agregó el análisis semántico.

### El borrado ante error

```c
remove(options->output);
```

Si la escritura falla a mitad de camino, el archivo parcial **se borra**. Eso
sostiene la garantía de que un error nunca deja un archivo de salida que podría
parecer válido.

---

## 8. Conexión con las fases vecinas

| Viene de | Qué cruza la frontera |
| --- | --- |
| `cli.c` | La estructura `Options` y la macro `BUILDS_EXECUTABLE` |
| `main.c` | El `FILE *` ya abierto en `yyin` |

| Va hacia | Qué llama |
| --- | --- |
| `lexer.l` | `yylex()` en bucle, solo en `-target scan` |
| `bison.y` | `yyparse()` |
| `semantic_analyzer.c` | `analyzeSemantics(astRoot, NULL)` |
| `intermediate_code.c` | `generateIntermediateCode(astRoot)` |
| `optimizer.c` | `optimizeIntermediateCode` |
| `live_variable_analysis.c` | El volcado del CFG con `-debug` |
| `object_code.c` | `generateAssembly` |
| `string_builder.c` | El acumulador de la salida textual |
| `error_handler.c` | Los contadores y `compileAborted` |
| El sistema operativo | `fork`, `execvp`, `waitpid`, `fopen`, `remove` |

**Es el único archivo que incluye a todas las fases.** Eso lo vuelve el punto de
acoplamiento del proyecto, y es el precio correcto a pagar: la alternativa sería
que cada fase llamara a la siguiente, y entonces ninguna se podría ejecutar
aislada ni testear sola.

---

## 9. Cómo se extiende

### Una etapa nueva de `-target`

Un `case` en el `switch` de `compile`, con el prefijo acumulativo de las fases
anteriores y la llamada a su propia función de etapa. Más la fila en `STAGES` de
la [CLI y entrada](01-cli-y-entrada.md).

Si se olvida el `case`, cae en el `default` que **devuelve 1 sin mensaje**: el
compilador falla silenciosamente para ese target. Es mejor que hacer algo
incorrecto, pero sin explicación.

### Otro ensamblador o enlazador

`assembleAndLink` tiene el arreglo de argumentos en una sola declaración. Cambiar
`gcc` por `as` + `ld` es reescribir ese arreglo y, probablemente, hacer dos
`fork` en vez de uno. El patrón de `fork`/`execvp`/`waitpid` se reusa igual.

### Compilar varios archivos

Hoy `main` abre uno y `compile` corre una vez. Soportar varios exigiría decidir
qué significa: ¿un ejecutable por archivo, o enlazar todos juntos? La segunda
opción pide que el TAC y la tabla de símbolos sobrevivan entre archivos, que es
un cambio de fondo en el front-end, no acá.

### Mover el optimizador de lugar

Está enchufado en `buildIntermediateCode` (§5), que es el único punto por donde
pasan los tres consumidores del TAC. Moverlo a otro lado rompería que `-opt`
afecte a las tres salidas con una sola línea.

---

## 10. Trampas y límites conocidos

- **El `default` del `switch` devuelve 1 sin mensaje** (§9).
- **El cast que descarta el `const` de la lista TAC** está en la línea 69 (§5).
  No es un descuido.
- **`-target scan` no construye el AST**, así que un programa con error de
  sintaxis pasa `-target scan` sin problema: el escaneo solo mira tokens.
- **Un error léxico no corta el parseo** (§4): se reporta al final del
  `runParse`. Eso significa que ante un símbolo inválido, Bison igual intenta
  parsear y puede emitir además un error de sintaxis derivado.
- **El `Scope` del análisis semántico se descarta** y **el AST nunca se libera**.
  Son fugas al salir, deliberadas: el proceso termina y el sistema operativo
  recupera todo. Está anotado en la documentación del [AST](../03-lexico-sintactico-y-ast/04-ast.md) y de la [tabla de símbolos](../04-semantico/01-tabla-de-simbolos.md).
- **`MAX_PATH_LENGTH` es 4096** y el chequeo de truncamiento existe, pero el
  nombre del temporal se deriva del de salida, así que un `-o` de 4090 caracteres
  haría fallar la compilación con un mensaje sobre la ruta intermedia y no sobre
  el `-o`.
- **El assembly intermedio se borra salvo con `-debug`.** Si una compilación
  falla en el ensamblado, el `.tmp.s` **se borra igual**, así que para inspeccionar
  qué assembly rompió hay que volver a correr con `-debug`.
- **`-no-pie` es obligatorio** con el código que genera el back-end (§6).
  Quitarlo hace fallar el enlazado, no el ensamblado, y el mensaje de `ld` no es
  obvio.

---

## 11. La teoría detrás

- **Driver de compilador**: el componente que secuencia las fases y maneja la
  entrada/salida. En los compiladores reales es también quien decide qué
  herramientas externas invocar —preprocesador, ensamblador, enlazador— y `gcc`
  mismo es, en rigor, un driver que llama a `cpp`, `cc1`, `as` y `ld`. Lo de acá
  es la versión mínima del mismo patrón.
- **Compilación por etapas y puntos de corte**: que se pueda detener el pipeline
  en cada fase y volcar su representación intermedia no es solo un requisito del
  enunciado, es la herramienta de depuración más importante de un compilador.
  `gcc` tiene los equivalentes: `-E`, `-fdump-tree-*`, `-S`.
- **Separación de fases y acoplamiento en un punto**: las fases no se conocen
  entre sí y el driver las conoce a todas. Concentrar el acoplamiento en un solo
  módulo es lo que permite que cada fase se compile y se teste por separado, que
  es lo que hacen las suites unitarias del proyecto, cada una sin enlazar las
  etapas anteriores a la que prueba.
- **Creación de procesos en POSIX**: `fork` duplica el proceso, `execvp` reemplaza
  la imagen del hijo por otro programa, y `waitpid` espera su terminación. Es el
  mecanismo de base sobre el que `system()` está construido, y usarlo directo es
  lo que evita el shell (§6).
- **Inyección por el shell**: pasar nombres de archivo a través de un shell los
  expone a la interpretación de metacaracteres. Evitarlo pasando un arreglo de
  argumentos es la misma razón por la que en cualquier lenguaje se recomienda la
  forma con lista sobre la forma con cadena al lanzar subprocesos.
- **Códigos de salida como protocolo**: `0` éxito, `1` error de compilación,
  `127` comando no encontrado. Es lo que permite encadenar el compilador en un
  `&&` de shell o en un script de tests.

---

## 12. Cómo se valida

- **Suite:** `tests/test_cli.c` cubre el driver de punta a punta: cada `-target`
  produce **solo** su archivo, los errores devuelven 1 y no dejan salida, y el
  ejecutable se genera y corre. Es testing de integración aunque esté en la suite
  de la CLI.
- **A mano**, el recorrido completo por las etapas sobre un mismo programa:

```bash
for etapa in scan parse sem codinter assembly; do
    ./mi_compilador -target $etapa examples/programa.ctds || echo "FALLO en $etapa"
done
```

  Debe dejar `programa.lex`, `.sint`, `.sem`, `.ci` y `.ass`, **sin imprimir
  nada**.

- **Ver el assembly que produjo un ejecutable**, que es para lo que sirve que
  `-debug` conserve el intermedio:

```bash
./mi_compilador -debug examples/factorial.ctds 2>&1 | grep "Assembly intermedio"
```

- **Y los casos del subproceso**, que son los que rompen en silencio:

```bash
./mi_compilador -o "con espacios.out" examples/factorial.ctds && ./"con espacios.out"
```

---

**Siguiente:** [Errores y Salida de Debug](03-errores-y-debug.md)
