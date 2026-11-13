# C-TDS Compiler

Compilador desarrollado como proyecto de la materia Taller de Diseño de Software (Cod. 3306)

El proyecto se aborda de forma incremental: cada entrega agrega una fase al
compilador. Hoy el camino está completo de punta a punta, desde el texto fuente
hasta un ejecutable x86-64, con un optimizador sobre el código intermedio.

## Autores

- Agustín Alieni
- Francisco Natale
- Julián Varea

## Estado actual

- **Análisis léxico**: reconoce todos los tokens válidos del lenguaje
- **Análisis sintáctico**: reconoce la gramática completa del lenguaje
- **AST**: las acciones semánticas de Bison construyen el árbol durante el parseo
- **Tabla de símbolos y ámbitos**:
  gestión de la jerarquía de `Scope`s (global, funciones, bloques), soporte de
  shadowing y asignación de identificadores únicos (`uid`) para cada entidad declarada.
- **Chequeo de tipos**:
  inferencia de tipos de expresiones, compatibilidad en asignaciones,
  coerción de `int` a `float`, y validación de llamadas a métodos.
- **Análisis semántico**:
  recorrido sobre el AST que valida identificadores no declarados o duplicados en el
  mismo ámbito, condiciones booleanas en `if`/`while`, consistencia en retornos
  (`return;` vs `return expr;`), firmas de métodos y la presencia del método `main()`.
- **Código intermedio (TAC)**:
  transforma el AST validado en instrucciones lineales de tres direcciones,
  representadas como cuádruplas con operandos tipados. Incluye evaluación de
  corto circuito para `&&` y `||`.
- **Código objeto x86-64**:
  traduce el TAC a assembly de GNU Assembler (sintaxis AT&T) y lo ensambla con
  `gcc` para producir el ejecutable. Enteros y booleanos, respetando la
  convención de llamada System V AMD64 (ver *Alcance de `float`*).
- **Optimizador**:
  siete optimizaciones sobre el TAC, cada una activable por separado con
  `-opt <nombre>` o todas juntas con `-opt all`: plegado de constantes
  (`const-fold`), propagación de constantes (`const-prop`), eliminación de
  código inalcanzable y de asignaciones muertas (`dead-code`), simplificación de
  condiciones constantes (`const-branch`), eliminación de autoasignaciones
  (`self-assign`), mirilla sobre instrucciones contiguas (`peephole`) y
  reducción de fuerza en multiplicaciones (`strength-reduce`). La eliminación de
  asignaciones muertas se apoya en un análisis de variables vivas sobre el grafo
  de flujo de control.

---

## Alcance de `float`

La **especificación del lenguaje** (`spec/01-TDS-spec-lenguaje.pdf`) incluye
`float` entre los tipos de C-TDS, y su regla 14 permite las conversiones entre
`int` y `float`. El **enunciado del proyecto** (`spec/00-TDS-proyecto.pdf`), en
cambio, acota las etapas de **código intermedio y código objeto** a **tipos
enteros y lógicos**, y la descripción de las asignaciones menciona solo `int` y
`boolean`.

El compilador respeta las dos cosas, y el corte está exactamente en la frontera
entre ellas:

| Fase | ¿Cubre `float`? |
| --- | --- |
| Léxico, sintáctico, [AST](wiki/03-ast.md), [tabla de símbolos](wiki/04-symbol-table.md) | **Sí.** Es un tipo del lenguaje: se reconoce y se representa |
| [Chequeo de tipos (F10)](wiki/04.5-type-checker.md) | **Sí.** La regla 14 es una de las catorce que el análisis semántico debe verificar |
| [Código intermedio (F13)](wiki/05.5-tac-generation.md), optimizador, [código objeto](wiki/06-object-code.md) | **No.** Un programa con `float` se rechaza acá, con su línea |

```
$ ./mi_compilador programa.ctds
Error en la linea 2: la generacion de codigo para el tipo 'float' no esta
soportada: esta etapa cubre tipos enteros y logicos.
```

Hasta `-target sem` el programa se acepta sin errores: el front-end lo entiende
entero, incluida la coerción de la regla 14. Lo que no está en alcance es
**generarle código**, y el rechazo ocurre en la primera fase que lo haría. El
detalle de cómo se implementa está en §9 de la
[ficha F13](wiki/05.5-tac-generation.md).

**Por qué rechazar y no ignorar.** Si el generador emitiera las operaciones de
todos modos, el código objeto trataría el patrón de bits IEEE-754 como si fuera
un entero: `1.0 + 1.0` se compilaría a una suma entera de dos patrones de bits y
no daría `2.0`, **sin una sola queja**. Un error explícito con su línea es
siempre mejor que un ejecutable que calcula mal.

## Extensión sobre el enunciado: el valor `sem` de `-target`

El enunciado enumera `scan`, `parse`, `codinter` y `assembly`, pero también
lista la extensión `.sem` entre las salidas posibles. Se agregó `-target sem`
porque de otro modo la etapa de análisis semántico no era alcanzable y esa
extensión no se podía producir.

## Por dónde empezar

- Si lo que querés es **compilar el proyecto y correrlo**, la guía de comandos
  está en [Primeros Pasos](wiki/00.5-getting-started.md): prerrequisitos, build,
  tests, opciones de la línea de comandos y prueba de humo.
- Si lo que querés es **entender cómo está hecho**, el recorrido es la
  [Wiki de Onboarding](wiki/00-index.md), que va de la visión general del
  compilador hasta cada fase en el orden en que se ejecutan.
- La sección **Documentación Técnica** del menú es material de consulta puntual
  (decisiones de diseño del AST, alcance del análisis semántico, el conflicto de
  Bison, el algoritmo de Sethi-Ullman), no está pensada para leerse de corrido.
- **Convenciones del Código** explica por qué los archivos están organizados como
  están: la metáfora del periódico, la regla del paso descendente y el formato de
  los comentarios. Es la respuesta a "¿por qué este archivo se lee de arriba
  hacia abajo de lo general a lo particular?", que es un criterio que las fichas
  invocan seguido.
- **Bugs Resueltos** son cuatro bugs reales del proyecto, cada uno con el programa
  que lo disparó, la traza y la corrección. Están ahí porque el *cómo se
  encontró* es tan parte del proyecto como el código final.
- [**Cambios a etapas anteriores**](cambios-etapas-anteriores.md) junta todo lo
  que se tocó de una fase ya entregada mientras se trabajaba en una posterior,
  con el motivo de cada cambio.

## Estructura del repositorio

```
.
├── include/              # archivos de cabecera (.h), uno por módulo
├── src/                  # implementaciones (.c), más lexer.l y bison.y
├── tests/                # tests con Unity: una suite por etapa más una de integración
├── examples/             # programas .ctds de ejemplo, válidos e inválidos
├── documentacion/        # las páginas de este sitio
├── build/                # artefactos de Flex, Bison y los binarios de test
├── build.sh              # compilar el proyecto
├── test_suite.sh         # correr las once suites de tests
└── mkdocs.yml            # configuración de este sitio
```

El pipeline está modularizado por fase, con cabeceras en `include/` e
implementaciones en subdirectorios de `src/`:

| Fase | Carpeta en `src/` | Módulos |
| --- | --- | --- |
| Léxico, sintáctico y AST | `src/parser/` | `lexer.l`, `bison.y`, `parser_helpers`, `ast` (`types.h` en `include/`) |
| Semántico | `src/semantic/` | `symbol_table`, `type_checker`, `semantic_analyzer` |
| Código intermedio | `src/intermediate_code/` | `tac`, `intermediate_code` |
| Optimización | `src/optimizer/` | `optimizer`, `live_variable_analysis` |
| Código objeto | `src/object_code/` | `function_frame`, `object_code` |
| Infraestructura | `src/core/` y `src/main.c` | `cli`, `driver`, `main`, `error_handler`, `string_builder`, `debug` |

`bison.tab.c`, `lex.yy.c`, el contenido de `build/` y el ejecutable
`mi_compilador` son artefactos generados y no se editan a mano.

## Problemas conocidos

El enunciado pide que toda deficiencia detectada se discuta **y se proponga una
alternativa**, así que cada punto lleva cómo se detectó y cómo se arreglaría.

### Un método solo ve a los declarados antes que él

No hay declaración adelantada en C-TDS, así que `main(){ g(); }` con `g`
definido más abajo da `Método no declarado`.

- **Cómo se detectó:** escribiendo programas de ejemplo con los métodos en otro
  orden.
- **Es lo que pide la letra** ("todo se declara antes de usarse"), así que no es
  un bug; conviene tenerlo presente al escribir casos de test.
- **Alternativa, si alguna vez se quisiera permitir:** una **pasada previa** que
  recorra los hijos de la raíz y registre todos los `METHOD_DECLARATION_NODE` en
  el ámbito global antes de analizar los cuerpos. Son pocas líneas en
  `analyzeSemantics` y no afecta a ninguna fase posterior, porque el TAC ya se
  genera recorriendo los métodos en orden y las llamadas se resuelven por
  nombre.

### El plegado de constantes desborda con signo en `+`, `-` y `*`

Al plegar `2147483647 * 3` el cálculo se hace en `int`, lo que en C es
comportamiento indefinido.

- **Cómo se detectó:** compilando el proyecto con **UBSan** y corriendo la
  batería de ejemplos.
- **Hoy no se nota:** GCC envuelve en silencio y el resultado coincide con el
  que produciría el assembly generado. Formalmente no está garantizado.
- **La división y el módulo sí están protegidos**, tanto contra el divisor cero
  como contra el único desborde posible (`INT_MIN / -1`).
- **Alternativa:** la misma forma que ya usan `/` y `%`. `evaluateBinary`
  devuelve `bool`, y devolver `false` significa "no se puede plegar": alcanza
  con verificar el desborde **antes** de operar —con los built-ins
  `__builtin_add_overflow` y compañía, o comparando contra `INT_MAX`/`INT_MIN`—
  y dejar la instrucción sin plegar cuando desbordaría. El programa compilado
  haría entonces la cuenta en ejecución, que es exactamente lo que haría sin el
  optimizador.

### El AST y los ámbitos no se liberan al terminar

El driver no llama a `freeAST` ni a `freeScope`: el árbol y la tabla de
símbolos viven hasta que termina el proceso.

- **Cómo se detectó:** con **valgrind**, que reporta un único bloque perdido
  por compilación (el árbol completo con sus ámbitos).
- **Es deliberado y acotado:** el compilador procesa un archivo y termina, así
  que el sistema operativo recupera todo. No afecta a la corrección.
- **Alternativa:** liberar el AST y el ámbito global al final de `compile`.
  Antes hay que resolver el doble dueño de los símbolos de declaración —hoy los
  libera tanto `freeAST` como `freeScope`—, por ejemplo haciendo que los ámbitos
  solo guarden punteros prestados.

La fuga que sí crecía con el programa, la de los símbolos provisorios que el
parser crea por cada referencia, **está resuelta**: el semántico los reemplaza
con `resolveNodeSymbol`, que los libera (ver la [ficha F08](wiki/03-ast.md)).

### Las tres optimizaciones de mirilla sobre assembly

`peephole` actúa sobre el TAC (saltos redundantes) y en el generador de código
objeto fusiona una comparación con el salto condicional siguiente. Los tres
patrones pensados sobre el assembly emitido —pares `movl` redundantes, `movl $0`
acortable a `xorl`, recargar un valor que ya está en el registro— no están
implementados.

- **Cómo se detectó:** al cerrar el catálogo de optimizaciones contra la lista
  acordada.
- **Alternativa:** el assembly se emite como texto directo con `buildStringf`,
  así que hacerlo sobre la salida obligaría a parsear de vuelta lo que se acaba
  de generar. Lo razonable es introducir una representación de instrucciones de
  máquina (opcode y operandos) entre el TAC y el texto, aplicar los patrones
  sobre esa secuencia y recién después imprimir. Está en la
  [ficha F15](wiki/05.9-optimizer.md).
