# Análisis Semántico

**Ficha F11** · Archivos: `include/semantic_analyzer.h`, `src/semantic_analyzer.c` (328 líneas)

El recorrido que valida las reglas contextuales del lenguaje, construyendo la
[tabla de símbolos](04-symbol-table.md) y apoyándose en el
[chequeo de tipos](04.5-type-checker.md) para las expresiones.

---

## 1. Qué problema resuelve

Verificar todo lo que la gramática no puede expresar: que un identificador esté
declarado antes de usarse, que no se repita en el mismo ámbito, que la condición
de un `if` sea booleana, que un método no `void` devuelva un valor en todos sus
caminos, que exista `main`.

Es la última fase del front-end. Si termina sin errores, **el programa es válido
y las fases siguientes no vuelven a validar nada**.

---

## 2. Interfaz pública

Una sola función. Las otras nueve son `static`.

```c
Scope *analyzeSemantics(NodeAST *root, Scope *parent);
```

| Parámetro | Significado |
| --- | --- |
| `root` | El nodo cuya lista de hijos se va a analizar: la raíz del programa, o un bloque |
| `parent` | El ámbito contenedor. **`NULL` significa "este es el ámbito global"** |

**La función cumple dos papeles según `parent`**, y eso es lo que la vuelve
compacta:

- `parent == NULL`: es el punto de entrada. Crea el ámbito global y, al terminar,
  verifica que exista `main`. `driver.c` la llama una sola vez así:
  `analyzeSemantics(astRoot, NULL)`.
- `parent != NULL`: abre un ámbito anidado para un bloque, hereda el `function`
  del padre y se cuelga de él con `appendChildScope`.

El valor de retorno se puede descartar, y `driver.c` lo descarta: los nodos del
AST ya quedaron anotados y apuntando a los símbolos, así que el árbol es la
salida real de la fase. El ámbito solo hace falta si alguien quiere recorrer la
tabla después, que es lo que hace el intérprete.

**No hay un contador de errores en la interfaz.** La fase comunica el resultado a
través de la variable global `semantic_errors` de `error_handler`, que `driver.c`
consulta. Ver §7.

---

## 3. Los dos niveles del recorrido

### `analyzeSemantics`: abre un ámbito y recorre sus hijos

```c
Scope *currentScope = createScope(level, parent, function);
if (!isGlobalScope) appendChildScope(parent, currentScope);

for (int i = 0; i < root->childCount; i++) {
    analyzeNodeSemantics(root->children[i], currentScope);
}

if (isGlobalScope) checkMainMethodExists(currentScope);
```

### `analyzeNodeSemantics`: despacha un nodo

Un `switch` sobre `nodeType` con ocho casos y un `default` que recursa en los
hijos.

| Caso | A dónde va |
| --- | --- |
| `BLOCK_NODE` | `analyzeSemantics(node, currentScope)` — **ámbito nuevo** |
| `METHOD_DECLARATION_NODE` | `analyzeMethodNode` |
| `METHOD_CALL_NODE` | `analyzeMethodCall(node, scope, false)` — `false` = es sentencia |
| `VARIABLE_DECLARATION_NODE` | `analyzeVariableDeclarationNode` |
| `ASSIGNMENT_NODE` | `analyzeAssignmentNode` |
| `RETURN_NODE` | `analyzeReturnNode` |
| `IF_ELSE_NODE` | `analyzeIfElseNode` |
| `WHILE_NODE` | `analyzeWhileNode` |
| `default` | Recursa en los hijos con el **mismo** ámbito |

**La recursión mutua entre las dos funciones es lo que genera la jerarquía de
ámbitos.** Un `BLOCK_NODE` vuelve a `analyzeSemantics`, que crea un ámbito hijo;
todo lo demás se queda en el ámbito actual. Así la estructura de la tabla de
símbolos termina siendo un reflejo exacto del anidamiento de llaves del fuente.

**El `isExpression = false` del `METHOD_CALL_NODE`** es lo que permite invocar un
método con resultado e ignorarlo (`f();`). Llegando por acá la llamada es una
sentencia; llegando por `analyzeExpression` es una expresión y un método `void`
se rechaza. El mismo nodo, dos contextos.

---

## 4. Inventario de funciones

### `analyzeMethodNode(node, currentScope)`

La más interesante, por el orden en que hace las cosas:

```c
1. lookupSymbolInScope -> ¿ya existe ese nombre acá?
2. appendSymbol(currentScope, node->symbol);      <- ANTES del cuerpo
3. methodScope = createScope(level+1, currentScope, node->symbol);
4. analizar los parámetros   en methodScope
5. analizar el cuerpo        en methodScope
6. guaranteesReturn(body) si el método no es void
7. appendChildScope(currentScope, methodScope);
```

Dos decisiones cargan todo el peso:

**El paso 2 va antes del 5, y eso es lo que habilita la recursión.** Cuando se
analiza el cuerpo de `fact`, el símbolo `fact` ya está en el ámbito global, así
que `fact(n-1)` resuelve. Con el orden invertido daría "método no declarado".
Verificado: `fact(5)` compila y devuelve 120.

**Los parámetros y el cuerpo se analizan en el mismo `methodScope`.** Nótese que
el paso 5 itera `body->childCount` **directamente**, en lugar de llamar a
`analyzeSemantics(body, methodScope)`. Si llamara, las llaves del método abrirían
un ámbito extra y un local podría ocultar a un parámetro. Como no lo hace,
comparten ámbito y repetir el nombre es una redeclaración:

```c
int f(int n){ int n; n = 1; return n; }
// Error: La variable 'n' ya fue declarada previamente en este ambito.
```

Pero desde un bloque interno sí se puede, porque ese bloque sí abre ámbito:

```c
int f(int n){ { int n; n = 99; } return n; }
int main(){ return f(7); }   // devuelve 7: el return ve el parámetro
```

### `analyzeVariableDeclarationNode(node, currentScope)`

Dos líneas de lógica: si `lookupSymbolInScope` encuentra el nombre, error y **no
registra** el duplicado; si no, `appendSymbol`.

La misma función atiende globales, locales y parámetros formales: los tres son
`VARIABLE_DECLARATION_NODE` y lo único que cambia es el ámbito en el que se
registran.

### `analyzeAssignmentNode(node, currentScope)`

```c
node->type = TYPE_ERROR;   /* pesimista: solo se corrige si todo sale bien */
```

Arranca marcando el nodo como erróneo y solo le pone el tipo real en el camino
exitoso. Así el nodo mismo lleva el registro de si la asignación es válida, sin
una bandera aparte.

Después: resolver el nombre, exigir que sea una variable y no un método,
reapuntar `node->symbol` al símbolo de la tabla, y comprobar
`isAssignable(tipoDeLaVariable, tipoDeLaExpresión)`.

### `analyzeReturnNode(node, currentScope)`

Cuatro combinaciones, decididas con `currentScope->function->type`:

| Tiene valor | Método | Resultado |
| --- | --- | --- |
| No | `void` | Correcto |
| No | No `void` | `espera un valor de retorno de tipo INT, pero no se especifico ninguno` |
| Sí | `void` | `es void y no deberia retornar un valor` |
| Sí | No `void` | `isAssignable`: `return 1;` en un método `float` es válido |

Que use `isAssignable` y no igualdad exacta significa que el `return` hereda la
política de ensanchamiento: devolver un `int` donde se espera un `float` está
permitido, al revés no.

**Acá se ve para qué sirve que `function` se herede** en los ámbitos anidados: un
`return` dentro de un `while` dentro de un `if` sigue encontrando el método que
lo contiene, porque cada ámbito copió el `function` de su padre.

### `analyzeIfElseNode` y `analyzeWhileNode`

Las dos tienen la misma forma: analizar la condición, exigir que sea `TYPE_BOOL`,
y recursar en los bloques.

```c
bool isInvalidConditionType = (conditionType != TYPE_ERROR && conditionType != TYPE_BOOL);
```

El `!= TYPE_ERROR` es la **supresión de cascada**: si la condición ya falló por
otro motivo, no se agrega un segundo mensaje sobre su tipo.

**C-TDS no tiene condiciones "truthy".** `if (a)` con `a` entero es un error, no
una comparación implícita con cero:

```
La condicion del if debe ser de tipo BOOL, pero es INT.
```

Los bloques se pasan a `analyzeNodeSemantics` con el ámbito **actual**, no uno
nuevo. El ámbito lo crea el propio `BLOCK_NODE` al llegar al despachador.

### `guaranteesReturn(node)`

El análisis de alcanzabilidad. Ver §5.

### `checkMainMethodExists(globalScope)`

Tres chequeos al cerrar el ámbito global:

| Chequeo | Mensaje |
| --- | --- |
| Que exista | `El programa no contiene la definicion del metodo 'main'.` |
| Que sea un método | `'main' debe ser un metodo, no una variable.` |
| Que no tenga parámetros | `El metodo 'main' no debe tener parametros.` |

**No exige que sea `void`.** Tanto `void main()` como `int main()` son válidos, y
verificado que los dos compilan. Es importante en la práctica: los programas de
prueba usan `int main()` justamente para poder devolver un valor observable como
código de salida del proceso, ya que el lenguaje no tiene salida por pantalla.

Usa `lookupSymbolInScope` y no `lookupSymbol`, lo cual es correcto: `main` tiene
que estar en el ámbito global, y como ese ámbito no tiene padre las dos darían lo
mismo, pero la intención queda explícita.

---

## 5. `guaranteesReturn`: que todos los caminos retornen

Un método no `void` tiene que devolver un valor por **todos** sus caminos de
ejecución. La función recorre el árbol contestando "¿este subárbol garantiza un
return?":

```c
case RETURN_NODE:
    return true;

case BLOCK_NODE:
    for (int i = 0; i < node->childCount; i++)
        if (guaranteesReturn(node->children[i])) return true;
    return false;

case IF_ELSE_NODE: {
    NodeAST *elseBlock = GET_ELSE_BLOCK(node);
    if (elseBlock == NULL) return false;              /* sin else, puede no entrar */
    return guaranteesReturn(GET_IF_BLOCK(node)) && guaranteesReturn(elseBlock);
}

default:
    return false;
}
```

Las tres reglas, en palabras:

- Un `return` garantiza.
- Un bloque garantiza si **alguno** de sus hijos garantiza (basta uno: lo que
  venga después es inalcanzable).
- Un `if` garantiza si tiene rama `else` y **las dos** ramas garantizan. Sin
  `else`, la condición puede ser falsa y el `if` entero no garantiza nada.

### Verificación

| Programa | Resultado |
| --- | --- |
| `int f(){ return 1; }` | OK |
| `int f(int n){ if (n>0) { return 1; } else { return 2; } }` | OK |
| `int f(int n){ if (n>0) { return 1; } }` | **Error** |
| `int f(int n){ if (n>0) { return 1; } return 2; }` | OK |
| `int f(){ { return 1; } }` | OK — el bloque anidado cuenta |
| `int f(){ while (true) { return 1; } }` | **Error** |
| `void f(){ }` | OK — los `void` no se chequean |

### La limitación conocida: `while (true)`

El último caso es el que conviene poder explicar. `while (true) { return 1; }`
**siempre** retorna, pero el compilador lo rechaza, porque el `default` de la
función devuelve `false` para `WHILE_NODE` sin mirar su condición.

Es una elección **conservadora y deliberada**: para saber que un ciclo siempre se
ejecuta hay que evaluar su condición en compilación, lo que requiere plegado de
constantes y, en general, es indecidible. El costo es rechazar algunos programas
válidos; el beneficio es no aceptar nunca uno inválido, que es el error que
importa. La solución estándar en los lenguajes que lo soportan es tratar
`while (true)` como caso especial reconocido sintácticamente.

Es, además, exactamente el tipo de respuesta que una pregunta sobre análisis de
alcanzabilidad busca: **el análisis estático de flujo es necesariamente
aproximado, y la pregunta es de qué lado se equivoca.**

---

## 6. El catálogo de errores semánticos

Los que reporta este archivo, todos verificados ejecutándolos. Los de las
expresiones (tipos de operandos, aridad de llamadas, argumentos incompatibles)
están en la [ficha F10](04.5-type-checker.md).

| Situación | Mensaje |
| --- | --- |
| Variable repetida en el mismo ámbito | `La variable 'g' ya fue declarada previamente en este ambito.` |
| Método repetido | `El metodo 'f' ya fue declarado previamente en este ambito.` |
| Asignación a algo no declarado | `Asignacion a variable no declarada 'noexiste'.` |
| Asignación al nombre de un método | `Ese nombre de simbolo corresponde al metodo 'f'.` |
| Condición de `if` no booleana | `La condicion del if debe ser de tipo BOOL, pero es INT.` |
| Condición de `while` no booleana | `La condicion del while debe ser de tipo BOOL, pero es INT.` |
| Tipo incompatible en asignación | `El tipo de la expresion no coincide con la variable 'a'.` |
| `return;` en método no `void` | `El metodo 'f' espera un valor de retorno de tipo INT, pero no se especifico ninguno.` |
| `return expr;` en método `void` | `El metodo 'f' es void y no deberia retornar un valor.` |
| Tipo de retorno incompatible | `El tipo de retorno no coincide en el metodo 'f': se esperaba INT pero se obtuvo FLOAT.` |
| Camino sin `return` | `El metodo 'f' no es void: todos sus caminos de ejecucion deben terminar en un return.` |
| Falta `main` | `El programa no contiene la definicion del metodo 'main'.` |
| `main` no es un método | `'main' debe ser un metodo, no una variable.` |
| `main` con parámetros | `El metodo 'main' no debe tener parametros.` |
| Sin memoria para un ámbito | `Error: no se pudo reservar memoria para el ambito.` |

Un detalle de redacción: declarar `int g;` y después `int g(){...}` reporta
**`El metodo 'g' ya fue declarado previamente`**, aunque el conflicto sea con una
variable. El mensaje lo emite `analyzeMethodNode`, que no mira de qué categoría
era el símbolo que ya estaba.

---

## 7. La política de diagnóstico acumulativo

**Esta fase no aborta en el primer error.** Sigue recorriendo el AST completo
para juntar todos los errores independientes del archivo:

```c
void main(){
    int a;
    a = true;        // linea 3
    if (a) { }       // linea 4
    noexiste = 1;    // linea 5
    while (3) { }    // linea 6
}
```

```
Error semantico en la linea 3: El tipo de la expresion no coincide con la variable 'a'.
Error semantico en la linea 4: La condicion del if debe ser de tipo BOOL, pero es INT.
Error semantico en la linea 5: Asignacion a variable no declarada 'noexiste'.
Error semantico en la linea 6: La condicion del while debe ser de tipo BOOL, pero es INT.

Compilacion abortada: se encontraron 4 errores semanticos.
```

Cuatro mensajes en una corrida. **Es la diferencia de fondo con el análisis
sintáctico**, que corta en el primero: después de un error de sintaxis el parser
no sabe en qué estado quedó y seguir produciría errores inventados, mientras que
acá cada sentencia se puede validar de forma independiente de las demás.

La contracara es el riesgo de **errores en cascada**: un identificador no
declarado podría generar un mensaje por cada operación que lo use. Eso se evita
con `TYPE_ERROR` propagándose en silencio, descrito en la
[ficha F10](04.5-type-checker.md).

### El corte del pipeline

Cada `semanticError()` incrementa la global `semantic_errors`. Al terminar el
recorrido, `runSemantic` en `driver.c` la consulta:

```c
if (semantic_errors > 0) {
    fprintf(stderr, "\nCompilacion abortada: se encontraron %d errores semanticos.\n", semantic_errors);
    return 1;
}
```

Si hubo al menos uno, la compilación termina con código 1 y **no se generan ni
el código intermedio ni el assembly**. Esa garantía es la que permite que el
generador de TAC no revalide nada: si llega a ejecutarse, el programa es válido.

---

## 8. Conexión con las fases vecinas

| Viene de | Qué cruza la frontera |
| --- | --- |
| `ast.c` (F08) | El árbol con `type` en `TYPE_VOID`, `uid` en 0 y símbolos provisorios |
| `symbol_table.c` (F09) | `createScope`, `appendSymbol`, las dos búsquedas |
| `type_checker.c` (F10) | `analyzeExpression`, `analyzeMethodCall`, `isAssignable` |

| Va hacia | Qué entrega |
| --- | --- |
| `intermediate_code.c` (F13) | El **mismo árbol**, anotado: tipos resueltos, `uid` asignados, símbolos apuntando a la tabla. Más la garantía de que el programa es válido |
| `interpreter.c` (F19) | Ídem, y además el `Scope` devuelto |

El reparto con F10, que es la confusión más natural:

| | `semantic_analyzer.c` (F11) | `type_checker.c` (F10) |
| --- | --- | --- |
| Recorre | Declaraciones y sentencias | Expresiones |
| Pregunta | "¿Está permitido esto acá?" | "¿De qué tipo es esto?" |
| Dueño de | La jerarquía de ámbitos, el catálogo de reglas | Las reglas de tipos de los operadores |

---

## 9. Cómo se extiende

### Una construcción de control nueva (`for`, `do while`)

Si se desazucara en la gramática a partir de `while` e `if`
(ver la [ficha F06](02.5-syntax-analysis.md)), **este archivo no se toca**: el
árbol que llega está hecho de nodos que ya se saben analizar. Es el argumento
más fuerte a favor de ese camino.

Si en cambio lleva un `NodeType` propio, hace falta:

1. Un caso en el `switch` de `analyzeNodeSemantics`.
2. Una función `analyzeXNode` que valide lo suyo (típicamente, que la condición
   sea booleana y recursar en el cuerpo).
3. Decidir su regla en `guaranteesReturn`. **Es el paso que se olvida**, y
   olvidarlo es seguro por el lado correcto: el `default` devuelve `false`, o sea
   que la construcción nueva no garantizará retorno y a lo sumo se rechazarán
   programas válidos.

Si se omite el paso 1, el `default` del despachador recursa en los hijos, así que
las sentencias de adentro se analizan **en el ámbito equivocado** y sin las
validaciones propias de la construcción. No da error de compilación: es un fallo
silencioso.

### `break` y `continue`

Necesitan un dato que hoy no existe: saber si el recorrido está dentro de un
ciclo. La forma natural es agregar un parámetro de profundidad de ciclo a
`analyzeNodeSemantics`, o un campo en `Scope` análogo a `function`. Ese segundo
camino es más elegante y reusa el mecanismo de herencia que ya funciona.

### Declaración adelantada de métodos

Hoy un método solo ve a los declarados antes que él, porque el registro y el
análisis de los cuerpos ocurren en el mismo recorrido. Se arregla con **dos
pasadas** sobre los hijos del ámbito global: la primera registra todas las firmas
con `appendSymbol`, la segunda analiza los cuerpos. La tabla de símbolos ya
soporta el caso; el cambio es de unas pocas líneas acá.

### Reconocer `while (true)`

Para aceptar `int f(){ while (true) { return 1; } }` alcanza con un caso en
`guaranteesReturn` que detecte un `WHILE_NODE` cuya condición sea un
`CONSTANT_NODE` booleano con valor verdadero, y en ese caso delegue en el cuerpo.
Es la extensión más chica que amplía el conjunto de programas aceptados.

---

## 10. Trampas y límites conocidos

- **`while (true)` no garantiza retorno** (§5). Es deliberado y conservador.
- **`node->symbol = resolvedSymbol` filtra el símbolo provisorio.** Ver la
  [ficha F07](02.7-parser-actions.md).
- **El chequeo de `return` fuera de una función es código muerto.** El mensaje
  `Sentencia 'return' fuera del cuerpo de una funcion.` no se puede provocar: un
  `return` suelto en el nivel global es **error de sintaxis**, porque la gramática
  solo admite sentencias dentro de un bloque. Verificado. Es defensa razonable,
  pero no corresponde a ningún programa alcanzable.
- **El `hasExpression` de `analyzeAssignmentNode` siempre es verdadero.** La
  gramática es `ID '=' Expression ';'`, así que el nodo siempre tiene su hijo. Otro
  chequeo defensivo sin caso real.
- **El mensaje de duplicado dice "metodo" aunque el conflicto sea con una
  variable** (§6).
- **El `default` del despachador no avisa.** Un `NodeType` nuevo sin su caso cae
  ahí y se recorre a medias, sin un solo error de compilación (§9).
- **La fase no exige que `main` sea `void`** (§4), al contrario de lo que decía
  esta misma página antes.
- **El ámbito global nunca se libera.** `analyzeSemantics` devuelve el `Scope` y
  `driver.c` lo descarta sin llamar a `freeScope`. Son las fugas que reporta el
  detector al salir, igual que con el AST.

---

## 11. La teoría detrás

- **Análisis semántico**: la fase que verifica las restricciones que una gramática
  libre de contexto no puede expresar. "Un identificador debe estar declarado
  antes de usarse" es el ejemplo canónico: requiere memoria de lo visto, y una
  gramática libre de contexto no tiene forma de llevarla.
- **Restricciones sensibles al contexto**: el nombre técnico de lo que valida esta
  fase. Se podrían expresar con una gramática sensible al contexto, pero el
  resultado sería impracticable; en la práctica todos los compiladores las
  resuelven con un recorrido sobre el árbol más una tabla de símbolos.
- **Análisis estático** frente a chequeo en tiempo de ejecución: todo lo que esta
  fase verifica se decide **sin ejecutar** el programa. Es también la razón de sus
  límites: la división por cero o el desborde no se pueden detectar acá en
  general.
- **Análisis de alcanzabilidad** (*reachability* / *definite return analysis*):
  `guaranteesReturn` es una versión mínima de un análisis de flujo de control. A
  diferencia del [análisis de variables vivas](../live_variable_analysis.md), que
  resuelve un punto fijo sobre un grafo, acá alcanza con una recursión sobre el
  árbol, porque las construcciones del lenguaje son estructuradas (no hay `goto`)
  y el único caso que bifurca es el `if`.
- **Aproximación conservadora**: un análisis estático que no puede ser exacto debe
  elegir de qué lado equivocarse. Acá se elige rechazar programas válidos antes
  que aceptar inválidos, que es la dirección correcta para un chequeo de
  corrección. La dirección opuesta (ser optimista) es la que corresponde a un
  optimizador, donde equivocarse significa perder una optimización y no romper el
  programa.
- **Recuperación de errores**: la decisión de acumular en lugar de abortar, más
  `TYPE_ERROR` como tipo propagable, es la técnica estándar para reportar varios
  errores independientes en una corrida sin generar cascadas.
- **Árbol decorado**: la salida de la fase no es una estructura nueva sino el
  mismo AST con atributos resueltos. Es la implementación práctica de una
  gramática de atributos cuando los atributos no se pueden calcular durante el
  parseo.

---

## 12. Cómo se valida

- **Suite:** `tests/test_semantic.c` (327 líneas), con un caso válido y uno
  inválido por regla.
- **A mano:** la etapa escribe el AST **anotado** en un `.sem`, que comparado con
  el `.sint` del mismo programa muestra exactamente lo que resolvió:

```bash
./mi_compilador -target parse examples/programa.ctds   # .sint: sin anotar
./mi_compilador -target sem   examples/programa.ctds   # .sem: tipos y uid resueltos
```

- Los ejemplos inválidos de `examples/` sirven de regresión del catálogo de
  errores. Estos cuatro **tienen que terminar con código 1**:

```bash
./mi_compilador -target sem examples/programa_con_error.ctds            # tipos en operación binaria
./mi_compilador -target sem examples/method_without_return.ctds         # camino sin return
./mi_compilador -target sem examples/asignation_method.ctds             # asignación al nombre de un método
./mi_compilador -target sem examples/aliasing_parameters_in_method.ctds # local que repite un parámetro
```

  Y `examples/two_returns.ctds` **tiene que compilar** (código 0): un `return`
  inalcanzable después de otro no es un error semántico, solo código muerto que
  el optimizador elimina después.

---

**Siguiente:** [La Representación Intermedia (TAC)](05-tac-representation.md)
