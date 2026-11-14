# Utilidades y Tipos Base

**Archivos:** `include/string_builder.h`, `src/core/string_builder.c`, `include/types.h`

Infraestructura transversal compartida por las etapas del compilador: el acumulador dinámico de texto (`string_builder`) empleado para construir la representación textual de TAC y de ensamblador x86-64, y las enumeraciones base (`DataType` y `OperationType`) definidas en `types.h`.

---

## 1. Propósito y alcance

- **`string_builder`:** permite construir en memoria la salida textual de una etapa mediante llamadas sucesivas de formato (`buildString`, `buildStringf`), redimensionando el buffer automáticamente antes de volcar el resultado completo a un archivo.
- **`types.h`:** concentra los tipos de datos y operadores del lenguaje en un encabezado sin dependencias internas, evitando acoplar entre sí al AST, la tabla de símbolos, el analizador semántico, el generador TAC y el backend.

---

## 2. Acumulador de cadenas (`string_builder`)

### Interfaz pública y estado interno

```c
void        buildString(const char *string);
void        buildStringf(const char *format, ...);
const char *getBuiltString(void);
bool        hasStringBuilderError(void);
void        resetBuildString(void);
```

El módulo administra un único buffer estático global en `src/core/string_builder.c`:

```c
static char  *code_buffer = NULL;
static size_t code_len    = 0;
static size_t code_cap    = 0;
static bool   builderFailed = false;
```

Al ser una instancia única en lugar de una estructura pasada por parámetro, los módulos emisores (como `object_code.c`, que realiza 50 llamadas a `buildString`/`buildStringf`) no necesitan propagar un puntero de contexto en sus firmas. Como contrapartida, no es posible construir dos salidas en paralelo y cada etapa debe iniciar su emisión con `resetBuildString()` (que libera el buffer previo y reinicia el estado). Si nunca se escribió en el buffer, `getBuiltString()` retorna `""` en lugar de `NULL`.

### Ciclo de uso y consumidores

Las etapas de código intermedio y de código objeto utilizan el mismo esquema en `driver.c`:

```c
resetBuildString();                              /* 1. Reiniciar buffer y flag de error */
formatTACList(tacList);                          /* 2. Acumular salida de la etapa */
if (hasStringBuilderError()) { ...; return 1; }  /* 3. Verificar fallos de memoria */
writeBuiltStringTo(options->output);             /* 4. Volcar a disco */
```

- `object_code.c` emite todas las directivas e instrucciones de ensamblador x86-64 directamente sobre `string_builder`.
- `tac.c` realiza una única llamada desde `printTACInstruction`, la cual delega el formateo en `formatTACInstruction` (que opera sobre un buffer local del llamador, permitiendo reutilizar el mismo formato en distintos destinos; ver [representación TAC](../05-codigo-intermedio/01-representacion-tac.md)).

### Crecimiento geométrico y prevención de desbordes

```c
static bool sb_ensure_capacity(size_t needed) {
    if (needed > SIZE_MAX - code_len - 1) { builderFailed = true; return false; }
    size_t required = code_len + needed + 1;
    if (required <= code_cap) return true;

    size_t new_cap = code_cap ? code_cap : 64;
    while (new_cap < required) {
        if (new_cap > SIZE_MAX / 2) { new_cap = required; break; }
        new_cap *= 2;
    }

    char *new_buf = realloc(code_buffer, new_cap);
    if (!new_buf) { perror("string_builder realloc failed"); builderFailed = true; return false; }
    code_buffer = new_buf; code_cap = new_cap;
    return true;
}
```

El buffer comienza en 64 bytes y duplica su capacidad cada vez que se agota, logrando un costo amortizado $O(1)$ por inserción. Antes de redimensionar, valida dos condiciones de desborde de entero sobre `size_t`:

1. Que `code_len + needed + 1` (incluyendo el byte para `'\0'`) no supere `SIZE_MAX`.
2. Que la duplicación `new_cap * 2` no desborde al calcular la nueva capacidad.

### Formateo en dos pasos con `va_copy` (`buildStringf`)

```c
va_list ap, ap_copy;
va_start(ap, format);
va_copy(ap_copy, ap);
int len = vsnprintf(NULL, 0, format, ap_copy);   /* 1. Calcular longitud exacta */
va_end(ap_copy);
...
if (!sb_ensure_capacity((size_t)len)) { ... }    /* 2. Asegurar espacio */
vsnprintf(code_buffer + code_len, code_cap - code_len, format, ap);  /* 3. Escribir */
```

La primera llamada a `vsnprintf(NULL, 0, ...)` calcula la cantidad exacta de bytes requeridos sin escribir nada. Dado que recorrer un `va_list` lo consume, se utiliza `va_copy` para medir sobre `ap_copy` y preservar `ap` intacto para la escritura final en el buffer redimensionado.

### Manejo de errores mediante bandera persistente (`builderFailed`)

Si `realloc` falla o se detecta un desborde de tamaño, se activa `builderFailed = true`. Mientras esa bandera permanece activa, las llamadas subsiguientes a `buildString` o `buildStringf` retornan inmediatamente sin modificar el buffer, hasta el próximo `resetBuildString()`. De esta forma, el código emisor no necesita verificar el retorno de cada instrucción emitida, y el driver comprueba `hasStringBuilderError()` una sola vez antes de escribir el archivo de salida, evitando dejar archivos truncados en disco.

---

## 3. Tipos y operadores base (`types.h`)

El archivo `include/types.h` define dos enumerados y declara una función auxiliar de conversión a texto.

### `DataType`

```c
typedef enum DataType {
    TYPE_INT,
    TYPE_BOOL,
    TYPE_FLOAT,
    TYPE_VOID,
    TYPE_ERROR
} DataType;
```

Además de los tipos primitivos del lenguaje (`TYPE_INT`, `TYPE_BOOL`, `TYPE_FLOAT`), incluye dos valores de control:

| Valor | Uso en el compilador |
| --- | --- |
| `TYPE_VOID` | Representa tanto el tipo de retorno de métodos `void` como el estado inicial provisorio de los nodos del AST antes de que corra el análisis semántico. |
| `TYPE_ERROR` | Valor centinela asignado cuando falla la validación de tipos de una subexpresión. Se propaga sin emitir nuevos diagnósticos para evitar cascadas de errores (ver [chequeo de tipos](../04-semantico/02-chequeo-de-tipos.md)). |

### `OperationType`

```c
typedef enum OperationType {
    OP_ADD, OP_UMINUS, OP_NOT, OP_SUB, OP_MUL, OP_DIV,
    OP_MOD, OP_LT, OP_GT, OP_EQ, OP_AND, OP_OR
} OperationType;
```

Enumera los doce operadores del AST (cinco aritméticos binarios, tres relacionales, dos lógicos binarios y dos unarios). A diferencia de `Target` en [CLI y entrada](01-cli-y-entrada.md), el orden de sus constantes no tiene significado semántico. Los operadores lógicos `OP_AND` y `OP_OR` no poseen instrucciones equivalentes en TAC, ya que el generador de código intermedio los traduce directamente a secuencias de saltos condicionales con evaluación por cortocircuito (ver [representación TAC](../05-codigo-intermedio/01-representacion-tac.md)).

### Ubicación de `getDataTypeName`

```c
const char *getDataTypeName(DataType type);
```

Aunque su prototipo se declara en `include/types.h`, su implementación (`"INT"`, `"BOOL"`, `"FLOAT"`, `"VOID"`, `"ERROR"`, `"UNKNOWN"`) se encuentra definida en `src/parser/ast.c` (no existe un archivo `src/types.c`). Por ello, cualquier binario de prueba que invoque a `getDataTypeName` debe enlazar `ast.c`.

---

## 4. Dependencias y acoplamiento

`types.h` no incluye ningún otro encabezado y `string_builder.h` solo incluye `<stdbool.h>`, constituyendo las hojas del grafo de dependencias del proyecto.

`types.h` es incluido directamente por ocho encabezados (`ast.h`, `function_frame.h`, `object_code.h`, `parser_helpers.h`, `semantic_analyzer.h`, `symbol_table.h`, `tac.h` y `type_checker.h`), propagando `DataType` de forma transitiva a todas las fases del compilador.

---

## 5. Cómo se extiende

### Agregar un nuevo tipo de dato

1. Incorporar la constante en `DataType` (`include/types.h`) y su representación textual en `getDataTypeName` (`src/parser/ast.c`).
2. Actualizar todos los `switch` y validaciones sobre `DataType` en el analizador semántico, el chequeo de tipos, la generación de TAC, el dimensionamiento de `FunctionFrame` y la emisión de código objeto (teniendo en cuenta que dichos `switch` poseen rama `default`, por lo que el compilador de C no advertirá automáticamente sobre casos faltantes).

### Agregar un nuevo operador

1. Añadir la constante en `OperationType` (`include/types.h`) y su nombre en `getOperationName` (`src/parser/ast.c`).
2. Incorporar su regla de validación en el [chequeo de tipos](../04-semantico/02-chequeo-de-tipos.md) y su traducción en el generador de código intermedio (y en el backend si requiere una nueva operación TAC).

### Soportar múltiples instancias de `string_builder`

Si se requiriera construir más de una salida textual en simultáneo, las variables estáticas de `string_builder.c` deberían encapsularse en un `struct StringBuilder` pasado por puntero a las funciones del módulo, actualizando los puntos de llamada en `object_code.c`, `driver.c` y `tac.c`.

### Separar la implementación de `types.c`

Para eliminar la dependencia de enlace hacia `ast.c` al usar `getDataTypeName`, se puede mover dicha función a un archivo `src/core/types.c` e incluirlo en `build.sh` y `test_suite.sh`.

---

## 6. Validación

Ambos componentes se validan de forma transversal en las suites unitarias y de integración: `test_tac.c`, `test_optimizer.c` y `test_object_code.c` verifican carácter por carácter las salidas generadas mediante `string_builder`, mientras que `DataType` y `OperationType` intervienen en todas las pruebas desde el AST hasta el código objeto.

Para verificar manualmente la integridad del buffer en salidas extensas:

```bash
./mi_compilador -target assembly examples/valido_completo.ctds
gcc -x assembler examples/valido_completo.ass -o /tmp/programa.out -no-pie
```

---

**Siguiente:** [Análisis Léxico](../03-lexico-sintactico-y-ast/01-analisis-lexico.md)
