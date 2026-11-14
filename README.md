# C-TDS Compiler

Compilador desarrollado como proyecto de la materia Taller de Diseño de Software (Cod. 3306)

El proyecto se abordó de forma incremental: cada entrega agregaba una fase al
compilador. **El pipeline está completo**, desde el texto fuente hasta un
ejecutable x86-64: análisis léxico y sintáctico, construcción del AST, tabla de
símbolos con scopes anidados, validación semántica, generación de código
intermedio, optimización sobre el código intermedio y emisión de código objeto.

## Autores

- Agustín Alieni
- Francisco Natale
- Julián Lucas Varea Grosso

## Estado actual

- **Análisis léxico**: reconoce todos los tokens validos del lenguaje
- **Análisis sintáctico**: reconoce la gramática completa del compilador
- **AST**:  Las acciones semánticas de bison construyen el árbol durante el parseo. 
- **Tabla de símbolos y ámbitos**:
  gestión de la jerarquía de `Scope`s (global, funciones, bloques), soporte de
  shadowing y asignación de identificadores únicos (`UID`) para cada entidad declarada.
- **Chequeo de tipos**:
  inferencia de tipos de expresiones, compatibilidad en asignaciones,
  coerción de `int` a `float`, y validación de llamadas a métodos.
- **Análisis semántico**:
  recorrido sobre el AST que valida identificadores no declarados o duplicados en el mismo ámbito, 
  condiciones booleanas en `if`/`while`, consistencia en retornos (`return;` vs `return expr;`), firmas de métodos y la presencia del método `main()`.
- **Código intermedio (TAC)**:
  transforma el AST validado en instrucciones lineales de tres direcciones,
  representadas como cuádruplas con operandos tipados. Incluye evaluación de
  corto circuito para `&&` y `||`.
- **Optimizador**:
  siete optimizaciones sobre el TAC, cada una activable por separado o todas
  juntas con `-opt all`. La eliminación de asignaciones muertas se apoya en un
  análisis de variables vivas sobre el grafo de flujo de control.
- **Código objeto x86-64**:
  traduce el TAC a assembly de GNU Assembler (sintaxis AT&T) y lo ensambla con
  `gcc` para producir el ejecutable. Respeta la convención de llamada
  System V AMD64.
Las optimizaciones disponibles son:

| Flag | Qué hace |
| --- | --- |
| `const-fold` | Plegado de constantes: evalúa en compilación las operaciones entre literales |
| `const-prop` | Propagación de constantes: reemplaza variables de valor conocido por su valor |
| `dead-code` | Elimina código inalcanzable y asignaciones cuyo resultado no se usa |
| `const-branch` | Simplifica `if`/`while` cuya condición es constante |
| `self-assign` | Elimina asignaciones de una variable a sí misma |
| `peephole` | Mirilla sobre instrucciones contiguas: saltos redundantes a la etiqueta siguiente |
| `strength-reduce` | Reducción de fuerza: multiplicaciones por potencias de 2 pasan a desplazamientos |

En la [Wiki](docs/wiki/index.md) se encuentra la documentación técnica de cada módulo; las instrucciones para levantar el sitio con MkDocs están en la sección [Documentación](#documentación).

## Estructura del repositorio

```
.
├── include/                                         # archivos de cabecera (.h)
│   ├── cli.h                                        # parseo de la línea de comandos
│   ├── driver.h                                     # orquestación de las etapas según -target
│   ├── debug.h                                      # convención de salida de debug (-debug)
│   ├── ast.h                                        # interfaz del AST
│   ├── types.h                                      # enumerados y tipos de datos
│   ├── parser_helpers.h                             # utilidades de las acciones de Bison
│   ├── symbol_table.h                               # gestión de scopes y símbolos
│   ├── type_checker.h                               # inferencia y chequeo de tipos
│   ├── semantic_analyzer.h                          # análisis semántico
│   ├── tac.h                                        # representación intermedia TAC (operaciones, operandos, cuádruplas y lista)
│   ├── intermediate_code.h                          # generación de código intermedio
│   ├── optimizer.h                                  # flags y pasadas de optimización sobre el TAC
│   ├── live_variable_analysis.h                     # CFG y análisis de variables vivas
│   ├── function_frame.h                             # gestión del marco de pila (stack frame)
│   ├── object_code.h                                # generación de código objeto
│   ├── string_builder.h                             # buffer dinámico de cadenas
│   └── error_handler.h                              # reporte de errores
├── src/                                             # implementaciones (.c, .l, .y) agrupadas por etapa
│   ├── main.c                                       # punto de entrada
│   ├── core/                                        # infraestructura y orquestación
│   │   ├── cli.c
│   │   ├── driver.c
│   │   ├── error_handler.c
│   │   └── string_builder.c
│   ├── parser/                                      # análisis léxico, sintáctico y AST
│   │   ├── lexer.l
│   │   ├── bison.y
│   │   ├── parser_helpers.c
│   │   └── ast.c
│   ├── semantic/                                    # tabla de símbolos, tipos y análisis semántico
│   │   ├── symbol_table.c
│   │   ├── type_checker.c
│   │   └── semantic_analyzer.c
│   ├── intermediate_code/                           # código intermedio de tres direcciones (TAC)
│   │   ├── tac.c
│   │   └── intermediate_code.c
│   ├── optimizer/                                   # optimizaciones sobre TAC y variables vivas
│   │   ├── live_variable_analysis.c
│   │   └── optimizer.c
│   └── object_code/                                 # generación de código objeto x86-64
│       ├── function_frame.c
│       └── object_code.c
├── tests/                                           # suites de tests con Unity: una por etapa + integración
├── examples/                                        # programas .ctds de ejemplo, válidos e inválidos
├── build/                                           # archivos de compilacion
├── docs/                                            # páginas del sitio de documentación (ver "Documentación")
│   ├── wiki/                                        # wiki agrupada por etapa (espejando src/)
│   ├── decisiones/                                  # alcance, ADRs y cambios a etapas anteriores
│   └── spec/                                        # enunciado y especificación del lenguaje (PDF)
├── mkdocs.yml                                       # configuración del sitio: docs_dir, nav y tema
├── pyproject.toml / uv.lock                         # dependencias de la documentación (MkDocs)
├── build.sh                                         # compilar proyecto
├── test_suite.sh                                    # correr tests
```

## Compilación

### Prerrequisitos

- **`gcc`** (soporte C99 o superior)
- **`bison`** (versión 3.x)
- **`flex`** (versión 2.6 o superior)

Para verificar que las herramientas estén instaladas:
```bash
bison --version && flex --version && gcc --version
```

### Proceso de compilación

Para compilar el proyecto completo:

```bash
bash build.sh
```

El script `build.sh` ejecuta las siguientes operaciones en orden:

1. **Crea el directorio de salida:** Crea la carpeta `build/` si aún no existe.
2. **Genera el parser sintáctico:** Corre `bison -d src/parser/bison.y -o build/bison.tab.c`, generando el analizador sintáctico y el archivo de cabecera con los tokens (`build/bison.tab.h`).
3. **Genera el scanner léxico:** Corre `flex -o build/lex.yy.c src/parser/lexer.l`, generando el analizador léxico a partir de las expresiones regulares de tokens.
4. **Compila y enlaza:** Invoca `gcc` con las rutas de inclusión `-Iinclude -Ibuild`, compilando los dos archivos generados junto con todos los módulos de `src/`:
   - `src/main.c` (Punto de entrada) y `src/core/cli.c` (Parseo de los argumentos de la línea de comandos)
   - `src/core/driver.c` (Orquestación de las etapas según `-target`)
   - `src/parser/ast.c` (Árbol de Sintaxis Abstracta) y `src/parser/parser_helpers.c` (Utilidades de las acciones de Bison)
   - `src/semantic/symbol_table.c` (Gestión de ámbitos y tabla de símbolos)
   - `src/semantic/type_checker.c` (Inferencia, reglas de tipos y coerción)
   - `src/semantic/semantic_analyzer.c` (Recorrido y validaciones semánticas)
   - `src/core/error_handler.c` (Formateo y reporte de errores)
   - `src/core/string_builder.c` (Buffer dinámico para la salida de cada etapa)
   - `src/intermediate_code/tac.c` (Estructura, operandos, cuádruplas y lista enlazada TAC)
   - `src/intermediate_code/intermediate_code.c` (Recorrido del AST que construye el TAC)
   - `src/optimizer/optimizer.c` (Pasadas de optimización sobre el TAC)
   - `src/optimizer/live_variable_analysis.c` (CFG y análisis de variables vivas)
   - `src/object_code/function_frame.c` (Marco de pila) y `src/object_code/object_code.c` (Generador de código objeto)
5. **Genera el binario final:** Produce el ejecutable `./mi_compilador` en la raíz del repositorio.

### Consideraciones y mantenimiento

- **Recompilación necesaria:** Debe volverse a correr `bash build.sh` tras cualquier cambio realizado en los archivos de `src/`.
- **Cero warnings:** Actualmente la compilación no emite ninguna advertencia (*warnings*). Si aparece alguna, debe corregirse antes de continuar.
- **Incorporación de nuevos módulos:** Si se agrega un nuevo archivo `.c` a `src/`, debe agregarse manualmente a la lista de archivos de la llamada a `gcc` en `build.sh`, y a `test_suite.sh` si los tests unitarios lo requieren.
- **Archivos generados:** El ejecutable `./mi_compilador` y el contenido de `build/` son artefactos generados automáticamente y no se editan a mano.


## Uso

```bash
./mi_compilador [opciones] archivo.ctds
```

| Opción | Acción |
| --- | --- |
| `-o <salida>` | Nombre del archivo de salida, en vez del que se deduce de la etapa |
| `-target <etapa>` | Compila hasta esa etapa: `scan`, `parse`, `sem`, `codinter`, `assembly` |
| `-opt [nombre]` | Aplica optimizaciones; `all` son todas. Por defecto ninguna |
| `-debug` | Imprime información de debugging por `stdout` |

El archivo de entrada es obligatorio, no puede empezar con `-` y debe tener
extensión `.ctds`.

**Sin `-debug`, una compilación exitosa no imprime nada.** El resultado de la
etapa se escribe en un archivo, y los errores siempre van a `stderr`. Cualquier
error (de argumentos, léxico, sintáctico o semántico) termina con código 1 y
**no** deja archivo de salida.

Cada etapa escribe un archivo con el nombre del fuente y su propia extensión, o
el que se haya pasado con `-o`:

| `-target` | Salida | Contenido |
| --- | --- | --- |
| `scan` | `.lex` | La secuencia de tokens, con su número de línea y su lexema |
| `parse` | `.sint` | El AST tal como lo construye el parser, sin anotar |
| `sem` | `.sem` | El AST anotado: tipo y `uid` de cada identificador |
| `codinter` | `.ci` | El código de tres direcciones |
| `assembly` | `.ass` | Assembly x86-64 (GNU Assembler, sintaxis AT&T) |

Con `-opt` la extensión pasa a ser `.opt`.

Las optimizaciones se piden por nombre, separadas por coma o repitiendo el flag;
las tres formas que siguen son equivalentes:

```bash
./mi_compilador -opt all examples/valido_factorial_recursivo.ctds
./mi_compilador -opt const-fold,dead-code examples/valido_factorial_recursivo.ctds
./mi_compilador -opt const-fold -opt dead-code examples/valido_factorial_recursivo.ctds
```

Un nombre de optimización desconocido es un error: el compilador lista las
soportadas y termina con código 1.

**Sin `-target` el compilador produce el ejecutable** `nombreArchivo.out`: genera el assembly
como paso intermedio y lo ensambla y enlaza con `gcc`. Ese intermedio se borra al terminar,
salvo que se pase `-debug`.

```bash
./mi_compilador examples/valido_factorial_recursivo.ctds                  # -> examples/valido_factorial_recursivo.out (ejecutable)
./mi_compilador -target assembly examples/valido_factorial_recursivo.ctds # -> examples/valido_factorial_recursivo.ass
./mi_compilador -target scan examples/valido_factorial_recursivo.ctds     # -> examples/valido_factorial_recursivo.lex
./mi_compilador -o miprograma examples/valido_factorial_recursivo.ctds    # -> miprograma
```

Sobre el archivo de entrada se ejecutan las siguientes fases en orden, hasta
donde indique `-target`:

1. **Análisis Léxico:** Escanea los tokens del lenguaje, descarta comentarios y espacios, y rastrea números de línea.
2. **Análisis Sintáctico:** Valida la gramática LALR(1) del programa y detiene la compilación ante el primer error formal.
3. **Construcción del AST:** Genera la representación intermedia en memoria durante las reducciones sintácticas.
4. **Análisis Semántico:** Recorre el AST para verificar declaraciones, resolución de ámbitos, compatibilidad de tipos en operaciones y asignaciones, condiciones de control, retornos y la presencia del método `main`. A diferencia de la etapa sintáctica, reporta todos los errores semánticos encontrados antes de abortar.
5. **Generación de Código Intermedio (TAC):** Si el programa no tiene errores léxicos, sintácticos ni semánticos, emite las instrucciones de Código de Tres Direcciones linealizadas.
6. **Optimización:** Solo si se pasó `-opt`. Aplica las pasadas pedidas sobre el TAC, repitiéndolas hasta que ninguna introduzca cambios nuevos.
7. **Generación de Código Objeto:** Traduce el TAC a assembly x86-64 y, si no se pidió `-target assembly`, lo ensambla y enlaza con `gcc` para producir el ejecutable.

El compilador finaliza con código de salida `0` si el programa es válido, o `1` si se detecta cualquier error.

## Tests

```bash
bash test_suite.sh
```

Compila y corre **once suites** con Unity, **294 tests** en total. Las unitarias prueban una sola etapa:
el test arma a mano la salida de la etapa anterior (AST o TAC, con `tests/support/`), así que un defecto
de una etapa previa no las hace fallar. La de integración es la única de punta a punta:

| Suite | Qué cubre |
| --- | --- |
| `tests/test_lexer.c` | Tokens que devuelve `yylex()`: palabras reservadas, identificadores, literales, operadores, comentarios, errores léxicos y números de línea |
| `tests/test_parser.c` | Programas que `yyparse()` debe aceptar o rechazar según la gramática |
| `tests/test_ast.c` | Forma del árbol que construyen las acciones de Bison |
| `tests/test_symbol_table.c` | Scopes anidados, shadowing y asignación de `uid` |
| `tests/test_semantic.c` | Las reglas semánticas del lenguaje, caso válido e inválido por regla, sobre un AST armado a mano |
| `tests/test_tac.c` | Cuádruplas generadas por cada construcción, incluido el corto circuito, desde un AST anotado armado a mano |
| `tests/test_live_variable_analysis.c` | Construcción del CFG y convergencia de `LIVE_IN`/`LIVE_OUT` |
| `tests/test_optimizer.c` | Que cada optimización transforme lo que debe y no toque el resto, partiendo de TAC |
| `tests/test_object_code.c` | Assembly emitido por cada instrucción TAC |
| `tests/test_integration.c` | Fuente → ejecutable: equivalencia con y sin optimizaciones y ejecución de programas completos |
| `tests/test_cli.c` | Parseo de argumentos, extensiones de salida y códigos de retorno |

La salida muestra una línea `[PASS]`/`[FAIL]` por test, agrupada por suite. Si un
test falla, se indica la línea del caso que falló y su programa:

```
  [FAIL] test_error_if_while_mal_formados
         -> linea 215: deberia RECHAZAR: void main(){ while x < 3 { } }
```

El script termina con código 0 si todos los tests pasan y 1 si alguno falla o
no compila. Para ver la salida cruda de Unity, junto con los warnings de Bison
y los mensajes de error que imprime el compilador en los casos inválidos:

```bash
VERBOSE=1 bash test_suite.sh
```

## Documentación

La documentación del proyecto se publica con
[MkDocs](https://www.mkdocs.org/) y el tema
[Material](https://squidfunk.github.io/mkdocs-material/) a partir de los
archivos `.md` de `docs/` y la configuración en `mkdocs.yml`.

### Prerrequisitos

Las dependencias de Python se gestionan con [`uv`](https://docs.astral.sh/uv/)
mediante `pyproject.toml` y `uv.lock`:

```bash
pipx install uv
```

### Levantar el sitio

```bash
uv run mkdocs serve
```

Levanta el servidor local en `http://127.0.0.1:8000/` con recarga automática al
modificar archivos en `docs/`. Alternativamente, sin `uv`, puede instalarse con
`pipx install mkdocs-material` y ejecutarse con `mkdocs serve`.

Para generar el sitio estático en `site/`:

```bash
uv run mkdocs build
```

### Organización de la documentación

- **Wiki (`docs/wiki/`):** arquitectura e implementación por etapas del compilador (`core/`, `parser/`, `semantic/`, `intermediate_code/`, `optimizer/` y `object_code/`).
- **Decisiones de Diseño (`docs/decisiones/`):** alcance e interpretación del enunciado (como el tratamiento de `float` y las opciones de CLI), ADRs y algoritmo de Sethi-Ullman.
- **Problemas Conocidos (`docs/problemas-conocidos.md`):** limitaciones documentadas del lenguaje y la implementación.
- **Referencia interna:** especificación oficial en `docs/spec/` y convenciones de código y commits en `docs/convenciones/`.

Para incorporar una página nueva al sitio, crear el archivo `.md` dentro de
`docs/` y declararlo en la clave `nav:` de `mkdocs.yml`.

## Flujo de trabajo

Para compilar el proyecto y ejecutar todas las suites de pruebas:

```bash
./build.sh && ./test_suite.sh
```

Tanto `build.sh` como `test_suite.sh` regeneran los archivos de Flex y Bison de
forma independiente (`test_suite.sh` enlaza sus propios ejecutables de prueba en
`tests/`). Para verificar el binario `./mi_compilador` sobre los programas de
ejemplo:

```bash
./mi_compilador examples/valido_completo.ctds
./mi_compilador examples/error_suma_booleano_entero.ctds
```

`valido_completo.ctds` ejercita todas las construcciones de la gramática y
debe finalizar con código `0`. `error_suma_booleano_entero.ctds` debe reportar
error semántico y finalizar con código `1` sin generar archivo de salida.

### Prueba completa por etapas

Para validar todas las etapas sobre un mismo programa:

```bash
for etapa in scan parse sem codinter assembly; do ./mi_compilador -target $etapa examples/valido_completo.ctds || echo "FALLO en $etapa"; done
```

Genera `valido_completo.lex`, `valido_completo.sint`, `valido_completo.sem`,
`valido_completo.ci` y `valido_completo.ass` en `examples/` (ignorados por Git).

### Verificar preservación semántica de las optimizaciones

Para comprobar que las pasadas de optimización no alteran el resultado de un
programa, se compila con y sin `-opt all` y se comparan los códigos de salida:

```bash
./mi_compilador -o /tmp/sin_opt examples/valido_optimizacion_codigo_muerto.ctds && /tmp/sin_opt; echo "sin opt -> $?"
./mi_compilador -opt all -o /tmp/con_opt examples/valido_optimizacion_codigo_muerto.ctds && /tmp/con_opt; echo "con opt -> $?"
```
