# C-TDS Compiler

Compilador desde cero para C-TDS (TDS26), un lenguaje de programación
imperativo simple, desarrollado como proyecto de la materia Taller de Diseño
de Software (Cod. 3306).

El proyecto se aborda de forma incremental: cada entrega agrega una fase al
compilador. Hoy está cerrado el front-end hasta el análisis semántico (léxico y
sintáctico con Flex y Bison, construcción del AST, tabla de símbolos y las
reglas semánticas del lenguaje). La próxima etapa es la generación de código
intermedio (TAC); el objetivo final es emitir assembly x86-64.

## Autores

- Agustín Alieni
- Fran Natale
- Julián Varea

## Estado actual

- **Análisis léxico** (`src/lexer.l`): reconoce todos los tokens del lenguaje
  (palabras reservadas, identificadores, constantes numéricas y booleanas,
  operadores, delimitadores y comentarios), con número de línea en los
  mensajes de error.
- **Análisis sintáctico** (`src/bison.y`): reconoce la gramática completa de
  C-TDS (declaraciones de variables y métodos intercaladas, bloques,
  condicionales, ciclos, llamadas a método, expresiones con precedencia de
  operadores), sin conflictos shift/reduce ni reglas inalcanzables.
- **AST** (`src/ast.h`, `src/ast.c`): implementado. Las acciones semánticas de
  `src/bison.y` construyen el árbol durante el parseo y `printAST` lo imprime
  indentado. Las firmas y la documentación de cada nodo están en `src/ast.h`.
- **Tabla de símbolos y análisis semántico** (`src/symbol_table.c`,
  `src/error_handler.c`): un recorrido sobre el AST que arma un árbol de scopes
  anidados (global, método, bloque) y valida las reglas del lenguaje:
  identificadores declarados antes de usarse y sin redeclarar en el mismo
  scope, compatibilidad de tipos en asignaciones y operaciones (con coerción
  `int` → `float`), condiciones de `if`/`while` de tipo `boolean`, tipo de
  retorno de cada método, métodos `void` que no pueden usarse como expresión,
  cantidad y tipo de argumentos en cada llamada, y existencia de `main` sin
  parámetros. Cada error se reporta con su línea y la compilación se aborta si
  hubo al menos uno.

Las decisiones de diseño están documentadas en
[`lexico-sintactico/analizador_sintactico.pdf`](lexico-sintactico/analizador_sintactico.pdf)
(el conflicto shift/reduce que resolvimos, por qué separamos el AST de la
gramática, cuándo un nodo se representa en el árbol),
[`arbol_AST/decisiones_diseno_ast.md`](arbol_AST/decisiones_diseno_ast.md)
y [`analisis-semantico.md`](analisis-semantico.md). En la [Wiki de Onboarding](wiki/00-index.md) hay
una versión navegable estructurada paso a paso.

## Estructura del repositorio

```
.
├── src/
│   ├── lexer.l                                      # Analizador léxico (Flex)
│   ├── bison.y                                      # Analizador sintáctico (Bison) + main() del compilador
│   ├── ast.h / ast.c                                # Interfaz e implementación del AST
│   ├── symbol_table.h / symbol_table.c              # Tabla de símbolos, scopes y análisis semántico
│   └── error_handler.h / error_handler.c            # Reporte y conteo de errores léxicos y semánticos
├── tests/                                           # Tests unitarios (Unity) de lexer y parser
├── examples/                                        # Programas de ejemplo para probar rápido el compilador
│   ├── programa.txt                                 # Programa válido con todas las construcciones
│   ├── programa_con_error.txt                       # Programa con un error semántico (tipos incompatibles)
│   └── factorial.txt                                # Factorial recursivo (usado también en los tests)
├── build.sh                                         # Compila el proyecto (ver Compilación)
├── test_suite.sh                                    # Corre los tests (ver Tests)
├── CLAUDE.md                                        # Contexto del proyecto para asistentes de IA
├── mkdocs.yml                                       # Configuración del portal de documentación
└── documentacion/                                   # Portal web y documentación centralizada
    ├── index.md                                     # Portada de la documentación
    ├── wiki/                                        # Wiki interactiva de onboarding (una página por etapa)
    ├── spec/                                        # Enunciados de la cátedra (todo el proyecto)
    ├── notas/                                       # Documentos internos del equipo
    ├── analisis-semantico.md                         # Documentación de la etapa semántica
    ├── arbol_AST/                                    # Decisiones de diseño del AST
    └── lexico-sintactico/                           # Documentación de la primera etapa
        ├── analizador_sintactico.pdf
        └── notas/                                    # Notas de trabajo previas al documento final
```

Los archivos generados (`build/`, `mi_compilador`) no se editan a mano: salen
de `build.sh`.

## Compilación

Requisitos: `bison` (3.x), `flex` (2.6+) y `gcc`.

```bash
bash build.sh
```

Hace tres cosas, en orden: corre `bison` sobre `src/bison.y` y `flex` sobre
`src/lexer.l` para generar el parser y el scanner en `build/` (`bison.tab.c`,
`bison.tab.h`, `lex.yy.c`), y después compila esos dos archivos generados junto
con todos los `.c` de `src/` en el ejecutable `./mi_compilador`, en la raíz del
repo.

Hay que volver a correrlo cada vez que se modifica cualquier cosa de `src/`.
Hoy la build sale sin warnings: si aparece alguno, es de un cambio nuevo.

Si se agrega un `.c` nuevo a `src/`, hay que sumarlo a mano a la línea de `gcc`
de `build.sh` **y** a las de `test_suite.sh`.

## Uso

```bash
./mi_compilador examples/programa.txt          # analiza un archivo
echo "void main(){ int x }" | ./mi_compilador  # o desde la entrada estándar
```

En `examples/` hay programas de ejemplo para probar rápidamente el compilador:
uno válido que usa todas las construcciones de la gramática (`programa.txt`),
uno con un error semántico (`programa_con_error.txt`, asigna `true + x` a un
`boolean`) y un factorial recursivo (`factorial.txt`).

Sobre el archivo de entrada corren cuatro fases, en este orden:

1. **Léxico** (`yylex`). Los símbolos no permitidos se reportan y se descartan;
   como el parseo puede terminar bien igual sobre lo que queda, los errores
   léxicos se cuentan aparte y abortan la compilación.
2. **Sintáctico** (`yyparse`), que **corta en el primer error**
   (`[Linea N] Error sintactico: ...`). Si no hay ninguno, imprime
   `--- Analisis sintactico sin errores formales. ---`.
3. **Construcción del AST**, hecha por las acciones semánticas durante el
   parseo. El árbol completo queda en la global `raizAST`.
4. **Análisis semántico** (`analyzeSemantics` + `checkMainMethodExists`), que
   recorre el AST, arma la tabla de símbolos y valida las reglas del lenguaje.
   A diferencia del sintáctico, **no corta en el primer error**: reporta todos
   los que encuentra, cada uno con su línea, y al final
   `Compilacion abortada: se encontraron N errores semanticos.`
5. **Impresión del AST**, solo si no hubo ningún error, seguida de
   `--- Analisis semantico completado sin errores. ---`.

O sea: ver el árbol impreso es la señal de que el programa pasó todas las fases.
Los errores salen por `stderr` y el resto por `stdout`, así que si se redirige
una sola de las dos el orden de las líneas se mezcla.

En el árbol, cada identificador aparece como `nombre #uid (Tipo T)`. El `uid`
identifica a la entidad declarada, así que dos variables distintas con el mismo
nombre (ocultamiento de scopes) se distinguen a simple vista.

Un programa válido termina con código de salida 0, y cualquier error (léxico,
sintáctico o semántico) con 1.

## Tests

```bash
bash test_suite.sh
```

No usa `./mi_compilador`: regenera Flex/Bison y compila los fuentes de `src/`
dentro de un ejecutable de test por archivo de tests (en `build/`), con
`-DUNITY_TESTING` para que el `main()` de `src/bison.y` quede afuera y el que
corra sea el de los tests. Framework
[Unity](https://github.com/ThrowTheSwitch/Unity), incluido en `tests/unity/`.

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
./mi_compilador examples/programa.txt
```

`programa.txt` usa todas las construcciones de la gramática, así que sirve de
prueba de humo de todas las fases de una sola pasada. Para chequear que los
errores se siguen detectando, `./mi_compilador examples/programa_con_error.txt`
tiene que reportar un error semántico y salir con código 1.

## Problemas conocidos

- **Falta la interfaz de línea de comandos que pide la letra**
  (`-target`, `-opt`, `-debug`, `-o`, extensión `.ctds` obligatoria, una salida
  por etapa). Hoy el compilador toma un único argumento posicional y, si no se
  le pasa ninguno, lee de la entrada estándar. Los ejemplos todavía usan `.txt`.
- **Sin `-debug`, el compilador igual imprime el AST.** La letra pide que una
  compilación exitosa no imprima nada salvo que se pida debugging.
- **Un método solo ve a los declarados antes que él.** No hay declaración
  adelantada en C-TDS, así que `main(){ g(); }` con `g` definido más abajo da
  `Método no declarado`. Es lo que pide la letra ("todo se declara antes de
  usarse"), pero conviene tenerlo presente al escribir ejemplos.
