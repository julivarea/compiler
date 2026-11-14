# Análisis Semántico

**Archivos:** `include/semantic_analyzer.h`, `src/semantic/semantic_analyzer.c` (299 líneas)

Recorre el AST para validar las restricciones sensibles al contexto del lenguaje, construyendo la [tabla de símbolos](01-tabla-de-simbolos.md) y delegando la inferencia de tipos de expresiones en el [chequeo de tipos](02-chequeo-de-tipos.md).

---

## 1. Qué problema resuelve

Verifica las reglas contextuales que una gramática libre de contexto no puede expresar: declaración previa de identificadores, ausencia de duplicados en un mismo ámbito, condiciones booleanas en estructuras de control, retorno garantizado en métodos no `void` y existencia de un método `main` sin parámetros.

Es la última etapa del front-end y produce un árbol decorado (el mismo AST con tipos y símbolos resueltos). Si finaliza sin errores, las fases siguientes asumen que el programa es semánticamente válido.

---

## 2. Interfaz pública

```c
Scope *analyzeSemantics(NodeAST *root, Scope *parent);
```

| Parámetro | Significado |
| --- | --- |
| `root` | Nodo cuya lista de hijos se analiza (la raíz del programa o un bloque) |
| `parent` | Ámbito contenedor (`NULL` indica el ámbito global) |

La función cumple dos roles según el valor de `parent`:

- **`parent == NULL`:** punto de entrada invocado desde `driver.c` (`analyzeSemantics(astRoot, NULL)`). Crea el ámbito global y, tras recorrer el árbol, verifica la existencia y firma de `main`.
- **`parent != NULL`:** abre un ámbito léxico anidado para un bloque, hereda el puntero `function` del ámbito padre y lo enlaza mediante `appendChildScope`.

El resultado de la validación se comunica incrementando el contador global `semantic_errors` de `error_handler`, consultado por `driver.c` al finalizar el recorrido (ver §6).

---

## 3. Recorrido del AST y reglas por construcción

### Despacho de nodos y apertura de ámbitos

`analyzeSemantics` crea un `Scope`, recorre los hijos del nodo mediante `analyzeChildren` e invoca al despachador interno `analyzeNodeSemantics`:

| Caso en `analyzeNodeSemantics` | Acción |
| --- | --- |
| `BLOCK_NODE` | Invoca `analyzeSemantics(node, currentScope)` abriendo un ámbito hijo |
| `METHOD_DECLARATION_NODE` | Registra el método y analiza sus parámetros y cuerpo |
| `METHOD_CALL_NODE` | Invoca `analyzeMethodCall(node, scope, false)` (llamada como sentencia) |
| `VARIABLE_DECLARATION_NODE` | Verifica unicidad en `currentScope` y registra la variable |
| `ASSIGNMENT_NODE` | Resuelve la variable destino y verifica compatibilidad de tipos |
| `RETURN_NODE` | Verifica el retorno contra `currentScope->function->type` |
| `IF_ELSE_NODE` / `WHILE_NODE` | Valida que la condición sea `TYPE_BOOL` y recorre los bloques hijos |
| `default` | Recorre recursivamente los hijos manteniendo el mismo ámbito |

La recursión mutua entre `analyzeSemantics` y `analyzeNodeSemantics` refleja en la jerarquía de `Scope` el anidamiento de bloques del código fuente. En `METHOD_CALL_NODE` se pasa `isExpression = false`, lo que permite invocar métodos `void` o descartar el valor de retorno cuando la llamada actúa como sentencia.

### Declaración de métodos y variables

Al procesar un `METHOD_DECLARATION_NODE` se sigue una secuencia específica:

1. Se verifica con `lookupSymbolInScope` que el nombre no esté repetido en el ámbito actual.
2. Se registra el símbolo del método en el ámbito global (`appendSymbol`) **antes** de analizar su cuerpo, habilitando la recursión directa.
3. Se crea el ámbito del método (`methodScope`) asociando `node->symbol` como `function`.
4. Se analizan los parámetros formales y luego las sentencias del cuerpo invocando `analyzeChildren(body, methodScope)` directamente sobre `methodScope` en lugar de abrir un sub-ámbito nuevo para las llaves del método.
5. Si el método no es `TYPE_VOID`, se verifica que todos los caminos retornen un valor mediante `guaranteesReturn(body)` (ver §4).

Debido al paso 4, los parámetros formales y las variables locales del nivel superior del método comparten ámbito, por lo que redeclarar el nombre de un parámetro produce un error semántico. En cambio, un bloque `{ ... }` interno pasa por `BLOCK_NODE`, abre un ámbito hijo y sí puede ocultar un parámetro:

```c
int f(int n){ int n; return n; }         // Error: 'n' ya fue declarada en este ambito
int g(int n){ { int n; n = 99; } return n; } // Valido: retorna el parametro n
```

### Asignaciones y sentencias `return`

- **Asignaciones (`ASSIGNMENT_NODE`):** se inicializa `node->type = TYPE_ERROR` de forma preventiva, se busca el identificador con `lookupSymbol`, se verifica que sea `VARIABLE_SYMBOL`, se vincula el nodo mediante `resolveNodeSymbol` (ver [AST](../03-lexico-sintactico-y-ast/04-ast.md)) y se comprueba `isAssignable(resolvedSymbol->type, exprType)`.
- **Retornos (`RETURN_NODE`):** se compara la presencia y tipo de la expresión de retorno contra `currentScope->function->type`:

| Expresión en `return` | Tipo del método | Resultado |
| --- | --- | --- |
| Ausente (`return;`) | `void` | Válido |
| Ausente (`return;`) | No `void` | Error: falta valor de retorno |
| Presente (`return expr;`) | `void` | Error: método `void` no debe retornar valor |
| Presente (`return expr;`) | No `void` | Válido si `isAssignable(method->type, exprType)` |

Al utilizar `isAssignable`, un método con retorno `float` admite retornar una expresión `int`.

### Condiciones booleanas y método `main`

- **Condiciones (`if` / `while`):** el lenguaje exige que la expresión condicional tenga tipo `TYPE_BOOL` estricto (no existen enteros evaluados como booleanos). Si la condición produjo `TYPE_ERROR`, se suprime el diagnóstico para evitar errores en cascada.
- **Método `main`:** al cerrar el ámbito global se verifica que exista el símbolo `main` en dicho ámbito, que sea `METHOD_SYMBOL` y que no declare parámetros. El tipo de retorno no está restringido a `void`: tanto `void main()` como `int main()` son válidos, lo que permite a los tests de integración observar el resultado a través del código de salida del proceso.

---

## 4. Análisis de retorno garantizado (`guaranteesReturn`)

Todo método con tipo de retorno distinto de `void` debe ejecutar un `return` en todos sus caminos de control. Como las construcciones del lenguaje son estructuradas (sin saltos arbitrarios `goto`), esta propiedad de alcanzabilidad se resuelve mediante una recursión estructural sobre el AST sin necesidad de construir el grafo de flujo de control que sí utiliza el [análisis de variables vivas](../06-optimizador/01-variables-vivas.md):

```c
case RETURN_NODE:
    return true;

case BLOCK_NODE:
    for (int i = 0; i < node->childCount; i++)
        if (guaranteesReturn(node->children[i])) return true;
    return false;

case IF_ELSE_NODE: {
    NodeAST *elseBlock = GET_ELSE_BLOCK(node);
    if (elseBlock == NULL) return false;
    return guaranteesReturn(GET_IF_BLOCK(node)) && guaranteesReturn(elseBlock);
}

default:
    return false;
```

- Un `RETURN_NODE` garantiza retorno.
- Un `BLOCK_NODE` garantiza retorno si al menos una de sus sentencias hijas lo garantiza.
- Un `IF_ELSE_NODE` garantiza retorno únicamente si posee rama `else` y ambos bloques garantizan retorno.
- Cualquier otro nodo (incluido `WHILE_NODE`) devuelve `false`.

| Caso | Resultado |
| --- | --- |
| `int f(){ return 1; }` | Válido |
| `int f(int n){ if (n>0) { return 1; } else { return 2; } }` | Válido |
| `int f(int n){ if (n>0) { return 1; } }` | Error (falta rama `else`) |
| `int f(int n){ if (n>0) { return 1; } return 2; }` | Válido |
| `int f(){ while (true) { return 1; } }` | Error |

El rechazo de `while (true) { return 1; }` responde a una aproximación estática conservadora: como `guaranteesReturn` no evalúa constantes en condiciones de ciclo, asume que la condición de cualquier `while` podría ser falsa al inicio. Esto preserva la corrección del chequeo (nunca acepta un método que pueda terminar sin retornar valor) a costa de rechazar ciclos infinitos con literal `true`.

---

## 5. Catálogo de errores semánticos

La siguiente tabla resume los diagnósticos emitidos directamente por `semantic_analyzer.c` (los diagnósticos de operadores, identificadores en expresiones y llamadas se listan en [chequeo de tipos](02-chequeo-de-tipos.md)):

| Situación | Mensaje emitido |
| --- | --- |
| Variable repetida en el mismo ámbito | `La variable 'g' ya fue declarada previamente en este ambito.` |
| Nombre de método repetido en el ámbito | `El metodo 'f' ya fue declarado previamente en este ambito.` |
| Asignación a identificador no declarado | `Asignacion a variable no declarada 'noexiste'.` |
| Asignación al nombre de un método | `Ese nombre de simbolo corresponde al metodo 'f'.` |
| Condición de `if` o `while` no booleana | `La condicion del if debe ser de tipo BOOL, pero es INT.` |
| Tipo incompatible en asignación | `El tipo de la expresion no coincide con la variable 'a'.` |
| `return;` sin valor en método no `void` | `El metodo 'f' espera un valor de retorno de tipo INT, pero no se especifico ninguno.` |
| `return expr;` en método `void` | `El metodo 'f' es void y no deberia retornar un valor.` |
| Tipo incompatible en `return` | `El tipo de retorno no coincide en el metodo 'f': se esperaba INT pero se obtuvo FLOAT.` |
| Método no `void` con camino sin retorno | `El metodo 'f' no es void: todos sus caminos de ejecucion deben terminar en un return.` |
| Ausencia de `main` | `El programa no contiene la definicion del metodo 'main'.` |
| `main` declarado como variable | `'main' debe ser un metodo, no una variable.` |
| `main` con parámetros formales | `El metodo 'main' no debe tener parametros.` |
| Fallo de reserva de memoria para ámbito | `Error: no se pudo reservar memoria para el ambito.` |

Nota: si se declara una variable global `int g;` y luego un método `int g(){...}`, `analyzeMethodNode` emite `El metodo 'g' ya fue declarado previamente en este ambito.` sin distinguir la categoría del símbolo preexistente.

---

## 6. Diagnóstico acumulativo y criterios sobre la especificación

### Recuperación y corte del pipeline

A diferencia del parser —que aborta en el primer error sintáctico—, el análisis semántico recorre el AST completo acumulando todos los errores independientes de la unidad de traducción. Al finalizar, `runSemantic` en `driver.c` inspecciona `semantic_errors`:

```c
if (semantic_errors > 0) {
    fprintf(stderr, "\nCompilacion abortada: se encontraron %d errores semanticos.\n", semantic_errors);
    return 1;
}
```

Si `semantic_errors > 0`, el proceso termina con código `1` antes de invocar la generación de código intermedio.

### Criterios adoptados ante puntos abiertos de la especificación

1. **Igualdad (`==`) frente a relacionales (`<`, `>`):** la regla 11 exige operandos del mismo tipo en `==`, mientras que la regla 10 pide operandos `int` o `float` en operadores relacionales y la regla 14 habilita coerciones. Se dio precedencia a la restricción específica de `==` (sin coerción entre `int` y `float`), manteniendo la promoción numérica en `<` y `>` (ver [chequeo de tipos](02-chequeo-de-tipos.md)).
2. **Resto entero (`%`):** aunque la regla 10 agrupa a `%` con los operadores aritméticos, la sección *Expresiones* de la especificación lo define como el resto de la división entera, por lo que se restringió a operandos `TYPE_INT`.
3. **Código inalcanzable tras `return`:** la especificación no prohíbe sentencias posteriores a un `return` ni múltiples `return` en un mismo bloque (`examples/two_returns.ctds`). El analizador semántico continúa validando las sentencias siguientes para no ocultar errores en ellas, y la eliminación del código inalcanzable queda a cargo del [optimizador](../06-optimizador/02-optimizador.md) mediante la pasada `dead-code`.

> Nota sobre el enunciado: el programa de ejemplo de la página 3 de la especificación declara `void main()` con `return 1;`, lo cual infringe la regla 6 del propio documento.

---

## 7. Conexión con las fases vecinas

| Módulo origen / destino | Interacción |
| --- | --- |
| `ast.c` (entrada) | Entrega el AST sin anotar (`type = TYPE_VOID`, `uid = 0`) |
| `symbol_table.c` (soporte) | Provee `createScope`, `appendSymbol`, `lookupSymbol` y `lookupSymbolInScope` |
| `type_checker.c` (soporte) | Provee `analyzeExpression`, `analyzeMethodCall` e `isAssignable` |
| `intermediate_code.c` (salida) | Recibe el AST decorado con tipos resueltos, `uid` asignados y garantía de validez semántica |

---

## 8. Cómo se extiende

### Una construcción de control nueva (`for`, `do while`)

Si la construcción se desazucara en el parser en términos de `while` e `if` (ver [análisis sintáctico](../03-lexico-sintactico-y-ast/02-analisis-sintactico.md)), este módulo no requiere cambios. Si se introduce un `NodeType` específico, se debe:

1. Agregar el caso en el `switch` de `analyzeNodeSemantics`.
2. Implementar la validación de su condición (`analyzeBooleanCondition`) y el recorrido de sus hijos.
3. Definir su comportamiento en `guaranteesReturn` (el `default` devuelve `false` de forma segura).

Omitir el paso 1 haría que el nodo caiga en el `default` de `analyzeNodeSemantics`, recorriendo sus hijos sin validar la condición.

### Sentencias `break` y `continue`

Requieren verificar que la sentencia se encuentre dentro de un ciclo activo. Puede implementarse añadiendo un contador o booleano heredado en `Scope` (análogo al puntero `function`) que se active al abrir el cuerpo de un `WHILE_NODE`.

### Declaración adelantada de métodos

Para permitir que un método invoque a otro declarado más abajo en el archivo, basta con dividir el análisis del ámbito global en dos pasadas: la primera registra todas las firmas de `METHOD_DECLARATION_NODE` con `appendSymbol` y la segunda analiza los cuerpos.

### Reconocer `while (true)` en `guaranteesReturn`

Puede añadirse un caso `WHILE_NODE` en `guaranteesReturn` que verifique si `GET_CONDITION(node)` es un `CONSTANT_NODE` booleano con valor verdadero y, en tal caso, delegue en `guaranteesReturn(GET_WHILE_BLOCK(node))`.

---

## 9. Trampas y límites conocidos

- **`while (true)` no garantiza retorno (§4):** decisión conservadora del análisis estructural.
- **Liberación de símbolos provisorios en asignaciones:** `analyzeAssignmentNode` utiliza `resolveNodeSymbol(node, resolvedSymbol)` para liberar el símbolo temporal creado por el parser antes de apuntar al de la tabla.
- **Chequeo defensivo de `return` fuera de método:** la validación `currentScope->function == NULL` en `analyzeReturnNode` es inalcanzable desde código fuente porque la gramática solo admite sentencias dentro del cuerpo de un método.
- **Mensaje de colisión de nombres en métodos (§5):** reporta que el método ya fue declarado incluso cuando el símbolo previo del mismo nombre era una variable global.
- **`default` permisivo en `analyzeNodeSemantics` (§8):** un nuevo `NodeType` no contemplado en el `switch` se recorre recursivamente sin emitir advertencia.
- **Retorno de `main` flexible (§3):** se acepta cualquier tipo de retorno en `main`, siempre que no reciba parámetros.
- **El ámbito global retornado no se libera:** `driver.c` descarta el puntero devuelto por `analyzeSemantics` sin llamar a `freeScope`.

---

## 10. Cómo se valida

- **Suite unitaria:** `tests/test_semantic.c` prueba cada regla contextual con casos válidos e inválidos construidos con `tests/support/ast_builder.h`, aislando el análisis semántico del lexer y del parser.
- **Comparación de volcados `.sint` y `.sem`:** permite inspeccionar las anotaciones de tipos y `uid` agregadas sobre el AST:

```bash
./mi_compilador -target parse examples/programa.ctds   # .sint: AST sin anotar
./mi_compilador -target sem   examples/programa.ctds   # .sem: AST con tipos y uid
```

- **Casos de regresión en `examples/`:**

```bash
# Deben finalizar con código de salida 1:
./mi_compilador -target sem examples/programa_con_error.ctds
./mi_compilador -target sem examples/method_without_return.ctds
./mi_compilador -target sem examples/asignation_method.ctds
./mi_compilador -target sem examples/aliasing_parameters_in_method.ctds

# Debe finalizar con código de salida 0:
./mi_compilador -target sem examples/two_returns.ctds
```

---

**Siguiente:** [La Representación Intermedia (TAC)](../05-codigo-intermedio/01-representacion-tac.md)
