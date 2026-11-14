# Árbol de Sintaxis Abstracta (AST)

**Archivos:** `include/ast.h`, `src/parser/ast.c` (535 líneas)

Estructura arbórea construida por el [parser](02-analisis-sintactico.md), anotada en el lugar por el análisis semántico y recorrida por el generador de código intermedio.

---

## 1. Qué problema resuelve

Representar la estructura jerárquica del programa donde cada nodo modela una construcción del lenguaje y sus hijos representan sus subcomponentes, descartando los elementos puramente sintácticos (paréntesis, llaves, comas, punto y coma, palabras clave).

El AST es la primera representación intermedia del compilador: conecta el front-end con el análisis semántico (que decora sus nodos con tipos y símbolos resueltos) y con la generación de [TAC](../05-codigo-intermedio/01-representacion-tac.md).

### Qué construcciones generan nodo y cuáles delegan

Si una producción de la gramática solo agrupa alternativas sintácticas sin introducir semántica propia, delega directamente en su hijo (`{ $$ = $1; }`):

- **`Statement` y `Expression` delegan:** no existen `STATEMENT_NODE` ni `EXPRESSION_NODE`; cada alternativa produce directamente su nodo específico (`ASSIGNMENT_NODE`, `OPERATOR_NODE`, `CONSTANT_NODE`, etc.), y `'(' Expression ')'` devuelve el subárbol interno sin envolverlo.
- **`Block` genera `BLOCK_NODE`:** agrupa una secuencia de declaraciones y sentencias y delimita además un nuevo ámbito léxico durante el análisis semántico.

---

## 2. Interfaz pública

`include/ast.h` define el enum `NodeType`, la estructura `NodeAST`, las macros de acceso a hijos (§6) y las siguientes funciones públicas:

| Función | Propósito |
| --- | --- |
| `newNode(nodeType, symbol, left, mid, right)` | Constructor genérico de hasta tres hijos; compacta hijos `NULL` (§5) |
| `newOperatorNode(operation, left, right)` | Construye un `OPERATOR_NODE` unario (`right == NULL`) o binario y asigna `operationType` |
| `newMethodDeclaration(returnType, name, parameters, body)` | Construye un `METHOD_DECLARATION_NODE` y registra `parameters` en `symbol->parameters` |
| `newParameterDeclaration(type, id)` | Construye un `VARIABLE_DECLARATION_NODE` para un parámetro formal |
| `newIdentifierNode(id)` | Construye un `ID_NODE` con símbolo provisorio y `TYPE_VOID` |
| `newAssignmentNode(id, expression)` | Construye un `ASSIGNMENT_NODE` con símbolo destino provisorio y la expresión como hijo |
| `newMethodCallNode(id, arguments)` | Construye un `METHOD_CALL_NODE` y adopta la lista de argumentos |
| `newBlockNode(items)` | Construye un `BLOCK_NODE` y adopta la lista de declaraciones y sentencias |
| `newParametersNode(parameters)` | Construye un `PARAMETERS_NODE` y adopta la lista de parámetros |
| `newReturnNode(expression)` | Construye un `RETURN_NODE` con 0 o 1 hijo |
| `newWhileNode(condition, body)` | Construye un `WHILE_NODE` con `[condition, body]` |
| `newIfElseNode(condition, ifBlock, elseBlock)` | Construye un `IF_ELSE_NODE` con 2 o 3 hijos |
| `newIntLiteralNode(int)` / `newFloatLiteralNode(float)` / `newBoolLiteralNode(int)` | Construyen un `CONSTANT_NODE` del tipo correspondiente |
| `resolveNodeSymbol(node, declared)` | Reapunta un nodo al `Symbol` declarado en la tabla y libera el símbolo provisorio (§7) |
| `freeAST(node)` | Libera recursivamente un subárbol en **postorden** (hijos antes que el padre) |
| `fprintAST(out, node, level)` / `printAST(node, level)` | Imprimen el árbol en **preorden** con indentación jerárquica (`  \| ` por nivel) |

Adicionalmente, `ast.c` define `getDataTypeName(type)` (declarada en `types.h`; ver [utilidades y tipos](../02-infraestructura/04-utilidades-y-tipos.md)) y las funciones internas `newSymbolNode` (factoriza la creación de nodos con `Symbol`), `resolveNodeArray` (adopta en $O(1)$ el buffer de un `NodeArray` creado en [acciones del parser](03-acciones-del-parser.md)), `describeNode` y `getOperationName` (formatean cada línea de salida de `.sint` y `.sem`). Si falla una reserva de memoria en cualquier constructor, el propio constructor libera el nombre, el símbolo y los subárboles ya recibidos para no dejar fugas.

---

## 3. La estructura `NodeAST`

```c
typedef struct NodeAST {
    NodeType nodeType;           /* Categoría sintáctica del nodo */
    DataType type;               /* Tipo de dato declarado o inferido */
    Symbol *symbol;              /* Símbolo asociado (NULL si no aplica) */
    OperationType operationType; /* Operador (válido solo si nodeType == OPERATOR_NODE) */
    int line;                    /* Línea en el código fuente */
    struct NodeAST **children;   /* Arreglo dinámico de punteros a hijos */
    int childCount;              /* Cantidad de hijos */
} NodeAST;
```

| Campo | Rol en el compilador |
| --- | --- |
| `nodeType` | Discriminante evaluado en los `switch` del análisis semántico, chequeo de tipos, impresión y TAC |
| `type` | Inicializado en `TYPE_VOID` por `newNode` (salvo en declaraciones tipadas y literales); completado por el análisis semántico |
| `symbol` | Puntero al `Symbol` con el nombre o valor literal. Es `NULL` en nodos estructurales (`BLOCK_NODE`, `IF_ELSE_NODE`, `WHILE_NODE`, `RETURN_NODE`, `OPERATOR_NODE`, `PARAMETERS_NODE`) |
| `operationType` | Operador aritmético, relacional o lógico. Solo lo inicializa `newOperatorNode` |
| `line` | Línea de origen para diagnósticos semánticos (ver §8) |
| `children` / `childCount` | Arreglo contiguo de subárboles hijos |

### Nodo uniforme frente a unión etiquetada por construcción

En lugar de definir una `struct` distinta por cada categoría sintáctica (`IfNode`, `WhileNode`, `BinaryOpNode`, etc.) dentro de una `union`, se utiliza un único tipo `NodeAST` con un arreglo dinámico `children`. Esto unifica el manejo de los nodos de aridad fija (`IF_ELSE_NODE`, `WHILE_NODE`, `OPERATOR_NODE`) con los de aridad variable (`BLOCK_NODE`, `PARAMETERS_NODE`, `METHOD_CALL_NODE`).

Como contrapartida, el rol semántico de cada posición `children[i]` no está codificado en el sistema de tipos de C sino por convención. Para centralizar esa convención, el orden de los hijos solo se escribe dentro de los constructores de `ast.c` y se lee mediante las macros de acceso de `ast.h` (§6).

---

## 4. Layout de hijos por tipo de nodo

| `NodeType` | `symbol` | Hijos (`children`) | `operationType` |
| --- | --- | --- | --- |
| `BLOCK_NODE` | `NULL` | `N` hijos: declaraciones locales primero, sentencias después | — |
| `VARIABLE_DECLARATION_NODE` | Variable o parámetro (`VARIABLE_SYMBOL`) | `0` | — |
| `METHOD_DECLARATION_NODE` | Método (`METHOD_SYMBOL`; `symbol->parameters` apunta a `children[0]`) | Siempre `2`: `[parameters, body]` | — |
| `PARAMETERS_NODE` | `NULL` | `N` nodos `VARIABLE_DECLARATION_NODE` (puede ser `0`) | — |
| `ASSIGNMENT_NODE` | Variable destino (`VARIABLE_SYMBOL`, inicialmente provisorio) | `1`: `[expression]` | — |
| `RETURN_NODE` | `NULL` | `0` (`return;`) o `1` (`[expression]`) | — |
| `IF_ELSE_NODE` | `NULL` | `2` sin `else`: `[cond, ifBlock]` · `3` con `else`: `[cond, ifBlock, elseBlock]` | — |
| `WHILE_NODE` | `NULL` | `2`: `[cond, block]` | — |
| `OPERATOR_NODE` | `NULL` | `1` si es unario (`OP_UMINUS`, `OP_NOT`), `2` si es binario | `OperationType` |
| `METHOD_CALL_NODE` | Método invocado (`METHOD_SYMBOL`, inicialmente provisorio) | `N` expresiones de argumento (puede ser `0`) | — |
| `ID_NODE` | Variable referenciada (`VARIABLE_SYMBOL`, inicialmente provisorio) | `0` | — |
| `CONSTANT_NODE` | Literal sin nombre (`CONSTANT_SYMBOL`, `id == NULL`) | `0` | — |

Invariantes relevantes para las fases consumidoras:

- **`IF_ELSE_NODE` sin rama `else` tiene `childCount == 2`**, no 3 con `children[2] == NULL`. La presencia de rama `else` se consulta mediante `GET_ELSE_BLOCK(node) != NULL`.
- **`METHOD_DECLARATION_NODE` tiene siempre 2 hijos:** cuando un método no recibe parámetros, `children[0]` es un `PARAMETERS_NODE` con `childCount == 0`, nunca `NULL`. Además, `newMethodDeclaration` guarda ese mismo puntero en `symbol->parameters` para que el chequeador de tipos valide aridad y tipos en las llamadas.
- **Representación de literales en `CONSTANT_NODE`:** su `Symbol` se crea con `id = NULL`; los literales booleanos almacenan `1` o `0` en `symbol->value.intValue`.

Ejemplo de salida en `-target sem`:

```
  |   | Bloque
  |   |   | Declaracion Variable: a #2 (Tipo INT)
  |   |   | Asignacion a: a #2 (Tipo INT)
  |   |   |   | Constante INT: 1
  |   |   | While
  |   |   |   | Operacion: <          <- children[0]: condición
  |   |   |   | Bloque                <- children[1]: cuerpo
  |   |   | If-Else
  |   |   |   | Operacion: >          <- children[0]: condición
  |   |   |   | Bloque                <- children[1]: bloque then (childCount == 2)
  |   |   | If-Else
  |   |   |   | Operacion: >
  |   |   |   | Bloque                <- children[1]: bloque then
  |   |   |   | Bloque                <- children[2]: bloque else (childCount == 3)
  |   |   | Return                    <- childCount == 0
```

---

## 5. Compactación de hijos en `newNode`

```c
int count = 0;
NodeAST *temp[3];
if (left)  temp[count++] = left;
if (mid)   temp[count++] = mid;
if (right) temp[count++] = right;
```

`newNode` recibe hasta tres punteros y compacta hacia la izquierda los que no son `NULL`, reservando `children` únicamente para los `count` hijos presentes (o dejando `children = NULL` si `count == 0`):

| Constructor | Argumentos pasados a `newNode` | `childCount` resultante |
| --- | --- | --- |
| `newIfElseNode(cond, ifBlock, NULL)` | `(cond, ifBlock, NULL)` | `2` |
| `newIfElseNode(cond, ifBlock, elseBlock)` | `(cond, ifBlock, elseBlock)` | `3` |
| `newReturnNode(NULL)` | `(NULL, NULL, NULL)` | `0` |
| `newOperatorNode(OP_NOT, expr, NULL)` | `(expr, NULL, NULL)` | `1` |

Debido a esta compactación, los hijos obligatorios deben pasarse siempre antes que los opcionales; si un argumento intermedio fuera `NULL` y uno posterior no lo fuera, el hijo posterior se desplazaría de índice y rompería las posiciones esperadas por las macros de acceso.

---

## 6. Macros de acceso

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

Documentan el rol del hijo accedido según el layout de §4 y verifican el límite `childCount > index`, devolviendo `NULL` cuando un hijo opcional no está presente (como `GET_ELSE_BLOCK` en un `if` sin `else` o `GET_EXPRESSION` en un `return;` vacío).

---

## 7. Propiedad de memoria de los símbolos

Durante el parseo, cada nodo con nombre crea su propio `Symbol`. Sin embargo, tras el análisis semántico, todos los nodos que referencian a una misma variable o método (`ID_NODE`, `ASSIGNMENT_NODE`, `METHOD_CALL_NODE`) pasan a compartir el único `Symbol` registrado en la tabla de símbolos.

### Resolución de referencias (`resolveNodeSymbol`)

Cuando el análisis semántico o el chequeador de tipos vinculan una referencia con su declaración en la tabla de símbolos, invocan `resolveNodeSymbol`:

```c
void resolveNodeSymbol(NodeAST *node, Symbol *declared) {
    bool isProvisional = (node->symbol != NULL && node->symbol->uid == 0);
    if (isProvisional) {
        freeSymbol(node->symbol);
    }
    node->symbol = declared;
}
```

Los símbolos creados por el parser nacen con `uid == 0`; `appendSymbol` les asigna un `uid > 0` recién al insertarlos en un ámbito. `resolveNodeSymbol` libera el símbolo provisorio (`uid == 0`) antes de reapuntar `node->symbol` al símbolo declarado. Si el nodo ya apuntaba a un símbolo registrado (`uid != 0`), solo reasigna el puntero sin liberarlo.

### Regla de liberación en `freeAST`

`freeAST` recorre el árbol en postorden y decide si debe liberar `node->symbol` según la siguiente condición:

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

| Condición | Motivo por el que el nodo es dueño del `Symbol` |
| --- | --- |
| `VARIABLE_DECLARATION_NODE` o `METHOD_DECLARATION_NODE` | El nodo representa la declaración que originó el símbolo |
| `CONSTANT_NODE` | El símbolo del literal es exclusivo de ese nodo y no se inserta en la tabla de símbolos |
| `symbol->uid == 0` | Símbolo provisorio de una referencia que nunca llegó a resolverse (por ejemplo, en un AST liberado tras el parseo o con un identificador no declarado) |

Si una referencia ya fue resuelta (`uid != 0`), `hasOwnedSymbol` es `false` y `freeAST` no toca el símbolo para evitar una doble liberación cuando también se libera el nodo de declaración.

En una ejecución normal del compilador (`main.c`), el proceso termina inmediatamente después de la última etapa y el sistema operativo recupera la memoria sin invocar `freeAST(astRoot)`; `freeAST` se utiliza en los caminos de error de alocación y entre casos de prueba en las suites unitarias.

---

## 8. Número de línea y su heurística

```c
node->line = yylineno;

// if/while/method se reducen tras su bloque: usar la línea del encabezado, no la del '}'.
if ((nodeType == IF_ELSE_NODE || nodeType == WHILE_NODE || nodeType == METHOD_DECLARATION_NODE) && left != NULL) {
    node->line = left->line;
}
```

Como las acciones sintácticas se ejecutan al reducir, `yylineno` refleja la línea del último token de la producción. En construcciones multilínea (`if`, `while`, declaración de método), ese último token es la llave de cierre `}` del cuerpo. Tomar `left->line` (la línea de la condición en `if`/`while`, o de `PARAMETERS_NODE` en un método) hace que los errores semánticos se reporten en la línea del encabezado:

```c
if (a)        // línea 4 -> reporta: "Error semantico en la linea 4: La condicion del if debe ser de tipo BOOL, pero es INT."
{
    a = 2;
}             // línea 7 (yylineno al momento de reducir el IF_ELSE_NODE)
```

En sentencias simples repartidas en varias líneas (como una asignación multilínea sin estructura de control), `node->line` conserva el `yylineno` del `;` final.

---

## 9. Anotación entre fases

El AST no se clona ni se reconstruye entre etapas:

1. **Salida del parser (`-target parse`, archivo `.sint`):** las expresiones tienen `type = TYPE_VOID` y todos los símbolos tienen `uid = 0`.
2. **Salida del análisis semántico (`-target sem`, archivo `.sem`):** el analizador semántico y el chequeador de tipos decoran el mismo árbol en el lugar, asignando `uid > 0` a las declaraciones, resolviendo referencias con `resolveNodeSymbol` y anotando el `DataType` resultante en cada nodo de expresión y asignación.
3. **Entrada a [Generación de TAC](../05-codigo-intermedio/02-generacion-tac.md):** recorre el AST ya anotado (en postorden para expresiones, de modo que los operandos se evalúen antes que la operación) para emitir la lista de instrucciones de tres direcciones.

---

## 10. Cómo se extiende

### Nueva construcción con nodo propio (`do-while`, `for`)

1. Agregar el identificador al enum `NodeType` en `ast.h`.
2. Definir su layout de hijos en la tabla de §4 y, de ser necesario, añadir macros de acceso en `ast.h`.
3. Implementar su constructor con nombre en `ast.c` pasando los hijos obligatorios antes que los opcionales (§5), e incluir el `NodeType` en la heurística de línea de `newNode` si la producción se reduce después de un bloque (§8).
4. Agregar el caso correspondiente en `describeNode` (`ast.c`) para su impresión en `.sint`/`.sem`.
5. Incorporar el manejo del nuevo `NodeType` en los `switch` de `semantic_analyzer.c`, `type_checker.c` e `intermediate_code.c` (dichos `switch` poseen `default`, por lo que omitir uno no genera advertencia del compilador).

### Nuevo operador

No requiere un nuevo `NodeType`: basta con agregar la constante en `OperationType` (`types.h`), su representación textual en `getOperationName` (`ast.c`) y su tratamiento en el chequeador de tipos, TAC y código objeto, reutilizando `OPERATOR_NODE`.

---

## 11. Consideraciones y límites

- **`operationType` solo se inicializa en `OPERATOR_NODE`:** `newNode` no asigna `node->operationType`; leer ese campo en cualquier otro `NodeType` accede a memoria sin inicializar.
- **Dependencia del orden de argumentos en `newNode`:** la compactación de hijos `NULL` (§5) exige no pasar nunca un `NULL` delante de un hijo no nulo.
- **Propiedad de símbolos codificada en `uid == 0`:** la distinción entre símbolo provisorio y símbolo registrado en tabla depende de que `appendSymbol` asigne siempre `uid >= 1` (§7).
- **`switch` con rama `default` en fases posteriores:** al agregar un `NodeType`, es necesario actualizar manualmente todas las fases consumidoras.

---

## 12. Cómo se valida

- **Tests unitarios:** `tests/test_ast.c` verifica la creación de cada tipo de nodo, cantidad y orden de hijos, compactación de opcionales (`if` con y sin `else`, `return` con y sin expresión), declaraciones múltiples y liberación con `freeAST`.
- **Comparación entre árbol sintáctico y árbol anotado:**

```bash
./mi_compilador -target parse examples/valido_completo.ctds   # genera .sint (sin anotar)
./mi_compilador -target sem   examples/valido_completo.ctds   # genera .sem  (anotado)
```

---

**Siguiente:** [Tabla de Símbolos y Ámbitos](../04-semantico/01-tabla-de-simbolos.md)
