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
  `gcc` para producir el ejecutable. Enteros, booleanos y reales de precisión
  simple (SSE), respetando la convención de llamada System V AMD64.
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

## Extensiones sobre el enunciado

Tres partes del compilador **no las pedía el enunciado** y se implementaron
igual. Conviene saberlo antes de leer las fichas, porque cambian el alcance
respecto de lo exigido y aparecen mencionadas en varias de ellas.

### Soporte de `float` y coerción `int`/`float`

La **especificación del lenguaje** (`spec/01-TDS-spec-lenguaje.pdf`) incluye
`float` entre los tipos, y su regla 14 permite las conversiones entre `int` y
`float`. Pero el **alcance de las etapas** definido en
`spec/00-TDS-proyecto.pdf` acotaba la generación de código intermedio y de
código objeto a **tipos enteros y lógicos**, y la descripción de las
asignaciones menciona solo `int` y `boolean`.

O sea que **generar código para `float` no era necesario**: alcanzaba con que el
front-end aceptara el tipo y el back-end lo ignorara.

Conviene separar qué parte era obligatoria y qué parte no:

| Fase | ¿Estaba en el alcance? | Qué hay implementado |
| --- | --- | --- |
| Léxico, AST, tabla de símbolos | **Sí** | `float` es un tipo del lenguaje: reconocerlo y representarlo era obligatorio |
| [Chequeo de tipos (F10)](wiki/04.5-type-checker.md) | **Sí** | La regla 14 de la especificación es una de las catorce que el semántico debe verificar |
| [Código intermedio (F12, F13)](wiki/05-tac-representation.md) | **No** | Operandos `float`, el opcode `TAC_INT_TO_FLOAT` y las cuatro coerciones materializadas |
| [Marco de pila (F17)](wiki/06.5-stack-frame.md) | **No** | Los dos contadores de parámetros por clase de registro |
| [Código objeto (F18)](wiki/06-object-code.md) | **No** | El camino SSE completo: `%xmm`, `movss`, `comiss`, pool de constantes en `.rodata` |

Dicho de otro modo: el front-end tenía que entender `float` de todas formas, y lo
voluntario fue **que ese entendimiento llegara a producir un ejecutable**.

**El argumento de la decisión:** aceptar `float` en el front-end y abandonarlo en
el back-end habría dejado al compilador incoherente con la especificación de su
propio lenguaje. Un programa que declara un `float` habría pasado el análisis
semántico y fallado o generado código incorrecto más abajo.

Dos aclaraciones de alcance que sí quedaron por debajo de la especificación:

- Los reales son de **precisión simple** (4 bytes, IEEE-754). No hay doble
  precisión.
- La implementación **solo ensancha** (`int` → `float`), nunca trunca. La
  especificación habla de coerciones *y truncamientos*; esto es más restrictivo,
  así que rechaza algunos programas que la letra permitiría pero nunca acepta uno
  inválido. Está explicado en [F10](wiki/04.5-type-checker.md).

### Intérprete

La tabla de calificación del enunciado incluye un ítem *Intérprete*, pero no
figura en el cronograma de entregas, y al consultarlo se confirmó que **no hacía
falta implementarlo**.

Se implementó igual, accesible con `-target interp`. Más allá del ítem, resultó
útil por una razón concreta de testing: es una **segunda implementación
independiente** del mismo lenguaje, así que sirve de oráculo contra el generador
de código. Si el intérprete y el ejecutable compilado difieren sobre el mismo
programa, uno de los dos tiene un bug. Lo describe la
[ficha F19](wiki/07-interpreter.md).

### El valor `sem` de `-target`

El enunciado enumera `scan`, `parse`, `codinter` y `assembly`, pero también lista
la extensión `.sem` entre las salidas posibles. Se agregó `-target sem` porque de
otro modo la etapa de análisis semántico no era alcanzable y esa extensión no se
podía producir.

---

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
├── tests/                # tests unitarios con Unity, una suite por módulo
├── examples/             # programas .ctds de ejemplo, válidos e inválidos
├── documentacion/        # las páginas de este sitio
├── build/                # artefactos de Flex, Bison y los binarios de test
├── build.sh              # compilar el proyecto
├── test_suite.sh         # correr las diez suites de tests
└── mkdocs.yml            # configuración de este sitio
```

El pipeline está modularizado por fase, y cada módulo tiene su cabecera en
`include/` y su implementación en `src/`:

| Fase | Módulos |
| --- | --- |
| Léxico y sintáctico | `lexer.l`, `bison.y`, `parser_helpers` |
| AST | `ast`, `types` |
| Semántico | `symbol_table`, `type_checker`, `semantic_analyzer` |
| Código intermedio | `tac_operation`, `tac_operand`, `tac_instruction`, `tac_list`, `intermediate_code` |
| Optimización | `optimizer`, `live_variable_analysis` |
| Código objeto | `asm_instruction`, `function_frame`, `object_code` |
| Intérprete | `interpreter` |
| Infraestructura | `cli`, `driver`, `main`, `error_handler`, `string_builder`, `debug` |

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

### `-0.0` no se puede producir

`emitUnaryMinus` niega un real como `0 - x`, y en IEEE-754 `0.0 - 0.0` da
`+0.0`. Negar el cero devuelve cero positivo.

- **Cómo se detectó:** leyendo la emisión del unario al inventariar las
  funciones de la ficha F18, y verificado ejecutando: con `z = 0.0` y `n = -z`,
  la comparación `1.0 / n > 0.0` da verdadero, o sea que quedó `+inf`.
- **No afecta a ninguna operación aritmética del lenguaje**, porque `+0.0` y
  `-0.0` son iguales para todas las comparaciones de C-TDS.
- **Alternativa:** invertir el **bit de signo** en lugar de restar, con un
  `xorps` contra una máscara `0x80000000` guardada en `.rodata`. Es una
  instrucción y da el resultado exacto, incluido el cero negativo.

### Los errores de ejecución del intérprete se reportan como semánticos

Un `10 / 0` interpretado sale como
`Error semantico en la linea 0: Division por cero.`

- **Cómo se detectó:** probando el intérprete contra programas con división por
  cero, al usarlo como oráculo de testing.
- **No es un error semántico y no tiene número de línea**, porque
  `evaluateBinaryOperator` recibe los valores y no el nodo del AST. El **código
  de salida sí es correcto**: `-target interp` devuelve 1, porque el driver
  consulta `semantic_errors` al terminar.
- **Alternativa:** un contador propio de errores de ejecución en el intérprete,
  con su función de reporte, y pasar el `line` del nodo hasta la evaluación de
  la operación. Son dos parámetros más en tres funciones.
- **Por qué no se hizo:** el intérprete es una extensión fuera de la entrega, el
  mensaje igual sale por `stderr` con el texto correcto y el código de salida ya
  distingue éxito de fracaso. Queda anotado como deuda consciente.

### `emitCondition` es código muerto

En `intermediate_code.c` hay un emisor declarado, definido y **sin ningún
llamador**.

- **Cómo se detectó:** GCC con `-Wall` lo avisa
  (`'emitCondition' defined but not used`) al revisar la capa de emisores del
  TAC.
- **Alternativa:** o se completa la sustitución —`generateOperatorCode` hace a
  mano lo que la función encapsula— o se elimina. Las dos son correctas; lo que
  no conviene es dejar el aviso del compilador encendido, porque entrena a
  ignorarlos.

### Los símbolos provisorios del parser quedan huérfanos

El parser crea un `Symbol` provisorio por cada aparición de un identificador, y
el análisis semántico sobrescribe el puntero al resolverlo contra la tabla **sin
liberar el provisorio**.

- **Cómo se detectó:** con **ASan**. Medido: `void main(){ }` pierde 6
  asignaciones; con una asignación en el programa, 8; con cuatro, 14.
- **Es acotado** —no crece con la ejecución, solo con el tamaño del programa— y
  el sistema operativo lo recupera al terminar, así que no afecta a la
  corrección de ninguna compilación.
- **Alternativa:** liberar el símbolo provisorio antes de reemplazar el puntero,
  en los dos lugares donde eso pasa: `analyzeAssignmentNode` en
  `semantic_analyzer.c` y `analyzeIdentifier` en `type_checker.c`. Está
  explicado en la [ficha F07](wiki/02.7-parser-actions.md).

### Las tres optimizaciones de mirilla sobre assembly

`peephole` hoy actúa sobre el TAC (saltos redundantes); los tres patrones
pensados sobre el assembly emitido —pares `movl` redundantes, `movl $0`
acortable a `xorl`— no están implementados.

- **Cómo se detectó:** al cerrar el catálogo de optimizaciones contra la lista
  acordada.
- **Alternativa:** trabajar sobre `asm_instruction` **antes** de imprimirlo, no
  sobre el texto ya emitido. El modelo de instrucciones existe justamente para
  eso: una pasada sobre la secuencia de `AsmInstruction` puede reconocer los
  patrones de forma estructural, mientras que hacerlo sobre el texto obligaría a
  parsear de vuelta lo que se acaba de generar. Está en la
  [ficha F15](wiki/05.9-optimizer.md).
