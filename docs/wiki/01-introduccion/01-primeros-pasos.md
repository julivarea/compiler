# Primeros Pasos

Guía rápida para compilar, probar y ejecutar el compilador.

---

## Prerrequisitos

- **GCC** (con soporte para C99 o superior)
- **Flex** (versión 2.6 o superior)
- **Bison** (versión 3.x)

Para verificar que estén instalados:

```bash
gcc --version && flex --version && bison --version
```

Para levantar esta documentación hace falta además `uv`; eso está explicado en la
sección *Documentación* del `README.md` del repositorio.

---

## Compilación

Compila el parser, el scanner y todos los módulos del compilador, generando el
ejecutable `./mi_compilador` en la raíz del repositorio:

```bash
bash build.sh
```

Hay que volver a correrlo después de cualquier cambio en `src/` o `include/`. Si
se agrega un `.c` nuevo, hay que sumarlo a mano a la lista de `gcc` en
`build.sh` y a las suites de `test_suite.sh` que lo necesiten.

La compilación no emite ningún *warning*. Si aparece alguno, se corrige antes de
seguir.

---

## Ejecución de Pruebas

```bash
bash test_suite.sh
```

Compila y corre once suites con Unity: diez unitarias, una por módulo (lexer,
parser, AST, tabla de símbolos, análisis semántico, TAC, análisis de variables
vivas, optimizador, código objeto y línea de comandos), y una de integración. En
total **294 tests**.

### Cómo está organizada la batería

Cada suite unitaria prueba **una sola etapa**: su Arrange arma a mano la salida de
la etapa anterior, el Act ejecuta la etapa bajo prueba y el Assert mira solo lo que
esa etapa produce. Así, un defecto del parser no hace fallar los tests del
optimizador, y un test rojo apunta directamente al módulo con el problema. Cada
suite enlaza solo los fuentes de su etapa y los módulos de datos que recibe.

| Suite | Entrada que arma el test | Cómo la arma |
| --- | --- | --- |
| `test_lexer.c`, `test_parser.c`, `test_ast.c` | Texto C-TDS | Cadena con el programa |
| `test_semantic.c` | AST **sin anotar** (lo que entrega el parser) | `tests/support/ast_builder.h`: `astProgram`, `astMethod`, `astAssignment`, … |
| `test_tac.c` | AST **anotado** (uid, alcance y tipos resueltos) | `ast_builder.h`: `astGlobal`, `astLocal`, `astUse`, `astStore`, `astInvoke`, … |
| `test_live_variable_analysis.c`, `test_optimizer.c`, `test_object_code.c` | TAC | Instrucciones construidas a mano, o el listado en formato `.ci` con `buildTAC` (`tests/support/tac_builder.h`) |
| `test_integration.c` | Texto C-TDS | Compila de punta a punta, ejecuta y compara el código de salida |

Dos excepciones, deliberadas:

- **Lexer, parser y AST parten de texto.** El parser consume tokens llamando a
  `yylex` directamente, así que no hay una "salida del lexer" que se pueda armar
  por separado; el texto es la entrada natural de esas tres suites.
- **`test_integration.c` atraviesa todas las etapas a propósito.** Es la única
  suite de punta a punta y tiene pocos tests, simples: un programa mínimo que
  corre de principio a fin, los ejemplos de `examples/`, llamadas y pasaje por
  pila, ámbitos, cortocircuito, los límites del generador y tres programas que
  deben dar el mismo resultado con y sin optimizar. El detalle de cada etapa ya
  lo cubren las suites unitarias.
  Un fallo acá sin un fallo en las suites unitarias señala un problema de
  integración entre etapas o un caso que ninguna suite unitaria cubre todavía.

Para agregar un caso a una etapa intermedia conviene obtener su entrada con el
propio compilador y pegarla en el test: `-target codinter` imprime el TAC en el
formato que acepta `buildTAC`, y `-target sem` muestra el AST anotado que hay que
reproducir con `ast_builder.h`.

La salida muestra una línea `[PASS]`/`[FAIL]` por test, agrupada por suite. Si
alguno falla, se indica la línea del caso y su programa:

```
  [FAIL] test_error_if_while_mal_formados
         -> linea 215: deberia RECHAZAR: void main(){ while x < 3 { } }
```

El script termina con código 0 si todos pasan y 1 si alguno falla o no compila.
Para ver la salida cruda de Unity, junto con los *warnings* de Bison y los
mensajes de error que imprime el compilador en los casos inválidos:

```bash
VERBOSE=1 bash test_suite.sh
```

---

## Ejecución del Compilador

```bash
./mi_compilador [opciones] archivo.ctds
```

El archivo de entrada es **obligatorio**, no puede empezar con `-` y **debe
tener extensión `.ctds`**. No se lee de la entrada estándar.

Sin `-target`, el compilador recorre todas las fases y produce el **ejecutable**
`nombreArchivo.out`:

```bash
./mi_compilador examples/factorial.ctds   # -> examples/factorial.out
./examples/factorial.out                  # ejecutarlo
```

Como C-TDS no tiene salida por pantalla, la forma de observar el resultado de un
programa es el **código de salida de `main`**, que es lo que devuelve el proceso.
Sirve solo con los programas cuyo `main` devuelve `int`; `factorial.ctds` tiene
`void main`, así que siempre termina en 0. Un ejemplo que sí devuelve un valor:

```bash
./mi_compilador examples/dead_code.ctds
./examples/dead_code.out; echo $?         # -> 10
```

### Opciones

| Opción | Acción |
| --- | --- |
| `-o <salida>` | Nombre del archivo de salida, en vez del que se deduce de la etapa |
| `-target <etapa>` | Compila hasta esa etapa: `scan`, `parse`, `sem`, `codinter`, `assembly` |
| `-opt [nombre]` | Aplica optimizaciones; `all` son todas. Por defecto ninguna |
| `-debug` | Imprime información de debugging por `stdout` |

**Sin `-debug`, una compilación exitosa no imprime nada**: el resultado se
escribe en un archivo. Los errores siempre van a `stderr`, y cualquier error
(de argumentos, léxico, sintáctico o semántico) termina con código 1 y no deja
archivo de salida.

### Compilar hasta una etapa

Con `-target`, cada etapa escribe un archivo con el nombre del fuente y su
propia extensión:

| `-target` | Salida | Contenido |
| --- | --- | --- |
| `scan` | `.lex` | La secuencia de tokens, con su número de línea y su lexema |
| `parse` | `.sint` | El AST tal como lo construye el parser, sin anotar |
| `sem` | `.sem` | El AST anotado: tipo y `uid` de cada identificador |
| `codinter` | `.ci` | El código de tres direcciones |
| `assembly` | `.ass` | Assembly x86-64 (GNU Assembler, sintaxis AT&T) |

Con `-opt` la extensión pasa a ser `.opt`.

```bash
./mi_compilador -target scan examples/factorial.ctds      # -> examples/factorial.lex
./mi_compilador -target codinter examples/factorial.ctds  # -> examples/factorial.ci
./mi_compilador -opt all -target codinter examples/factorial.ctds  # -> examples/factorial.opt
./mi_compilador -o miprograma examples/factorial.ctds     # -> miprograma (ejecutable)
```

---

## Prueba de humo

Después de cada cambio, estos dos pasos en orden:

```bash
./build.sh && ./test_suite.sh
```

El `&&` importa: si la build falla, no tiene sentido correr los tests. Y ojo que
los dos scripts regeneran Flex y Bison por su cuenta, así que `test_suite.sh`
compila su propia copia y **no** valida el ejecutable que dejó `build.sh`. Por
eso al final conviene probar el compilador de verdad sobre un ejemplo:

```bash
./mi_compilador examples/programa.ctds
```

`programa.ctds` usa todas las construcciones de la gramática, así que sirve de
prueba de humo de todas las fases de una sola pasada. Para chequear que los
errores se siguen detectando, `./mi_compilador examples/programa_con_error.ctds`
tiene que reportar un error semántico y salir con código 1.

Para verificar que una optimización no cambió la semántica, el chequeo es
compilar el mismo programa con y sin `-opt all`, ejecutar los dos binarios y
comparar sus códigos de salida: tienen que coincidir entre sí y con el valor
esperado.

---

**Siguiente:** [Visión General y Arquitectura](02-vision-general.md)
