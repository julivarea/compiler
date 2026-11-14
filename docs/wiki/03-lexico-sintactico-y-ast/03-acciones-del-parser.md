# Acciones del Parser

**Archivos:** `include/parser_helpers.h`, `src/parser/parser_helpers.c` (146 líneas)

Funciones auxiliares invocadas por las acciones de [la gramática](02-analisis-sintactico.md) para acumular listas de nodos durante las reducciones ascendentes.

---

## 1. Qué problema resuelve

En un parser ascendente (LALR), las construcciones de longitud variable —declaraciones, parámetros formales, sentencias de un bloque, argumentos de una llamada— se reducen elemento por elemento, mientras que un nodo padre del [AST](04-ast.md) requiere recibir su arreglo de hijos completo (`NodeAST **children`).

Este módulo implementa el contenedor temporal `NodeArray` que acumula los nodos a medida que el parser reduce las producciones recursivas. Una vez completada la lista, su buffer interno se transfiere al nodo padre y el `NodeArray` se descarta: ningún `NodeArray` sobrevive una vez finalizado `yyparse()`.

### Reparto de responsabilidades con `ast.c`

| Módulo | Responsabilidad |
| --- | --- |
| `parser_helpers.c` | Acumular nodos en listas dinámicas temporales (`NodeArray`) y resolver declaraciones múltiples (`int a, b, c;`) |
| [`ast.c`](04-ast.md) | Construir nodos del AST, adoptar el buffer de un `NodeArray` como hijos (`resolveNodeArray`) y liberar subárboles (`freeAST`) |

---

## 2. Interfaz pública

`include/parser_helpers.h` expone la estructura `NodeArray` (utilizada en la `%union` y en las declaraciones `%type <array>` de `bison.y`) y cuatro funciones:

| Función | Comportamiento |
| --- | --- |
| `createNodeArray(int initialCapacity)` | Reserva un `NodeArray` y su arreglo `items` (usa capacidad `4` si `initialCapacity <= 0`). Si falla el segundo `malloc`, libera la estructura antes de devolver `NULL` |
| `appendNode(NodeArray *array, NodeAST *node)` | Agrega `node` al final de `array` duplicando la capacidad cuando se llena. Si `array` es `NULL`, crea la lista; si `node` es `NULL`, no hace nada. Si falla `realloc`, libera `node` con `freeAST` y conserva la lista previa |
| `mergeNodeArrays(NodeArray *first, NodeArray *second)` | Concatena los punteros de `second` al final de `first` mediante `memcpy` (ajustando el tamaño con `realloc`), libera el contenedor `second` y devuelve `first`. Si falla `realloc`, libera los nodos de `second` con `freeAST` |
| `resolveVariableDefinition(DataType type, NodeArray *identifiers)` | Transforma en el lugar una lista de `ID_NODE` provisorios en nodos `VARIABLE_DECLARATION_NODE` con el `DataType` indicado (ver §6) |

Las acciones de `bison.y` no llaman a `createNodeArray` directamente: toda lista comienza en `NULL` en la producción base y es instanciada bajo demanda por la primera llamada a `appendNode`.

---

## 3. La estructura `NodeArray`

```c
typedef struct NodeArray {
    NodeAST **items;    /* Arreglo dinámico de punteros a nodos AST */
    int count;          /* Cantidad actual de nodos */
    int capacity;       /* Capacidad reservada */
} NodeArray;
```

- **Arreglo contiguo en lugar de lista enlazada:** como el destino final de los elementos es `NodeAST.children` (también de tipo `NodeAST **`), almacenar los punteros en un arreglo contiguo permite transferir el buffer directamente al nodo padre en $O(1)$ sin copiar ni recorrer los elementos (ver §5).
- **Crecimiento amortizado:** `appendNode` duplica `capacity` cuando `count == capacity`, realizando $O(\log N)$ realojamientos para $N$ inserciones. En cambio, `mergeNodeArrays` redimensiona `first->items` al total exacto (`first->count + second->count`), dado que se utiliza para combinar listas de declaraciones ya formadas.

---

## 4. El convenio de `NULL`

Las funciones de listas tratan `NULL` como representación válida de una lista vacía o de un nodo inexistente. Gracias a este invariante, ninguna acción semántica de `bison.y` necesita condicionales `if`:

| Llamada | Resultado | Caso de uso en `bison.y` |
| --- | --- | --- |
| `appendNode(NULL, node)` | Crea un `NodeArray` de capacidad 4 e inserta `node` | Primera reducción tras una producción `%empty` que devolvió `NULL` |
| `appendNode(array, NULL)` | Devuelve `array` sin cambios | Sentencia vacía `';'` (`Statement : ';' { $$ = NULL; }`) |
| `mergeNodeArrays(NULL, x)` / `mergeNodeArrays(x, NULL)` | Devuelve `x` | Bloques sin declaraciones locales o sin sentencias |
| `newBlockNode(NULL)` / `newParametersNode(NULL)` / `newMethodCallNode(id, NULL)` | Inicializa el nodo con `children = NULL` y `childCount = 0` | Bloques vacíos `{ }`, métodos sin parámetros y llamadas sin argumentos |

Por ejemplo, en la regla de sentencias:

```
Statements : %empty               { $$ = NULL; }
           | Statements Statement { $$ = appendNode($1, $2); }
```

En la primera sentencia no vacía, `$1` es `NULL` y `appendNode` crea el acumulador; si aparece un `;` suelto, `$2` es `NULL` y `appendNode` lo ignora sin dejar rastro en el AST.

---

## 5. Transferencia de propiedad en $O(1)$

Cuando se reduce el nodo padre (`newBlockNode`, `newParametersNode` o `newMethodCallNode`), la función interna `resolveNodeArray` en [`ast.c`](04-ast.md) adopta el buffer del `NodeArray` sin copiar el arreglo de punteros:

```c
parent->children   = array->items;   /* el nodo padre adopta el buffer */
parent->childCount = array->count;
free(array);                         /* se libera únicamente la envoltura */
```

Una vez invocada esta transferencia, el `NodeArray` deja de existir y el arreglo `items` pasa a ser propiedad exclusiva del `NodeAST` padre (que lo liberará en `freeAST`). Por ejemplo, al reducir un bloque:

```
Block : '{' VariableDeclarations Statements '}' { $$ = newBlockNode(mergeNodeArrays($2, $3)); }
```

`mergeNodeArrays` concatena las declaraciones delante de las sentencias y libera el contenedor de `$3`; luego `newBlockNode` adopta el buffer resultante y libera el contenedor de `$2`.

| Caso de borde | Comportamiento |
| --- | --- |
| Fallo al reservar el nodo padre (`parent == NULL`) | `resolveNodeArray` libera todos los subárboles de la lista con `freeAST`, libera `array->items` y libera `array` |
| `array == NULL` o `array->count == 0` | El nodo padre queda con `children = NULL` y `childCount = 0` |

---

## 6. Declaraciones múltiples (`int a, b, c;`)

En la producción `VariableDeclaration : Type IdentifierList ';'`, el parser reduce primero `IdentifierList` de izquierda a derecha antes de reducir `VariableDeclaration`. Durante las reducciones de `IdentifierList`, el tipo declarado en `Type` está en la pila del parser pero fuera del alcance de la acción de `IdentifierList`.

Por ello, la construcción se realiza en dos pasos:

1. **Creación provisoria en `IdentifierList`:** cada identificador se crea mediante `newIdentifierNode($1)` como un `ID_NODE` con `type = TYPE_VOID` y su `Symbol` también en `TYPE_VOID`.
2. **Resolución en `VariableDeclaration`:** al reducir la regla completa, `resolveVariableDefinition(type, identifiers)` recorre el `NodeArray` y modifica cada nodo en el lugar:

```c
node->nodeType = VARIABLE_DECLARATION_NODE;
node->type = type;
node->symbol->type = type;
```

No se reservan ni liberan nodos adicionales: cada `ID_NODE` pasa a ser un `VARIABLE_DECLARATION_NODE` dueño de su `Symbol`. Así, `int a, b, c;` produce tres nodos de declaración independientes en el bloque contenedor:

```
.sint (tras el parseo, uid = 0):
  | Declaracion Variable: a #0 (Tipo INT)
  | Declaracion Variable: b #0 (Tipo INT)
  | Declaracion Variable: c #0 (Tipo INT)

.sem (tras el análisis semántico, con uid asignado):
  | Declaracion Variable: a #1 (Tipo INT)
  | Declaracion Variable: b #2 (Tipo INT)
  | Declaracion Variable: c #3 (Tipo INT)
```

Los parámetros formales (`Parameters : Type ID | Parameters ',' Type ID`) no requieren este paso intermedio porque cada parámetro incluye su propio `Type` en la misma producción, permitiendo que `newParameterDeclaration` construya el nodo con su tipo definitivo en una sola reducción.

---

## 7. Cómo se extiende

### Nueva construcción con una lista de hijos

No requiere modificar `parser_helpers.c`: la producción recursiva en `bison.y` acumula los elementos con `appendNode` (devolviendo `NULL` en el caso base) y pasa el `NodeArray *` al constructor correspondiente en `ast.c`, que adopta el buffer mediante `resolveNodeArray`.

### Declaraciones con inicialización (`int a = 5, b = 10;`)

Requiere que cada elemento de `IdentifierList` pueda llevar como hijo opcional la expresión inicializadora (o crear ya un nodo de declaración con `TYPE_VOID` e hijo opcional), manteniendo `resolveVariableDefinition` para propagar el `DataType` común al reducir `VariableDeclaration`, además de validar el tipo del inicializador en el análisis semántico y emitir la asignación en TAC.

### Agregar un nuevo `DataType`

No requiere cambios en este módulo: `resolveVariableDefinition` recibe `DataType` por valor y lo asigna de forma opaca a `node->type` y `node->symbol->type`.

---

## 8. Consideraciones y límites

- **Invalidación del `NodeArray *` tras construir el padre:** una vez entregado a `newBlockNode`, `newParametersNode` o `newMethodCallNode`, el contenedor `NodeArray` fue liberado; reutilizar ese puntero produce *use-after-free*.
- **Diferencia de política de redimensionado:** `appendNode` crece por duplicación de capacidad, mientras que `mergeNodeArrays` ajusta el bloque al tamaño exacto de la concatenación.
- **Símbolos provisorios en referencias:** para usos de identificadores (`ID_NODE`), asignaciones y llamadas a métodos, el parser crea un `Symbol` provisorio con `uid = 0`. A diferencia de las declaraciones múltiples (donde `resolveVariableDefinition` convierte el nodo en declaración dueña de ese símbolo), en las referencias el análisis semántico reemplaza el símbolo provisorio por el de la tabla de símbolos mediante `resolveNodeSymbol` (definida en [`ast.c`](04-ast.md)), que libera el símbolo provisorio para evitar fugas de memoria.

---

## 9. Cómo se valida

- **Tests unitarios:** `tests/test_ast.c` verifica el armado de listas, el orden de hijos, declaraciones múltiples y bloques vacíos; `tests/test_parser.c` valida las producciones de la gramática que los invocan.
- **Inspección de `.sint` y `.sem`:**

```bash
./mi_compilador -target parse examples/programa.ctds
./mi_compilador -target sem   examples/programa.ctds
```

- **Control de fugas de memoria:** `valgrind --leak-check=full ./mi_compilador -target sem examples/factorial.ctds` verifica que no queden contenedores `NodeArray` ni símbolos provisorios huérfanos desde `newSymbol` (ver §8).

---

**Siguiente:** [Árbol de Sintaxis Abstracta (AST)](04-ast.md)
