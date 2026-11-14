# Entrada y Línea de Comandos

**Archivos:** `src/main.c`, `include/cli.h`, `src/core/cli.c`

Módulo encargado de interpretar y validar los argumentos de línea de comandos, abrir el archivo de entrada y transferir el control al orquestador del pipeline.

---

## 1. Propósito y alcance

`cli.c` traduce la invocación de consola (por ejemplo, `./mi_compilador -opt all -target codinter programa.ctds`) en una estructura `Options` con la configuración resuelta. Aplica validación temprana (*fail fast*): cualquier error de argumentos se rechaza antes de abrir archivos o iniciar fases de compilación.

El módulo se limita estrictamente a interpretar argumentos: no abre archivos de código fuente ni ejecuta etapas del compilador, lo que permite probar la interfaz de línea de comandos de forma aislada en `tests/test_cli.c`.

---

## 2. Punto de entrada (`main.c`)

La función `main` ejecuta cuatro pasos secuenciales:

```c
int main(int argc, char **argv) {
    Options options;
    if (parseArguments(argc, argv, &options) != CLI_SUCCESS) return 1;   /* 1 */

    yyin = fopen(options.input, "r");                                    /* 2 */
    if (yyin == NULL) {
        perror(options.input);
        freeOptions(&options);
        return 1;
    }

    int exitCode = compile(&options);                                    /* 3 */
    fclose(yyin);
    freeOptions(&options);                                               /* 4 */
    return exitCode;
}
```

1. **Parseo de opciones:** si los argumentos son inválidos, finaliza con código `1` sin tocar el sistema de archivos.
2. **Apertura de la entrada:** asigna el archivo abierto a `yyin`, variable global de la que Flex consume el flujo de caracteres. Si `fopen` falla, informa el motivo del sistema operativo mediante `perror`.
3. **Ejecución del pipeline:** delega la compilación en `compile(&options)`, implementada en el [driver](02-driver.md).
4. **Limpieza:** cierra `yyin`, libera la memoria dinámica de `Options` y retorna el código de salida.

`main.c` define además la variable global `NodeAST *astRoot = NULL;`, donde el parser deposita la raíz del árbol sintáctico. Dado que los binarios de pruebas unitarias no enlazan `main.c`, la gramática en `bison.y` declara su propia instancia bajo `#ifdef UNITY_TESTING` (ver [análisis sintáctico](../03-lexico-sintactico-y-ast/02-analisis-sintactico.md)).

---

## 3. Estructura `Options` e interfaz pública

```c
typedef struct Options {
    const char *input;              /* apunta a argv: no se libera */
    char *output;                   /* reservado en heap: liberado por freeOptions */
    Target target;
    int explicitTarget;             /* 1 si el usuario pasó -target */
    int debug;
    OptimizationSet optimizations;
} Options;
```

| Campo | Descripción |
| --- | --- |
| `input` | Ruta del archivo `.ctds`. Apunta directamente al elemento de `argv`, por lo que su tiempo de vida está atado al proceso y no debe liberarse. |
| `output` | Ruta del archivo de salida, reservada dinámicamente en el heap (ya sea copiada con `strdup` desde `-o` o construida al reemplazar la extensión). |
| `target` | Etapa objetivo hasta la cual ejecutar el pipeline (`Target`). |
| `explicitTarget` | Indica si el usuario especificó `-target` explícitamente (`1`) o si se utilizó el valor por defecto (`0`). Ver §5. |
| `debug` | Estado del flag `-debug`. |
| `optimizations` | Máscara de bits (`OptimizationSet`) con las optimizaciones habilitadas. |

### Interfaz pública (`include/cli.h`)

| Función | Descripción |
| --- | --- |
| `parseArguments(argc, argv, options)` | Recorre `argv`, valida restricciones y completa `options`. Retorna `CLI_SUCCESS` (`0`) o `CLI_ERROR` (`1`). |
| `freeOptions(options)` | Libera `options->output` y asigna `NULL` al puntero. Tolera `NULL`. |
| `getTargetName(target)` | Devuelve el nombre textual de un valor de `Target` (o `"?"` si está fuera de rango). |

---

## 4. Tablas de despacho y validación de entrada

El módulo utiliza tablas estáticas en lugar de cadenas de condicionales para asociar nombres de opciones con sus valores internos y generar el mensaje de ayuda (`printUsage`), evitando desincronizaciones entre la validación y la documentación en consola.

### `STAGES`: etapas y extensiones

| Nombre CLI | Valor `Target` | Extensión por defecto |
| --- | --- | --- |
| `scan` | `TARGET_SCAN` | `.lex` |
| `parse` | `TARGET_PARSE` | `.sint` |
| `sem` | `TARGET_SEM` | `.sem` |
| `codinter` | `TARGET_CODINTER` | `.ci` |
| `assembly` | `TARGET_ASSEMBLY` | `.ass` |

El orden de las constantes en el enumerado `Target` refleja el orden de ejecución del pipeline, permitiendo comparar etapas con operadores relacionales.

### `OPT_NAMES`: catálogo de optimizaciones

Reconoce ocho identificadores: `all` (que activa todos los bits de `OPTIMIZATION_ALL`) y las siete pasadas individuales: `const-fold`, `const-prop`, `dead-code`, `const-branch`, `self-assign`, `peephole` y `strength-reduce`.

### `INPUT_EXTENSION`

La extensión `.ctds` es obligatoria. La validación comprueba que el argumento posicional termine en `.ctds` y que su longitud total sea estrictamente mayor a la del sufijo (rechazando un archivo llamado únicamente `.ctds`). Solo se admite un archivo de entrada por invocación.

---

## 5. Objetivo por defecto y macro `BUILDS_EXECUTABLE`

La constante `CURRENT_TARGET` está definida como `TARGET_ASSEMBLY`. Sin embargo, omitir `-target` y pasar `-target assembly` tienen comportamientos finales distintos aunque ambos asignen `target = TARGET_ASSEMBLY`:

- Sin `-target`, el compilador recorre todas las etapas y enlaza el ejecutable `.out`.
- Con `-target assembly`, el compilador detiene el proceso tras emitir el archivo de texto `.ass`.

La distinción se resuelve mediante el campo `explicitTarget` y la macro:

```c
#define BUILDS_EXECUTABLE(options) (!(options)->explicitTarget)
```

```bash
./mi_compilador programa.ctds                   # -> programa.out  (ejecutable)
./mi_compilador -target assembly programa.ctds  # -> programa.ass  (ensamblador en texto)
```

---

## 6. Deducción y protección del archivo de salida

Cuando no se indica `-o`, la función interna `getTargetExtension` determina la extensión de salida evaluando tres condiciones en orden de prioridad:

```c
if (BUILDS_EXECUTABLE(options)) return ".out";                   /* 1 */
if (options->optimizations != OPTIMIZATION_NONE) return ".opt";  /* 2 */
return <extension de la etapa>;                                  /* 3 */
```

1. Si se compila hacia un ejecutable (sin `-target`), la extensión es `.out`.
2. Si se especificó `-target` junto con alguna optimización (`-opt`), la extensión pasa a ser `.opt` en lugar de la propia de la etapa, permitiendo conservar en disco tanto la salida sin optimizar (p. ej. `.ci`) como la optimizada (`.opt`).
3. En caso contrario, se emplea la extensión definida en `STAGES`.

`buildOutputName` reemplaza el sufijo `.ctds` por la extensión seleccionada.

### Protección contra sobrescritura del archivo fuente

Para impedir que un argumento `-o` destruya el archivo de entrada, `validateOutputNotInput` verifica primero si ambas rutas son idénticas como cadenas y, si difieren, consulta `stat` para comparar dispositivo e inodo (`st_dev` y `st_ino`):

```c
static bool validateOutputNotInput(const Options *options) {
    bool isSameFile = (strcmp(options->output, options->input) == 0);
    if (!isSameFile) {
        struct stat statInput, statOutput;
        if (stat(options->input, &statInput) == 0 && stat(options->output, &statOutput) == 0) {
            isSameFile = (statInput.st_dev == statOutput.st_dev &&
                          statInput.st_ino == statOutput.st_ino);
        }
    }
    ...
}
```

De este modo se rechazan también rutas relativas equivalentes (`./p.ctds`), rutas absolutas o enlaces duros/simbólicos que apunten al mismo inodo que el archivo fuente.

---

## 7. Parseo de `-opt` y acumulación de pasadas

La opción `-opt` admite un nombre de optimización opcional; si se omite, equivale a `-opt all`. Para determinar si el siguiente elemento de `argv` pertenece a `-opt` o es otro argumento del comando, se aplica la siguiente condición:

```c
bool nextIsOption    = (hasNextArgument && argv[*index + 1][0] == '-');
bool nextIsInputFile = (hasNextArgument && hasInputExtension(argv[*index + 1]));
bool hasOptimizationName = (hasNextArgument && !nextIsOption && !nextIsInputFile);

const char *optName = "all";
if (hasOptimizationName) optName = argv[++(*index)];
```

El token siguiente se consume como argumento de `-opt` siempre que no comience con `-` (otra bandera) ni termine en `.ctds` (el archivo de entrada):

```bash
./mi_compilador -opt programa.ctds             # aplica 'all' sobre programa.ctds
./mi_compilador -opt -debug programa.ctds      # aplica 'all' junto con -debug
./mi_compilador -opt const-fold programa.ctds  # aplica solo 'const-fold'
```

### Acumulación en máscara de bits

Cada pasada se representa como una potencia de dos en `OptimizationSet`. La función de parseo acumula los valores mediante OR bit a bit (`*optimizations |= accumulated`), soportando tanto listas separadas por comas (recorridas con `strchr`) como múltiples apariciones del flag `-opt`:

```bash
-opt all
-opt const-fold,dead-code
-opt const-fold -opt dead-code
```

Si algún identificador no coincide con `OPT_NAMES`, el parseo falla e imprime la lista de optimizaciones soportadas.

---

## 8. Conexión con otros módulos

| Origen / Destino | Interacción |
| --- | --- |
| **Recibe de** `main` (SO) | `argc`, `argv` |
| **Recibe de** `optimizer.h` | Enumerado `Optimization` y máscara `OPTIMIZATION_ALL` |
| **Entrega a** `driver.c` | Estructura `Options` validada vía `compile(&options)` |
| **Entrega a** `lexer.l` | Descriptor `FILE *` en `yyin` (abierto en `main.c`) |

`cli.c` no incluye encabezados del AST, de la tabla de símbolos ni del generador de código; su única dependencia interna es `optimizer.h`. Gracias a este aislamiento, el binario de pruebas de CLI se compila enlazando únicamente `cli.c`:

```bash
gcc -DUNITY_TESTING ... tests/test_cli.c src/core/cli.c -o build/run_cli_tests
```

---

## 9. Cómo se extiende

### Agregar una etapa a `-target`

1. Añadir la constante correspondiente en el enumerado `Target` (`include/cli.h`) respetando el orden del pipeline.
2. Agregar una fila en la tabla `STAGES` (`src/core/cli.c`) con el nombre de la etapa, su constante `Target` y su extensión de archivo. El parseo, la deducción del nombre de salida y `printUsage` se actualizan automáticamente a partir de la tabla.
3. Incorporar el `case` correspondiente en el `switch` de `compile` en el [driver](02-driver.md).

### Agregar una optimización

1. Definir el nuevo bit en `enum Optimization` y sumarlo a `OPTIMIZATION_ALL` en `include/optimizer.h`.
2. Agregar la fila con su nombre CLI en `OPT_NAMES` (`src/core/cli.c`). Si se omite en `OPT_NAMES`, la optimización solo se activará mediante `-opt all` pero no podrá seleccionarse individualmente por nombre.

### Agregar una opción de línea de comandos

1. Incorporar el campo en `struct Options` e inicializarlo en `initDefaultOptions`.
2. Añadir la rama `else if` en el bucle de `parseArguments` (usando el patrón de `parseOutputOption` si requiere un argumento adicional) y documentarla en `printUsage`.

### Soportar lectura desde `stdin`

Actualmente el archivo `.ctds` es obligatorio. Soportar `-` para leer desde la entrada estándar requeriría omitir `validateInputFile`, exigir `-o` explícito cuando no haya nombre base del cual deducir la salida, y evitar el `fopen`/`fclose` sobre `yyin` en `main.c`.

---

## 10. Validación

La suite `tests/test_cli.c` verifica el parseo de cada opción, la deducción de extensiones según combinaciones de `-target` y `-opt`, los códigos de retorno, la protección por inodo contra sobrescritura del archivo fuente y el manejo de rutas con caracteres especiales.

Verificaciones manuales representativas:

```bash
./mi_compilador                                  # falta archivo -> retorna 1 + usage
./mi_compilador programa.txt                     # extensión inválida -> retorna 1
./mi_compilador a.ctds b.ctds                    # múltiples entradas -> retorna 1
./mi_compilador -target noexiste programa.ctds   # etapa inválida -> retorna 1 + usage
./mi_compilador -opt inventada programa.ctds     # optimización inválida -> retorna 1
./mi_compilador -o programa.ctds programa.ctds   # misma ruta/inodo de entrada -> retorna 1
./mi_compilador -target assembly programa.ctds   # genera programa.ass
./mi_compilador programa.ctds                    # genera ejecutable programa.out
```

Para comprobar que una compilación exitosa sin `-debug` no emita salida por `stdout` ni `stderr`:

```bash
test -z "$(./mi_compilador examples/programa.ctds 2>&1)" && echo "OK"
```

---

**Siguiente:** [Orquestación de las Etapas](02-driver.md)
