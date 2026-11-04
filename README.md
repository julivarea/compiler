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
- Julián Varea

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
- **Intérprete**:
  ejecuta el programa recorriendo el AST, sin pasar por el backend. Es una
  extensión nuestra, ver [Extensiones sobre el enunciado](#extensiones-sobre-el-enunciado).

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

En la [Wiki de Onboarding](documentacion/wiki/00-index.md) hay
una versión navegable estructurada paso a paso. Se lee mucho mejor servida como
sitio que como archivos sueltos: ver [Documentación](#documentación).

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
│   ├── tac_operation.h                              # enumerado de opcodes TAC
│   ├── tac_operand.h                                # representación y constructores de operandos TAC
│   ├── tac_instruction.h                            # cuádruplas e instrucciones TAC
│   ├── tac_list.h                                   # lista enlazada de instrucciones TAC
│   ├── intermediate_code.h                          # generación de código intermedio
│   ├── optimizer.h                                  # flags y pasadas de optimización sobre el TAC
│   ├── live_variable_analysis.h                     # CFG y análisis de variables vivas
│   ├── asm_instruction.h                            # instrucciones de ensamblador x86-64
│   ├── function_frame.h                             # gestión del marco de pila (stack frame)
│   ├── object_code.h                                # generación de código objeto
│   ├── interpreter.h                                # intérprete sobre el AST
│   ├── string_builder.h                             # buffer dinámico de cadenas
│   └── error_handler.h                              # reporte de errores
├── src/                                             # implementaciones (.c, .l, .y)
│   ├── main.c                                       # punto de entrada
│   ├── cli.c
│   ├── driver.c
│   ├── lexer.l
│   ├── bison.y
│   ├── ast.c
│   ├── parser_helpers.c
│   ├── symbol_table.c
│   ├── type_checker.c
│   ├── semantic_analyzer.c
│   ├── tac_operand.c
│   ├── tac_instruction.c
│   ├── tac_list.c
│   ├── intermediate_code.c
│   ├── optimizer.c
│   ├── live_variable_analysis.c
│   ├── asm_instruction.c
│   ├── function_frame.c
│   ├── object_code.c
│   ├── interpreter.c
│   ├── string_builder.c
│   └── error_handler.c
├── tests/                                           # diez suites de tests unitarios con Unity
├── examples/                                        # programas .ctds de ejemplo, válidos e inválidos
├── build/                                           # archivos de compilacion
├── documentacion/                                   # páginas del sitio de documentación (ver "Documentación")
│   ├── wiki/                                        # wiki de onboarding, una página por fase
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
2. **Genera el parser sintáctico:** Corre `bison -d src/bison.y -o build/bison.tab.c`, generando el analizador sintáctico y el archivo de cabecera con los tokens (`build/bison.tab.h`).
3. **Genera el scanner léxico:** Corre `flex -o build/lex.yy.c src/lexer.l`, generando el analizador léxico a partir de las expresiones regulares de tokens.
4. **Compila y enlaza:** Invoca `gcc` con las rutas de inclusión `-Iinclude -Isrc -Ibuild`, compilando los dos archivos generados junto con todos los módulos de `src/`:
   - `src/main.c` (Punto de entrada) y `src/cli.c` (Parseo de los argumentos de la línea de comandos)
   - `src/driver.c` (Orquestación de las etapas según `-target`)
   - `src/ast.c` (Árbol de Sintaxis Abstracta) y `src/parser_helpers.c` (Utilidades de las acciones de Bison)
   - `src/symbol_table.c` (Gestión de ámbitos y tabla de símbolos)
   - `src/type_checker.c` (Inferencia, reglas de tipos y coerción)
   - `src/semantic_analyzer.c` (Recorrido y validaciones semánticas)
   - `src/error_handler.c` (Formateo y reporte de errores)
   - `src/string_builder.c` (Buffer dinámico para la salida de cada etapa)
   - `src/tac_operand.c` (Estructura y constructores de operandos TAC)
   - `src/tac_instruction.c` (Estructura de instrucciones TAC)
   - `src/tac_list.c` (Lista enlazada de cuádruplas TAC)
   - `src/intermediate_code.c` (Recorrido del AST que construye el TAC)
   - `src/optimizer.c` (Pasadas de optimización sobre el TAC)
   - `src/live_variable_analysis.c` (CFG y análisis de variables vivas)
   - `src/asm_instruction.c` (Instrucciones x86-64) y `src/function_frame.c` (Marco de pila)
   - `src/object_code.c` (Generador de código objeto)
   - `src/interpreter.c` (Intérprete sobre el AST)
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
| `-target <etapa>` | Compila hasta esa etapa: `scan`, `parse`, `sem`, `codinter`, `assembly`, `interp` |
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

Con `-opt` la extensión pasa a ser `.opt`. El valor `interp` no escribe archivo:
interpreta el programa y, con `-debug`, informa el valor que devolvió `main`.

Las optimizaciones se piden por nombre, separadas por coma o repitiendo el flag;
las tres formas que siguen son equivalentes:

```bash
./mi_compilador -opt all examples/factorial.ctds
./mi_compilador -opt const-fold,dead-code examples/factorial.ctds
./mi_compilador -opt const-fold -opt dead-code examples/factorial.ctds
```

Un nombre de optimización desconocido es un error: el compilador lista las
soportadas y termina con código 1.

**Sin `-target` el compilador produce el ejecutable** `nombreArchivo.out`: genera el assembly
como paso intermedio y lo ensambla y enlaza con `gcc`. Ese intermedio se borra al terminar,
salvo que se pase `-debug`.

```bash
./mi_compilador examples/factorial.ctds                  # -> examples/factorial.out (ejecutable)
./mi_compilador -target assembly examples/factorial.ctds # -> examples/factorial.ass
./mi_compilador -target scan examples/factorial.ctds     # -> examples/factorial.lex
./mi_compilador -o miprograma examples/factorial.ctds    # -> miprograma
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

**Decisión de diseño sobre el comportamiento por defecto:** el enunciado dice que
sin `-target` se compila "hasta la etapa corriente" generando un archivo con la
extensión de esa etapa, y también que sin `-target` se produce el ejecutable
`nombreArchivo.out`. Mientras el compilador estuvo incompleto las dos cosas no
podían valer a la vez, así que se usó una constante `CURRENT_TARGET`
(en `include/cli.h`) que se iba moviendo al terminar cada etapa. Ahora que el
pipeline está completo vale `TARGET_ASSEMBLY`, con lo cual las dos lecturas
coinciden: sin `-target` se recorre todo y queda el ejecutable
`nombreArchivo.out`.



## Tests

```bash
bash test_suite.sh
```

Compila y corre **diez suites** con Unity, una por módulo, **322 tests** en total:

| Suite | Qué cubre |
| --- | --- |
| `tests/test_lexer.c` | Tokens que devuelve `yylex()`: palabras reservadas, identificadores, literales, operadores, comentarios, errores léxicos y números de línea |
| `tests/test_parser.c` | Programas que `yyparse()` debe aceptar o rechazar según la gramática |
| `tests/test_ast.c` | Forma del árbol que construyen las acciones de Bison |
| `tests/test_symbol_table.c` | Scopes anidados, shadowing y asignación de `uid` |
| `tests/test_semantic.c` | Las reglas semánticas del lenguaje, caso válido e inválido por regla |
| `tests/test_tac.c` | Cuádruplas generadas por cada construcción, incluido el corto circuito |
| `tests/test_live_variable_analysis.c` | Construcción del CFG y convergencia de `LIVE_IN`/`LIVE_OUT` |
| `tests/test_object_code.c` | Assembly emitido por cada instrucción TAC |
| `tests/test_optimizer.c` | Que cada optimización transforme lo que debe y no toque el resto |
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

La documentación del proyecto es un sitio navegable hecho con
[MkDocs](https://www.mkdocs.org/) y el tema
[Material](https://squidfunk.github.io/mkdocs-material/). Las páginas son los
`.md` de `documentacion/` y la configuración está en `mkdocs.yml`.

### Prerrequisitos

Las dependencias de Python se manejan con [`uv`](https://docs.astral.sh/uv/),
que es lo que fija `pyproject.toml` y `uv.lock`. Si no lo tenés instalado:

```bash
pipx install uv
```

No hace falta crear ni activar un entorno virtual a mano: `uv` arma el `.venv`
solo la primera vez, leyendo las versiones exactas de `uv.lock`. Por eso todos
vemos el sitio igual.

### Levantar el sitio

```bash
uv run mkdocs serve
```

Imprime la dirección donde quedó escuchando (por defecto
`http://127.0.0.1:8000/`) y se corta con `Ctrl+C`. Mientras está corriendo
recarga sola: guardás un `.md` y el navegador se actualiza sin reiniciar nada.
Es la forma recomendada de escribir documentación, porque las tablas, los
bloques de código y los diagramas Mermaid se ven renderizados al instante y no
como texto plano.

Si preferís no instalar `uv`, también alcanza con
`pipx install mkdocs-material` y después `mkdocs serve`. El único plugin que usa
`mkdocs.yml` es `search`, que viene incorporado al tema, así que no falta nada
más; lo que se pierde es la garantía de usar las mismas versiones que el resto
del grupo.

Para generar el sitio estático en vez de servirlo:

```bash
uv run mkdocs build
```

Deja el HTML en `site/`, que es un artefacto generado y está en `.gitignore`.

### Por dónde empezar a leer

El recorrido pensado para alguien que llega al proyecto es la **Wiki de
Onboarding**, que va de la visión general del compilador hasta cada fase en el
orden en que se ejecutan: léxico y sintáctico, AST, análisis semántico, código
intermedio, código objeto y análisis de variables vivas. La sección
**Documentación Técnica** es material de consulta puntual (decisiones de diseño
del AST, alcance del análisis semántico, el conflicto de Bison), no se lee de
corrido.

### Agregar una página nueva

Dos pasos, y el segundo es el que se olvida:

1. Crear el `.md` **dentro de `documentacion/`**. Es el `docs_dir` del sitio, así
   que todo lo que esté en la raíz del repo (`optimizaciones.md`, los
   `TODO_*.md`, este mismo README) queda afuera y no aparece.
2. Agregarlo al `nav:` de `mkdocs.yml`. Una página que no está en el `nav` igual
   se compila, pero no figura en el menú y solo se llega a ella por el buscador
   o por un link directo, así que en la práctica es invisible.

## Flujo de trabajo

Después de cada cambio en `src/` o `include/`, estos dos pasos en orden:

```bash
./build.sh && ./test_suite.sh
```

El `&&` importa: si la build falla, no tiene sentido correr los tests. Ojo que
los dos scripts regeneran Flex/Bison por su cuenta, así que `test_suite.sh`
compila su propia copia y **no** valida el ejecutable que dejó `build.sh`. Por
eso al final conviene probar el compilador de verdad sobre un ejemplo:

```bash
./mi_compilador examples/programa.ctds
```

`programa.ctds` usa todas las construcciones de la gramática, así que sirve de
prueba de humo de todas las fases de una sola pasada. Para chequear que los
errores se siguen detectando, `./mi_compilador examples/programa_con_error.ctds`
tiene que reportar un error semántico y salir con código 1 sin dejar archivo de
salida.

Con `-debug` se ve, además, el grafo de flujo de control con los conjuntos de
variables vivas y —si se pasó `-opt`— qué instrucciones eliminó cada
optimización.

### Prueba completa por etapas

Para validar todas las etapas sobre un mismo programa:

```bash
for etapa in scan parse sem codinter assembly; do ./mi_compilador -target $etapa examples/programa.ctds || echo "FALLO en $etapa"; done
```

Debe generar `programa.lex`, `programa.sint`, `programa.sem`, `programa.ci` y
`programa.ass` en `examples/`, sin imprimir nada. Son artefactos generados:
están en `.gitignore` y se borran con
`rm -f examples/*.lex examples/*.sint examples/*.sem examples/*.ci examples/*.ass`.

### Verificar que una optimización no cambió la semántica

El chequeo que importa no es que el TAC quede más corto, sino que el programa
siga haciendo lo mismo. Para eso se compila dos veces y se comparan los
resultados de la ejecución:

```bash
./mi_compilador -o /tmp/sin_opt examples/dead_code.ctds && /tmp/sin_opt; echo "sin opt -> $?"
./mi_compilador -opt all -o /tmp/con_opt examples/dead_code.ctds && /tmp/con_opt; echo "con opt -> $?"
```

Los dos códigos de salida tienen que coincidir entre sí y con el valor esperado.
Como C-TDS no tiene salida por pantalla, el código de salida de `main` es la
única forma de observar lo que hizo un programa, y solo sirve con los `main` que
devuelven `int`.

## Extensiones sobre el enunciado

Tres cosas del compilador no las pedía el enunciado y se implementaron igual. Se
dejan documentadas acá porque cambian el alcance respecto de lo exigido.

### Soporte de `float` y coerción `int`/`float`

La especificación del lenguaje incluye `float` como tipo y su regla 14 permite
las conversiones entre `int` y `float`. Sin embargo, el alcance de las etapas de
código intermedio y código objeto estaba acotado a **tipos enteros y lógicos**,
y la descripción de las asignaciones menciona solo `int` y `boolean`. Es decir
que generar código para `float` no era necesario.

Se implementó de punta a punta igual: inferencia y coerción en el chequeo de
tipos, operandos `float` en el TAC, y el camino SSE completo en el generador de
código objeto (registros `%xmm`, instrucciones `movss`/`addss`/`comiss`,
constantes en `.rodata`, y la clase de registros separada que impone la
convención de llamada para argumentos reales). La decisión fue mantener el
lenguaje coherente con su propia especificación, en vez de aceptar `float` en el
front-end y abandonarlo en el back-end.

Los `float` son de **precisión simple** (4 bytes, IEEE-754). No hay doble
precisión.

### Intérprete

La tabla de calificación del enunciado incluye un ítem *Intérprete*, pero no
figura en el cronograma de entregas, y al consultarlo se confirmó que **no hacía
falta implementarlo**.

Se implementó igual, en `src/interpreter.c`, accesible con `-target interp`.
Ejecuta el programa recorriendo el AST directamente, sin pasar por el TAC ni por
el backend. Más allá del ítem de la tabla, resultó útil por una razón concreta
de testing: es una **segunda implementación independiente** del mismo lenguaje,
así que sirve de oráculo para el generador de código. Si el intérprete y el
ejecutable compilado difieren sobre el mismo programa, uno de los dos tiene un
bug, y eso se detecta sin tener que calcular a mano el resultado esperado.

### El valor `sem` de `-target`

El enunciado enumera `scan`, `parse`, `codinter` y `assembly`, pero también
lista la extensión `.sem` entre las salidas posibles. Se agregó `-target sem`
porque, de otro modo, la etapa de análisis semántico no era alcanzable y esa
extensión no se podía producir.

## Problemas conocidos

- **Un método solo ve a los declarados antes que él.** No hay declaración
  adelantada en C-TDS, así que `main(){ g(); }` con `g` definido más abajo da
  `Método no declarado`. Es lo que pide la letra ("todo se declara antes de
  usarse"), pero conviene tenerlo presente al escribir ejemplos.
- **El plegado de constantes desborda con signo en `+`, `-` y `*`.** Al plegar
  una expresión como `2147483647 * 3` el cálculo se hace en `int`, lo que en C es
  comportamiento indefinido. Hoy no se nota, porque GCC envuelve en silencio y el
  resultado coincide con el que produciría el assembly generado, pero formalmente
  no está garantizado. La división y el módulo sí están protegidos, tanto contra
  el divisor cero como contra el único desborde posible (`INT_MIN / -1`). Se
  detectó compilando el proyecto con UBSan.
- **Los errores de ejecución del intérprete se reportan como errores semánticos
  y sin número de línea**, y `-target interp` termina con código 0 aunque los
  haya reportado. Está asumido: el intérprete no es entregable, el mensaje igual
  sale por `stderr`, y arreglarlo pedía un contador de errores de ejecución
  propio que no se justificaba.