# Generación de Código Intermedio

**Archivos:** `include/intermediate_code.h`, `src/intermediate_code/intermediate_code.c` (392 líneas)

El recorrido que aplana el [AST](../03-lexico-sintactico-y-ast/04-ast.md) anotado a la lista lineal de
[cuádruplas](01-representacion-tac.md).

!!! note "Esta etapa cubre tipos enteros y lógicos"
    El enunciado acota el código intermedio a **tipos enteros y lógicos**. El
    generador lo hace explícito: antes de emitir nada recorre el AST y, si
    encuentra un `float`, **rechaza el programa** con su línea en vez de generar
    código que el back-end no sabría traducir. Ver §9.

---

## 1. Qué problema resuelve

Convertir un árbol en una secuencia. El AST dice *qué* hace el programa de forma
jerárquica; el TAC dice *en qué orden* se ejecutan las operaciones elementales.
El trabajo concreto es triple:

- **Descomponer las expresiones anidadas** en operaciones de a una, inventando
  temporales para los resultados intermedios.
- **Traducir el control de flujo** —`if`, `while`, corto circuito— a etiquetas y
  saltos, que es la única forma de control que tiene una lista plana.
- **Rechazar lo que queda fuera del alcance de la etapa**, que es el único
  chequeo que hace: ver §9.

Concluye el front-end. Si esta fase corre, el programa ya es válido: **no
revalida nada**.

---

## 2. Interfaz pública

```c
const TACList *generateIntermediateCode(NodeAST *root);
bool hasUnsupportedTypeError(void);
```

La segunda solo le dice al driver si la generación se abortó por un `float`
(§9).

La llama `buildIntermediateCode` en `driver.c`, una vez por compilación. Al
entrar, descarta la lista de una generación anterior y reinicia los contadores,
así que es reentrante entre invocaciones aunque no durante una.

**El `const` del retorno es una promesa que el compilador mismo rompe.** El
optimizador necesita modificar la lista, así que `driver.c` lo descarta con un
cast:

```c
int changes = optimizeIntermediateCode((TACList *)tacList, enabled);
```

Es decir que el `const` documenta la intención ("el generador ya terminó con
esto") pero no la hace cumplir. Conviene saberlo: no es un descuido, es un cast
deliberado en `buildIntermediateCode` de `driver.c`.

---

## 3. El estado del módulo

Cinco variables estáticas. Un generador con estado, no una función pura:

| Variable | Para qué |
| --- | --- |
| `currentCode` | La lista que se está construyendo. `emit` le agrega al final |
| `tacMemoryError` | Bandera pegajosa: si una reserva falla, se descarta todo al terminar |
| `tacUnsupportedType` | Si la última generación se abortó por un `float` (§9) |
| `temporaryCount` | El próximo número de temporal. **Se reinicia en cada método** |
| `labelCount` | El próximo número de etiqueta. **Es global a todo el programa** |

**Los temporales se numeran por método y las etiquetas por programa.**
`generateMethodDeclarationCode` pone `temporaryCount = 0` al empezar, así que
cada función arranca en `t0`:

```c
int g(int x){ return x + 1; }      ->  t0 = x@2 + 1
int f(int a, int b){ return a * b; }  ->  t0 = a@4 * b@5
```

Repetir `t0` entre funciones no confunde a nadie: un temporal solo existe dentro
de su función, y el marco de pila de cada una es independiente (ver [marco de pila](../07-codigo-objeto/01-marco-de-pila.md)). Las
etiquetas, en cambio, terminan siendo símbolos del assembly (`.L3`), así que
tienen que ser únicas en todo el archivo; por eso `labelCount` no se reinicia.

---

## 4. Un único punto de emisión

Todos los generadores agregan instrucciones con la misma función:

```c
static void emit(TACOperation operation, TACOperand result,
                 TACOperand firstOperand, TACOperand secondOperand);
```

`emit` llama a `appendInstruction` de la [representación TAC](01-representacion-tac.md)
sobre `currentCode` y, si la reserva falla, prende `tacMemoryError` (§11). Cada
sitio pasa los cuatro campos de la cuádrupla, con `tacNone()` en los que el
opcode no usa:

```c
emit(TAC_IF_FALSE, tacNone(), condition, elseIf);   /* condición y etiqueta destino */
emit(TAC_LABEL, elseIf, tacNone(), tacNone());       /* la etiqueta va en result */
```

Hay que conocer en qué campo va cada cosa, y la asimetría que más confunde es la
de las etiquetas: en `TAC_LABEL` la etiqueta va en **`result`**, en `TAC_GOTO` en
**`firstOperand`** y en los saltos condicionales en **`secondOperand`**. La tabla
de opcodes de [representación TAC](01-representacion-tac.md) lo resume.

## 5. Los dos dispatchers

La estructura espeja la del [análisis semántico](../04-semantico/03-analisis-semantico.md), y la
diferencia entre las dos funciones es la idea central del archivo.

### `generateStatementCode(node)` — no devuelve nada

Una sentencia **se ejecuta**: emite instrucciones y no produce un valor.

| Caso | A dónde va |
| --- | --- |
| `METHOD_DECLARATION_NODE` | `generateMethodDeclarationCode` |
| `ASSIGNMENT_NODE` | `generateAssignmentCode` |
| `METHOD_CALL_NODE` | `generateCallStatementCode` |
| `RETURN_NODE` | `generateReturnCode` |
| `IF_ELSE_NODE` | `generateIfElseCode` |
| `WHILE_NODE` | `generateWhileCode` |
| `VARIABLE_DECLARATION_NODE` | `generateVariableDeclarationCode` |
| `BLOCK_NODE` y `default` | `generateBlockCode`: recursa en los hijos |

`generateIntermediateCode` arranca con `generateBlockCode(root)`, así que la raíz
se recorre igual que cualquier bloque. **Lo que separa el `global x@1, INT` del
`local x@3, INT` es el símbolo, no la posición en el árbol:**
`generateVariableDeclarationCode` emite `TAC_GLOBAL_DECL` si
`node->symbol->isGlobal` y `TAC_LOCAL_DECL` si no. Esa marca la dejó la tabla de
símbolos al registrar la variable en el ámbito global (ver el
[ADR 002](../../decisiones/adr-002-resolucion-variables-globales-isglobal.md)).

### `generateExpressionCode(node)` — devuelve un `TACOperand`

Una expresión **produce un valor**, y la función devuelve **el operando donde
quedó ese valor**. Esa convención es lo que hace que la recursión componga:

| Caso | Qué devuelve | ¿Emite? |
| --- | --- | --- |
| `ID_NODE` | `tacVariable(symbol)` | **No** |
| `CONSTANT_NODE` | `generateConstantOperand`: el `tacConstant*` del tipo | **No** |
| `OPERATOR_NODE` | El temporal del resultado | Sí |
| `METHOD_CALL_NODE` | El temporal del resultado | Sí |

**Que los identificadores y las constantes no emitan nada es lo que evita el
código inútil.** `x + 1` no genera `t0 = x; t1 = 1; t2 = t0 + t1`, sino
directamente `t0 = x@3 + 1`, porque los dos operandos ya eran operandos válidos
sin necesidad de copiarlos a un temporal.

---

## 6. Cómo se aplana una expresión

`generateOperatorCode` es el caso general:

```c
TACOperand left = generateExpressionCode(GET_LEFT(node));        /* 1 */
TACOperand result = newTemporary(node->type);                    /* 2 */
NodeAST *rightNode = GET_RIGHT(node);
TACOperand right = (rightNode != NULL) ? generateExpressionCode(rightNode)  /* 3 */
                                       : tacNone();

emit(getTACOperation(node->operationType), result, left, right);  /* 4 */
return result;
```

**Unario o binario se decide por la forma del nodo**, no por el operador: un
`OPERATOR_NODE` unario (`-x`, `!x`) tiene un solo hijo, así que `GET_RIGHT`
devuelve `NULL` y el segundo operando queda en `tacNone()`. No hace falta una
lista de qué operadores son unarios.

El recorrido es **postorden**: los operandos se generan antes de la operación
que los consume, que es justamente lo que el TAC necesita (una instrucción no
puede contener expresiones).

`a + b * c` produce:

```
t0 = b * c      <- la multiplicacion, que esta mas abajo en el arbol
t1 = a + t0     <- la suma, que la consume
```

### Un detalle que se ve en la numeración

El temporal del resultado se reserva en el paso **2**, o sea **entre** el
operando izquierdo y el derecho. Entonces los números de temporal no siguen el
orden de emisión:

```c
int main(){ return a(1) + b(2); }
```

```
param 1
t0 = call a, 1      <- izquierdo: se emite primero, usa t0
param 2
t2 = call b, 1      <- derecho: se emite despues, usa t2
t1 = t0 + t2        <- el resultado reservo t1 antes de generar el derecho
```

`t1` se reservó antes que `t2` pero se emite después. No es un bug: es el orden
en que las llamadas a `newTemporary` ocurren en la recursión. Verlo ayuda a
entender el recorrido.

---

## 7. El corto circuito

`&&` y `||` se desvían a `generateShortCircuitCode` antes de llegar al caso
general, porque **no se pueden expresar como una operación binaria**: eso
obligaría a evaluar los dos operandos.

```text
       result = a && b                          result = a || b
    ───────────────────────                  ───────────────────────
    result = a                               result = a
    if result false goto L_end               if result true goto L_end
    result = b                               result = b
    L_end:                                   L_end:
```

La implementación es la misma para los dos; lo único que cambia es la variante
del salto:

```c
TACOperation jumpOp = (node->operationType == OP_AND) ? TAC_IF_FALSE : TAC_IF_TRUE;
emit(jumpOp, tacNone(), result, endLabel);
```

Y ahí está la razón de que existan **las dos** variantes del salto condicional en
el catálogo de opcodes: con una sola habría que emitir una negación intermedia
para uno de los dos operadores.

### Por qué un temporal nuevo y no la variable izquierda

Se podría pensar en reusar el operando izquierdo como acumulador. No se puede:
si la expresión izquierda era una **variable del programa**, asignarle el
resultado la modificaría. El temporal es el lugar neutro donde acumular.

Verificado, con el segundo operando teniendo un efecto observable:

```c
int d;
boolean side() { d = 1; return true; }
int main() { boolean r; d = 0; r = false && side(); if (d == 0) { return 42; } return 1; }
```

Devuelve **42**: `side()` nunca se ejecutó, así que `d` siguió en 0. El salto
hizo su trabajo.

---

## 8. El control de flujo sin backpatching

### `if` sin `else`

```
if cond false goto endIf
   <bloque>
endIf:
```

### `if` con `else`

```
if cond false goto elseIf
   <bloque then>
goto endIf
elseIf:
   <bloque else>
endIf:
```

### `while`

```
start:
   <condicion>
if cond false goto end
   <cuerpo>
goto start
end:
```

La condición queda **adentro** del ciclo, después de `start:`, que es lo que hace
que se reevalúe en cada iteración.

### Por qué no hace falta backpatching

En la bibliografía, generar saltos hacia adelante durante un recorrido de una
sola pasada requiere **backpatching**: emitir el salto con el destino vacío,
anotar su posición en una lista, y volver a completarlo cuando se conoce la
dirección.

Acá no hace falta, y la razón es doble:

1. **Las etiquetas son nombres, no direcciones.** `newLabel()` devuelve `L3`
   antes de que exista ninguna instrucción `L3:`. Emitir `if t false goto L3` es
   perfectamente válido: la referencia se resuelve recién en la generación de
   código objeto, cuando el ensamblador calcula los desplazamientos.
2. **El AST ya está completo.** El backpatching es necesario cuando se genera
   código *durante* el parseo, donde el cuerpo de un `if` todavía no se leyó. Acá
   la generación ocurre después, sobre un árbol entero, así que la estructura se
   conoce de antemano.

El patrón que resulta es siempre el mismo: pedir las etiquetas **primero**, usarlas
en los saltos, y emitir sus definiciones cuando corresponda.

```c
TACOperand elseIf = newLabel();                       /* se piden las dos */
TACOperand endIf  = newLabel();
emit(TAC_IF_FALSE, tacNone(), condition, elseIf);     /* se usa una... */
...
emit(TAC_LABEL, elseIf, tacNone(), tacNone());        /* ...y se define despues */
```

### Verificación con ciclos anidados

```c
int main(){
    int i; int j; int s;
    i = 0; s = 0;
    while (i < 3) {
        j = 0;
        while (j < 2) {
            if (j > 0) { s = s + 1; }
            j = j + 1;
        }
        i = i + 1;
    }
    return s;
}
```

```
L0:                      <- while externo: inicio
t0 = i@2 < 3
if t0 false goto L1
j@3 = 0
L2:                      <- while interno: inicio
t1 = j@3 < 2
if t1 false goto L3
t2 = j@3 > 0
if t2 false goto L4      <- if sin else
t3 = s@4 + 1
s@4 = t3
L4:
t4 = j@3 + 1
j@3 = t4
goto L2                  <- cierra el interno
L3:
t5 = i@2 + 1
i@2 = t5
goto L0                  <- cierra el externo
L1:
ret s@4
```

Los pares quedan correctamente anidados: `L0`/`L1` el externo, `L2`/`L3` el
interno, `L4` el `if`. El programa devuelve 3, que es el valor correcto.

Nótese que `L0` y `L1` se numeran juntos aunque `L1:` aparezca al final: las dos
etiquetas del ciclo se piden al empezar, antes de generar nada del cuerpo.

---

## 9. El rechazo de los tipos fuera de alcance

Es lo **único** que esta fase valida, y no valida semántica: verifica que el
programa esté dentro del alcance que el enunciado le fijó a la etapa, que es
**tipos enteros y lógicos**.

```c
NodeAST *unsupported = findUnsupportedType(root);
if (unsupported != NULL) {
    fprintf(stderr, "Error en la linea %d: la generacion de codigo para el tipo"
                    " 'float' no esta soportada: esta etapa cubre tipos enteros"
                    " y logicos.\n", unsupported->line);
    tacUnsupportedType = true;
    return NULL;
}
```

`findUnsupportedType` recorre el AST y devuelve el **primer** nodo cuyo tipo, o
el de su símbolo, sea `TYPE_FLOAT`. Mirar las dos cosas es lo que hace que
alcance con un solo recorrido: el tipo del nodo cubre las expresiones y el del
símbolo cubre las declaraciones, los parámetros y el tipo de retorno de un
método.

**Por qué acá y no en el análisis semántico.** Porque no es un error del
programa: `float` es un tipo legítimo de C-TDS y la regla 14 de la
especificación lo contempla, así que el front-end lo acepta entero —`-target
sem` sobre un programa con reales termina sin errores—. Lo que no está en
alcance es **generarle código**, y esta es la primera fase que lo haría. El
corte queda justo en la frontera entre lo que el lenguaje permite y lo que la
etapa cubre.

**Por qué rechazar y no ignorar.** Porque la alternativa es peor: si el
generador emitiera las operaciones de todos modos, el código objeto trataría el
patrón de bits IEEE-754 como si fuera un entero y haría sumas enteras sobre él.
`1.0 + 1.0` no daría `2.0`, y el programa **compilaría sin una sola queja**. Un
error explícito con su línea es siempre mejor que un ejecutable que calcula mal.

`hasUnsupportedTypeError()` permite al driver distinguir este rechazo —ya
reportado, con línea— de una falla interna del generador, y por eso no imprime
encima su mensaje genérico.

## 10. Métodos, llamadas y el retorno implícito

### La estructura de un método

```c
temporaryCount = 0;                                            /* t0 en cada método */
emit(TAC_BEGIN_FUNC, method, tacConstantInt(parameterCount), tacNone());
for (cada parámetro) emit(TAC_PARAM_DECL, tacVariable(...), tacNone(), tacNone());
generateStatementCode(cuerpo);
if (es void y la última instrucción no es ret) emit(TAC_RETURN, tacNone(), tacNone(), tacNone());
emit(TAC_END_FUNC, method, tacNone(), tacNone());
```

El `parameterCount` va en `BEGIN_FUNC` para que el generador de código objeto
sepa cuántos argumentos levantar de los registros sin contar los `PARAM_DECL`.

### El retorno implícito

```c
bool needsImplicitReturn = (node->type == TYPE_VOID && !lastInstructionIsReturn());
```

Un método `void` puede terminar sin `return` escrito, pero el código objeto
necesita un punto de salida. `lastInstructionIsReturn` resuelve la pregunta
mirando `currentCode->tail`:

```c
return currentCode->tail != NULL && currentCode->tail->operation == TAC_RETURN;
```

Es el **puntero a la cola de la lista usado como mirada hacia atrás de una
instrucción**. Verificado: `void vacio(){ }` produce `begin_func vacio, 0` /
`ret` / `end_func vacio`.

Los métodos no `void` no necesitan el agregado, porque el análisis semántico ya
garantizó con `guaranteesReturn` que todos sus caminos terminan en un `return`.
**La corrección de esta fase depende de esa garantía.**

### Las llamadas

`generateCallArguments` emite un `TAC_PARAM` por argumento, en orden de índice
creciente, o sea **de izquierda a derecha** como exige el lenguaje:

```c
for (int i = 0; i < node->childCount; i++) {
    TACOperand argument = generateExpressionCode(node->children[i]);
    emit(TAC_PARAM, tacNone(), argument, tacNone());
}
```

**El `node->symbol` de una llamada tiene que apuntar al método declarado**, no
al provisorio que armó el parser con solo el nombre: de ahí salen el nombre y el
tipo de retorno que lleva el `TAC_CALL`. Quien lo reapunta es
`analyzeMethodCall` en el chequeo de tipos, con `resolveNodeSymbol(node, method)`.

### Llamada como expresión contra llamada como sentencia

| | `generateCallExpressionCode` | `generateCallStatementCode` |
| --- | --- | --- |
| `result` | Un temporal nuevo | `tacNone()` |
| Salida | `t1 = call suma, 2` | `call nada, 0` |

Las dos emiten los mismos `TAC_PARAM` antes. La diferencia es solo si hay destino,
y es lo que permite invocar un método con resultado e ignorarlo.

---

## 11. Detección de errores internos

El archivo tiene tres puntos donde reporta un **error interno del TAC**, que
señalan un bug del compilador y no un programa inválido:

| Dónde | Cuándo |
| --- | --- |
| `generateIntermediateCode` | No se pudo crear la lista |
| `emit` | No se pudo reservar una instrucción; prende `tacMemoryError` |
| `getTACOperation` | Un `OperationType` sin traducción |

El de `getTACOperation` es el interesante. Su `default` reporta y devuelve
`TAC_ASSIGN`:

```c
default:
    fprintf(stderr, "Error interno del TAC: tipo de operacion no soportada (%d).\n", operation);
    return TAC_ASSIGN;
```

Sin ese aviso, un operador nuevo agregado al AST pero olvidado acá generaría una
asignación en lugar de la operación, y el programa compilaría **produciendo un
resultado incorrecto en silencio**. El comentario sobre la función aclara además
que `OP_AND` y `OP_OR` no están en el `switch` a propósito, porque se desvían
antes.

`tacMemoryError` es pegajoso: si se prende, al terminar se descarta la lista
entera y se devuelve `NULL`, en lugar de entregar un TAC truncado que el
generador de código objeto traduciría a un programa roto.

---

## 12. Conexión con las fases vecinas

| Viene de | Qué cruza la frontera |
| --- | --- |
| El AST anotado | `node->type` resuelto en cada expresión, `node->symbol` apuntando a la tabla, y la garantía de validez |
| `tac.c` | Los constructores de operandos y `appendInstruction` |

| Va hacia | Qué entrega |
| --- | --- |
| `optimizer.c` | La lista, que modifica en el lugar tras el cast de `const` |
| `object_code.c` | La lista, traducida instrucción por instrucción |
| `driver.c` | La lista, que se vuelca al `.ci` con `formatTACList` |

Lo que esta fase **garantiza** a las siguientes, y que conviene poder enumerar:

- Toda expresión está descompuesta en operaciones de a lo sumo dos operandos.
- Todo método está delimitado por `begin_func`/`end_func` y **termina en un
  `ret`**, sea escrito o implícito.
- Toda etiqueta referenciada por un salto está definida en alguna parte de la
  lista.
- Los nombres de variables son únicos en todo el programa, por el sufijo `@uid`.

---

## 13. Cómo se extiende

### Un operador binario nuevo

Un caso en `getTACOperation` que lo mapee a su opcode. Nada más acá: el caso
general de `generateOperatorCode` ya hace el resto.

### Una construcción de control nueva

Si se desazucaró en la gramática a `while` e `if`, **este archivo no se toca**,
que es el argumento fuerte de ese camino. Si lleva nodo propio:

1. Su caso en `generateStatementCode`.
2. Una función `generateXCode` que emita el patrón de etiquetas, siguiendo el
   esquema de §8: pedir las etiquetas primero, usarlas en los saltos, definirlas
   después.

Olvidar el paso 1 hace que el nodo caiga en el `default`, que es
`generateBlockCode`: **recorrería los hijos sin emitir el control de flujo**. Un
`for` olvidado generaría el cuerpo una sola vez, sin ciclo y sin un solo error de
compilación. Es el fallo silencioso más peligroso de esta ficha.

### `break` y `continue`

Necesitan saber a qué etiqueta saltar, o sea una **pila de etiquetas de ciclo**
en el estado del módulo: al entrar a un `while` se apilan su `start` y su `end`,
al salir se desapilan, y `break` salta al `end` del tope. Es el agregado más
natural al estado actual.

---

## 14. Trampas y límites conocidos

- **El `const` del retorno se descarta con un cast** en `driver.c` (§2).
- **Los números de temporal no siguen el orden de emisión** (§6), porque el
  temporal del resultado se reserva entre los dos operandos.
- **Los temporales se repiten entre métodos** (§3): cada función arranca en
  `t0`. Las etiquetas, en cambio, son únicas en todo el programa.
- **No hay plegado de constantes acá.** `2 + 3` genera `t0 = 2 + 3`, no `t0 = 5`.
  Es deliberado: el plegado es trabajo del [optimizador](../06-optimizador/02-optimizador.md), y mezclarlo
  acá haría que el `.ci` sin `-opt` no reflejara el programa escrito.
- **El `default` de `generateStatementCode` es `generateBlockCode`**, que recorre
  los hijos. Un `NodeType` nuevo sin caso propio no da error: hace algo
  parcialmente razonable y mal (§13).
- **`getTACOperation` devuelve `TAC_ASSIGN` ante un operador desconocido**
  después de avisar. El aviso va a `stderr`, así que en una corrida con salida
  redirigida se puede pasar por alto.
- **La fase confía en `guaranteesReturn`.** Si el análisis semántico dejara pasar
  un método no `void` sin `return` en todos sus caminos, el TAC quedaría con un
  camino que cae en el `end_func` sin `ret`, y el código objeto generaría una
  función que se ejecuta más allá de su epílogo.
- **`generateMethodDeclarationCode` asume que el método tiene su
  `PARAMETERS_NODE`** (`GET_LEFT(node)`), aunque esté vacío. Lo garantiza el
  parser, que siempre lo crea.

---

## 15. La teoría detrás

- **Generación de código intermedio**: la fase que traduce la representación del
  front-end a la IR. Es el último paso que depende del lenguaje fuente y el
  primero que se piensa en términos de ejecución.
- **Recorrido en postorden para expresiones**: generar los operandos antes de la
  operación es lo que respeta la dependencia de datos. Es el mismo orden en que
  una máquina de pila evaluaría la expresión.
- **Traducción dirigida por la sintaxis** (*syntax-directed translation*): a cada
  construcción del lenguaje le corresponde un esquema fijo de código a emitir. Las
  funciones `generateIfElseCode`, `generateWhileCode` y
  `generateShortCircuitCode` son exactamente los esquemas de traducción de la
  bibliografía, escritos como código.
- **Backpatching**: la técnica para completar saltos hacia adelante cuando el
  destino todavía no se conoce. Acá **no se usa**, y poder explicar por qué
  —etiquetas simbólicas más un AST ya completo— es una buena respuesta (§8). El
  backpatching es inevitable cuando se genera código durante el parseo, como en
  los compiladores de una sola pasada.
- **Código de salto para expresiones booleanas** (*jumping code*): representar
  `a && b` con control de flujo en lugar de con un valor. Es la implementación
  canónica del corto circuito, y la bibliografía la presenta justamente como la
  alternativa al **código de valor numérico**, que calcularía los dos operandos.
- **Temporales ilimitados**: generar tantos como haga falta y posponer la decisión
  de dónde viven. Es lo que permite separar la traducción de la asignación de
  almacenamiento, que acá resuelve el [marco de pila](../07-codigo-objeto/01-marco-de-pila.md).

---

## 16. Cómo se valida

- **Suite:** `tests/test_tac.c` compara la lista generada por cada
  construcción, instrucción por instrucción. Cada caso arma con
  `tests/support/ast_builder.h` el AST **anotado** que dejaría el análisis
  semántico (símbolos resueltos con su `uid`, alcance global o local y tipo de cada
  expresión), de modo que la suite no depende del front-end: si falla, el defecto
  está en el generador.
- **A mano:** el `.ci` es la salida directa de esta fase:

```bash
./mi_compilador -target codinter examples/programa.ctds
```

- **Chequeos estructurales** que el TAC siempre debe cumplir, útiles como
  regresión al tocar el generador:

| Propiedad | Cómo se mira |
| --- | --- |
| Toda etiqueta referenciada está definida | Comparar los `goto L` e `if ... goto L` con las líneas `L:` |
| Cada `begin_func` tiene su `end_func` | Contar los dos y ver que coincidan |
| Antes de cada `end_func` hay un `ret` | Mirar la línea anterior a cada `end_func` |

- **La prueba que importa de verdad** es que el programa compilado haga lo mismo
  que se escribió. Para el corto circuito, con el segundo operando teniendo un
  efecto observable:

```bash
./mi_compilador -o /tmp/sc examples/programa.ctds && /tmp/sc; echo $?
```

---

**Siguiente:** [Análisis de Variables Vivas](../06-optimizador/01-variables-vivas.md)
