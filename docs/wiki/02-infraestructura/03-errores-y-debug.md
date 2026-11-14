# Errores y Salida de Debug

**Archivos:** `include/error_handler.h`, `src/core/error_handler.c`, `include/debug.h`

Gestionan los dos canales de salida por consola del compilador: el reporte de errores de compilación por `stderr` y la traza diagnóstica de desarrollo por `stdout` (habilitada exclusivamente mediante `-debug`).

---

## 1. Propósito y separación de canales

El compilador separa estrictamente los diagnósticos dirigidos al usuario de la información interna de depuración:

- **Errores (`error_handler`):** se emiten siempre por `stderr`, actualizan los contadores de errores de la fase correspondiente y determinan si el pipeline continúa o se interrumpe.
- **Depuración (`debug.h`):** se emite por `stdout` únicamente cuando el usuario especifica `-debug`. Si no se pasa `-debug`, una compilación exitosa no escribe ningún carácter en `stdout` ni en `stderr`.

---

## 2. Manejador de errores (`error_handler`)

### Interfaz pública

```c
void lexicalError (int line, const char *format, ...);
void syntaxError  (int line, const char *format, ...);
void semanticError(int line, const char *format, ...);
void compileAborted(const char *stage, int errorCount);
void resetErrorHandler(void);

extern int lexical_errors;
extern int semantic_errors;
```

Las funciones de reporte son variádicas con formato estilo `printf`:

```c
lexicalError(yylineno, "simbolo no permitido '%s'", yytext);
semanticError(node->line, "La variable '%s' ya fue declarada previamente en este ambito.", id);
```

Para errores globales no asociados a una línea específica (como la ausencia del método `main`), se utiliza la línea `0` como valor centinela (`semanticError(0, ...)`).

### Políticas de recuperación y contadores por fase

| Función | Contador incrementado | Llamador | Política de la fase |
| --- | --- | --- | --- |
| `lexicalError` | `lexical_errors` | `lexer.l` | Acumula errores y continúa escaneando hasta `EOF`. |
| `syntaxError` | Ninguno | `bison.y` (`yyerror`) | Aborta en el primer error sintáctico (`yyparse` retorna no cero). |
| `semanticError` | `semantic_errors` | `semantic_analyzer.c`, `type_checker.c` | Acumula todos los errores semánticos antes de abortar. |

La ausencia de un contador `syntax_errors` responde a que la gramática no emplea producciones de recuperación con el token `error` de Bison: ante el primer error sintáctico el análisis se detiene. En cambio, las fases léxica y semántica continúan analizando el resto del archivo para reportar todos los errores independientes en una sola ejecución (apoyadas en `TYPE_ERROR` para evitar cascadas de errores de tipos; ver [chequeo de tipos](../04-semantico/02-chequeo-de-tipos.md)).

Internamente, las tres funciones delegan en `reportError`, que recibe un puntero opcional al contador (`NULL` en el caso de `syntaxError`) y formatea directamente sobre `stderr` con `vfprintf`, sin utilizar buffers intermedios de longitud fija:

```c
static void reportError(const char *stage, int *counter, int line,
                        const char *format, va_list formatArguments) {
    fprintf(stderr, "\033[1;31mError %s en la linea %d:\033[0m ", stage, line);
    vfprintf(stderr, format, formatArguments);
    fprintf(stderr, "\n");
    if (counter) (*counter)++;
}
```

### Resumen de cierre (`compileAborted`) y formato ANSI

Cuando una etapa con acumulación finaliza con contador mayor a cero, `driver.c` invoca a `compileAborted` para emitir el resumen final por `stderr`:

```c
void compileAborted(const char *stage, int errorCount) {
    fprintf(stderr, "\n\033[1;31mCompilacion abortada: se encontraron %d errores %s.\033[0m\n",
            errorCount, stage);
}
```

Los códigos de escape ANSI `\033[1;31m` (rojo en negrita) y `\033[0m` se emiten siempre en `stderr` (incluso si el descriptor está redirigido a un archivo). Es el único módulo del proyecto que utiliza secuencias de color.

### Reinicio de estado en pruebas (`resetErrorHandler`)

Dado que `lexical_errors` y `semantic_errors` son variables globales estáticas al proceso, las suites unitarias que ejecutan múltiples casos de prueba en un mismo binario (`test_semantic`, `test_ast`, `test_integration`) llaman a `resetErrorHandler()` en su `setUp` para poner ambos contadores en cero antes de cada test.

---

## 3. Salida de depuración (`debug.h`)

El encabezado `include/debug.h` define la función `debugPrintf` y establece la convención de trazas del compilador:

```c
static void debugPrintf(bool debugEnabled, const char *format, ...) {
    if (!debugEnabled) return;
    va_list arguments;
    va_start(arguments, format);
    vprintf(format, arguments);
    va_end(arguments);
}
```

- **Control explícito por parámetro:** en lugar de mantener una bandera global, `debugPrintf` recibe `debugEnabled` (`options->debug`) en cada llamada.
- **Prefijos por etapa:** las líneas emitidas identifican su origen mediante prefijos en texto plano sin códigos ANSI: `[SCAN]` y `[ASM]` en `driver.c`, y `[OPT]` y `[ELIMINADA]` en `driver.c` y `optimizer.c`.
- **Volcados multilínea:** el optimizador (`printTACWithDeletedInRed`) y el [análisis de variables vivas](../06-optimizador/01-variables-vivas.md) (`printLiveVariableAnalysis`, que formatea bloques con separadores `=====`) imprimen con `printf`/`fprintf` directo, pero solo son invocados cuando `options->debug` es verdadero.
- **Vinculación `static`:** `debugPrintf` está declarada como `static` (no `static inline`) en `debug.h`. Actualmente solo `driver.c` incluye este encabezado; si otro archivo lo incluyera sin invocar a `debugPrintf`, GCC emitiría una advertencia `-Wunused-function` a menos que se cambie a `static inline`.

---

## 4. Comportamiento de canales y códigos de salida

| Escenario | `stdout` | `stderr` | Código de retorno | Archivo de salida |
| --- | --- | --- | --- | --- |
| Compilación exitosa sin `-debug` | Vacío | Vacío | `0` | Generado |
| Compilación exitosa con `-debug` | Trazas de las etapas ejecutadas | Vacío | `0` | Generado (más `.tmp.s` si aplica) |
| Error en cualquier fase | Vacío (o trazas previas si hay `-debug`) | Mensajes de error y resumen | `1` | No generado / eliminado |

La separación entre `stdout` y `stderr` permite redirigir la traza de `-debug` a un archivo mientras los errores continúan mostrándose en la terminal:

```bash
./mi_compilador -debug -target codinter programa.ctds > traza.txt
```

---

## 5. Conexión con otros módulos

| Módulo | Uso de `error_handler.h` / `debug.h` |
| --- | --- |
| `lexer.l` | Invoca a `lexicalError` ante caracteres inválidos y comentarios sin cerrar. |
| `bison.y` | Invoca a `syntaxError` desde `yyerror`. |
| `type_checker.c` | Invoca a `semanticError` ante incompatibilidades de tipos. |
| `semantic_analyzer.c` | Invoca a `semanticError` ante errores de declaración, alcance y firmas de métodos. |
| `driver.c` | Consulta `lexical_errors` y `semantic_errors`, llama a `compileAborted` y emite las trazas con `debugPrintf`. |

---

## 6. Cómo se extiende

### Agregar un nuevo nivel de error

1. Declarar la función variádica y su contador `extern` en `include/error_handler.h`.
2. Implementarla en `src/core/error_handler.c` delegando en `reportError` con la etiqueta de la etapa y el puntero al nuevo contador, y reiniciar dicho contador en `resetErrorHandler`.
3. Consultar el contador en `driver.c` al finalizar la etapa correspondiente.

### Redirigir errores a un `FILE *` configurable

Actualmente `reportError` escribe de forma fija en `stderr`. Parametrizar el destino mediante un `FILE *` (por defecto `stderr`) permitiría capturar y verificar el texto exacto de los mensajes de error desde las suites unitarias sin redirigir el descriptor del proceso.

### Niveles de verbosidad en depuración

Para soportar múltiples niveles de detalle (p. ej. `-v`, `-vv`), `options->debug` puede cambiarse de booleano a nivel entero, ajustando `debugPrintf` para recibir el umbral mínimo requerido por cada mensaje.

---

## 7. Validación

El módulo se valida a través de las suites unitarias que reinician y consultan los contadores (`test_ast.c`, `test_semantic.c`, `test_integration.c`) y de `test_cli.c`, que verifica los códigos de salida y la ausencia de impresiones en compilaciones exitosas sin `-debug`.

Comprobaciones en línea de comandos:

```bash
# 1. Compilación exitosa sin -debug: salida vacía en ambos canales
test -z "$(./mi_compilador examples/valido_completo.ctds 2>&1)" && echo "OK silencio"

# 2. Compilación exitosa con -debug: trazas en stdout, stderr vacío
test -z "$(./mi_compilador -debug examples/valido_completo.ctds 2>&1 >/dev/null)" && echo "OK stderr vacio"

# 3. Acumulación de errores semánticos y salida por stderr
./mi_compilador -target sem examples/error_suma_booleano_entero.ctds
```

---

**Siguiente:** [Utilidades y Tipos Base](04-utilidades-y-tipos.md)
