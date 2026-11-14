# Primeros Pasos

Guía para compilar, ejecutar la suite de pruebas y utilizar el compilador desde la línea de comandos.

---

## Prerrequisitos

- **GCC** (con soporte para C99 o superior)
- **Flex** (versión 2.6 o superior)
- **Bison** (versión 3.x)

Para verificar las versiones instaladas:

```bash
gcc --version && flex --version && bison --version
```

Para servir la documentación localmente se requiere además `uv` (ver sección *Documentación* en el `README.md` del repositorio).

---

## Compilación

El script principal genera el analizador léxico, el analizador sintáctico y compila todos los módulos en el ejecutable `./mi_compilador` en la raíz del repositorio:

```bash
bash build.sh
```

Debe ejecutarse nuevamente tras cualquier modificación en `src/` o `include/`. Si se incorpora un nuevo archivo `.c`, debe agregarse a la invocación de `gcc` en `build.sh` y a las suites correspondientes en `test_suite.sh`. La compilación está configurada para no emitir *warnings*.

---

## Ejecución de Pruebas

```bash
bash test_suite.sh
```

Compila y ejecuta once suites basadas en Unity: diez suites unitarias (lexer, parser, AST, tabla de símbolos, análisis semántico, TAC, análisis de variables vivas, optimizador, código objeto y línea de comandos) y una suite de integración, sumando 294 tests.

### Organización de la batería de pruebas

Cada suite unitaria evalúa una única etapa del pipeline: la fase *Arrange* construye la estructura de entrada (equivalente a la salida de la etapa previa), *Act* ejecuta el módulo bajo prueba y *Assert* verifica únicamente su salida. Cada binario de prueba enlaza solo los archivos fuente de su etapa y las estructuras de datos de las que depende.

| Suite | Entrada | Construcción de la entrada |
| --- | --- | --- |
| `test_lexer.c`, `test_parser.c`, `test_ast.c` | Código fuente C-TDS | Cadena de texto en el test |
| `test_semantic.c` | AST sin anotar | Constructores de `tests/support/ast_builder.h` (`astProgram`, `astMethod`, `astAssignment`, …) |
| `test_tac.c` | AST anotado (`uid`, alcance y tipos resueltos) | Constructores de `ast_builder.h` (`astGlobal`, `astLocal`, `astUse`, `astStore`, `astInvoke`, …) |
| `test_live_variable_analysis.c`, `test_optimizer.c`, `test_object_code.c` | Lista TAC | Instrucciones manuales o texto en formato `.ci` parseado con `buildTAC` (`tests/support/tac_builder.h`) |
| `test_integration.c` | Código fuente C-TDS | Compilación completa, ejecución del binario y verificación del código de salida |

Existen dos particularidades en este esquema:

- **Lexer, parser y AST parten de texto fuente:** dado que Bison invoca a `yylex` directamente durante el análisis sintáctico, las tres primeras suites toman cadenas con código C-TDS como entrada.
- **`test_integration.c` evalúa el pipeline de punta a punta:** verifica la interacción entre todas las fases mediante programas completos (los ejemplos de `examples/`, llamadas y pasaje de argumentos por pila, ámbitos, cortocircuito, casos límite del generador y equivalencia semántica con y sin optimizaciones).

Para construir casos de prueba en etapas intermedias se puede utilizar el propio compilador: `-target codinter` emite el TAC en el formato aceptado por `buildTAC`, y `-target sem` muestra la estructura del AST anotado a reproducir mediante `ast_builder.h`.

La salida por defecto muestra el estado `[PASS]` o `[FAIL]` de cada test agrupado por suite:

```
  [FAIL] test_error_if_while_mal_formados
         -> linea 215: deberia RECHAZAR: void main(){ while x < 3 { } }
```

El script finaliza con código `0` si todas las pruebas pasan y `1` si alguna falla o no compila. Para inspeccionar la salida completa de Unity junto con los mensajes emitidos por el compilador en los casos de error:

```bash
VERBOSE=1 bash test_suite.sh
```

---

## Ejecución del Compilador

```bash
./mi_compilador [opciones] archivo.ctds
```

El archivo de entrada es obligatorio, no puede comenzar con `-` y debe tener extensión `.ctds`. No se soporta lectura desde `stdin`.

Cuando no se especifica `-target`, el compilador ejecuta todas las etapas y genera el ejecutable `nombreArchivo.out`:

```bash
./mi_compilador examples/factorial.ctds   # genera examples/factorial.out
./examples/factorial.out                  # ejecuta el binario
```

Dado que C-TDS carece de funciones de entrada/salida estándar, el resultado de ejecución se observa a través del código de retorno del proceso (en programas donde `main` retorna `int`; si `main` es `void`, el proceso retorna siempre `0`):

```bash
./mi_compilador examples/dead_code.ctds
./examples/dead_code.out; echo $?         # imprime 10
```

### Opciones de línea de comandos

| Opción | Descripción |
| --- | --- |
| `-o <salida>` | Define la ruta del archivo de salida en lugar de deducirla del nombre de entrada |
| `-target <etapa>` | Ejecuta el pipeline hasta la etapa indicada: `scan`, `parse`, `sem`, `codinter`, `assembly` |
| `-opt [nombre]` | Habilita optimizaciones (`all` o lista separada por comas). Desactivado por defecto |
| `-debug` | Imprime información diagnóstica de las etapas en ejecución por `stdout` |

Sin la opción `-debug`, una compilación exitosa no produce salida por consola y escribe únicamente el archivo resultante. Los diagnósticos de error se emiten por `stderr`; ante cualquier error (de invocación, léxico, sintáctico o semántico), el proceso retorna `1` sin dejar archivo de salida.

### Salida por etapa (`-target`)

Al especificar `-target`, el compilador detiene el pipeline tras completar la fase indicada y genera un archivo con la extensión correspondiente:

| `-target` | Extensión | Contenido |
| --- | --- | --- |
| `scan` | `.lex` | Secuencia de tokens reconocidos con número de línea y lexema |
| `parse` | `.sint` | Árbol de sintaxis abstracta (AST) sin anotaciones semánticas |
| `sem` | `.sem` | AST anotado con tipos resueltos y `uid` de símbolos |
| `codinter` | `.ci` | Código intermedio de tres direcciones (TAC) |
| `assembly` | `.ass` | Código ensamblador x86-64 (sintaxis AT&T para GNU Assembler) |

Si se combina `-target` con `-opt`, la extensión de salida pasa a ser `.opt`:

```bash
./mi_compilador -target scan examples/factorial.ctds               # -> examples/factorial.lex
./mi_compilador -target codinter examples/factorial.ctds           # -> examples/factorial.ci
./mi_compilador -opt all -target codinter examples/factorial.ctds  # -> examples/factorial.opt
./mi_compilador -o miprograma examples/factorial.ctds              # -> miprograma (ejecutable)
```

---

## Verificación Rápida

Tras realizar cambios en el código fuente, se recomienda recompilar y ejecutar la batería de pruebas:

```bash
./build.sh && ./test_suite.sh
```

Dado que `test_suite.sh` compila sus propios binarios de prueba de manera independiente, para verificar también el ejecutable `./mi_compilador` generado por `build.sh` se puede compilar `examples/programa.ctds` (que ejercita todas las construcciones de la gramática) y comprobar el rechazo de programas inválidos:

```bash
./mi_compilador examples/programa.ctds
./mi_compilador examples/programa_con_error.ctds   # debe reportar error semántico y salir con 1
```

Para validar que las pasadas de optimización preserven el comportamiento observable, se compila un mismo programa con y sin `-opt all`, se ejecutan ambos binarios y se verifica que sus códigos de salida coincidan.

---

**Siguiente:** [Visión General y Arquitectura](02-vision-general.md)
