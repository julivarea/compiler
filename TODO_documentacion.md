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

## Aclaraciones de alcance en el sitio — HECHO

El enunciado pide documentar las **extensiones**, y había un hueco: la aclaración
de que `float` no era obligatorio existía **solo en el `README.md`**, que está
fuera del `docs_dir` y por lo tanto fuera del sitio navegable. El wiki hablaba de
reales en cinco fichas (23 menciones en F10, 20 en F18, 9 en F17, 8 en F12, 7 en
F13) sin aclararlo en ninguna.

Resuelto así:

- **`documentacion/index.md`** tiene ahora una sección *Extensiones sobre el
  enunciado* con el argumento completo de los tres ítems (`float`, intérprete,
  `-target sem`), espejando la del README. Es el lugar canónico dentro del sitio.
- **F10, F12, F13, F17 y F18** llevan una nota al principio que remite ahí.

**Precisión que hubo que corregir sobre la marcha:** la primera versión de la
nota decía que el enunciado acotaba "el alcance de esta etapa" en las cinco
fichas por igual. Es inexacto. El recorte a tipos enteros y lógicos era sobre las
etapas de **generación** (código intermedio y código objeto); el front-end tenía
que entender `float` de todas formas, y **la regla 14 es una de las catorce que el
análisis semántico debe verificar**. O sea que lo voluntario no fue chequear los
tipos de `float`, sino **que ese chequeo llegara a producir un ejecutable**. La
nota de F10 quedó con el texto correcto y la portada con una tabla que separa qué
estaba en el alcance y qué no. El README se ajustó al mismo criterio.

También se corrigió una afirmación falsa que escribí en F10: decía que el límite
del ensanchamiento quedaba registrado en `documentacion/analisis-semantico.md`.
Esa página menciona la coerción `int`→`float` pero **no** el hecho de que la
implementación solo ensancha y nunca trunca, al contrario de lo que permite la
regla 14. La referencia ahora apunta solo a la portada; si se quiere, es una línea
para agregar a `analisis-semantico.md` cuando se toque.

---

## Al día con el refactor del generador de TAC — HECHO

Después de cerrar el inventario entraron tres commits que dejaron dos fichas
**contradiciendo el código**, no solo con nombres faltantes:

| Commit | Qué cambió | Ficha afectada |
| --- | --- | --- |
| `1d9a143 refactor(ir): semantic emitter helpers` | 17 emisores nuevos en `intermediate_code.c` (24 → 41 funciones), `appendTACInstruction` en `tac_list` | **F13**, **F12** |
| `88becbe simplify statmnet` | No terminal `Assignment` extraído de `Statement` en `bison.y` | **F06** |

Resuelto así:

- **F13** tiene una sección nueva (§4) sobre **la capa de emisores semánticos**:
  por qué existen —una cuádrupla tiene cuatro campos y cada opcode usa solo
  algunos, así que llamar al `emit` genérico obligaba a recordar qué va en cada
  posición—, la tabla de los diecisiete con qué resuelve cada uno, y el contraste
  que mejor lo ilustra: `TAC_LABEL` pone la etiqueta en `result` y `TAC_GOTO` en
  `firstOperand`, asimetría que era fuente de error y que `emitLabel`/`emitGoto`
  eliminan. Se actualizaron los **seis fragmentos de código** que mostraban la
  forma vieja.
- **F12** documenta ahora la separación entre `createTACInstruction` (reserva) y
  `appendTACInstruction` (enlaza), con `appendInstruction` como composición de las
  dos, y por qué el `emit` del generador la aprovecha.
- **F06** refleja el no terminal `Assignment`, y se explicó **para qué sirve**:
  es una asignación sin el punto y coma, que es exactamente la pieza que la
  cabecera de un `for` necesita. La sección de extensión del `for` se actualizó
  para usarla (`FOR '(' Assignment ';' Expression ';' Assignment ')' Block`), que
  es mejor que la forma que tenía antes.

**Hallazgo del refactor: `emitCondition` es código muerto.** Está declarado,
definido y **ningún generador lo llama**. GCC lo confirma con `-Wall`:
`'emitCondition' defined but not used`. Es el segundo aviso del proyecto, junto
al `debugPrintf` de `debug.h`. Queda documentado en F13 §4 con la intención que
se le ve —encapsular "evaluar una comparación y quedarse con el temporal"— y la
nota de que la sustitución no se completó.

**Error propio corregido:** F06 decía "las 15 reglas" y **siempre fueron 16**;
con `Assignment` son **17**. Contado con un script sobre `bison.y`.

Verificado después del refactor: build sin errores, todas las suites en verde, y
la batería de 6 programas correcta sin optimizar, con `-opt all` y cruzada contra
el intérprete.

---

## Inventarios de método de F15 y F18 — HECHO

Las dos fichas más grandes del back-end tenían las secciones de la plantilla
completas pero **no nombraban todas sus funciones**: F15 tenía 11 de 30 y F18
32 de 55. Las dos cerraron en **30/30** y **55/55**.

| Ficha | Sección nueva | Agrupada por |
| --- | --- | --- |
| F15 | §9 Inventario de funciones | públicas · las ocho pasadas · la tabla de constantes · la evaluación · el desenlazado · clasificación de opcodes · el conjunto de etiquetas · la depuración |
| F18 | §10 Inventario de funciones | públicas · el armado del marco · una función por opcode · los auxiliares de emisión · el acceso a operandos · las constantes reales |

Las secciones que venían después se renumeraron (F15: 9→10, 10→11, 11→12; F18:
10→11 … 14→15) y las dos referencias internas a esos números se corrigieron.

### Hallazgos de F15

- **`printTACWithDeletedInRed` no imprime nada en rojo.** El nombre dice *InRed*
  y no hay un solo código ANSI en `optimizer.c`: marca las líneas con el prefijo
  `[ELIMINADA]`. Verificado con `cat -v` sobre la salida de `-debug`. La
  documentación repetía lo que decía el nombre: **dos páginas afirmaban "en
  rojo"** —F15, en cómo se valida, y F12, en la tabla de consumidores del formato
  del TAC— y las dos se corrigieron. El color sí existe, pero en
  `error_handler.c`.
- **`unlinkInstructionList` tiene dos contratos implícitos** que no están
  escritos en el código: el arreglo que recibe **tiene que estar en orden de
  lista** —recorre la lista una sola vez, así que un arreglo desordenado dejaría
  instrucciones sin borrar en silencio— y **libera el arreglo**, incluido el
  camino de salida temprana, así que quien lo llama no debe liberarlo. Las cinco
  pasadas que lo usan cumplen lo primero por construcción.
- **El conjunto de etiquetas referenciadas guarda punteros al `name` de las
  instrucciones, no copias.** Por eso `removeUnreferencedLabels` termina de
  marcar, libera el arreglo de punteros, y recién entonces desenlaza: consultar
  el conjunto después de liberar instrucciones sería leer memoria liberada.
- **`forgetConstant` borra pisando con el último elemento**, no desplazando. Es
  correcto porque la tabla es un conjunto y nadie depende de su orden, y
  convierte un borrado O(n) en O(1).
- **`isGlobalVariable` corta en el primer `begin_func`**, lo que codifica el
  orden de emisión del generador de TAC (globales antes que funciones).
- Quedó documentado el límite ya conocido: las guardas de desborde están en
  `/` y `%` pero **no** en `+`, `-` y `*`.

### Hallazgos de F18

- **Qué operando se le pasa a `isFloatOperand` no es indistinto, y es la clase de
  error que solo se ve con reales.** Las aritméticas miran `result`; las
  comparaciones miran `firstOperand`, porque el resultado de una comparación es
  siempre `boolean` y preguntarle a él nunca diría que la comparación es de
  reales. Las dos reglas quedaron en una tabla.
- **`-0.0` no se puede producir.** `emitUnaryMinus` niega un real como `0 - x`, y
  en IEEE-754 `0.0 - 0.0` es `+0.0`. Verificado ejecutando: `n = -z` con
  `z = 0.0`, después `1.0 / n > 0.0` da verdadero, o sea que quedó `+inf`, o sea
  que `n` es cero **positivo**. La forma correcta sería invertir el bit de signo
  con `xorps`. No afecta a ninguna operación aritmética del lenguaje; es una
  diferencia real con C y quedó anotada como límite.
- **`emitNot` usa `xorl $1` y eso depende de un invariante.** Solo es correcto si
  un booleano vale exactamente 0 o 1, lo que garantizan el `movzbl` de las
  comparaciones y las constantes booleanas del TAC. A cambio es una instrucción
  en lugar de tres.
- **`emitModulo` es el único `emitX` que no pregunta si el operando es real.**
  Verificado que corresponde: `1.5 % 2.0` es error semántico
  (*Tipos incompatibles en la operacion binaria*), así que el generador nunca ve
  un módulo de reales.
- **`internFloatConstant` compara patrones de bits y no valores**, lo que resuelve
  dos problemas de IEEE-754 a la vez: `-0.0 == 0.0` es verdadero (las dos
  constantes compartirían etiqueta y una saldría con el signo equivocado) y
  `NaN == NaN` es falso (un NaN repetido nunca se reutilizaría). Su camino de
  error, en cambio, **degrada en silencio**: devuelve la etiqueta de la primera
  constante del pool, que haría calcular con otra constante. Anotado como límite.
- **`powerOfTwoExponent` devuelve `0` para el valor 1** y el llamador trata ese
  `0` como "no aplica" (`exponent > 0`). No es un bug: el caso `× 1` ya se
  resolvió en su propia rama antes.
- **Corrección a la tabla de errores internos de F18:** decía que uno de los
  cinco se reporta en `buildFrameLayout`. Tras el commit
  `21b2ea3 fix: propagate allocation failures safely`, esa función devuelve
  `bool` y **el mensaje lo emite `generateAssembly`**, que es quien tiene el
  nombre de la función a mano. Son dos sitios en `generateAssembly`, no uno.
- **Conteo de líneas corregido:** la cabecera decía 1136 y el total real de los
  dos archivos es **1112** (el archivo se acortó con los refactors del back-end).
  Y "~45 funciones `static`" eran **50**.

Verificado después de los cambios: build sin warnings nuevos, las suites en
verde, los 6 programas correctos de `examples/` con el mismo código de salida sin
optimizar y con `-opt all`, y ningún link roto en el sitio.

**Queda para el último prompt:** los inventarios de **F14** (análisis de
variables vivas, 9 de 29 sin nombrar) y **F19** (intérprete, 8 de 27).

---

## Inventarios de método de F14 y F19 — HECHO (cierra la cobertura)

Las dos últimas fichas con funciones sin nombrar: F14 tenía 20 de 29 y F19 16 de
27. Las dos cerraron en **29/29** y **27/27**, y con eso

> **no queda ninguna función de `src/` ni de `include/` sin nombrar en la
> documentación.** Medido con un script sobre los 44 archivos: 422/422.

| Ficha | Sección nueva | Agrupada por |
| --- | --- | --- |
| F14 | §9 Inventario de funciones | públicas · las cinco etapas de la construcción · el índice de variables · los predicados sobre instrucciones · la impresión |
| F19 | §9 Inventario de funciones | la única pública · el registro de declaraciones · la llamada · las sentencias · las expresiones · los tres auxiliares |

Renumeradas las secciones siguientes (las dos fichas: 9→10 … 13→14) y
corregidas las tres referencias internas que apuntaban a esos números.

### Hallazgos de F14

- **Ningún bloque básico cruza la frontera entre dos funciones, y ninguna arista
  tampoco.** `isLeader` trata `begin_func` como líder y `endsBasicBlock` trata
  `end_func` como terminador, así que el bloque anterior cierra ahí y no recibe
  la arista de caída. Eso es lo que hace **sano el análisis intraprocedural**
  aunque los bloques de todas las funciones vivan en un mismo arreglo: no existe
  camino en el grafo del final de una función al principio de la siguiente. Es
  la respuesta a "¿cómo separa las funciones si el CFG es uno?".
- **`solveFixedPointLiveness` recorre los bloques en orden inverso**, que es el
  orden natural de un análisis hacia atrás: propaga en una ronda lo que el orden
  directo tardaría varias. El punto fijo es el mismo; cambia cuántas vueltas
  cuesta llegar.
- **`USE` y `DEF` no se guardan.** Viven en dos arreglos locales de
  `createLiveVariableAnalysis` que se liberan en cuanto converge; solo `liveIn` y
  `liveOut` quedan en los bloques. Por eso `printLiveVariableAnalysis` **los
  recalcula** para poder mostrarlos.
- **`computeBlockUseAndDef` saltea las declaraciones**, y con razón: si un
  `local_decl` contara como definición, la variable entraría en `DEF(B)` y eso la
  sacaría de `LIVE_IN(B)`, que es lo contrario de lo que corresponde.
- **Las dos versiones de la pregunta no son duplicación.** `isDeadDefinition`
  tiene que buscar el bloque (lineal sobre bloques × instrucciones);
  `isDeadDefinitionInBlock` lo recibe. `removeDeadStores` usa la segunda porque
  ya recorre por bloques: la diferencia es cuadrático contra lineal.
- Aproximar "destino de un salto" por "toda etiqueta" puede partir un bloque de
  más si la etiqueta quedó sin referencias. Es inofensivo —más bloques dan el
  mismo punto fijo— y quedó documentado.

### Hallazgos de F19

- **La asimetría entre leer y asignar es el pasaje por valor.**
  `evaluateIdentifier` devuelve `*symbol`, o sea **una copia**; `executeAssignment`
  busca el `Symbol *` y escribe ahí. Por eso asignarle a un parámetro cambia solo
  la copia local. Es la pregunta de parcial sobre pasaje por valor contestada en
  dos líneas de código.
- **El corto circuito vive en dos ramas que se evalúan antes del caso general**
  de `evaluateOperator`. El caso general evalúa los dos operandos y después opera,
  que es justo lo que `&&` y `||` no pueden hacer; las dos ramas lo saltean. Es la
  misma semántica que el TAC consigue con etiquetas, y la razón de que el
  intérprete sirva de oráculo para probarla.
- **`createFunctionScope` tiene una línea y es la decisión más importante del
  módulo.** `createScope(1, globalScope, NULL)`: el padre es el **global**, no el
  llamador. Cambiarlo por `callerScope` convertiría el lenguaje a alcance
  dinámico y los programas seguirían corriendo, dando otros resultados.
- **El orden de `executeFunctionBody` importa:** los argumentos se evalúan
  **antes** de crear el ámbito de la función, en el ámbito del llamador. Al revés,
  un argumento que nombra una local del llamador no se encontraría.
- **`registerVariableDeclaration` sirve para globales y locales**, y en el segundo
  caso agrega el símbolo al ámbito **del bloque**: eso es todo lo que hace falta
  para que el ocultamiento de nombres funcione.
- **`evaluateArguments` devuelve `NULL` para cero argumentos y eso no es un
  error**, de ahí el `bool *outOfMemory` aparte: `NULL` ya significa otra cosa.
- **`ExecResult.hasReturned` reemplaza al `goto` al epílogo** del código objeto:
  cada nivel del recorrido mira la bandera y corta. Un `return` dentro de tres
  `while` anidados sale de los tres porque los tres consultan lo mismo.

### Nota para `CLAUDE.md`

El archivo lista el enumerado `OperationType` con `OP_NEGATIVE` y `OP_NEGATION`.
Los nombres reales en `include/types.h` son **`OP_UMINUS`** y **`OP_NOT`**; los
otros diez coinciden. Conviene corregir esos dos en `CLAUDE.md` para que no
mande a buscar identificadores que no existen.

Verificado: build limpio, las tres suites en verde (51 + 35 + 45 tests), los seis
programas correctos con el mismo código de salida sin optimizar y con `-opt all`,
el intérprete terminando en 0 en los seis, el sitio armando con `--strict` sin
avisos y ningún link roto.

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

- [x] **F01 — Entrada y línea de comandos** · `main.c`, `cli.h`, `cli.c` · 500 líneas
  - **Hecha:** `wiki/01.2-cli.md`. `main.c` paso por paso, `Options` campo por
    campo, las tres tablas estáticas que gobiernan el parseo, y las dieciséis
    funciones.
  - **Cómo se resolvió la ambigüedad del enunciado**: `BUILDS_EXECUTABLE` es
    `!explicitTarget`, y de ahí sale por qué `explicitTarget` no es redundante con
    `target` (los dos valen `TARGET_ASSEMBLY` sin `-target`).
  - **Verificado:** `interpret` es alias de `interp` y el usage lo imprime una
    sola vez; la heurística del argumento opcional de `-opt` (`-opt programa.ctds`
    se entiende como `all`); y la protección contra sobrescribir el fuente, que
    compara **dispositivo e inodo** y por eso atrapa también `./p.ctds` y la ruta
    absoluta. Los ocho casos de error de la CLI, cada uno con su mensaje.
- [x] **F02 — Orquestación de las etapas** · `driver.h`, `driver.c` · 325 líneas
  - **Hecha:** `wiki/01.4-driver.md`. El `switch` de seis casos leído como el
    pipeline, las cinco funciones de etapa, y dónde se enchufa el optimizador.
  - **El hallazgo más interesante: `assembleAndLink` usa `fork` + `execvp`, no
    `system()`.** Con `system` el comando pasa por un shell y un nombre de archivo
    con espacios, `'`, `;` o `$` lo rompería o ejecutaría otra cosa. Verificado con
    los cuatro: los cuatro compilan y ejecutan bien. Documentados también los dos
    flags (`-x assembler` y `-no-pie`) y por qué el hijo usa `_exit(127)`.
  - **Verificado:** un programa con error de sintaxis **pasa `-target scan`**
    (exit 0) porque el escaneo no construye el AST; y `-debug` conserva el
    `.tmp.s` mientras sin `-debug` se borra.
- [x] **F03 — Errores y salida de debug** · `error_handler.h`, `error_handler.c`, `debug.h` · 144 líneas
  - **Hecha:** `wiki/01.6-errors.md`. Los tres niveles con **dos contadores**, y
    por qué la asimetría no es un olvido: el léxico y el semántico acumulan porque
    sus errores son independientes, el sintáctico aborta porque después de un error
    el parser no sabe en qué estado quedó. Más la tabla de los tres casos de salida
    (éxito silencioso, éxito con `-debug`, error).
  - **Hallazgo: `getLastErrorLine` y `getLastErrorMessage` son API muerta.**
    Verificado con una búsqueda en `src/`, `include/` y `tests/`: no se llaman
    desde ninguna parte. El buffer de 256 bytes se llena en cada error solo para
    alimentarlas. Anotado también que usarlas sería la mejora de testing más
    barata que habilita el módulo.
  - **La convención de `debug.h` está desincronizada con el código:** nombra
    `[TAC]`, que no se usa, y omite `[DRIVER]` y `[ELIMINADA]`, que sí. Y el
    volcado del análisis de variables vivas no respeta el formato de prefijos.
- [x] **F04 — Utilidades y tipos base** · `string_builder.h`, `string_builder.c`, `types.h` · 164 líneas
  - **Hecha:** `wiki/01.8-utilities.md`. El acumulador global con su ciclo de uso,
    el crecimiento por duplicación con **los dos chequeos de desborde de
    `size_t`** y por qué importan, y el truco de `vsnprintf(NULL, 0, ...)` con la
    explicación de por qué `va_copy` es obligatorio y no estilo.
  - **El reparto de uso, medido:** 56 llamadas desde `object_code.c`, 10 desde
    `asm_instruction.c`, 7 desde `driver.c` y **una sola** desde
    `tac_instruction.c`. Esa última cuenta la historia de por qué el formato del
    TAC tiene tres consumidores.
  - **`types.h` no tiene `.c`**, y `getDataTypeName` está declarada ahí pero
    **definida en `ast.c:220`**: una incoherencia de ubicación que acopla cualquier
    usuario del enum a que `ast.c` esté en el enlazado. Documentado también que
    `TYPE_VOID` cumple dos papeles (tipo de retorno y "todavía no se sabe") y que
    `OP_AND`/`OP_OR` existen en el enum **sin opcode TAC equivalente**.

> **Bloque 0 completo, y con eso el inventario entero.** Las cuatro fichas eran el
> último hueco de archivos: `main.c` y `driver.c` son por donde arranca la
> ejecución, así que sin ellas el recorrido empezaba por el lexer sin explicar
> quién lo llamaba. **Cobertura a nivel método: 45/45 funciones nombradas.**
> En el `nav` el bloque quedó entre la visión general y el análisis léxico, con la
> cadena de *Siguiente* enlazada.

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

- [x] **F09 — Tabla de símbolos y ámbitos** · `symbol_table.h`, `symbol_table.c` · 358 líneas
  - **Hecha:** `documentacion/wiki/04-symbol-table.md`, página nueva. Se extrajo
    de `04-semantic-analysis.md` el material de `Symbol` y `Scope` (que mezclaba
    F09, F10 y F11) y se reescribió completo: las cuatro variantes de
    `SymbolType` —la anterior documentaba solo tres y faltaba
    `FUNCTION_ENTRY_SYMBOL`, que usa únicamente el intérprete—, las **dos vidas
    de `Symbol`** (`newSymbol` en heap para la tabla contra `make*Symbol` por
    valor para evaluar en el intérprete), por qué `parent` y `children` no son
    redundantes, la herencia de `scope->function` que permite que un `return`
    anidado sepa qué tipo devolver, y el `uid` como contador global.
  - Verificado con un programa de tres `x` anidados: `uid` 1/3/4, el TAC los
    nombra `x@1`/`x@3`/`x@4` y el `return x` resuelve al del método. También que
    un local **no** puede ocultar a un parámetro (comparten ámbito) pero sí desde
    un bloque interno, y que la recursión funciona porque el método se registra
    antes de analizar su cuerpo.
  - **Arreglados 4 de los 22 links** a la máquina de Fran, los de
    `wiki/04-semantic-analysis.md`, apuntándolos a GitHub. Quedan 18, en
    `analisis-semantico.md` (15) y `convenciones_comentarios.md` (3).
  - La jerarquía de `Scope`s, la búsqueda de adentro hacia afuera, el
    ocultamiento (shadowing) y los `uid`. Teoría: tabla de símbolos jerárquica.
- [x] **F10 — Chequeo de tipos** · `type_checker.h`, `type_checker.c` · 257 líneas
  - **Hecha:** `documentacion/wiki/04.5-type-checker.md`, página nueva. La **tabla
    de reglas de tipos de los siete grupos de operadores**, verificada con 20
    expresiones ejecutadas; la asimetría entre `<` (coerciona) y `==` (exige
    tipos idénticos) con su justificación; que `isAssignable` solo **ensancha**
    `int`→`float` y nunca al revés; la supresión de errores en cascada vía
    `TYPE_ERROR` como tipo; y el reparto de responsabilidades con F11
    (expresiones contra sentencias).
  - **Dónde se ve una coerción:** no en el `.sem`, porque `describeNode` no
    imprime el tipo de un `OPERATOR_NODE`, sino en el `.ci`, donde aparece como
    una instrucción `int_to_float`. Queda documentado como técnica de
    verificación.
  - **Corrección a documentación previa:** `04-semantic-analysis.md` decía
    "`<`, `>`, `==`: Comparan tipos compatibles", agrupando los tres. Es
    impreciso: `int == float` es **error** y `int < float` es válido.
  - Se le extrajo a `04-semantic-analysis.md` la sección de verificación de
    tipos; quedó con las reglas sobre sentencias, que son F11.
  - **Nota para F11:** esa misma página afirma que `checkMainMethodExists`
    comprueba que exista `void main()`. Verificado que **no exige `void`**: solo
    que `main` exista, sea un método y no tenga parámetros. `int main()` es
    válido y es lo que usan los ejemplos para devolver un código de salida.
  - Inferencia del tipo de cada expresión, compatibilidad en asignaciones, la
    coerción `int`/`float` y por qué `==` es más estricto que `<` y `>`.
- [x] **F11 — Recorrido semántico** · `semantic_analyzer.h`, `semantic_analyzer.c` · 328 líneas
  - **Hecha:** `documentacion/wiki/04-semantic-analysis.md`, reescrita como ficha
    F11. Las diez funciones; el doble papel de `analyzeSemantics` según
    `parent == NULL`; la recursión mutua con `analyzeNodeSemantics` que es lo que
    genera la jerarquía de ámbitos; **el catálogo completo de los 15 mensajes de
    error** de este archivo, todos verificados ejecutándolos; y la política de
    diagnóstico acumulativo contrastada con el corte del sintáctico.
  - **`guaranteesReturn` con su tabla de siete casos verificados.** Incluye la
    limitación: `while (true) { return 1; }` se **rechaza**, porque el `default`
    devuelve `false` para `WHILE_NODE` sin mirar la condición. Documentado como
    aproximación conservadora deliberada, con la extensión mínima que lo
    arreglaría.
  - **Corrección confirmada:** `checkMainMethodExists` **no exige que `main` sea
    `void`**. `int main()` y `void main()` compilan los dos. La página anterior
    afirmaba lo contrario.
  - Se documentaron dos chequeos defensivos que son **código muerto**: el de
    `return` fuera de una función (un `return` suelto es error de *sintaxis*, no
    semántico) y el `hasExpression` de `analyzeAssignmentNode` (la gramática
    siempre da el hijo).
  - El diagrama de clases Mermaid que tenía la página anterior se movió a F09,
    que es donde viven esas estructuras.
  - Las reglas semánticas del lenguaje, el registro de métodos antes de analizar
    el cuerpo (que es lo que habilita la recursión) y `guaranteesReturn`.

### Bloque 4 — Código intermedio

- [x] **F12 — La representación intermedia** · `tac_operation.h`, `tac_operand.h`, `tac_operand.c`, `tac_instruction.h`, `tac_instruction.c`, `tac_list.h`, `tac_list.c` · 659 líneas
  - **Hecha:** `documentacion/wiki/05-tac-representation.md`, página nueva. Se le
    extrajo a `05-intermediate-code.md` el modelo de datos y el catálogo de
    opcodes (que mezclaba F12 con F13) y se reescribió: **qué es una cuádrupla**
    y por qué se eligieron cuádruplas y no triplas (las triplas referencian por
    posición, así que mover o borrar una instrucción invalida las demás: hostil
    para un optimizador), la tabla texto-contra-estructura que justifica la
    migración, las seis categorías de `OperandKind` y por qué distinguir temporal
    de variable habilita optimizar con confianza, el nombre calificado `x@uid`, y
    el formateador como única fuente de verdad del formato textual.
  - **Lo que no está y por qué:** no hay `TAC_AND` ni `TAC_OR`, porque el corto
    circuito que exige el lenguaje no se puede expresar con un opcode cuyos dos
    operandos deban estar calculados antes. Se muestra la secuencia de saltos que
    los reemplaza, con `if ... false goto` para `&&` y `if ... true goto` para
    `||`. Eso cierra la referencia obsoleta que quedaba en `optimizaciones.md`.
  - Verificado que **los 24 opcodes declarados se emiten**: ninguno muerto. La
    página incluye un programa de referencia que los recorre todos de una pasada,
    comprobado.
  - **Corregido el `BINARYOPERATOR_NODE`** que quedaba en la página de generación:
    el enum tiene `OPERATOR_NODE`. Ya no queda ninguna mención en toda la
    documentación (sigue en `CLAUDE.md`, que no es del sitio).
  - El archivo se renombró con `git mv` a `05.5-tac-generation.md` para dejarle
    el número a F12; los 4 links entrantes quedaron actualizados.
  - **Qué es una cuádrupla** y por qué se migró de texto plano a una estructura:
    `{operación, resultado, operando1, operando2}` con operandos tipados. Es una
    de las decisiones de diseño más defendibles del proyecto y conviene tenerla
    muy clara.
- [x] **F13 — Generación del TAC** · `intermediate_code.h`, `intermediate_code.c` · 403 líneas
  - **Hecha:** `documentacion/wiki/05.5-tac-generation.md`, reescrita como ficha.
    La distinción entre los dos dispatchers (una sentencia no devuelve nada, una
    expresión **devuelve el operando donde quedó su resultado**, y eso es lo que
    hace que la recursión componga); el recorrido postorden; el corto circuito
    con su esquema y el porqué del temporal nuevo; los tres patrones de etiquetas
    de `if`/`if-else`/`while`; las cuatro coerciones en correspondencia uno a uno
    con las reglas de F10; y los tres puntos de detección de errores internos.
  - **Por qué no hace falta backpatching**, que es la pregunta natural de esta
    ficha: las etiquetas son **nombres y no direcciones**, y el AST ya está
    completo cuando se genera. El backpatching es inevitable solo si se genera
    código durante el parseo.
  - Verificado: ciclos anidados producen pares de etiquetas correctamente
    anidados (`L0`/`L1` externo, `L2`/`L3` interno, `L4` el `if`) y el programa
    da el resultado correcto; `void vacio(){ }` recibe su `ret` implícito; el
    corto circuito no evalúa el segundo operando (efecto observable comprobado).
  - **Detalles contraintuitivos documentados:** los números de temporal **no
    siguen el orden de emisión** (el temporal del resultado se reserva entre los
    dos operandos: `t3 = t2 + t4`), y el `const` del retorno de
    `generateIntermediateCode` **se descarta con un cast** en `driver.c:69`
    porque el optimizador necesita mutar la lista.
  - Confirmado que **los contadores de temporales son globales al programa**, que
    es la decisión abierta §3.6 de `TODO_intermediate_code.md`; queda explicado
    ahí con el costo que tiene para dimensionar el marco de pila.
  - El recorrido del AST, la generación de temporales y etiquetas, el **corto
    circuito** de `&&` y `||`, y por qué las etiquetas se resuelven en una sola
    pasada sin backpatching. Teoría: código de tres direcciones.

### Bloque 5 — Optimización

- [x] **F14 — Grafo de flujo y variables vivas** · `live_variable_analysis.h`, `live_variable_analysis.c` · 481 líneas
  - **Hecha:** la página existente era buena (tenía el modelo de datos, las dos
    ecuaciones, un ejemplo a mano y las limitaciones), así que se **completó en
    vez de reescribir**. Se movió con `git mv` de `documentacion/` a
    `wiki/05.7-live-variables.md` y se reubicó en el recorrido: iba **al final,
    después del código objeto**, y ahora va donde corresponde, entre la
    generación de TAC y el back-end.
  - **Agregado:** por qué converge el punto fijo (ecuaciones monótonas sobre
    conjuntos acotados: sucesión creciente y acotada se estabiliza; cota
    `O(variables × bloques)`), los nombres de bibliografía (marco monótono,
    líder, problema distributivo, análisis hacia atrás), cómo reusar la
    infraestructura para un análisis hacia adelante como definiciones
    alcanzables, y el aislamiento natural de las funciones en el CFG.
  - **El límite de 64 variables, medido:** un programa de 70 variables reporta
    `Variables candidatas (64)` y descarta el resto en silencio, **pero la
    compilación sigue siendo correcta** (69 con y sin `-opt all`). La razón es que
    `getOperandVariableIndex` devuelve `-1` y `isDeadDefinitionInBlock` responde
    "no es muerta", que es el lado conservador. Pasarse del límite degrada la
    optimización, no la corrección.
  - **Qué detecta y qué no:** encuentra asignaciones muertas, no cómputos
    inútiles. Documentado con el caso del acumulador de un ciclo que sobrevive a
    `-opt all` porque **el ciclo se sostiene a sí mismo** (`a = a + i` lee `a`, lo
    que la mantiene viva vía la arista de retorno). Romperlo pide *faint variable
    analysis*, que es otro análisis.
  - **Corregida una limitación obsoleta:** la página afirmaba una fuga cuando
    `blockCount == 0`. El código ya usa `freeLiveVariableAnalysis` ahí, así que no
    existe. Se reemplazó por la real: `addSuccessor` escribe en `successors[2]`
    **sin verificar el límite** (seguro por construcción, pero sin guarda).
  - Construcción de bloques básicos, el CFG, y las ecuaciones de vivacidad
    resueltas hasta el punto fijo. Teoría: análisis de flujo de datos,
    `LIVE_IN`/`LIVE_OUT`, análisis hacia atrás, por qué converge.
- [x] **F15 — El optimizador** · `optimizer.h`, `optimizer.c` · 839 líneas
  - **Hecha:** `optimizaciones.md` se movió con `git mv` de la raíz a
    `wiki/05.9-optimizer.md`, así que **el optimizador por fin figura en el
    sitio** (era el hueco que el plan marcaba como "casi gratis"). Se le agregaron
    las secciones que le faltaban de la plantilla y se corrigió lo que había
    quedado obsoleto.
  - **La corrección grande: `hasExecute` ya no existe.** El documento lo describía
    como un mecanismo vigente en **seis lugares**, incluida una subsección entera
    y el estado de implementación que afirmaba "quedó funcionando". Verificado que
    el campo **no está en el código**: ninguna optimización terminó trabajando
    sobre el AST, quedó sin usuarios y se eliminó de `NodeAST`. Se reescribió como
    decisión de diseño registrada —por qué se descartó optimizar el árbol— en vez
    de borrarlo, porque es la respuesta a "¿por qué no optimizaron el AST?".
  - **Hallazgo estructural:** de los siete flags, **seis actúan sobre el TAC y
    `strength-reduce` actúa sobre el assembly**, en `object_code.c:710`. No está en
    el bucle de `optimizeIntermediateCode`; el conjunto se le pasa aparte a
    `generateAssembly`. El documento no lo distinguía; ahora hay una tabla
    flag → nivel → función.
  - **Agregado:** la máscara de bits de `OptimizationSet`; el bucle de punto fijo
    con las tres preguntas (por qué se repiten las pasadas, por qué termina, por
    qué igual hay un tope de 16 rondas); la `KnownConstantTable` con el porqué de
    `destinationKind` y de vaciarla en cada frontera de control; que la
    propagación es **local** y la eliminación de asignaciones muertas es **global**;
    los cinco pasos de código para agregar una optimización; y la teoría con los
    nombres de bibliografía.
  - **Estado actualizado:** la eliminación de asignaciones muertas ya está
    conectada al análisis de vivacidad, y `removeUnreferencedLabels` ya corre.
    Lo único pendiente son los tres patrones de mirilla sobre el assembly.
  - Se dedujo §5 contra la nueva §9 (criterios de diseño contra pasos de código) y
    se corrigió la referencia del README, que citaba `optimizaciones.md` como
    ejemplo de archivo fuera del sitio.
  - Las siete optimizaciones, el bucle de punto fijo que las repite, el orden en
    que se aplican y por qué, y qué garantiza que preservan la semántica.
  - Ya existe `optimizaciones.md` en la raíz con el catálogo completo: esta ficha
    lo **mueve a `documentacion/`** y lo suma al `nav:`, en vez de reescribirlo.

### Bloque 6 — Código objeto

- [x] **F16 — Instrucciones x86-64** · `asm_instruction.h`, `asm_instruction.c` · 391 líneas
  - La representación de una instrucción de assembly y el repertorio usado.
  - **Hecha:** `documentacion/wiki/06-asm-instructions.md`, página nueva. Era un
    **hueco completo**: `06-object-code.md` no tenía ni una sección sobre este
    módulo. Cubre los 11 opcodes, los 5 tipos de operando, los 13 constructores,
    y dónde vive la sintaxis AT&T.
  - **Hallazgo medido: el módulo cubre solo la aritmética entera de 32 bits.** En
    `object_code.c` hay **14 llamadas a `emitAsm` contra 56 a `buildStringf`**, o
    sea que ~20% del assembly pasa por el modelo tipado. Reales (SSE), saltos,
    llamadas, marco de pila, `setCC` y directivas se emiten como texto directo.
    Queda documentado como limitación real y no como diseño: la sintaxis AT&T
    está centralizada solo para una parte del back-end.
  - **`asmLoad` y `asmStore` son la misma función**, carácter por carácter. No es
    un bug: x86 tiene un solo `mov` para leer y escribir memoria, a diferencia de
    las RISC. Los dos nombres existen para que el generador exprese la intención.
  - **El enum de registros quedó chico y `customReg` lo compensa.** Hay 3
    registros en `AsmRegister` y 6 llamadas a `opRegStr` contra 2 a `opReg`, así
    que el camino por nombre es el principal. Documentado por qué `customReg` no
    puede estar en la unión, y que mientras conviva un registro se puede nombrar
    de dos maneras sin garantía de que se traten como equivalentes.
  - **Dos `default` silenciosos:** `getRegisterName` devuelve `"%eax"` y
    `getOpcodeMnemonic` devuelve `"nop"`. Un opcode nuevo sin su caso emite un
    `nop` y el programa hace algo distinto sin un solo aviso. Comparado con
    `getTACOperation` de F13, que sí reporta error interno, acá falta ese aviso:
    es la mejora más chica y más valiosa del módulo.
  - Explicado por qué **no hay lista de `AsmInstruction`** y qué cambio
    estructural haría falta para habilitar los tres patrones de mirilla sobre el
    assembly que siguen pendientes en F15.
- [x] **F17 — Marco de pila** · `function_frame.h`, `function_frame.c` · 263 líneas
  - Asignación de un slot por variable, parámetro y temporal; el cálculo del
    tamaño del frame y la alineación a 16 bytes.
  - **Hecha:** `documentacion/wiki/06.5-stack-frame.md`. Se le extrajeron a
    `06-object-code.md` las secciones del marco (§2 a §5, que eran F17 mezclada
    con F18) y se completó con los 14 campos de `FunctionFrame`, las cinco
    funciones, el mecanismo de argumentos pendientes y la teoría.
  - **El mecanismo de argumentos pendientes, explicado y verificado.** Resuelve
    que una llamada anidada destruiría los argumentos ya cargados en registros:
    cada `param` copia su valor a una ranura propia del marco y el `call` consume
    las últimas N. El dimensionamiento **simula la pila** de argumentos para
    hallar el pico, y reserva solo esa cantidad de ranuras anónimas, no una por
    argumento del programa. Verificado con `f(g(1), h(2))`: pico 2, marco de 32
    bytes (3 temporales + 2 ranuras = 20, alineado a 32), offsets `-4` a `-20`, y
    el programa devuelve 112.
  - **Por qué hay dos contadores de parámetros formales por clase:** la ABI
    reparte enteros y reales en **dos secuencias de registros independientes**, así
    que en `f(int a, float b, int c)` los registros son `%edi`, `%xmm0`, `%esi`:
    la posición en los registros no es la posición en la firma.
  - **Corregido el diagrama de clases**, que estaba desactualizado: mostraba un
    `paramCount` que **no existe** (los campos reales son `integerParamCount`,
    `floatParamCount` y `stackParamCount`), le faltaban `returnType` y los cinco
    campos de argumentos pendientes, y declaraba `char name[64]` cuando
    `TAC_NAME_LENGTH` es 256.
  - **`MAX_PENDING_PARAMS` es un alias muerto:** está definido y **no se usa en
    ninguna parte**; quedó de cuando el arreglo de argumentos era de tamaño fijo.
  - Documentadas dos asperezas verificadas: `addSlot` devuelve `0` ante error y 0
    es también un offset sintácticamente válido (ambiguo por contrato, inocuo en
    la práctica porque el primer slot es `-4`), y `ensurePendingParamCapacity`
    puede dejar `pendingParamCapacity` desfasado si falla el segundo `realloc`.
  - **Tres invariantes del marco verificados** sobre `examples/factorial.ctds` con
    un script: `frameSize` múltiplo de 16, ningún offset usado excede el
    `frameSize`, y todos los offsets negativos. Quedaron documentados como
    chequeo de regresión.
- [x] **F18 — Generador de código objeto** · `object_code.h`, `object_code.c` · 1112 líneas
  - **Hecha:** `documentacion/wiki/06-object-code.md`, reescrita como ficha F18
    (233 → 600). Las tres secciones del objeto, el despachador con sus tres
    patrones de traducción, la convención de llamada de los dos lados, las cinco
    traducciones que tienen algo que contar, el pool de constantes reales, la
    resolución de operandos y los cinco errores internos.
  - **Corrección a lo que decía la página:** describía que `TAC_PARAM` carga el
    argumento **directo al registro** y que `TAC_CALL` reinicia el índice a 0. Las
    dos cosas cambiaron: hoy cada `param` copia su valor a una ranura del marco y
    el `call` restaura el contador a la **base** de esa llamada, no a cero, que es
    justamente lo que permite las llamadas anidadas. Tampoco mencionaba los
    registros `%xmm` para reales.
  - **Error mío, corregido antes de quedar:** escribí que las globales se emiten
    como `g@1`. Es falso: `getGlobalName` devuelve `symbol->id`, o sea el nombre
    **sin** el sufijo. Verificado con `nm` sobre el ejecutable (`D g`, `D gf`). El
    porqué queda documentado: el `@uid` desambigua nombres repetidos y una global
    no puede repetirse, así que usar el nombre limpio deja legible la tabla de
    símbolos del objeto.
  - **Lo verificado ejecutando:** división y módulo de negativos (`-17/5 = -3`,
    `-17%5 = -2`, que valida el `cltd` y el divisor en `%ecx`); llamadas de 7 y de
    8 argumentos (relleno de alineación con cantidad impar en la pila);
    `f(int a, float b, int c, float d)` recibiendo `%edi`/`%xmm0`/`%esi`/`%xmm1`,
    que muestra que **la posición en los registros no es la posición en la
    firma**; y `NaN == NaN` dando falso vía `0.0 / 0.0`, que valida el filtro de
    paridad con `setnp`.
  - **Los tres detalles de x86 que más se preguntan**, cada uno con su porqué:
    `cltd` obligatorio antes de `idivl`; el divisor nunca en `%edx` porque `cltd`
    lo sobrescribe; y que `comiss` setea los flags como comparación **sin signo**,
    así que las condiciones son `setb`/`seta` y no `setl`/`setg`.
  - Documentado que **división y módulo son la misma instrucción**: `idivl` deja
    el cociente en `%eax` y el resto en `%edx`, así que las dos comparten
    `emitDivisionOp` y solo cambian el registro que leen.
  - La traducción de cada instrucción TAC, la **convención de llamada System V
    AMD64**, el caso de `idivl` con `cltd`, el camino SSE para `float` y la
    semántica de NaN en las comparaciones. El archivo más grande del proyecto.

### Bloque 7 — Intérprete

- [x] **F19 — Intérprete sobre el AST** · `interpreter.h`, `interpreter.c` · 590 líneas
  - **Hecha:** `documentacion/wiki/07-interpreter.md`, página nueva. Era el otro
    hueco completo: el intérprete no tenía ni una página, solo una mención en la
    portada. Los dos recorridos, `ExecResult`, el ámbito de una llamada, las
    coerciones, lo que asume y lo que cuesta.
  - **`ExecResult` como respuesta a "¿cómo se implementa un `return` en un
    intérprete de árbol?"**: cada sentencia devuelve si retornó, y cada bloque y
    cada ciclo lo chequean y lo propagan. Es la alternativa explícita a
    `setjmp`/`longjmp` o a una excepción que C no tiene.
  - **El hallazgo conceptual: `createFunctionScope` usa el ámbito global como
    padre, no el del llamador.** Esa única línea es la que hace que el lenguaje
    tenga **alcance léxico y no dinámico**. Los argumentos se evalúan en el
    ámbito del llamador y sus valores se copian, que es el pasaje por valor
    materializado en el arreglo `argumentValues` intermedio.
  - **Lo medido: la memoria crece lineal con las iteraciones, no con el
    programa.** `executeBlock` crea un ámbito por cada ejecución de un bloque y lo
    cuelga del árbol sin liberarlo, y el cuerpo de un `while` **es** un bloque.
    Medido: 10 000 iteraciones → 4224 KB, 20 000 → 7040 KB, 40 000 → 12 416 KB,
    contra 1664 KB del mismo programa compilado. Unos 280 bytes por iteración. Es
    correcto pero no escalable, y la corrección —liberar el ámbito al salir— es
    viable porque los valores ya viajan por copia.
  - **Lo que asume sin verificar:** `isTruthy` acepta cualquier valor distinto de
    cero, que es más permisivo que el lenguaje; funciona porque el semántico ya
    filtró. Y reimplementa las reglas de tipos, que es lo que lo hace útil como
    oráculo y lo que duplica el mantenimiento.
  - **Reverificado en esta sesión:** `-target interp` sobre `INT_MIN / -1` reporta
    el desborde y **termina con código 1** (la propagación del error que
    agregaron los compañeros está en su lugar), y el cruce intérprete/ejecutable
    da 5/5 sobre recursión, corto circuito, ocultamiento de scopes, 8 argumentos
    y reales. El corpus completo de 20 ya se había cruzado antes con 20/20.
  - Dejado explícito el argumento a favor de haberlo hecho: es **testing
    diferencial**: dos implementaciones independientes de la misma semántica
    comparadas entre sí encuentran bugs sin escribir los resultados esperados a
    mano. Y que el guard de `INT_MIN` fue un bug **del intérprete**, encontrado
    con UBSan, no del back-end.
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
| `wiki/06-asm-instructions.md` | 438 | **F16, hecha.** |
| `wiki/06.5-stack-frame.md` | 528 | **F17, hecha.** |
| `wiki/06-object-code.md` | 625 | **F18, hecha.** |
| `wiki/04.5-type-checker.md` | 457 | **F10, hecha.** |
| `wiki/04-semantic-analysis.md` | 538 | **F11, hecha.** |
| `wiki/05-tac-representation.md` | 573 | **F12, hecha.** |
| `wiki/05.5-tac-generation.md` | 631 | **F13, hecha.** |
| `wiki/05.7-live-variables.md` | 438 | **F14, hecha.** |
| `wiki/03-ast.md` | 569 | **F08, hecha.** |
| `wiki/04-symbol-table.md` | 506 | **F09, hecha.** |
| `wiki/02-lexical-analysis.md` | 452 | **F05, hecha.** |
| `wiki/02.5-syntax-analysis.md` | 480 | **F06, hecha.** |
| `wiki/02.7-parser-actions.md` | 40 | F07, esqueleto con la lista de lo que falta |
| `wiki/05.9-optimizer.md` | 827 | **F15, hecha.** Movida al sitio desde la raíz |
| `wiki/07-interpreter.md` | 437 | **F19, hecha.** |
| `wiki/01.2-cli.md` | 454 | **F01, hecha.** |
| `wiki/01.4-driver.md` | 499 | **F02, hecha.** |
| `wiki/01.6-errors.md` | 385 | **F03, hecha.** |
| `wiki/01.8-utilities.md` | 405 | **F04, hecha.** |

> **Bloques 1 a 7 completos.** Queda solo el bloque 0 (infraestructura:
> F01–F04). Las tres fichas del back-end estaban cubiertas por una sola página
> de 370 líneas; ahora son 438 + 528 + 625.

### Pendiente transversal: 22 links rotos a la máquina de Fran

Tres páginas tienen links absolutos de la forma
`file:///home/francisco/compiler/include/symbol_table.h#L16-L20`, que apuntan al
disco de quien las escribió: están muertos para cualquier otro y además exponen
una ruta personal en el sitio publicado.

| Página | Links rotos | Estado |
| --- | --- | --- |
| `analisis-semantico.md` | 15 | pendiente: es página de Documentación Técnica, no de una ficha |
| `wiki/04-semantic-analysis.md` | 4 | **arreglados en F09** |
| `convenciones_comentarios.md` | 3 | pendiente |

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
3. ~~**F15** (optimizador), que es casi gratis: mover `optimizaciones.md` al sitio.~~ **HECHA.**
4. El resto de los bloques en orden.
