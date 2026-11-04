# Plan de Documentación para la Defensa

Documento de trabajo para cubrir **todo el repositorio** con documentación
orientada a la defensa del proyecto. No es una página del sitio: vive en la raíz
y no se publica.

## Para qué sirve este documento

La defensa son dos partes: un **escrito de una hora** sobre un concepto
teórico/práctico y alguna extensión posible, y una **oral**. Las preguntas
esperables son de dos tipos:

1. **"¿Qué hace esta parte del código?"** — hay que poder explicar cualquier
   archivo del repositorio, qué resuelve, con qué estructura de datos y por qué
   se eligió así.
2. **"¿Cómo extenderías X?"** — por ejemplo agregar un `for each`. Hay que saber
   qué archivos se tocan, en qué orden y qué cambia en cada fase.

Entonces la documentación tiene que servir para **estudiar**, no solo para
entregar. El criterio es: leyendo las fichas en orden, se entiende el compilador
completo sin tener que abrir el código, y queda claro dónde se mete mano para
cambiarlo.

## Cómo trabajamos esto

Una ficha por vez, en el orden del inventario. Para cada una:

- Decime **"documentá la ficha Fnn"**.
- Yo leo los archivos de esa ficha, escribo la página siguiendo la plantilla de
  abajo, y te paso los archivos tocados y el nombre del commit.
- Marcás el checkbox acá y seguimos con la siguiente.

Las páginas van en `documentacion/` y hay que agregarlas al `nav:` de
`mkdocs.yml`, si no quedan invisibles en el sitio. Eso lo hago yo en el mismo
commit de cada ficha.

El orden importa y no es alfabético: **primero la infraestructura** (lo que no
pertenece a ninguna fase pero todas usan), **después cada fase en el orden en
que se ejecuta**. Así cada ficha solo depende de las anteriores.

---

## Plantilla de cada ficha

Toda página de ficha responde estas ocho preguntas, en este orden. Las dos
últimas son las que sirven para el escrito y hoy no están en ninguna parte.

1. **Qué problema resuelve**, en una oración. Si no se puede decir en una
   oración, la ficha está mal partida.
2. **Interfaz pública**: qué expone el `.h`, quién lo llama y a quién llama. La
   frontera del módulo.
3. **Estructuras de datos**: cada campo, su tipo y **por qué existe**. Los
   campos que parecen redundantes son justo los que se preguntan.
4. **El algoritmo central**: el recorrido o la decisión no trivial, explicada con
   un **ejemplo chico de entrada → salida** (programa C-TDS de 3 líneas y lo que
   produce). Un ejemplo concreto vale más que tres párrafos.
5. **Conexión con las fases vecinas**: qué consume de la anterior y qué entrega a
   la siguiente, con el formato exacto del dato que cruza la frontera.
6. **La teoría detrás**: qué concepto de la materia implementa y **cómo se llama
   en la bibliografía** (análisis descendente/ascendente, LALR(1), tabla de
   símbolos jerárquica, código de tres direcciones, grafo de flujo de control,
   análisis de flujo de datos, asignación de registros, convención de llamada).
   Esto es lo que se pregunta en el escrito teórico, y es lo que la
   documentación actual casi no dice: explica el *cómo* y no el *qué concepto es*.
7. **Cómo se extiende**: qué habría que tocar **en este archivo** para agregar una
   construcción nueva, con un caso concreto. Es la media pregunta de la defensa.
8. **Trampas y decisiones contraintuitivas**: lo que parece un bug y no lo es, lo
   que se intentó de otra forma y falló, y los límites conocidos.

---

## Inventario completo

Los **44 archivos** de `include/` y `src/` están repartidos en 19 fichas, cada
archivo en exactamente una. Verificado por diferencia de conjuntos contra el
contenido real de los directorios: no sobra ni falta ninguno.

Los tests (`tests/`, 10 suites, 6703 líneas) quedan fuera a propósito: no se
preguntan y documentarlos no ayuda a estudiar. Lo que sí conviene es que cada
ficha mencione **qué suite la cubre**, para poder decir en la oral cómo se
validó esa fase.

La columna *Líneas* es el tamaño real de los archivos de la ficha, para
dimensionar el trabajo y detectar dónde está la complejidad.

### Bloque 0 — Infraestructura

No es ninguna fase del compilador, pero todas las fases la usan. Va primero
porque es lo que explica **cómo se orquesta todo lo demás**.

- [ ] **F01 — Entrada y línea de comandos** · `main.c`, `cli.h`, `cli.c` · 500 líneas
  - El contrato de la CLI del enunciado, el parseo de argumentos, la deducción
    de la extensión de salida por etapa y la constante `CURRENT_TARGET`.
- [ ] **F02 — Orquestación de las etapas** · `driver.h`, `driver.c` · 325 líneas
  - El `switch` sobre `-target` que decide hasta dónde compilar, el corte
    temprano ante errores, y el ensamblado/enlace final con `gcc`.
- [ ] **F03 — Errores y salida de debug** · `error_handler.h`, `error_handler.c`, `debug.h` · 144 líneas
  - Los tres niveles de error y sus contadores, por qué el sintáctico aborta al
    primero y el semántico acumula, y la convención de `-debug`.
- [ ] **F04 — Utilidades y tipos base** · `string_builder.h`, `string_builder.c`, `types.h` · 164 líneas
  - El buffer dinámico con el que se arma la salida de cada etapa, y los
    enumerados (`DataType`, `OperationType`) que atraviesan todo el compilador.

### Bloque 1 — Análisis léxico y sintáctico

- [x] **F05 — Analizador léxico** · `lexer.l` · 78 líneas
  - Las expresiones regulares de cada token, el tratamiento de comentarios y el
    conteo de líneas. Teoría: autómatas finitos, de ER a AFD.
  - **Hecha:** `documentacion/wiki/02-lexical-analysis.md`. Incluye el inventario
    completo de reglas, el desempate de Flex (match más largo antes que orden)
    con los casos verificados, el autómata de los comentarios, la cadena de
    propiedad de la memoria de los identificadores y cuatro extensiones.
  - De paso se corrigió un error de la página anterior: decía que un
    identificador podía empezar con `_`, y `_foo` es error léxico.
- [x] **F06 — Gramática y parser** · `bison.y` · 171 líneas
  - La gramática de C-TDS, precedencia y asociatividad de operadores, y el
    conflicto LALR(1) que hubo que resolver. Teoría: análisis ascendente,
    LALR(1), conflictos shift/reduce.
  - **Hecha:** `documentacion/wiki/02.5-syntax-analysis.md`. Incluye el inventario
    de las 15 producciones, la tabla de precedencias contrastada con la
    especificación y verificada con nueve expresiones ejecutadas, el pseudo-token
    `UMINUS`, el conflicto histórico variable/método y su resolución, y por qué
    **no existe el problema del `else` colgante** en esta gramática.
  - Se confirmó con `bison -Wall` que la gramática tiene **0 conflictos y 0
    warnings**; el comando quedó documentado en la página como chequeo de
    regresión.
- [x] **F07 — Acciones del parser** · `parser_helpers.h`, `parser_helpers.c` · 193 líneas
  - Las funciones que las acciones semánticas de Bison llaman para construir el
    árbol durante las reducciones.
  - **Hecha:** `documentacion/wiki/02.7-parser-actions.md`. Las seis funciones,
    el convenio del `NULL` que permite que ninguna acción de la gramática tenga
    un `if`, la transferencia de propiedad en O(1) de `resolveNodeArray`, y por
    qué `int a, b, c;` necesita dos pasos (los `ID_NODE` provisorios con
    `TYPE_VOID` y el transplante del símbolo).
  - **Corrección al inventario:** este archivo **no** contiene `newNode`,
    `newMethodDeclaration`, `newOperatorNode` ni los `new*LiteralNode`. Están en
    `ast.c`, así que pertenecen a F08. El esqueleto previo los listaba acá por
    error.

> **Bloque 1 completo.** La página que cubría estas tres fichas juntas tenía
> **39 líneas**; ahora son 452 + 586 + 432. Era el hueco más grande de la
> documentación y la fase con más teoría pura, así que se arrancó por acá.

### Bloque 2 — Árbol de sintaxis abstracta

- [x] **F08 — El AST** · `ast.h`, `ast.c` · 350 líneas
  - `NodeAST`, el layout de hijos de cada tipo de nodo, las macros de acceso
    (`GET_CONDITION`, `GET_LEFT`…) y por qué se eligió un nodo genérico con
    arreglo de hijos en vez de un nodo por construcción.
  - **Hecha:** `documentacion/wiki/03-ast.md`, reescrita. Lo que agrega sobre la
    versión anterior: la **tabla completa de layout de hijos** de los 12 tipos de
    nodo verificada contra `bison.y` y la salida real, la compactación de hijos
    de `newNode` y por qué es carga estructural (el cuerpo del `while` aterriza
    en el índice 1 justamente porque el hijo del medio es `NULL`), la regla de
    propiedad de símbolos de `freeAST` basada en `uid == 0`, y la heurística del
    campo `line` con la verificación de su asimetría.
  - **Correcciones a documentación previa:** la página anterior documentaba un
    `BINARYOPERATOR_NODE` que **no existe** en el código; el enum tiene
    `OPERATOR_NODE`, que cubre unarios y binarios. El mismo nombre inexistente
    aparece todavía en `wiki/05-intermediate-code.md` y en `CLAUDE.md`.
  - Se confirmó que **`freeAST` nunca se llama sobre el árbol completo**: las
    únicas llamadas están en `parser_helpers.c` y son caminos de falta de
    memoria. El AST vive hasta que termina el proceso.

### Bloque 3 — Análisis semántico

- [ ] **F09 — Tabla de símbolos y ámbitos** · `symbol_table.h`, `symbol_table.c` · 358 líneas
  - La jerarquía de `Scope`s, la búsqueda de adentro hacia afuera, el
    ocultamiento (shadowing) y los `uid`. Teoría: tabla de símbolos jerárquica.
- [ ] **F10 — Chequeo de tipos** · `type_checker.h`, `type_checker.c` · 257 líneas
  - Inferencia del tipo de cada expresión, compatibilidad en asignaciones, la
    coerción `int`/`float` y por qué `==` es más estricto que `<` y `>`.
- [ ] **F11 — Recorrido semántico** · `semantic_analyzer.h`, `semantic_analyzer.c` · 328 líneas
  - Las reglas semánticas del lenguaje, el registro de métodos antes de analizar
    el cuerpo (que es lo que habilita la recursión) y `guaranteesReturn`.

### Bloque 4 — Código intermedio

- [ ] **F12 — La representación intermedia** · `tac_operation.h`, `tac_operand.h`, `tac_operand.c`, `tac_instruction.h`, `tac_instruction.c`, `tac_list.h`, `tac_list.c` · 659 líneas
  - **Qué es una cuádrupla** y por qué se migró de texto plano a una estructura:
    `{operación, resultado, operando1, operando2}` con operandos tipados. Es una
    de las decisiones de diseño más defendibles del proyecto y conviene tenerla
    muy clara.
- [ ] **F13 — Generación del TAC** · `intermediate_code.h`, `intermediate_code.c` · 403 líneas
  - El recorrido del AST, la generación de temporales y etiquetas, el **corto
    circuito** de `&&` y `||`, y por qué las etiquetas se resuelven en una sola
    pasada sin backpatching. Teoría: código de tres direcciones.

### Bloque 5 — Optimización

- [ ] **F14 — Grafo de flujo y variables vivas** · `live_variable_analysis.h`, `live_variable_analysis.c` · 481 líneas
  - Construcción de bloques básicos, el CFG, y las ecuaciones de vivacidad
    resueltas hasta el punto fijo. Teoría: análisis de flujo de datos,
    `LIVE_IN`/`LIVE_OUT`, análisis hacia atrás, por qué converge.
- [ ] **F15 — El optimizador** · `optimizer.h`, `optimizer.c` · 839 líneas
  - Las siete optimizaciones, el bucle de punto fijo que las repite, el orden en
    que se aplican y por qué, y qué garantiza que preservan la semántica.
  - Ya existe `optimizaciones.md` en la raíz con el catálogo completo: esta ficha
    lo **mueve a `documentacion/`** y lo suma al `nav:`, en vez de reescribirlo.

### Bloque 6 — Código objeto

- [ ] **F16 — Instrucciones x86-64** · `asm_instruction.h`, `asm_instruction.c` · 391 líneas
  - La representación de una instrucción de assembly y el repertorio usado.
- [ ] **F17 — Marco de pila** · `function_frame.h`, `function_frame.c` · 263 líneas
  - Asignación de un slot por variable, parámetro y temporal; el cálculo del
    tamaño del frame y la alineación a 16 bytes.
- [ ] **F18 — Generador de código objeto** · `object_code.h`, `object_code.c` · 1072 líneas
  - La traducción de cada instrucción TAC, la **convención de llamada System V
    AMD64**, el caso de `idivl` con `cltd`, el camino SSE para `float` y la
    semántica de NaN en las comparaciones. El archivo más grande del proyecto.

### Bloque 7 — Intérprete

- [ ] **F19 — Intérprete sobre el AST** · `interpreter.h`, `interpreter.c` · 535 líneas
  - Evaluación directa del árbol, el manejo de ámbitos en tiempo de ejecución y
    por qué sirve como oráculo contra el backend. Es una extensión nuestra.

---

## Catálogo de extensiones

La otra mitad de la defensa. Para cada extensión, los archivos que se tocan **en
orden de pipeline**. Conviene poder recitar esta tabla: es la respuesta
estructural a cualquier "¿cómo agregarías…?", incluso para una construcción que
no esté en la lista.

| Extensión | Léxico | Gramática/AST | Semántico | TAC | Código objeto |
| --- | --- | --- | --- | --- | --- |
| `for` / `for each` | token `for` | regla nueva + nodo (o azúcar sobre `while`) | validar la variable de control y su alcance | ¿se desazucara a `while` o lleva opcodes propios? | nada nuevo si se desazucara |
| `do while` | token `do` | regla + nodo | igual que `while` | condición **al final** del cuerpo | sin cambios |
| `break` / `continue` | dos tokens | nodos nuevos | error si están fuera de un ciclo | pila de etiquetas de ciclo para saber a dónde saltar | sin cambios |
| `!=`, `<=`, `>=` | tres tokens | precedencia igual a `==`/`<` | mismas reglas de tipos | opcodes nuevos, o negar el existente | `setne`/`setle`/`setge` |
| Operador ternario | token `?` y `:` | regla + nodo | los dos brazos deben tipar igual | etiquetas, igual que un `if` que deja valor | sin cambios |
| Arrays | `[` `]` | nodo de indexación | chequeo de tipo del índice y del elemento | direccionamiento: `base + i * tamaño` | modos de direccionamiento indexado |
| Tipo nuevo (`char`, `double`) | literales | `DataType` nuevo | reglas de coerción | operandos y tamaños | registros y tamaño de slot |
| `print` / salida | token | nodo de llamada | firma fija | `call` a una rutina externa | enlazar con `printf` de libc |
| Paso por referencia | — | marca en el parámetro | el argumento debe ser asignable | pasar dirección en vez de valor | `leaq` en el llamador, indirección en el llamado |
| Optimización nueva | — | — | — | pasada nueva + flag en `OptimizationSet` | — |

Para cada fila, las preguntas que hay que poder contestar son siempre las
mismas: **¿hace falta un nodo nuevo en el AST o alcanza con azúcar sintáctica
sobre uno existente?**, **¿aparece algún error semántico nuevo?**, **¿hacen
falta opcodes de TAC nuevos?** y **¿cambia el marco de pila?**.

---

## Estado de la documentación existente

Lo que ya hay, para no reescribir de cero:

| Página | Líneas | Qué le falta para servir de estudio |
| --- | --- | --- |
| `wiki/01-overview.md` | 619 | Es la más completa. Revisar que el pipeline incluya optimizador e intérprete |
| `wiki/06-object-code.md` | 370 | Cubre F16–F18. Falta la teoría de la convención de llamada |
| `wiki/04-semantic-analysis.md` | 316 | Cubre F09–F11. Falta separar tabla de símbolos de chequeo de tipos |
| `wiki/05-intermediate-code.md` | 285 | Cubre F12–F13. Falta el "qué es una cuádrupla" conceptual |
| `live_variable_analysis.md` | 211 | Cubre F14. Falta por qué el punto fijo converge |
| `wiki/03-ast.md` | 520 | **F08, hecha.** |
| `wiki/02-lexical-analysis.md` | 452 | **F05, hecha.** |
| `wiki/02.5-syntax-analysis.md` | 480 | **F06, hecha.** |
| `wiki/02.7-parser-actions.md` | 40 | F07, esqueleto con la lista de lo que falta |
| — | — | **No hay nada de F01–F04 ni de F19**, y `optimizaciones.md` (F15) está fuera del sitio |

### Pendiente transversal: 22 links rotos a la máquina de Fran

Tres páginas tienen links absolutos de la forma
`file:///home/francisco/compiler/include/symbol_table.h#L16-L20`, que apuntan al
disco de quien las escribió: están muertos para cualquier otro y además exponen
una ruta personal en el sitio publicado.

| Página | Links rotos |
| --- | --- |
| `analisis-semantico.md` | 15 |
| `wiki/04-semantic-analysis.md` | 4 |
| `convenciones_comentarios.md` | 3 |

Como `mkdocs.yml` ya define `repo_url`, el reemplazo natural es el link a GitHub
(`https://github.com/julivarea/compiler/blob/main/include/symbol_table.h#L16-L20`),
que además conserva el ancla de líneas. Conviene hacerlo junto con las fichas
F09–F11, que son las dueñas de esas páginas.

### Pendiente de código: fuga de los símbolos provisorios

Encontrado al documentar F07, medido con ASan. El parser crea un `Symbol`
provisorio para cada aparición de un identificador, y el análisis semántico
sobreescribe el puntero al resolverlo contra la tabla de símbolos **sin liberar
el provisorio**:

- `src/semantic_analyzer.c`, `analyzeAssignmentNode`: `node->symbol = resolvedSymbol;`
- `src/type_checker.c`, `analyzeIdentifier`: `node->symbol = symbol;`

Medición: `void main(){ }` pierde 6 asignaciones (los ámbitos, que no se liberan
al terminar); con una asignación en el programa son 8; con cuatro, 14; con una
asignación más tres usos de un identificador, 13. Es acotado y el sistema
operativo lo recupera al terminar, así que no afecta la corrección.

**Acción:** liberar el símbolo provisorio antes de reemplazar el puntero, en los
dos lugares. Corresponde a las fichas F10 y F11; queda anotado acá para no
perderlo. Está documentado en §10 de `wiki/02.7-parser-actions.md`.

Además hay siete páginas en `documentacion/` que no están en el `nav:` y por eso
no se ven: las cuatro de `bugs/`, `convenciones_comentarios.md`, `refactor.md` y
`notas/algoritmo_sethi_ullman.md`. Las de `bugs/` son trazas de bugs reales y
cómo se corrigieron, que es material de oral ya escrito. Sumarlas al `nav:` es
una línea cada una.

---

## Orden sugerido para empezar

No el del inventario, sino el de mayor rendimiento para la defensa:

1. **F05–F07** (léxico y sintáctico). Es el hueco más grande *y* la fase con más
   teoría pura, que es lo que entra en el escrito.
2. **F01–F02** (CLI y driver). Son los que explican cómo se arma todo; sin esto
   no se puede contestar "¿qué pasa cuando corrés el compilador?".
3. **F15** (optimizador), que es casi gratis: mover `optimizaciones.md` al sitio.
4. El resto de los bloques en orden.
