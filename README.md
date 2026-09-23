# C-TDS Compiler

Compilador desde cero para C-TDS (TDS26), un lenguaje de programación
imperativo simple, desarrollado como proyecto de la materia Taller de Diseño
de Software (Cod. 3306).

El proyecto se aborda de forma incremental: esta primera entrega cubre el
análisis léxico y sintáctico del lenguaje, usando Flex y Bison, junto con la
interfaz del árbol de sintaxis abstracta (AST) que van a usar las próximas
etapas.

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
- **AST** (`src/ast.h`, `src/ast.c`): por ahora solo está definida la
  interfaz. Las firmas y la documentación de cada función están en
  `src/ast.h`; los cuerpos en `src/ast.c` están vacíos a propósito, y las
  acciones semánticas de `src/bison.y` están escritas pero comentadas, a la
  espera de cerrar en equipo algunas decisiones de diseño antes de
  implementarlas.

Los detalles y las decisiones de diseño de esta etapa (el conflicto
shift/reduce que resolvimos, por qué separamos el AST de la gramática, el
criterio para decidir cuándo un nodo se representa en el árbol, etc.) están
documentados en
[`docs/lexico-sintactico/analizador_sintactico.pdf`](docs/lexico-sintactico/analizador_sintactico.pdf).

## Estructura del repositorio

```
.
├── src/
│   ├── lexer.l                                      # Analizador léxico (Flex)
│   ├── bison.y                                      # Analizador sintáctico (Bison)
│   ├── ast.h                                        # Interfaz del AST
│   └── ast.c                                        # Implementación del AST (pendiente)
├── tests/                                           # Tests unitarios (Unity) de lexer y parser
├── examples/                                        # Programas de ejemplo para probar rápido el compilador
│   ├── programa.txt                                 # Programa válido con todas las construcciones
│   ├── programa_con_error.txt                       # Programa con un error de sintaxis
│   └── factorial.txt                                # Factorial recursivo (usado también en los tests)
├── build.sh                                         # Compila el proyecto (ver Compilación)
├── test_suite.sh                                    # Corre los tests (ver Tests)
└── docs/
    ├── spec/                                         # Enunciados de la cátedra (todo el proyecto)
    ├── notas/                                        # Documentos internos del equipo
    └── lexico-sintactico/                            # Documentación de esta etapa
        ├── analizador_sintactico.pdf
        └── notas/                                    # Notas de trabajo previas al documento final
```

## Compilación

Requisitos: `bison` (3.x), `flex` (2.6+) y `gcc`.

```bash
bash build.sh
```

Genera el parser y el scanner en `build/` (`bison.tab.c`, `bison.tab.h`,
`lex.yy.c`) y el ejecutable `./mi_compilador` en la raíz del repo. Hay que
volver a correrlo cada vez que se modifica `src/lexer.l` o `src/bison.y`.

Los warnings de Bison del tipo `type clash on default action` son esperables:
salen porque las acciones semánticas están comentadas hasta implementar el AST.

## Uso

```bash
./mi_compilador examples/programa.txt          # analiza un archivo
echo "void main(){ int x }" | ./mi_compilador  # o desde la entrada estándar
```

En `examples/` hay programas de ejemplo para probar rápidamente el compilador:
uno válido que usa todas las construcciones de la gramática (`programa.txt`),
uno con un error de sintaxis (`programa_con_error.txt`) y un factorial
recursivo (`factorial.txt`).

Si el programa es sintácticamente correcto imprime
`--- Analisis sintactico sin errores formales. ---`. Si no, informa el error
con su número de línea (`[Linea N] Error sintactico: ...` o
`[Linea N] ERROR LEXICO: simbolo no permitido '...'`). El análisis se detiene
en el primer error sintáctico.

## Tests

```bash
bash test_suite.sh
```

Regenera Flex/Bison, compila y corre los tests unitarios (framework
[Unity](https://github.com/ThrowTheSwitch/Unity), incluido en `tests/unity/`):

- `tests/test_lexer.c`: tokens que devuelve `yylex()` (palabras reservadas,
  identificadores, literales, operadores, comentarios, errores léxicos y
  números de línea).
- `tests/test_parser.c`: programas que `yyparse()` debe aceptar o rechazar
  según la gramática de C-TDS.

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
