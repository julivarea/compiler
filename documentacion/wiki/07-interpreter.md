# Intérprete

**Ficha F19** · Archivos: `include/interpreter.h`, `src/interpreter.c` (590 líneas)

Ejecuta el programa recorriendo el [AST](03-ast.md) directamente, sin pasar por
el TAC ni por el back-end. **Es una extensión nuestra y no forma parte de la
entrega.**

---

## 1. Qué problema resuelve

Dos cosas distintas, y conviene separarlas:

**La declarada.** La tabla de calificación del enunciado incluye un ítem
*Intérprete*, aunque no figura en el cronograma de entregas. Al consultarlo se
confirmó que no hacía falta implementarlo; se hizo igual.

**La que terminó importando.** Es una **segunda implementación independiente** de
la semántica de C-TDS, y por lo tanto un **oráculo** contra el cual verificar el
back-end. Si el intérprete y el ejecutable compilado difieren sobre el mismo
programa, uno de los dos tiene un bug — y eso se detecta sin tener que calcular a
mano el resultado esperado de cada programa de prueba.

Está cruzado contra el ejecutable sobre un corpus de 20 programas (aritmética,
corto circuito, recursión, `while` anidados, métodos `void`, ocultamiento de
scopes, más de 6 argumentos, reales y globales): **coinciden en los 20**.

---

## 2. Interfaz pública

Una sola función.

```c
Scope *interpreteSemantics(NodeAST *root, bool debug);
```

La llama `driver.c` en el caso `TARGET_INTERP`, después del análisis semántico:

```c
Scope *globalScope = interpreteSemantics(astRoot, options->debug);
freeScope(globalScope);
```

Devuelve el ámbito global con los valores finales, que el driver libera. **Es la
única fase que libera su propia estructura**: ni el AST ni la tabla de símbolos
del compilador se liberan.

El parámetro `debug` llega hasta `executeMainMethod`, que es el único lugar que
informa algo: el valor que devolvió `main`, y **solo si se pasó `-debug`**.

```bash
./mi_compilador -debug -target interp examples/factorial.ctds
[INTERPRETE] 'main' finalizo correctamente (void)
```

---

## 3. Las dos estructuras que lo hacen funcionar

### `ExecResult`: cómo se propaga un `return`

```c
typedef struct ExecResult {
    bool hasReturned;
    Symbol value;
} ExecResult;
```

Es la pieza central del diseño, y resuelve la pregunta "¿cómo se implementa un
`return` en un intérprete que recorre un árbol?".

El problema: un `return` puede estar anidado dentro de un `if` dentro de un
`while` dentro de un bloque, y tiene que **abandonar todos esos niveles de
recursión de C** y entregar el valor al llamador. Las opciones son un salto no
local (`setjmp`/`longjmp`), una excepción —que C no tiene—, o lo que se hizo acá:

**cada sentencia devuelve si retornó, y cada nivel lo chequea y lo propaga.**

```c
for (int i = 0; i < node->childCount; i++) {
    result = executeStatement(node->children[i], blockScope);
    if (result.hasReturned) {
        return result;        /* corta el bloque y sube */
    }
}
```

```c
while (isTruthy(evaluateExpression(GET_CONDITION(node), currentScope))) {
    result = executeStatement(GET_WHILE_BLOCK(node), currentScope);
    if (result.hasReturned) {
        break;                /* corta el ciclo */
    }
}
```

El `break` del ciclo y el `return` del bloque son los que convierten un `return`
de C-TDS en el desenrollado de la pila de C. Es más verboso que una excepción y
perfectamente explícito.

### `Symbol` como valor, no como entrada de tabla

El intérprete usa la misma estructura `Symbol` de la
[tabla de símbolos](04-symbol-table.md) para **dos papeles distintos**:

| Papel | Cómo se obtiene |
| --- | --- |
| Entrada de la tabla | `newSymbol(...)` en el heap, registrado en un `Scope` |
| **Valor de una expresión** | `makeIntSymbol(7)`, `makeFloatSymbol(1.5)`… **por valor** |

Evaluar `2 + 3` devuelve un `Symbol` por valor con `type = TYPE_INT` y
`value.intValue = 5`. No tiene nombre ni `uid`: no es una declaración, es un
resultado.

Por eso `evaluateIdentifier` termina en `return *symbol;` — **copia** el símbolo
de la tabla. Leer una variable produce una copia de su valor, que es exactamente
lo que corresponde en un lenguaje por valor.

Esa reutilización es economía de código a costa de que `Symbol` signifique dos
cosas según de dónde venga. Un tipo `Value` propio sería más claro.

---

## 4. Los dos recorridos

La estructura espeja la del front-end: un despachador de sentencias y uno de
expresiones.

### `executeStatement` → `ExecResult`

| Caso | Qué hace |
| --- | --- |
| `BLOCK_NODE` | `executeBlock`: **abre un ámbito nuevo** y ejecuta los hijos |
| `VARIABLE_DECLARATION_NODE` | Registra el símbolo con valor inicial 0 |
| `ASSIGNMENT_NODE` | Evalúa la expresión y asigna |
| `IF_ELSE_NODE` | Evalúa la condición y ejecuta una rama |
| `WHILE_NODE` | Itera mientras la condición sea verdadera |
| `RETURN_NODE` | Devuelve `hasReturned = true` con el valor |
| `METHOD_CALL_NODE` | Ejecuta la llamada y **descarta** el resultado |
| `default` | Nada |

El caso de `METHOD_CALL_NODE` es lo que implementa "un método con resultado puede
invocarse como sentencia, ignorando el valor".

### `evaluateExpression` → `Symbol`

| Caso | Qué devuelve |
| --- | --- |
| `CONSTANT_NODE` | `makeIntSymbol` / `makeFloatSymbol` / `makeBoolSymbol` según el tipo |
| `ID_NODE` | Una **copia** del símbolo de la tabla |
| `OPERATOR_NODE` | El resultado de aplicar el operador |
| `METHOD_CALL_NODE` | El valor de retorno de la llamada |
| `default` | Void |

La simetría con el generador de TAC es exacta: ahí una expresión devolvía *dónde
quedó* su resultado; acá devuelve *cuál es*. Misma estructura de recursión,
distinto producto.

---

## 5. El ámbito de una llamada: alcance léxico, no dinámico

Esta es la línea más importante del archivo:

```c
static Scope *createFunctionScope(Scope *globalScope) {
    return createScope(1, globalScope, NULL);
}
```

El ámbito de la función se crea con **el ámbito global como padre**, no con el
del llamador. La diferencia decide la semántica del lenguaje:

| Si el padre fuera… | Alcance resultante |
| --- | --- |
| `globalScope` (lo que hace) | **Léxico / estático**: la función ve sus locales y las globales, nada más |
| `callerScope` | **Dinámico**: la función vería también los locales de quien la llamó |

El alcance dinámico es lo que tenían algunos Lisp antiguos y es casi siempre un
error de diseño: haría que el significado de un identificador dependiera de la
cadena de llamadas en tiempo de ejecución, no del texto del programa.

`executeFunctionBody` recibe los dos ámbitos y los usa para cosas distintas:

```c
Symbol *argumentValues = evaluateArguments(argumentsNode, callerScope, ...);  /* en el del LLAMADOR */
Scope *functionScope = createFunctionScope(globalScope);                      /* hijo del GLOBAL */
bindParameters(functionScope, methodDeclarationNode->children[0], argumentValues);
```

Los argumentos se evalúan en el ámbito del llamador —donde sus nombres tienen
sentido— y sus **valores** se copian a los parámetros del ámbito nuevo. Eso es
**pasaje por valor**, y el `Symbol *argumentValues` intermedio es lo que lo
materializa: para cuando el cuerpo empieza a ejecutarse, los argumentos ya son
copias y modificar un parámetro no afecta al llamador.

`getGlobalScope` sube por la cadena de `parent` hasta la raíz, que es cómo
`executeMethodCall` obtiene el global sin que se lo pasen.

---

## 6. Las coerciones, en los mismos tres lugares

El intérprete reimplementa la conversión `int`→`float` donde el chequeo de tipos
la había autorizado:

| Dónde | Función |
| --- | --- |
| Asignación | `assignSymbolValue`: si el destino es `float` y la fuente `int`, convierte |
| Paso de argumentos | El mismo `assignSymbolValue`, llamado desde `bindParameters` |
| Retorno | `coerceReturnValue` |

Y en las operaciones binarias, `getFloatValue` promueve el operando entero
cuando el otro es real:

```c
static float getFloatValue(Symbol symbol) {
    return (symbol.type == TYPE_FLOAT) ? symbol.value.floatValue : (float)symbol.value.intValue;
}
```

Son las mismas cuatro situaciones que el generador de TAC resuelve con
`TAC_INT_TO_FLOAT`. Que las dos implementaciones coincidan es lo que hace útil al
oráculo; que haya **dos** implementaciones de la misma regla es el costo de
tenerlo.

---

## 7. Lo que el intérprete asume

No valida nada: **confía en que el análisis semántico ya pasó**. Los ejemplos más
claros:

```c
static bool isTruthy(Symbol symbol) {
    return symbol.value.intValue != 0;
}
```

Esto es semántica *truthy*: cualquier valor distinto de cero es verdadero. **El
lenguaje no la tiene** —`if (a)` con `a` entero es un error semántico— pero al
intérprete le da igual, porque para cuando se ejecuta, el analizador ya rechazó
ese programa. Casualmente el código objeto hace lo mismo (`cmpl $0` + `je`), así
que si alguna vez la regla se relajara los dos coincidirían.

Lo mismo con los tipos de los operandos: `evaluateBinaryOperator` asume que un
`&&` recibe dos booleanos y que un `%` recibe dos enteros. No lo chequea.

Las únicas condiciones que sí verifica en ejecución son las que el análisis
estático **no puede** decidir:

| Chequeo | Mensaje |
| --- | --- |
| División por cero | `Division por cero.` |
| Módulo por cero | `Division por cero en operacion modulo.` |
| `INT_MIN / -1` | `Desborde en la division de enteros: INT_MIN / -1 no es representable.` |
| `INT_MIN % -1` | `Desborde en el modulo de enteros: INT_MIN %% -1 no es representable.` |

Los dos últimos son el único desborde posible de la división entera: el cociente
no entra en 32 bits. Sin el guard, el intérprete abortaba con `SIGFPE` —o sea
**el compilador se caía**— porque en C es comportamiento indefinido y en x86
`idivl` trapea. El mismo guard ya existía en el plegado de constantes del
optimizador y no se había replicado acá.

---

## 8. Lo que cuesta: un ámbito por iteración

`executeBlock` crea un ámbito nuevo cada vez que se ejecuta un bloque, y lo cuelga
del padre con `appendChildScope`:

```c
Scope *blockScope = createScope(currentScope->level + 1, currentScope, NULL);
appendChildScope(currentScope, blockScope);
```

El cuerpo de un `while` **es** un bloque, así que cada iteración crea un ámbito
que queda colgado del árbol hasta el `freeScope` final. **La memoria crece lineal
con la cantidad de iteraciones ejecutadas**, no con el tamaño del programa.

Medido con un ciclo que declara una local en su cuerpo:

| Iteraciones | Memoria máxima |
| :---: | :---: |
| 10 000 | 4224 KB |
| 20 000 | 7040 KB |
| 40 000 | 12 416 KB |

El mismo programa compilado en lugar de interpretado usa **1664 KB**. Son unos
280 bytes por iteración: el `Scope` con sus dos arreglos, más el `Symbol` de la
local con su nombre duplicado.

**Es correcto pero no escalable.** Un ciclo de diez millones de iteraciones
agotaría la memoria, y un programa compilado no tendría problema. La corrección
es liberar el ámbito del bloque al salir —`freeScope(blockScope)` antes de
devolver— lo cual exige asegurarse de que el `Symbol` que viaja en el
`ExecResult` sea una copia y no apunte a algo de ese ámbito. Dado que los valores
viajan **por valor** (§3), la copia ya está hecha, así que el cambio es viable;
no se hizo porque el intérprete no se entrega.

---

## 9. Conexión con las fases vecinas

| Viene de | Qué cruza la frontera |
| --- | --- |
| El AST anotado (F08, F10, F11) | El árbol con tipos resueltos y la garantía de validez |
| `symbol_table.c` (F09) | `Scope`, `newSymbol`, los `make*Symbol`, `lookupSymbol`, `freeScope` |
| `error_handler.c` (F03) | `semanticError`, reusado para los errores de ejecución |

| Va hacia | Qué entrega |
| --- | --- |
| `driver.c` (F02) | El ámbito global con los valores finales, y el valor de `main` por `-debug` |

**Construye su propia tabla de símbolos**, separada de la que armó el análisis
semántico: `registerGlobalDeclarations` recorre la raíz del AST y registra las
globales y los métodos de nuevo. Los métodos los registra con
`FUNCTION_ENTRY_SYMBOL` guardando el **nodo del AST** en `value.function`, que es
cómo `executeMethodCall` encuentra el cuerpo a ejecutar. Esa variante del enum
existe únicamente para esto.

---

## 10. Cómo se extiende

### Una construcción nueva

Un `case` en `executeStatement` (si es sentencia) o en `evaluateExpression` (si
es expresión). Los dos `default` devuelven void en silencio, así que **olvidarlo
hace que la construcción no haga nada** sin ningún aviso: el mismo patrón de
fallo silencioso que en el generador de TAC y en el de código objeto.

### `break` y `continue`

Encajan naturalmente en `ExecResult`: alcanza con agregar dos banderas más
—`hasBroken`, `hasContinued`— y que `executeWhile` las consuma en lugar de
propagarlas. Es el caso donde este diseño se ve bien: el mecanismo de propagación
ya existe.

### Salida por pantalla

Sería la extensión que más valor le daría, porque hoy la única forma de observar
un programa es el valor de retorno de `main`. Un `print` requeriría un nodo nuevo
y un caso en `executeStatement` que haga `printf`. En el lado compilado habría
que enlazar con libc.

### Liberar los ámbitos de bloque

Ver §8. Es la corrección pendiente más clara del módulo.

---

## 11. Trampas y límites conocidos

- **La memoria crece con las iteraciones, no con el programa** (§8).
- **`isTruthy` acepta cualquier valor distinto de cero** (§7), que es más
  permisivo que el lenguaje. No se nota porque el semántico ya filtró.
- **`Symbol` significa dos cosas**: entrada de tabla o valor de expresión (§3).
  Un `Symbol` con `id == NULL` y `uid == 0` no es una entrada rota, es un valor.
- **Los errores de ejecución se reportan como errores semánticos.** Reusan
  `semanticError`, así que salen como `Error semantico en la linea 0`: no son
  semánticos y no tienen número de línea, porque `evaluateBinaryOperator` no
  recibe el nodo. Está asumido.
- **`-target interp` devolvía 0 aunque el intérprete reportara un error.** Eso se
  corrigió: hoy el target propaga el fallo y termina con código 1.
- **Reimplementa las reglas de tipos** (§6). Es lo que lo hace útil como oráculo y
  lo que duplica el mantenimiento: un cambio en la semántica del lenguaje hay que
  hacerlo en dos lugares.
- **Los dos `default` de los despachadores son silenciosos** (§10).
- **El desborde con signo en `+`, `-` y `*` sigue siendo comportamiento
  indefinido**, igual que en el plegado de constantes del optimizador. UBSan lo
  marca; hoy GCC envuelve y el resultado coincide con el del assembly generado.

---

## 12. La teoría detrás

- **Intérprete contra compilador**: un intérprete ejecuta el programa; un
  compilador lo traduce a otro lenguaje que después se ejecuta. Este proyecto
  tiene los dos sobre el mismo front-end, lo que es la ilustración más clara de
  que **el front-end es independiente de qué se haga con el resultado**.
- **Intérprete de recorrido de árbol** (*tree-walking interpreter*): la forma más
  directa de interpretar, recursando sobre el AST. Es simple y lenta: cada
  operación paga el costo de un `switch` y de varias llamadas. Las alternativas,
  en orden de complejidad, son interpretar un **bytecode** —lineal, como el TAC,
  que es por lo que un intérprete de TAC sería más rápido que este— y la
  **compilación JIT**.
- **Alcance léxico contra dinámico** (§5): la decisión de quién es el padre del
  ámbito de una llamada. Es el ejemplo de libro de cómo una línea de
  implementación define una propiedad del lenguaje.
- **Pasaje por valor contra por referencia**: acá los argumentos se copian antes
  de entrar al cuerpo, así que modificar un parámetro no afecta al llamador. Es lo
  que la especificación de C-TDS exige, y el arreglo `argumentValues` intermedio
  es dónde se materializa.
- **Entorno de ejecución** (*environment*): la cadena de ámbitos con los valores
  actuales. En un intérprete es una estructura **viva en tiempo de ejecución**; en
  un compilador la misma información se resuelve en compilación y se convierte en
  offsets del marco de pila. Comparar la ficha F17 con esta §5 muestra las dos
  soluciones al mismo problema.
- **Oráculo de prueba** (*test oracle*): una fuente independiente de la respuesta
  correcta. Tener dos implementaciones de la misma semántica y compararlas es
  **testing diferencial**, y es la técnica con la que se encuentran bugs sin
  escribir los resultados esperados a mano. Es el argumento más fuerte a favor de
  haber hecho el intérprete.
- **Errores detectables solo en ejecución**: la división por cero y el desborde
  de `INT_MIN / -1` no se pueden decidir estáticamente en general. Es la frontera
  entre lo que el análisis semántico puede garantizar y lo que no.

---

## 13. Cómo se valida

- **Suite:** no tiene una propia. Se valida por **comparación con el ejecutable**,
  que es su razón de ser.
- **El cruce que importa**, sobre cualquier programa cuyo `main` devuelva `int`:

```bash
./mi_compilador -o /tmp/f examples/dead_code.ctds && /tmp/f; echo "ejecutable -> $?"
./mi_compilador -debug -target interp examples/dead_code.ctds | grep retorno
```

  Los dos valores tienen que coincidir. Sobre el corpus de 20 programas coinciden
  en los 20.

- **Cuando difieren**, el que está mal suele ser el back-end, porque el intérprete
  es mucho más simple. Pero no siempre: el guard de `INT_MIN` de §7 fue un bug
  **del intérprete**, encontrado compilando el proyecto con UBSan y ASan:

```bash
gcc -fsanitize=address,undefined -g -Iinclude -Isrc -Ibuild <fuentes> -o /tmp/asan
ASAN_OPTIONS=detect_leaks=0 /tmp/asan -target interp programa.ctds
```

---

Con esto se cierra el recorrido del proyecto. El índice está en
[la página inicial](../index.md).
