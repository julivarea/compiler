# La Representación Intermedia (Cuádruplas TAC)

**Archivos:** `include/tac.h`, `src/intermediate_code/tac.c` (~610 líneas)

Estructura de datos del Código de Tres Direcciones (*Three-Address Code*, TAC) que articula el front-end con el optimizador y el generador de código objeto.

!!! note "El TAC cubre tipos enteros y lógicos"
    El enunciado acota esta etapa a tipos enteros y lógicos. El operando constante almacena un `int`, y cualquier programa que utilice `float` se rechaza al iniciar la [generación de TAC](02-generacion-tac.md).

---

## 1. Qué problema resuelve

El [AST](../03-lexico-sintactico-y-ast/04-ast.md) representa la estructura jerárquica de la sintaxis del lenguaje, mientras que el lenguaje ensamblador exige una secuencia lineal de instrucciones sobre registros y direcciones de memoria. El TAC actúa como representación intermedia lineal e independiente de la arquitectura: emplea operandos con nombre y temporales ilimitados, separando el aplanamiento del árbol ([generación TAC](02-generacion-tac.md)) de la asignación de almacenamiento ([marco de pila](../07-codigo-objeto/01-marco-de-pila.md)).

Además, provee la estructura sobre la cual opera el optimizador antes de la traducción a x86-64.

---

## 2. La instrucción cuádrupla (`TACInstruction`)

Cada instrucción TAC se modela como una cuádrupla compuesta por un operador y hasta tres operandos (un destino y dos fuentes):

```c
typedef struct TACInstruction {
    TACOperation operation;     /* código de operación */
    TACOperand   result;        /* operando destino */
    TACOperand   firstOperand;  /* primer operando fuente */
    TACOperand   secondOperand; /* segundo operando fuente */
    struct TACInstruction *next;
} TACInstruction;
```

```text
x = a + b             -> (TAC_ADD, x, a, b)
x = -a                -> (TAC_UMINUS, x, a, —)
goto L1               -> (TAC_GOTO, —, L1, —)
if t false goto L2    -> (TAC_IF_FALSE, —, t, L2)
```

En código de tres direcciones los operandos son siempre atómicos: una instrucción no contiene subexpresiones anidadas. Expresiones como `a + b * c` se descomponen en operaciones elementales encadenadas mediante temporales (`t0 = b * c`, `t1 = a + t0`).

### Elección de cuádruplas estructuradas frente a triplas o texto

- **Cuádruplas frente a triplas:** en una representación de triplas el resultado no se nombra explícitamente, sino que se referencia por la posición de la instrucción en la lista. Cualquier eliminación o reordenamiento en el optimizador invalidaría esos índices. En cambio, al explicitar el operando `result`, las cuádruplas permiten insertar, mover o eliminar instrucciones de forma local.
- **Estructuras tipadas frente a texto plano:** representar cada instrucción y operando como estructuras en memoria permite al optimizador y al backend consultar la categoría (`OperandKind`), el tipo (`DataType`) y el valor numérico de una constante, o reemplazar un operando y desenlazar una instrucción muerta en $O(1)$ sin re-parsear cadenas.

---

## 3. El operando tipado (`TACOperand`)

```c
typedef struct TACOperand {
    OperandKind kind;              /* categoría del operando */
    DataType type;                 /* tipo de dato (INT, BOOL, VOID) */
    char name[TAC_NAME_LENGTH];    /* representación textual (256 bytes) */
    Symbol *symbol;                /* puntero al símbolo (solo OPERAND_VARIABLE) */
    union {
        int intValue;
    } value;                       /* valor literal (solo OPERAND_CONSTANT) */
} TACOperand;
```

| Campo | Propósito |
| --- | --- |
| `kind` | Identifica la naturaleza del operando sin inspeccionar `name` |
| `type` | Tipo de dato del operando o tipo de retorno del método |
| `name` | Buffer de tamaño fijo (`TAC_NAME_LENGTH = 256`) utilizado al formatear la instrucción |
| `symbol` | Referencia al `Symbol` de la tabla para variables y parámetros |
| `value` | Valor inmediato de una constante entera o booleana (`1` / `0`), usado por el optimizador |

### Categorías de `OperandKind`

| Categoría | Representa | Ejemplo de `name` | Campos activos |
| --- | --- | --- | --- |
| `OPERAND_NONE` | Operando no utilizado por el opcode | `""` | Ninguno |
| `OPERAND_TEMP` | Temporal generado por el compilador | `t0` | `type` |
| `OPERAND_VARIABLE` | Variable o parámetro del programa | `x@7` | `type`, `symbol` |
| `OPERAND_CONSTANT` | Constante literal entera o booleana | `42`, `true` | `type`, `value` |
| `OPERAND_LABEL` | Etiqueta de control de flujo | `L3` | — |
| `OPERAND_METHOD` | Identificador de método | `main` | `type` (retorno) |

`OPERAND_NONE` uniformiza el formato de las cuádruplas cuando una instrucción usa menos de tres operandos. Por su parte, distinguir `OPERAND_TEMP` de `OPERAND_VARIABLE` permite al optimizador diferenciar temporales internos de variables del usuario (incluidas las globales, cuyo efecto es observable entre llamadas).

### Constructores y predicados públicos

Todos los operandos se inicializan mediante constructores que limpian la estructura con `memset` a cero antes de poblar sus campos:

| Función | Descripción |
| --- | --- |
| `tacNone()` | Operando vacío (`OPERAND_NONE`) |
| `tacTemp(name, type)` | Temporal con nombre y tipo |
| `tacVariable(symbol)` | Variable o parámetro con nombre calificado `id@uid` (ver §4) |
| `tacConstantInt(value)` | Constante entera (`TYPE_INT`) |
| `tacConstantBool(value)` | Constante booleana (`TYPE_BOOL`, `"true"` o `"false"`) |
| `tacMethod(name, returnType)` | Símbolo de método con su tipo de retorno |
| `tacLabel(name)` | Etiqueta de salto (`OPERAND_LABEL`) |

Además, `tac.h` expone dos predicados `static inline` reutilizados por el optimizador, el análisis de variables vivas y el generador de código objeto:

```c
static inline bool holdsValue(const TACOperand *operand) {
    return operand->kind == OPERAND_TEMP || operand->kind == OPERAND_VARIABLE;
}

static inline bool isConditionalJump(TACOperation operation) {
    return operation == TAC_IF_FALSE || operation == TAC_IF_TRUE;
}
```

---

## 4. El nombre calificado `x@uid`

Al construir un operando de variable, `tacVariable` concatena el identificador con el `uid` asignado por la [tabla de símbolos](../04-semantico/01-tabla-de-simbolos.md):

```c
snprintf(qualifiedName, sizeof(qualifiedName), "%s@%d", id, symbol->uid);
```

Como el TAC es una secuencia plana sin ámbitos léxicos, este renombrado único desambigua variables homónimas declaradas en distintos bloques:

```c
int x;                      ->  global x@1, INT
int main(){
    int x; x = 10;          ->  local x@3, INT   /  x@3 = 10
    { int x; x = 20; }      ->  local x@4, INT   /  x@4 = 20
    return x;               ->  ret x@3
}
```

El carácter `@` se utiliza como separador porque no es válido en identificadores del lenguaje fuente, evitando colisiones con nombres escritos por el programador. Si un símbolo tuviera `uid == 0` (no registrado en ningún ámbito), se formatea sin sufijo, lo que indicaría una anomalía previa en el análisis semántico.

Tanto `makeOperand` como `tacVariable` verifican el valor de retorno de `snprintf` contra `TAC_NAME_LENGTH`. Si un nombre calificado excede los 256 bytes, se reporta un error interno en lugar de truncar silenciosamente el sufijo `@uid`.

---

## 5. Catálogo de operaciones (`TACOperation`)

El enumerado `TACOperation` define 23 códigos de operación distribuidos en cuatro grupos:

### Métodos y llamadas

| Opcode | Formato textual | Operandos utilizados |
| --- | --- | --- |
| `TAC_BEGIN_FUNC` | `begin_func suma, 2` | `result` = método, `first` = cantidad de parámetros |
| `TAC_END_FUNC` | `end_func suma` | `result` = método |
| `TAC_PARAM_DECL` | `param_decl a@4` | `first` = parámetro formal |
| `TAC_PARAM` | `param g@1` | `first` = argumento de llamada |
| `TAC_CALL` | `t1 = call suma, 2` / `call nada, 0` | `result` (opcional), `first` = método, `second` = cantidad de argumentos |
| `TAC_RETURN` | `ret t0` / `ret` | `first` = valor de retorno (opcional) |

`TAC_BEGIN_FUNC` y `TAC_END_FUNC` delimitan el alcance de cada función para la emisión de prólogos/epílogos en x86-64 y acotan el análisis intraprocedimiento del optimizador.

### Declaraciones de datos

| Opcode | Formato textual |
| --- | --- |
| `TAC_GLOBAL_DECL` | `global g@1, INT` |
| `TAC_LOCAL_DECL` | `local x@8, INT` |

Explicitar si la declaración es global o local en el opcode permite al backend ubicar las globales en la sección `.data` y reservar las locales en el marco de pila sin reconsultar la tabla de símbolos.

### Control de flujo

| Opcode | Formato textual |
| --- | --- |
| `TAC_LABEL` | `L3:` |
| `TAC_GOTO` | `goto L3` |
| `TAC_IF_FALSE` | `if t9 false goto L4` |
| `TAC_IF_TRUE` | `if t2 true goto L0` |

Contar con ambas variantes de salto condicional (`TAC_IF_FALSE` para `if`, `while` y `&&`; `TAC_IF_TRUE` para `||`) evita emitir instrucciones de negación intermedias.

### Asignación y operadores

| Opcode | Formato textual |
| --- | --- |
| `TAC_ASSIGN` | `x@8 = t1` |
| `TAC_ADD`, `TAC_SUB`, `TAC_MUL`, `TAC_DIV`, `TAC_MOD` | `t0 = a@4 + b@5` |
| `TAC_LT`, `TAC_GT`, `TAC_EQ` | `t4 = x@8 > 5` |
| `TAC_UMINUS` | `t0 = -x@2` |
| `TAC_NOT` | `t8 = !b@10` |

### Ausencia de `TAC_AND` y `TAC_OR`

No existen opcodes binarios para `&&` ni `||`. Como el lenguaje requiere evaluación de corto circuito, evaluar ambos operandos antes de aplicar una instrucción binaria violaría la semántica cuando el primer operando determina el resultado. Por ello, `&&` y `||` se traducen exclusivamente mediante saltos condicionales `TAC_IF_FALSE` y `TAC_IF_TRUE` (ver [generación de TAC](02-generacion-tac.md)).

---

## 6. Formateo textual y lista enlazada (`TACList`)

### Formateo unificado (`formatTACInstruction`)

```c
void formatTACInstruction(char *buffer, size_t size, const TACInstruction *instruction);
```

`formatTACInstruction` centraliza la conversión de una cuádrupla a texto. Sobre ella se construyen:

- **`printTACInstruction` / `formatTACList`:** vuelcan el código al `string_builder` para generar los archivos `.ci` y `.opt`.
- **`fprintTACInstruction(output, instruction)`:** escribe en un `FILE *` durante la salida `-debug` del optimizador y del análisis de variables vivas.

En el `switch` de formateo, el caso `default` maneja todos los operadores aritméticos, relacionales y unarios consultando `getOperatorSymbol`: si `secondOperand.kind == OPERAND_NONE` imprime la forma unaria (`result = op first`), y en caso contrario la binaria (`result = first op second`).

### Contenedor `TACList`

```c
typedef struct TACList {
    TACInstruction *head;
    TACInstruction *tail;
    int count;
} TACList;
```

La lista es simplemente enlazada y mantiene un puntero `tail` para insertar al final en $O(1)$:

| Función | Descripción |
| --- | --- |
| `createTACList()` | Crea una lista vacía |
| `createTACInstruction(op, result, first, second)` | Reserva e inicializa una instrucción sin enlazarla |
| `appendTACInstruction(list, instruction)` | Enlaza al final una instrucción existente en $O(1)$ |
| `appendInstruction(list, op, result, first, second)` | Crea y enlaza una instrucción al final; devuelve el puntero o `NULL` si falla `malloc` |
| `freeTACInstruction(instruction)` | Libera un nodo instrucción individual |
| `freeTACList(list)` | Libera todas las instrucciones y la estructura `TACList` |
| `formatTACList(list)` | Vuelca todas las instrucciones de la lista al acumulador de texto |

Los tres `TACOperand` están embebidos por valor dentro de `TACInstruction`. Cada nodo es autocontenido (~800 bytes debido a los tres buffers de 256 bytes) y se libera con una única llamada a `free`.

---

## 7. Conexión con las fases vecinas

| Módulo origen / destino | Interacción |
| --- | --- |
| `symbol_table.c` / `types.h` (entrada) | Proveen `Symbol` (con `uid`) y `DataType` para construir operandos |
| `intermediate_code.c` (productor) | Usa los constructores y `appendInstruction` para poblar la `TACList` |
| `optimizer.c` (consumidor/mutador) | Recorre y transforma la `TACList` en el lugar |
| `live_variable_analysis.c` (consumidor) | Particiona la `TACList` en bloques básicos y construye el CFG |
| `object_code.c` (consumidor) | Traduce cada `TACInstruction` a ensamblador x86-64 |
| `driver.c` (consumidor) | Invoca `formatTACList` para emitir los archivos `.ci` y `.opt` |

---

## 8. Cómo se extiende

### Un operador binario o unario nuevo

1. Añadir la constante al `enum TACOperation`.
2. Agregar su representación en `getOperatorSymbol` (el caso `default` de `formatTACInstruction` ya formatea expresiones unarias y binarias automáticamente).
3. Implementar su traducción en el generador de código objeto y, si aplica, su regla de plegado en el optimizador.

### Un opcode con sintaxis propia

Si la instrucción no sigue el patrón `result = first op second` (por ejemplo, un acceso indexado `t0 = a[t1]`), debe añadirse además su `case` específico en `formatTACInstruction`.

### Un `OperandKind` nuevo

Requiere definir su función constructora y revisar los `switch` o predicados sobre `kind` en el optimizador y en el generador de código objeto (`holdsValue`, carga y almacenamiento de operandos).

### Identificadores de más de 256 caracteres

Basta con incrementar la constante `TAC_NAME_LENGTH` (y proporcionalmente `TAC_INSTRUCTION_TEXT_LENGTH`), dado que el chequeo de truncamiento en `makeOperand` y `tacVariable` (§4) depende de `sizeof`.

---

## 9. Trampas y límites conocidos

- **Inexistencia deliberada de `TAC_AND` y `TAC_OR` (§5):** los operadores lógicos se codifican con saltos condicionales para preservar la evaluación de corto circuito.
- **Símbolos con `uid == 0` (§4):** se imprimen sin sufijo `@uid`, lo que delata un símbolo que no fue registrado en ningún ámbito durante el análisis semántico.
- **Tamaño por instrucción (~800 bytes, §6):** cada `TACInstruction` aloja tres buffers de `TAC_NAME_LENGTH` (256 bytes) por valor.
- **Campos en cero para categorías inactivas:** `makeOperand` limpia con `memset`, por lo que `value.intValue` vale `0` en operandos que no son constantes; siempre debe verificarse `operand.kind == OPERAND_CONSTANT` antes de leer `value`.
- **Tiempo de vida de `getOperandName`:** devuelve un puntero al arreglo interno `operand->name`, válido únicamente mientras exista la `TACInstruction` que lo contiene.
- **Operadores sin símbolo en `getOperatorSymbol`:** devuelven `"?"`, por lo que un opcode nuevo olvidado tanto en `formatTACInstruction` como en `getOperatorSymbol` se formatea como `t0 = a ? b`.

---

## 10. Cómo se valida

- **Suite unitaria:** `tests/test_tac.c` comprueba instrucción por instrucción las cuádruplas generadas para cada construcción del lenguaje a partir de ASTs anotados construidos en memoria.
- **Inspección manual:** el archivo `.ci` vuelca directamente la `TACList` generada:

```bash
./mi_compilador -target codinter examples/valido_completo.ctds
```

- **Programa de cobertura de los 23 opcodes:**

```c
int g;
int suma(int a, int b){ return a + b; }
void nada(){ return; }
int main(){
    int x; boolean b; boolean c;
    g = 10;                               // global, assign
    x = suma(g, 2);                       // param, call con destino
    nada();                               // call sin destino
    x = x + 1;                            // add
    b = (x > 5) && (x < 100);             // gt, lt, if_false, label
    c = (x == 0) || (x > 1);              // eq, if_true
    if (b) { x = -x; } else { c = !b; }   // uminus, not, goto
    while (x < 3) { x = x + 1; }
    return x % 7;                         // mod, ret con valor
}
```

---

**Siguiente:** [Generación de Código Intermedio](02-generacion-tac.md)
