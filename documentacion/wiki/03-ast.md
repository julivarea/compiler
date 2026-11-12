# Árbol de Sintaxis Abstracta (AST)

**Ficha F08** · Archivos: `include/ast.h`, `src/ast.c` (350 líneas)

La estructura que produce el [parser](02.5-syntax-analysis.md) y que consumen
todas las fases siguientes.

---

## 1. Qué problema resuelve

Representar el programa como un árbol donde cada nodo es una construcción del
lenguaje y sus hijos son sus partes, descartando todo lo que era solo sintaxis
(paréntesis, llaves, comas, palabras clave). Es **la interfaz entre el front-end
y el resto del compilador**: el semántico lo anota y el generador de TAC lo
recorre.

Si hay una estructura que conviene conocer de memoria para la defensa, es esta.

---

## 2. Interfaz pública

| Función | Para qué |
| --- | --- |
| `newNode(nodeType, symbol, left, mid, right)` | Constructor genérico, hasta tres hijos |
| `newOperatorNode(operation, left, right)` | Nodo de operación unaria o binaria |
| `newMethodDeclaration(returnType, name, parameters, body)` | Declaración de método |
| `newIntLiteralNode(int)` | Literal entero |
| `newFloatLiteralNode(float)` | Literal real |
| `newBoolLiteralNode(int)` | Literal booleano |
| `freeAST(node)` | Libera un subárbol en postorden |
| `fprintAST(out, node, level)` | Imprime el árbol a un stream |
| `printAST(node, level)` | Lo mismo, a `stdout` |

Más el enum `NodeType`, la estructura `NodeAST` y ocho macros de acceso (§7).

**Quién lo usa:** `bison.y` y `parser_helpers.c` para construirlo;
`semantic_analyzer.c` y `type_checker.c` para anotarlo; `intermediate_code.c`
para generar el TAC; `driver.c` para imprimirlo en `-target parse` y `-target sem`.

---

## 3. La estructura `NodeAST`

```c
typedef struct NodeAST {
    NodeType nodeType;           /* Categoría sintáctica */
    DataType type;               /* Tipo de dato inferido o declarado */
    Symbol *symbol;              /* Símbolo asociado (NULL si no aplica) */
    OperationType operationType; /* Operador (solo válido en OPERATOR_NODE) */
    int line;                    /* Línea del fuente */
    struct NodeAST **children;   /* Arreglo dinámico de hijos */
    int childCount;              /* Cuántos hijos hay */
} NodeAST;
```

Siete campos, y conviene saber por qué está cada uno:

| Campo | Por qué existe |
| --- | --- |
| `nodeType` | Lo que discrimina el `switch` de todas las fases. Es el "qué soy" |
| `type` | El tipo de dato. Lo pone el **análisis semántico**, no el parser: al construirse vale `TYPE_VOID` |
| `symbol` | El nombre y los datos del identificador o constante. `NULL` en los nodos que no nombran nada (`BLOCK_NODE`, `IF_ELSE_NODE`, `WHILE_NODE`, `RETURN_NODE`, `OPERATOR_NODE`, `PARAMETERS_NODE`) |
| `operationType` | Qué operador es. **Solo tiene sentido si `nodeType == OPERATOR_NODE`**: ver §12 |
| `line` | Para que los errores de las fases posteriores apunten a algún lugar del fuente. Ver §9 |
| `children` / `childCount` | Los hijos. Arreglo y no punteros fijos: ver abajo |

### Por qué un nodo genérico y no uno por construcción

La alternativa de manual es una estructura por tipo de nodo (`IfNode` con campos
`condition`, `thenBlock`, `elseBlock`; `WhileNode` con `condition` y `body`…),
unidas por una unión etiquetada. Da seguridad de tipos: `ifNode->elseBlock` no
compila sobre un `while`.

Acá se eligió un **nodo genérico con arreglo de hijos**, y la razón es que hay
construcciones de aridad variable: un bloque tiene N sentencias, un método N
parámetros, una llamada N argumentos. Con campos fijos esas tres igual
necesitarían un arreglo, así que se unificó todo en el mismo mecanismo.

El precio es que **el significado de cada posición de `children` no está en el
tipo**, es una convención. Las macros de §7 son el parche para eso, y la tabla de
§4 es la convención completa. Es la decisión de diseño más discutible del
proyecto, y la pregunta "¿por qué no un nodo por construcción?" tiene esa
respuesta: aridad variable.

---

## 4. El layout de hijos de cada nodo

**Esta es la tabla que hay que saber.** Verificada contra `bison.y`, contra
`describeNode` y contra la salida real de `-target sem`.

| `NodeType` | `symbol` | Hijos | `operationType` |
| --- | --- | --- | --- |
| `BLOCK_NODE` | — | N: declaraciones primero, sentencias después | — |
| `VARIABLE_DECLARATION_NODE` | La variable | 0 | — |
| `METHOD_DECLARATION_NODE` | El método | **Siempre 2**: `[parameters, body]` | — |
| `PARAMETERS_NODE` | — | N declaraciones de parámetro (puede ser 0) | — |
| `ASSIGNMENT_NODE` | La variable destino | 1: la expresión | — |
| `RETURN_NODE` | — | 0 o 1: la expresión si la hay | — |
| `IF_ELSE_NODE` | — | 2: `[cond, ifBlock]` · 3 con else: `[cond, ifBlock, elseBlock]` | — |
| `WHILE_NODE` | — | 2: `[cond, block]` | — |
| `OPERATOR_NODE` | — | 1 si es unario, 2 si es binario | **El operador** |
| `METHOD_CALL_NODE` | El método | N argumentos (puede ser 0) | — |
| `ID_NODE` | La variable referenciada | 0 | — |
| `CONSTANT_NODE` | El valor del literal | 0 | — |

Tres consecuencias que se usan todo el tiempo:

- **Un `if` sin `else` tiene 2 hijos, no 3 con el tercero en `NULL`.** La forma de
  preguntar si hay rama else es `GET_ELSE_BLOCK(node) == NULL`, que devuelve
  `NULL` porque `childCount` es 2.
- **`METHOD_DECLARATION_NODE` siempre tiene exactamente 2 hijos**, incluso si el
  método no tiene parámetros: en ese caso `children[0]` es un `PARAMETERS_NODE`
  vacío, no `NULL`. Por eso ninguna fase necesita chequear antes de entrar.
- **Un bloque mezcla declaraciones y sentencias en un solo arreglo**, en el orden
  del fuente. La gramática garantiza que las declaraciones vienen primero.

### Verificación

```c
void main(){
    int a;
    a = 1;
    while (a < 3) { a = a + 1; }
    if (a > 2) { a = 5; }
    if (a > 9) { a = 6; } else { a = 7; }
    return;
}
```

```
  |   | Bloque
  |   |   | Declaracion Variable: a #2 (Tipo INT)
  |   |   | Asignacion a: a #2 (Tipo INT)
  |   |   |   | Constante INT: 1
  |   |   | While
  |   |   |   | Operacion: <          <- hijo 0: condicion
  |   |   |   | Bloque                <- hijo 1: cuerpo
  |   |   | If-Else
  |   |   |   | Operacion: >          <- hijo 0
  |   |   |   | Bloque                <- hijo 1, sin else: solo 2 hijos
  |   |   | If-Else
  |   |   |   | Operacion: >
  |   |   |   | Bloque                <- hijo 1
  |   |   |   | Bloque                <- hijo 2: rama else
  |   |   | Return                    <- 0 hijos: return sin expresion
```

---

## 5. La compactación de hijos en `newNode`

Este es el mecanismo menos obvio del archivo y **es carga estructural**: si
alguien lo "arregla", se rompe el recorrido del `while`.

```c
int count = 0;
NodeAST *temp[3];
if (left)  temp[count++] = left;
if (mid)   temp[count++] = mid;
if (right) temp[count++] = right;
```

`newNode` recibe hasta tres hijos y **descarta los `NULL`, corriendo los que
quedan hacia la izquierda**. O sea que la posición en la que un hijo termina no
es la posición del parámetro.

El caso donde esto importa es el `while`. La gramática lo construye así:

```
WHILE '(' Expression ')' Block   { $$ = newNode(WHILE_NODE, NULL, $3, NULL, $5); }
```

El cuerpo se pasa como **tercer** hijo (`right`), con el del medio en `NULL`.
Como la compactación corre los no nulos hacia la izquierda, el cuerpo **aterriza
en el índice 1**, que es exactamente donde lo busca `GET_WHILE_BLOCK`:

```c
#define GET_WHILE_BLOCK(node)  GET_CHILD(node, 1)
```

Verificado arriba: el `While` del ejemplo tiene 2 hijos y el bloque está en el 1.

Si la compactación se cambiara por una que preserve posiciones (`children[2] =
right`), el `while` quedaría con `children[1] == NULL` y `GET_WHILE_BLOCK`
devolvería `NULL`: el cuerpo del ciclo desaparecería del TAC sin un solo error de
compilación. Es el tipo de cambio "de limpieza" que rompe todo en silencio.

Lo mismo hace funcionar al `if` sin `else`: se pasa `(cond, ifBlock, NULL)` y
queda con 2 hijos.

---

## 6. Inventario de funciones

### `newNode(nodeType, symbol, left, mid, right)`

El constructor genérico. Reserva el nodo, pone `type = TYPE_VOID`, `line =
yylineno`, compacta los hijos (§5) y aplica la heurística de línea (§9).

**No inicializa `operationType`.** Ver §12.

### `newOperatorNode(operation, left, right)`

Envuelve a `newNode` con `OPERATOR_NODE` y después setea `operationType`. Para un
unario se llama con `right == NULL` y el nodo queda con un solo hijo.

Si el `newNode` falla, libera los dos subárboles que recibió: no los deja
colgando sin dueño.

### `newMethodDeclaration(returnType, methodName, parameters, body)`

La única que **no** usa la compactación de `newNode`: crea el nodo sin hijos y
después arma el arreglo de 2 a mano.

```c
node->children = malloc(2 * sizeof(NodeAST *));
node->childCount = 2;
node->children[0] = parameters;
node->children[1] = body;
```

Y tiene que ser así: si pasara `parameters` y `body` por `newNode`, un método sin
parámetros compactaría el cuerpo al índice 0 y el layout `[parameters, body]`
dejaría de valer. Acá se fuerza la aridad fija.

Crea además el `Symbol` del método con `METHOD_SYMBOL` y el tipo de retorno, y
guarda el nodo de parámetros en `symbol->parameters`, que es lo que después usa
el chequeo de tipos para validar las llamadas.

Tiene cuatro caminos de error, cada uno liberando exactamente lo que ya había
reservado más los subárboles recibidos.

### `newIntLiteralNode`, `newFloatLiteralNode`, `newBoolLiteralNode`

Las tres tienen la misma forma: crean un `Symbol` con `CONSTANT_SYMBOL`, le
guardan el valor en la unión y lo envuelven en un `CONSTANT_NODE`.

Dos detalles:

- **El símbolo de una constante no tiene nombre**: `newSymbol(NULL, ...)`. No hay
  identificador que nombrar.
- **Los booleanos usan `value.intValue`**, no un campo propio: `true` es 1 y
  `false` es 0. Esa representación se mantiene en todo el compilador, hasta el
  assembly.

### `freeAST(node)`

Recorrido **postorden**: primero los hijos, después el arreglo, después el
símbolo si le pertenece, y al final el nodo. El orden importa: liberar el nodo
antes que los hijos perdería los punteros.

La regla de propiedad del símbolo es lo interesante, y está en §8.

### `fprintAST(out, node, level)` y `printAST(node, level)`

El impresor. Recorrido **preorden** con indentación de `  | ` por nivel: primero
se describe el nodo, después se bajan los hijos con `level + 1`.

`printAST` es `fprintAST(stdout, ...)`. Los dos se usan: `driver.c` llama a
`fprintAST` para escribir el archivo `.sint`/`.sem` y a `printAST` para la salida
de `-debug`.

### `describeNode(out, node)` (privada)

El `switch` que imprime una línea por tipo de nodo. Es lo que define el formato
de los archivos `.sint` y `.sem`, así que **cambiarlo cambia la salida de dos
etapas**.

Vale notar qué incluye cada línea, porque es lo que se lee al depurar:
`Declaracion Variable: a #2 (Tipo INT)` muestra nombre, `uid` y tipo;
`Operacion: +` muestra el operador; las constantes muestran el valor.

El `default` imprime `Nodo Desconocido (Tipo %d)`, que es la red de seguridad si
se agrega un `NodeType` y se olvida el caso acá.

### `getOperationName(operation)` (privada)

Traduce el `OperationType` al símbolo que se imprime. `OP_UMINUS` se muestra como
`- (unario)` para distinguirlo de la resta.

### `getDataTypeName(type)`

Traduce el `DataType` a `"INT"`, `"BOOL"`, `"FLOAT"`, `"VOID"`, `"ERROR"` o
`"UNKNOWN"`. **Está declarada en `types.h` pero definida acá**, que es una
pequeña incoherencia de ubicación: pertenece conceptualmente a la ficha F04.

---

## 7. Las macros de acceso

```c
#define GET_CHILD(node, index) ((node)->childCount > (index) ? (node)->children[(index)] : NULL)
#define GET_EXPRESSION(node)   GET_CHILD(node, 0)
#define GET_LEFT(node)         GET_CHILD(node, 0)
#define GET_RIGHT(node)        GET_CHILD(node, 1)
#define GET_CONDITION(node)    GET_CHILD(node, 0)
#define GET_IF_BLOCK(node)     GET_CHILD(node, 1)
#define GET_ELSE_BLOCK(node)   GET_CHILD(node, 2)
#define GET_WHILE_BLOCK(node)  GET_CHILD(node, 1)
```

Son **nombres para las posiciones de la tabla de §4**. Siete de las ocho son
alias de `GET_CHILD(node, 0)` o `(node, 1)`: `GET_EXPRESSION`, `GET_LEFT` y
`GET_CONDITION` son literalmente lo mismo. La diferencia es de documentación:
`GET_CONDITION(ifNode)` dice qué se está leyendo, `node->children[0]` no.

**`GET_CHILD` chequea el rango** y devuelve `NULL` si el índice no existe. Eso es
lo que hace que `GET_ELSE_BLOCK` sea seguro sobre un `if` sin else, y lo que
permite que el código de las fases posteriores no tenga que mirar `childCount`.

Al ser macros y no funciones, `node` se evalúa varias veces: pasarles una
expresión con efectos secundarios (`GET_LEFT(siguiente())`) la ejecutaría tres
veces. En el proyecto siempre se les pasa una variable, así que no se nota.

---

## 8. La regla de propiedad de los símbolos

En `freeAST`:

```c
bool hasOwnedSymbol = (node->symbol != NULL &&
    (node->nodeType == VARIABLE_DECLARATION_NODE ||
     node->nodeType == METHOD_DECLARATION_NODE ||
     node->nodeType == CONSTANT_NODE ||
     node->symbol->uid == 0));
if (hasOwnedSymbol) {
    freeSymbol(node->symbol);
}
```

La pregunta que contesta es: **¿este nodo es dueño de su símbolo, o solo lo
referencia?** Y es crítica, porque los símbolos se **comparten**: después del
análisis semántico, todos los `ID_NODE` que mencionan la misma variable apuntan al
*mismo* `Symbol`, el que está registrado en la tabla de símbolos.

| Condición | Significa |
| --- | --- |
| Es una declaración de variable o de método | El nodo creó ese símbolo: es el dueño |
| Es una constante | El símbolo se creó solo para el literal, nadie más lo comparte |
| `symbol->uid == 0` | **El símbolo nunca se registró en ningún ámbito**: sigue siendo el provisorio que creó el parser |

El `uid` lo asigna `appendSymbol` al registrar un símbolo en un ámbito, y arranca
en 0. Entonces `uid == 0` es la forma de preguntar "¿este símbolo quedó fuera de
la tabla?". Si está en la tabla (`uid != 0`), liberarlo desde el nodo sería una
doble liberación: el dueño es la tabla.

Es una regla que funciona pero que **codifica la propiedad en un valor de dato**
en lugar de en el tipo. Un campo `bool ownsSymbol` sería más explícito.

### Dónde esto se conecta con una fuga real

La regla está pensada para que los símbolos provisorios (`uid == 0`) los libere
`freeAST`. Pero no alcanza, por dos motivos:

1. **El análisis semántico sobreescribe el puntero.** Cuando resuelve un
   identificador, hace `node->symbol = resolvedSymbol`, y el provisorio deja de
   ser alcanzable desde cualquier nodo: `freeAST` ya no puede verlo. Es la fuga
   medida en la [ficha F07](02.7-parser-actions.md), sección *Trampas*.
2. **`freeAST` nunca se llama sobre el árbol completo.** `main.c` no lo invoca:
   el AST vive hasta que termina el proceso. Las únicas llamadas a `freeAST` en
   todo el compilador están en `parser_helpers.c`, y son caminos de error por
   falta de memoria.

Eso explica también por qué el detector de fugas no reporta el AST entero: el
árbol sigue siendo alcanzable desde la variable global `astRoot`, así que no
cuenta como perdido. Lo que sí reporta son los símbolos que quedaron sin
referencia y los ámbitos.

Para un compilador que procesa un archivo y termina es una decisión defendible
—el sistema operativo recupera todo— pero conviene poder decirlo como decisión y
no como olvido.

---

## 9. El campo `line` y su heurística

```c
node->line = yylineno;

bool hasConditionLine = ((nodeType == WHILE_NODE || nodeType == IF_ELSE_NODE) && left != NULL);
if (hasConditionLine) {
    node->line = left->line;
}
```

Por defecto, la línea de un nodo es `yylineno` **en el momento de la reducción**,
que es la línea del *último* token que se consumió. Para una construcción que
ocupa varias líneas, eso apunta al final y no al principio.

Para `if` y `while` se corrige tomando la línea de la condición, que está mucho
más cerca de donde el programador escribió la construcción.
`newMethodDeclaration` hace lo análogo con la línea de los parámetros.

### Verificación del efecto

Con la heurística, un `if` cuya condición está en la línea 4 y cuyo cierre está
en la 7 reporta la línea 4:

```c
    if (a)        // linea 4
    {
        a = 2;
    }             // linea 7
```

```
Error semantico en la linea 4: La condicion del if debe ser de tipo BOOL, pero es INT.
```

Sin heurística, una asignación que empieza en la línea 7 y termina en la 9
reporta la 9:

```c
    b =           // linea 7
        a +
        1;        // linea 9
```

```
Error semantico en la linea 9: El tipo de la expresion no coincide con la variable 'b'.
```

O sea: la precisión de los números de línea **no es uniforme**. Está arreglada
donde más molestaba (las estructuras de control, que son las más largas) y queda
aproximada en el resto. Una solución completa pediría que el léxico guardara la
línea de inicio de cada token y que cada acción de la gramática la propagara.

---

## 10. Conexión con las fases vecinas

| Viene de | Qué cruza la frontera |
| --- | --- |
| `bison.y` y `parser_helpers.c` (F06, F07) | Los nodos se crean durante las reducciones; `children` lo puebla `resolveNodeArray` |
| `symbol_table.c` (F09) | `Symbol`, `newSymbol`, `freeSymbol` |

| Va hacia | En qué estado |
| --- | --- |
| Análisis semántico (F09–F11) | `type` en `TYPE_VOID`, `uid` en 0, símbolos provisorios. **El semántico anota el árbol en el lugar** |
| Generación de TAC (F13) | Árbol ya anotado: `type` resuelto en cada expresión, símbolos apuntando a la tabla |

El punto importante: **el AST no se copia ni se transforma entre fases**. Es la
misma estructura, que el semántico enriquece. Comparar el `.sint` con el `.sem`
del mismo programa muestra exactamente qué agrega:

```
.sint ->   | Declaracion Variable: a #0 (Tipo INT)
.sem  ->   | Declaracion Variable: a #1 (Tipo INT)
```

```
.sint ->   | Asignacion a: a #0 (Tipo VOID)
.sem  ->   | Asignacion a: a #1 (Tipo INT)
```

---

## 11. Cómo se extiende

### Una construcción nueva que necesita nodo propio (`do while`, `for`)

1. Agregar el valor al enum `NodeType`.
2. Agregar su caso a `describeNode`, o el `.sint` lo va a imprimir como
   `Nodo Desconocido`.
3. **Definir y documentar su layout de hijos** en la tabla de §4, y agregar las
   macros de acceso si hacen falta.
4. Si la aridad es fija y algún hijo puede ser `NULL`, **no usar `newNode`**:
   construir el arreglo a mano como hace `newMethodDeclaration`, porque la
   compactación correría las posiciones.
5. Agregar el caso en el semántico y el generador de TAC. El
   `switch` de cada uno tiene un `default`, así que **olvidarse de uno no da error
   de compilación**: la construcción simplemente no hace nada.

El punto 5 es el que conviene mencionar en la defensa: agregar un `NodeType` es
barato, pero hay cuatro lugares que lo tienen que saber y el compilador no avisa
si falta uno.

### Un operador nuevo

No toca `NodeType`: alcanza con un valor nuevo en `OperationType` (`types.h`), su
caso en `getOperationName` y el manejo en las fases posteriores. Reutiliza el
`OPERATOR_NODE` que ya existe.

### Hacer el layout seguro por tipo

La mejora estructural que más reduciría el riesgo: reemplazar las macros por
funciones de acceso que validen `nodeType`, por ejemplo
`NodeAST *getIfElseBlock(NodeAST *node)` que falle si el nodo no es un
`IF_ELSE_NODE`. Convierte un error silencioso en un error ruidoso, al costo de
una llamada por acceso.

---

## 12. Trampas y límites conocidos

- **`operationType` no se inicializa en `newNode`.** Solo `newOperatorNode` lo
  setea. Leerlo en un nodo que no es `OPERATOR_NODE` es leer memoria sin
  inicializar, y el valor va a ser basura reproducible, que es la peor clase: en
  una corrida parece funcionar. El header lo aclara ("solo válido en
  `OPERATOR_NODE`") pero nada lo impide.
- **La compactación de hijos es carga estructural** (§5). El `while` depende de
  ella para que su cuerpo caiga en el índice 1.
- **`type` vale `TYPE_VOID` al construirse**, no es el tipo real. Cualquier fase
  que lea `type` antes del análisis semántico lee un valor sin sentido. Es lo que
  se ve en el `.sint`.
- **Un `if` sin `else` tiene `childCount == 2`**, no 3 con `NULL`. Recorrer con
  `for (i = 0; i < 3; i++)` lee fuera del arreglo.
- **El árbol nunca se libera** (§8). `main.c` no llama a `freeAST`.
- **La propiedad del símbolo depende de `uid == 0`**, un valor de dato, no de un
  campo explícito. Si alguna vez `appendSymbol` empezara a asignar `uid` 0 a algo
  registrado, la regla se rompería en silencio y produciría dobles liberaciones.
- **Los `switch` de las fases consumidoras tienen `default` silencioso.** Agregar
  un `NodeType` y olvidar un caso compila sin avisos.

---

## 13. La teoría detrás

- **Árbol de sintaxis abstracta** frente a **árbol de parseo** (*parse tree* o
  *concrete syntax tree*): el árbol de parseo tiene un nodo por cada símbolo de
  cada producción aplicada, incluidos los terminales sin contenido semántico. El
  AST conserva solo lo necesario para las fases siguientes. Acá se construye
  directamente el AST desde las acciones de la gramática, sin materializar nunca
  el árbol de parseo: `'(' Expression ')'` devuelve el nodo de adentro y los
  paréntesis no dejan rastro.
- **Representación intermedia arbórea**: el AST es la primera de las tres
  representaciones del programa que maneja este compilador. Después vienen el
  TAC (lineal, F12–F13) y el assembly. Cada una es más cercana a la máquina y
  más lejana de la estructura del lenguaje.
- **Anotación del árbol**: el análisis semántico no genera una estructura nueva
  sino que **decora** la existente (el campo `type`, los punteros a símbolos
  resueltos). Es la forma habitual de implementar una gramática de atributos
  cuando los atributos no se pueden calcular en una sola pasada ascendente.
- **Recorridos**: `freeAST` es **postorden** (los hijos antes que el padre,
  porque hay que liberar de abajo hacia arriba) y `fprintAST` es **preorden** (el
  padre antes que los hijos, porque es el orden en que se lee la indentación). La
  generación de TAC usa postorden para las expresiones, que es lo que hace que
  los operandos estén calculados antes de la operación.
- **Unión etiquetada** (*tagged union*): el patrón que `NodeAST` implementa a
  mano con `nodeType` como etiqueta. En un lenguaje con tipos algebraicos cada
  construcción sería un constructor del tipo y el compilador verificaría que no
  falte ningún caso, que es exactamente la red de seguridad que acá falta (§12).

---

## 14. Cómo se valida

- **Suite:** `tests/test_ast.c` (483 líneas). Verifica la forma del árbol que
  produce cada construcción: cantidad de hijos, orden, layouts de `if`/`while`,
  declaraciones múltiples y bloques vacíos.
- **A mano:** los dos archivos que imprimen el árbol son la herramienta de
  depuración principal de todo el front-end:

```bash
./mi_compilador -target parse examples/programa.ctds   # -> .sint, sin anotar
./mi_compilador -target sem   examples/programa.ctds   # -> .sem, anotado
```

  Compararlos con `diff` muestra exactamente lo que agregó el análisis semántico.

---

**Siguiente:** [Tabla de Símbolos y Ámbitos](04-symbol-table.md)
