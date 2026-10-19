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

En la [Wiki de Onboarding](wiki/00-index.md) hay
una versión navegable estructurada paso a paso.

## Estructura del repositorio

```
.
├── src/
│   ├── lexer.l                                      # analizador léxico
│   ├── bison.y                                      # analizador sintáctico
│   ├── ast.h / ast.c                                # interfaz e implementación del AST
│   ├── types.h                                      # enumerados 
│   ├── symbol_table.h / symbol_table.c              # gestión de scopes y symbols
│   ├── type_checker.h / type_checker.c              # inferencia, chequeo y compatibilidad de tipos
│   ├── semantic_analyzer.h / semantic_analyzer.c    # recorrido semántico y validaciones del lenguaje
│   ├── intermediate_code.h / intermediate_code.c    # generador de código de tres direcciones
│   ├── string_builder.h / string_builder.c          # buffer dinámico de cadenas para la salida TAC
│   └── error_handler.h / error_handler.c            # reporte y conteo de errores léxicos y semánticos
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
4. **Compila y enlaza:** Invoca `gcc` con las rutas de inclusión `-Isrc -Ibuild`, compilando los dos archivos generados junto con todos los módulos de `src/`:
   - `src/ast.c` (Árbol de Sintaxis Abstracta)
   - `src/symbol_table.c` (Gestión de ámbitos y tabla de símbolos)
   - `src/type_checker.c` (Inferencia, reglas de tipos y coerción)
   - `src/semantic_analyzer.c` (Recorrido y validaciones semánticas)
   - `src/error_handler.c` (Formateo y reporte de errores)
   - `src/string_builder.c` (Buffer dinámico para código intermedio)
   - `src/intermediate_code.c` (Generador de Código de Tres Direcciones - TAC)
5. **Genera el binario final:** Produce el ejecutable `./mi_compilador` en la raíz del repositorio.

### Consideraciones y mantenimiento

- **Recompilación necesaria:** Debe volverse a correr `bash build.sh` tras cualquier cambio realizado en los archivos de `src/`.
- **Cero warnings:** Actualmente la compilación no emite ninguna advertencia (*warnings*). Si aparece alguna, debe corregirse antes de continuar.
- **Incorporación de nuevos módulos:** Si se agrega un nuevo archivo `.c` a `src/`, debe agregarse manualmente a la lista de archivos de la llamada a `gcc` en `build.sh`, y a `test_suite.sh` si los tests unitarios lo requieren.
- **Archivos generados:** El ejecutable `./mi_compilador` y el contenido de `build/` son artefactos generados automáticamente y no se editan a mano.


## Uso

```bash
./mi_compilador examples/programa.txt          # analiza un archivo
echo "void main(){ int x }" | ./mi_compilador  # o desde la entrada estándar
```

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
