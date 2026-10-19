# C-TDS Compiler

Compilador desde cero para C-TDS (TDS26), un lenguaje de programación
imperativo simple, desarrollado como proyecto de la materia Taller de Diseño
de Software (Cod. 3306).

El proyecto se aborda de forma incremental: cada entrega agrega una fase al
compilador. Hoy está cerrado el front-end completo (análisis léxico y sintáctico
con Flex y Bison, construcción y decorado del AST, tabla de símbolos con scopes
anidados y validación semántica estricta) y la generación de código intermedio
(TAC - Three-Address Code) con buffer dinámico en memoria. La próxima etapa es
la emisión de código assembly x86-64.

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
- **Tabla de símbolos y ámbitos** (`src/symbol_table.h`, `src/symbol_table.c`):
  gestión de la jerarquía de `Scope`s (global, funciones, bloques), soporte de
  shadowing y asignación de identificadores únicos (`UID`) para cada entidad declarada.
- **Chequeo de tipos** (`src/type_checker.h`, `src/type_checker.c`):
  inferencia de tipos de expresiones, compatibilidad en asignaciones (`isAssignable`),
  coerción de `int` a `float`, y validación de llamadas a métodos.
- **Análisis semántico** (`src/semantic_analyzer.h`, `src/semantic_analyzer.c`, `src/error_handler.c`):
  recorrido sobre el AST (`analyzeSemantics`) que valida identificadores no declarados
  o duplicados en el mismo ámbito, condiciones booleanas en `if`/`while`, consistencia
  en retornos (`return;` vs `return expr;`), firmas de métodos y la presencia del método `main()`.
- **Código intermedio (TAC)** (`src/intermediate_code.h`, `src/intermediate_code.c`, `src/string_builder.h`, `src/string_builder.c`):
  transforma el AST validado en instrucciones lineales de tres direcciones con
  variables temporales (`t0, t1...`) y etiquetas de salto (`L0, L1...`).

Las decisiones de diseño están documentadas en
[`documentacion/lexico-sintactico/analizador_sintactico.pdf`](documentacion/lexico-sintactico/analizador_sintactico.pdf),
[`documentacion/arbol_AST/decisiones_diseno_ast.md`](documentacion/arbol_AST/decisiones_diseno_ast.md),
[`documentacion/analisis-semantico.md`](documentacion/analisis-semantico.md) y
[`documentacion/wiki/05-intermediate-code.md`](documentacion/wiki/05-intermediate-code.md). Toda la documentación técnica y las guías de onboarding están organizadas en `documentacion/`.

## Estructura del repositorio

```
.
├── src/
│   ├── lexer.l                                      # Analizador léxico (Flex)
│   ├── bison.y                                      # Analizador sintáctico (Bison) + main() del compilador
│   ├── ast.h / ast.c                                # Interfaz e implementación del AST
│   ├── types.h                                      # Enumerados DataType y OperationType
│   ├── symbol_table.h / symbol_table.c              # Gestión de Scopes y Symbols
│   ├── type_checker.h / type_checker.c              # Inferencia, chequeo y compatibilidad de tipos
│   ├── semantic_analyzer.h / semantic_analyzer.c    # Recorrido semántico y validaciones del lenguaje
│   ├── intermediate_code.h / intermediate_code.c    # Generador de Código de Tres Direcciones (TAC)
│   ├── string_builder.h / string_builder.c          # Buffer dinámico de cadenas para la salida TAC
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

1. **Crea el directorio de salida temporal:** Crea la carpeta `build/` si aún no existe.
2. **Genera el parser sintáctico:** Ejecuta `bison -d src/bison.y -o build/bison.tab.c`, creando el analizador sintáctico y el archivo de cabecera con los tokens (`build/bison.tab.h`).
3. **Genera el scanner léxico:** Ejecuta `flex -o build/lex.yy.c src/lexer.l`, creando el analizador léxico a partir de las expresiones regulares.
4. **Compila y enlaza:** Invoca `gcc` con las banderas de inclusión `-Isrc -Ibuild`, compilando los dos archivos generados junto con todos los módulos de `src/`:
   - `src/ast.c` (Árbol de Sintaxis Abstracta)
   - `src/symbol_table.c` (Gestión de ámbitos y tabla de símbolos)
   - `src/type_checker.c` (Inferencia, reglas de tipos y coerción)
   - `src/semantic_analyzer.c` (Recorrido y validaciones semánticas)
   - `src/error_handler.c` (Formateo y conteo de errores)
   - `src/string_builder.c` (Buffer dinámico para salida)
   - `src/intermediate_code.c` (Generación de Código de Tres Direcciones - TAC)
5. **Genera el binario final:** Produce el ejecutable `./mi_compilador` en la raíz del repositorio.

### Consideraciones y mantenimiento

- **Recompilación necesaria:** Debe ejecutarse `bash build.sh` tras cualquier cambio realizado dentro de `src/`.
- **Cero warnings:** La compilación actual no emite ninguna advertencia (*warnings*). Si surge alguna, debe corregirse antes de continuar.
- **Incorporación de nuevos módulos:** Si se agrega un nuevo archivo `.c` a `src/`, debe agregarse manualmente a la lista de archivos de la llamada a `gcc` en `build.sh`, y en las secciones correspondientes de `test_suite.sh` si los tests unitarios lo consumen.
- **Archivos generados:** El ejecutable `./mi_compilador` y el contenido de `build/` no deben editarse a mano ni incluirse en el control de versiones.


## Uso

```bash
./mi_compilador examples/programa.txt          # analiza un archivo
echo "void main(){ int x }" | ./mi_compilador  # o desde la entrada estándar
```

En `examples/` hay programas de ejemplo para probar rápidamente el compilador:
uno válido que usa todas las construcciones de la gramática (`programa.txt`),
uno con un error semántico (`programa_con_error.txt`, asigna `true + x` a un
`boolean`) y un factorial recursivo (`factorial.txt`).

Sobre el archivo de entrada se ejecutan las siguientes fases en orden:

1. **Análisis Léxico:** Escanea los tokens del lenguaje, descarta comentarios y espacios, y rastrea números de línea.
2. **Análisis Sintáctico:** Valida la gramática LALR(1) del programa y detiene la compilación ante el primer error formal.
3. **Construcción del AST:** Genera la representación intermedia en memoria durante las reducciones sintácticas.
4. **Análisis Semántico:** Recorre el AST para verificar declaraciones, resolución de ámbitos, compatibilidad de tipos en operaciones y asignaciones, condiciones de control, retornos y la presencia del método `main`. A diferencia de la etapa sintáctica, reporta todos los errores semánticos encontrados antes de abortar.
5. **Generación de Código Intermedio (TAC):** Si el programa no tiene errores léxicos, sintácticos ni semánticos, emite las instrucciones de Código de Tres Direcciones linealizadas.

El compilador finaliza con código de salida `0` si el programa es válido, o `1` si se detecta cualquier error.


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

## Documentación del Proyecto

El proyecto incluye un portal web de documentación interactivo construido con [Material for MkDocs](https://squidfunk.github.io/mkdocs-material/), que reúne la wiki de onboarding, las especificaciones de diseño y las notas de implementación con buscador integrado y navegación por pestañas.

Toda la documentación fuente se encuentra centralizada en la carpeta [`documentacion/`](documentacion/).

### Cómo levantar la documentación localmente

Para iniciar el servidor local con recarga en vivo (*live reload*):

```bash
uv run mkdocs serve
```

Una vez levantado, abre en tu navegador:
 **[http://localhost:8000](http://localhost:8000)** (o `http://127.0.0.1:8000`)

Cualquier cambio que realices en los archivos de `documentacion/` se actualizará automáticamente en pantalla.

### Compilar el sitio estático

Para generar la versión HTML estática del sitio (se crea en la carpeta `site/`):

```bash
uv run mkdocs build
```

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
