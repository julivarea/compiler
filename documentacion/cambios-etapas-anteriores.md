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

### 2.3 Constructores de `Symbol` por valor

**Qué cambió.** `symbol_table` ganó `makeIntSymbol`, `makeFloatSymbol`,
`makeBoolSymbol` y `makeVoidSymbol`: constructores que devuelven un `Symbol`
**por valor**, sin reservar nada.

**Por qué hizo falta.** Cada expresión evaluada produce un valor, y armarlo a
mano eran cuatro líneas de inicialización repetidas por todos lados. Con los
constructores, esa inicialización quedó centralizada en `symbol_table`.

Es el cambio que mejor muestra la **doble naturaleza de `Symbol`**: entrada de
la tabla de símbolos cuando tiene `id` y `uid`, y valor de una expresión cuando
no.

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

**Por qué hizo falta.** El soporte de `float` obligó a llevar **dos contadores
independientes** —uno para los argumentos enteros y otro para los de punto
flotante, porque System V AMD64 los reparte en dos secuencias de registros
distintas— y el contador único quedó sin sentido. Mantenerlo era invitar a que
alguien lo leyera creyendo que decía cuántos argumentos tiene la función.

### 4.2 Emisión modularizada por la regla de descenso

**Qué cambió.** `object_code.c` se partió en una función por opcode del TAC más
una capa de auxiliares, ordenadas de lo general a lo particular.

**Por qué hizo falta.** El despachador era un `switch` con el cuerpo de cada
traducción adentro, de varios cientos de líneas. Con una función por caso, cada
traducción se lee y se testea sola, y el `switch` queda como índice. El criterio
es el de *Convenciones del Código*; el resultado está inventariado en la
[ficha F18](wiki/06-object-code.md).

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
