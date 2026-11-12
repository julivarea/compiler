# Cambios a Etapas Anteriores

El enunciado pide **incluir los cambios realizados en etapas anteriores y
explicar la necesidad de los mismos**. Esta página los junta en un solo lugar,
porque de otro modo quedan repartidos entre las fichas y el historial de git.

El criterio es: acá está todo cambio a una fase **ya entregada**, hecho mientras
se trabajaba en una etapa posterior. Los cambios internos de la etapa corriente
no entran.

El patrón que se repite es uno solo y vale enunciarlo de entrada: **casi todos
los cambios al front-end salieron de necesidades del back-end**. Hasta que no
hubo que generar código, varias cosas del análisis parecían correctas; recién
cuando una fase posterior las consumió se vio qué faltaba.

---

## Resumen

| Etapa modificada | Cambios | Cuándo |
| --- | :---: | --- |
| Léxico y sintáctico (entregada 23/09) | 4 | Durante código intermedio, optimizador y entrega final |
| AST y tabla de símbolos (entregada 30/09) | 4 | Durante el optimizador |
| Análisis semántico (entregado 07/10) | 3 | Durante código objeto y optimizador |
| Código objeto (entregado 26/10) | 2 | Durante el optimizador |
| **Cuatro a la vez** (§5) | 1 | Recorte de alcance en la entrega final |

---

## 1. Léxico y sintáctico

### 1.1 Los errores léxicos no abortaban la compilación

**Qué cambió.** `lexer.l` pasó a reportar sus errores a través de
`error_handler`, que los cuenta, y el driver pasó a consultar ese contador.

**Por qué hizo falta.** Un error léxico imprimía su mensaje y el compilador
seguía como si nada. Mientras la única etapa era el análisis eso se notaba poco;
con el generador de TAC en el medio, un programa con basura léxica llegaba a
generar código. La regla del proyecto es que la generación solo corre si no hubo
errores, y para cumplirla había que **contarlos**.

Es también lo que dio origen a la separación entre `lexical_errors`,
`syntax_errors` y `semantic_errors`, que la [ficha F03](wiki/01.6-errors.md)
desarrolla.

### 1.2 Un comentario multilínea sin cerrar terminaba en silencio

**Qué cambió.** El autómata de comentarios reporta un error léxico si llega al
fin de archivo con el comentario abierto.

**Por qué hizo falta.** La condición de arranque exclusiva `%x COMMENT` consume
todo hasta encontrar `*/`. Si no aparece, el lexer llegaba a EOF **sin una sola
queja** y el parser recibía un archivo que terminaba antes de tiempo, con un
error de sintaxis confuso o ninguno. Lo detectó un test que abría un comentario
y no lo cerraba.

### 1.3 Recursión a izquierda en todas las listas

**Qué cambió.** Las producciones de listas pasaron a recursión a izquierda y se
eliminó el helper `prependNode`, que ya no tenía usuarios.

**Por qué hizo falta.** Bison es **LALR(1)**, o sea ascendente: con recursión a
derecha tiene que apilar todos los elementos antes de empezar a reducir, y la
pila crece con el largo de la lista. Con recursión a izquierda reduce de a uno y
la pila queda acotada. Es la recomendación estándar para generadores
ascendentes, y de paso sacó el helper que existía solo para dar vuelta las
listas construidas al revés.

### 1.4 El no terminal `Assignment`

**Qué cambió.** La asignación se extrajo de `Statement` a su propio no terminal:

```
Assignment : ID '=' Expression
Statement  : Assignment ';' | ...
```

**Por qué hizo falta.** No por una necesidad del compilador actual, sino de
diseño: una asignación **sin** el punto y coma es la pieza que pide la cabecera
de un `for`, y embebida en `Statement` no se podía escribir. Lo desarrolla la
[ficha F06](wiki/02.5-syntax-analysis.md), que además muestra cómo queda la
extensión del `for` apoyada en el no terminal nuevo.

---

## 2. AST y tabla de símbolos

### 2.1 `BINARYOPERATOR_NODE` pasó a llamarse `OPERATOR_NODE`

**Qué cambió.** Tres renombres y una simplificación:

| Antes | Ahora |
| --- | --- |
| `BINARYOPERATOR_NODE` | `OPERATOR_NODE` |
| `newBinaryOperatorNode` | `newOperatorNode` |
| `OP_NEGATIVE`, `OP_NEGATION` | `OP_UMINUS`, `OP_NOT` |
| Siete macros `GET_*` con su propio chequeo de rango | Las mismas siete sobre un `GET_CHILD(node, i)` común |

**Por qué hizo falta.** El nombre mentía: **ese nodo también representa los
operadores unarios** (`-x`, `!b`), que tienen un solo hijo. Quien leía el
generador de código buscaba el caso de los unarios en un tipo de nodo que no
existía. Y `OP_NEGATIVE` / `OP_NEGATION` eran casi indistinguibles entre sí:
`OP_UMINUS` y `OP_NOT` dicen cuál es cuál.

La unificación de las macros en `GET_CHILD` eliminó siete copias del mismo
chequeo `childCount > n`.

### 2.2 La tabla de símbolos toma posesión del nombre

**Qué cambió.** `newSymbol` dejó de duplicar el `char *id` que recibe y pasó a
quedarse con el puntero.

**Por qué hizo falta.** El parser ya venía haciendo `strdup` del lexema, así que
el nombre se duplicaba dos veces: una en el parser y otra en la tabla, y la
primera copia quedaba huérfana. Transferir la propiedad deja **un solo dueño**,
que es lo que vuelve clara la pregunta de quién libera.

### 2.3 Constructores de `Symbol` por valor: se agregaron y se quitaron

**Qué cambió.** `symbol_table` ganó `makeIntSymbol`, `makeFloatSymbol`,
`makeBoolSymbol` y `makeVoidSymbol` —constructores que devuelven un `Symbol`
**por valor**— y después se eliminaron. Hoy no existen.

**Por qué se agregaron.** Los pidió el intérprete: cada expresión evaluada
produce un valor, y armarlo a mano eran cuatro líneas de inicialización
repetidas por todos lados.

**Por qué se quitaron.** Al sacar el intérprete del proyecto (§5) quedaron sin
un solo usuario fuera de sus propios tests. Es el mismo criterio que con
`hasExecute`: código que ya no sostiene nada se va.

### 2.4 El campo `hasExecute`: se agregó y se quitó

**Qué cambió.** Se agregó un `bool hasExecute` a `NodeAST` y se eliminó poco
después. Hoy no existe.

**Por qué se agregó.** La idea era que el optimizador marcara con `false` los
nodos que no hay que emitir —una rama descartada, una sentencia inalcanzable—
**sin desenganchar el nodo del árbol**, que es la operación riesgosa porque hay
que arreglar los punteros del padre.

**Por qué se quitó.** Porque ninguna optimización terminó trabajando sobre el
AST. Todas operan sobre el TAC, que es una lista plana donde sacar un elemento
es trivial y no hay punteros de padre que arreglar. El campo quedó sin un solo
usuario.

Se documenta en lugar de borrarse en silencio porque es la respuesta a **"¿por
qué no optimizaron el árbol?"**, que es una pregunta razonable. El argumento
completo está en la [ficha F15](wiki/05.9-optimizer.md).

---

## 3. Análisis semántico

### 3.1 El nombre de un método usado como valor

**Qué cambió.** El chequeo de tipos rechaza `x = f;` cuando `f` es un método.

**Por qué hizo falta.** Antes el identificador se resolvía contra la tabla, se
encontraba el símbolo del método y se tomaba su tipo de retorno como si fuera el
tipo de una variable. El programa pasaba el semántico y llegaba al generador de
código, que buscaba una ranura para algo que no es un dato. C-TDS no tiene
punteros a función: un método solo puede aparecer en una llamada.

### 3.2 Errores en cascada

**Qué cambió.** Una expresión con un operando ya inválido devuelve `TYPE_ERROR`
**sin reportar nada nuevo**.

**Por qué hizo falta.** Un único error producía una catarata de mensajes: el
primero real y el resto consecuencia del primero. Propagar `TYPE_ERROR` en
silencio hace que **un error primario produzca exactamente un mensaje**, que es
lo que vuelve usable la salida. Lo desarrolla la
[ficha F10](wiki/04.5-type-checker.md).

### 3.3 Validaciones redundantes

**Qué cambió.** Se quitaron chequeos del analizador semántico que el chequeo de
tipos ya hacía.

**Por qué hizo falta.** Estaban duplicados entre `semantic_analyzer.c` y
`type_checker.c`, con el riesgo clásico: dos copias de una regla que se
desincronizan y empiezan a dar mensajes distintos para el mismo programa. El
reparto quedó explícito —el analizador recorre y abre ámbitos, el chequeador
decide tipos— y está documentado en las fichas F10 y F11.

---

## 4. Código objeto

Estos dos son cambios a una etapa ya entregada (26/10) hechos durante el
optimizador.

### 4.1 `paramCount` obsoleto en el marco de pila

**Qué cambió.** Se eliminó el campo `paramCount` del `FunctionFrame` y los
envoltorios de registros que ya no se usaban.

**Por qué hizo falta.** Mientras existió el camino de punto flotante hubo
**dos contadores independientes** —uno por cada secuencia de registros que
define System V AMD64— y el contador único quedó sin sentido. Mantenerlo era
invitar a que alguien lo leyera creyendo que decía cuántos argumentos tiene la
función. Al recortar el alcance a enteros (§5) quedó un solo contador, pero la
distinción entre *cuántos entraron por registro* y *cuántos por la pila* sigue
haciendo falta.

### 4.2 Emisión modularizada por la regla de descenso

**Qué cambió.** `object_code.c` se partió en una función por opcode del TAC más
una capa de auxiliares, ordenadas de lo general a lo particular.

**Por qué hizo falta.** El despachador era un `switch` con el cuerpo de cada
traducción adentro, de varios cientos de líneas. Con una función por caso, cada
traducción se lee y se testea sola, y el `switch` queda como índice. El criterio
es el de *Convenciones del Código*; el resultado está inventariado en la
[ficha F18](wiki/06-object-code.md).

---

## 5. El recorte de alcance: fuera el intérprete y el `float` del back-end

Es el cambio más grande de la lista y toca **cuatro etapas a la vez**, así que
va aparte. La decisión fue ajustar el proyecto a lo que el enunciado pide y
nada más.

### Qué se sacó

| Qué | Dónde |
| --- | --- |
| El intérprete completo | `interpreter.c/h`, su suite, su página del sitio, el valor `interp` de `-target` |
| Los constructores `make*Symbol` | `symbol_table`, que solo los usaba el intérprete (§2.3) |
| El camino de punto flotante del generador de objeto | `object_code.c`: registros `%xmm`, `movss`, `comiss`, el pool de `.rodata` |
| Las coerciones `int`→`float` del TAC | `intermediate_code.c`: `coerceToFloat`, `coerceBinaryOperands` y el opcode `TAC_INT_TO_FLOAT` |
| El plegado de constantes reales | `optimizer.c`: `asFloat` y las ramas de `TYPE_FLOAT` |
| El operando constante real | `tac_operand`: `tacConstantFloat` y el campo `floatValue` |
| `floatParamCount` | `function_frame`, sin usuarios tras lo anterior |

### El bug que apareció en el medio, y por qué importa

El recorte se hizo en dos pasos, y **entre uno y otro el compilador quedó
miscompilando en silencio**. Al sacar el punto flotante del generador de código
objeto pero dejarlo en el TAC y en el chequeo de tipos, un `float` llegaba al
back-end y se traducía **como si fuera un entero**:

```asm
movl $1065353216, %eax     # los bits de 1.0f, cargados como entero
movl $1065353216, %edx
addl %edx, %eax            # suma ENTERA de dos patrones de bits
```

`1.0 + 1.0` no daba `2.0`, y el programa compilaba sin una sola queja. Las
comparaciones acertaban **por casualidad** en varios casos, porque el orden de
los patrones de bits IEEE-754 coincide con el de los enteros con signo, lo que
lo hacía todavía más difícil de notar.

La lección es la que ya estaba escrita en la documentación antes de que pasara:
**un tipo que el front-end acepta y el back-end no entiende es la peor
combinación posible**. Por eso el recorte terminó con un rechazo explícito en el
generador de TAC en lugar de con un hueco. Está en §9 de la
[ficha F13](wiki/05.5-tac-generation.md).

### Qué se mantuvo, y por qué

`float` **sigue siendo un tipo de C-TDS**: el lexer lo reconoce, la gramática lo
acepta, el AST y la tabla de símbolos lo representan y el chequeo de tipos
verifica la regla 14, que es una de las catorce que el análisis semántico debe
implementar. Sacarlo del front-end habría sido violar la especificación del
lenguaje para cumplir el alcance de una etapa. El corte quedó justo en la
frontera: **el lenguaje tiene reales, esta implementación no les genera
código**.

---

## Lo que no cambió, y por qué conviene decirlo

- **La gramática del lenguaje.** Fuera del no terminal `Assignment` —que no
  agrega ni saca programas válidos— la especificación sintáctica se respetó sin
  desvíos.
- **Las catorce reglas semánticas.** Ninguna se relajó ni se reinterpretó. Los
  cambios de esta página corrigen casos que **faltaban** verificar, no reglas
  distintas.
- **El formato del TAC.** Desde que se fijó, el generador de código objeto, el
  optimizador y los tests lo consumen sin que haya cambiado, que era la
  condición para que las tres cosas se pudieran escribir en paralelo.
