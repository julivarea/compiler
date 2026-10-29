# C-TDS Compiler

Compilador desarrollado como proyecto de la materia Taller de Diseño de Software (Cod. 3306)

El proyecto se aborda de forma incremental: cada entrega agrega una fase al compilador.
 Hoy estan funcionales las siguientes fases: Análisis léxico y sintáctico, construcción del AST, tabla de símbolos con scopes anidados y validación semántica, y la generación de código intermedio.
 La próxima etapa es la emisión de código assembly x86-64.

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
  transforma el AST validado en instrucciones lineales de tres direcciones
- **Código objeto x86-64**:
  traduce el TAC a assembly de GNU Assembler y lo ensambla con `gcc` para producir el
  ejecutable. Enteros, booleanos y reales de precisión simple (SSE)

En la [Wiki de Onboarding](wiki/00-index.md) hay
una versión navegable estructurada paso a paso.

## Estructura del repositorio

```
.
├── include/                                         # archivos de cabecera (.h)
│   ├── cli.h                                        # parseo de la línea de comandos
│   ├── ast.h                                        # interfaz del AST
│   ├── types.h                                      # enumerados y tipos de datos
│   ├── symbol_table.h                               # gestión de scopes y símbolos
│   ├── type_checker.h                               # inferencia y chequeo de tipos
│   ├── semantic_analyzer.h                          # análisis semántico
│   ├── tac_operand.h                                # representación y constructores de operandos TAC
│   ├── tac_instruction.h                            # cuádruplas e instrucciones TAC
│   ├── tac_list.h                                   # lista enlazada de instrucciones TAC
│   ├── intermediate_code.h                          # generación de código intermedio
│   ├── asm_instruction.h                            # instrucciones de ensamblador x86-64
│   ├── object_code.h                                # generación de código objeto
│   ├── string_builder.h                             # buffer dinámico de cadenas
│   └── error_handler.h                              # reporte de errores
├── src/                                             # implementaciones (.c, .l, .y)
│   ├── cli.c
│   ├── lexer.l
│   ├── bison.y
│   ├── ast.c
│   ├── symbol_table.c
│   ├── type_checker.c
│   ├── semantic_analyzer.c
│   ├── tac_operand.c
│   ├── tac_instruction.c
│   ├── tac_list.c
│   ├── intermediate_code.c
│   ├── asm_instruction.c
│   ├── object_code.c
│   ├── string_builder.c
│   └── error_handler.c
├── tests/                                           # tests unitarios de lexer y parser
├── build/                                           # archivos de compilacion
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
   - `src/ast.c` (Árbol de Sintaxis Abstracta)
   - `src/symbol_table.c` (Gestión de ámbitos y tabla de símbolos)
   - `src/type_checker.c` (Inferencia, reglas de tipos y coerción)
   - `src/semantic_analyzer.c` (Recorrido y validaciones semánticas)
   - `src/error_handler.c` (Formateo y reporte de errores)
   - `src/string_builder.c` (Buffer dinámico para código intermedio)
   - `src/tac_operand.c` (Estructura y constructores de operandos TAC)
   - `src/tac_instruction.c` (Estructura de instrucciones TAC)
   - `src/tac_list.c` (Lista enlazada de cuádruplas TAC)
   - `src/intermediate_code.c` (Recorrido del AST que construye el TAC)
   - `src/asm_instruction.c` (Instrucciones x86-64)
   - `src/object_code.c` (Generador de código objeto)
   - `src/cli.c` (Parseo de los argumentos de la línea de comandos)
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
| `assembly` | `.ass` | Assembly x86-64 (GNU Assembler) |

Con `-opt` la extensión pasa a ser `.opt`.

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

El compilador finaliza con código de salida `0` si el programa es válido, o `1` si se detecta cualquier error.

**Decisión de diseño sobre el comportamiento por defecto:** el enunciado dice que
sin `-target` se compila "hasta la etapa corriente" generando un archivo con la
extensión de esa etapa, y también que sin `-target` se produce el ejecutable
`nombreArchivo.out`. Mientras el compilador esté incompleto las dos cosas no
pueden valer a la vez, así que el default es la etapa corriente (hoy
`codinter`, salida `.ci`). La constante `CURRENT_TARGET` en `src/cli.h` es lo
único que hay que mover al terminar cada etapa. El valor `sem` de `-target` es
una extensión nuestra: el enunciado no lo enumera, pero sí lista la extensión
`.sem`, con lo cual la etapa semántica no era alcanzable de otro modo.



## Tests

```bash
bash test_suite.sh
```

- `tests/test_lexer.c`: tokens que devuelve `yylex()` (palabras reservadas,
  identificadores, literales, operadores, comentarios, errores léxicos y
  números de línea).
- `tests/test_parser.c`: programas que `yyparse()` debe aceptar o rechazar
  según la gramática de C-TDS.

Todavía no hay tests del AST ni del análisis semántico: esas dos fases solo se
prueban a mano con `examples/`.

La salida muestra una línea `[PASS]`/`[FAIL]` por test. Si un test falla,
se indica la línea del caso que falló y su programa:

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

## Flujo de trabajo

Después de cada cambio en `src/`, estos tres pasos en orden:

```bash
./build.sh && ./test_suite.sh
```

El `&&` importa: si la build falla, no tiene sentido correr los tests. Ojo que
los dos scripts regeneran Flex/Bison por su cuenta, así que `test_suite.sh`
compila su propia copia y **no** valida el ejecutable que dejó `build.sh`. Por
eso al final conviene probar el compilador de verdad sobre un ejemplo:

```bash
./mi_compilador -debug examples/programa.ctds
```

`programa.ctds` usa todas las construcciones de la gramática, así que sirve de
prueba de humo de todas las fases de una sola pasada, y con `-debug` se ve el
TAC generado. Para chequear que los errores se siguen detectando,
`./mi_compilador examples/programa_con_error.ctds` tiene que reportar un error
semántico y salir con código 1 sin dejar archivo de salida.

### Prueba completa por etapas

Para validar las cuatro etapas implementadas sobre un mismo programa:

```bash
for etapa in scan parse sem codinter; do ./mi_compilador -target $etapa examples/programa.ctds || echo "FALLO en $etapa"; done
```

Debe generar `programa.lex`, `programa.sint`, `programa.sem` y `programa.ci` en
`examples/`, sin imprimir nada. Son artefactos generados: están en `.gitignore`
y se borran con `rm -f examples/*.lex examples/*.sint examples/*.sem examples/*.ci`.

## Problemas conocidos

- **`-opt` se acepta y se ignora con un aviso**, porque todavía no hay ninguna optimización.
- **Los `float` son de precisión simple** (4 bytes, IEEE-754, instrucciones `movss`/`addss`).
  No hay doble precisión.
- **Un método solo ve a los declarados antes que él.** No hay declaración
  adelantada en C-TDS, así que `main(){ g(); }` con `g` definido más abajo da
  `Método no declarado`. Es lo que pide la letra ("todo se declara antes de
  usarse"), pero conviene tenerlo presente al escribir ejemplos.

## Proximos pasos
Revisar `TODO_intermediate_code.md` y para las tareas restantes `TODO_semantic_analyzer.md`