# Chequeo de Tipos

**Archivos:** `include/type_checker.h`, `src/semantic/type_checker.c` (256 líneas)

Sintetiza el tipo de cada expresión del [AST](../03-lexico-sintactico-y-ast/04-ast.md) y verifica la compatibilidad de tipos en las operaciones, resolviendo los identificadores contra la [tabla de símbolos](01-tabla-de-simbolos.md).

!!! note "Alcance del soporte de `float`"
    Las reglas de tipos para `float` (incluida la regla 14 de la especificación sobre conversiones entre `int` y `float`) están implementadas en el análisis semántico. En cambio, las etapas posteriores de código intermedio y código objeto están acotadas por el enunciado a tipos enteros y lógicos. El detalle de esta decisión se documenta en [Alcance y Extensiones](../../decisiones/alcance-y-extensiones.md#1-alcance-de-float).

---

## 1. Qué problema resuelve

El módulo implementa un sistema de tipos estático, nominal y monomórfico sobre las expresiones del lenguaje. Para cada nodo de expresión determina si la operación aplicada a sus operandos es válida y calcula su tipo resultante (`node->type`) como un atributo sintetizado de abajo hacia arriba (postorden).

El tipo inferido y el puntero al símbolo resuelto quedan anotados en el propio nodo del AST, de modo que las fases posteriores consultan directamente `node->type` y `node->symbol` sin recalcular reglas de tipos.

---

## 2. Interfaz pública

| Función | Propósito | Llamador |
| --- | --- | --- |
| `analyzeExpression(node, scope)` | Infiere, anota y devuelve el tipo de una expresión | `semantic_analyzer.c` y llamadas recursivas internas |
| `analyzeMethodCall(node, scope, isExpression)` | Valida una llamada a método contra su declaración | `analyzeExpression` (`isExpression = true`) y `semantic_analyzer.c` (`false`) |
| `isAssignable(expected, actual)` | Indica si un valor de tipo `actual` puede asignarse a un destino de tipo `expected` | `semantic_analyzer.c` (asignaciones y `return`) y validación de argumentos |

El parámetro `isExpression` de `analyzeMethodCall` distingue una llamada utilizada dentro de una expresión (`x = f();`) de una invocada como sentencia (`f();`), permitiendo rechazar métodos `void` cuando se exige un valor de retorno.

Además de devolver el `DataType`, `analyzeExpression` y `analyzeMethodCall` escriben `node->type` y reemplazan el símbolo provisorio del parser por el símbolo de la tabla mediante `resolveNodeSymbol` (definida en [AST](../03-lexico-sintactico-y-ast/04-ast.md); ver también [acciones del parser](../03-lexico-sintactico-y-ast/03-acciones-del-parser.md)).

---

## 3. Resolución de expresiones y supresión de cascadas

### Despacho por tipo de nodo

`analyzeExpression` evalúa las cuatro formas posibles de una expresión en C-TDS:

| Nodo | Comportamiento |
| --- | --- |
| `ID_NODE` | Busca el identificador con `lookupSymbol`, verifica que sea `VARIABLE_SYMBOL`, vincula `node->symbol` y copia `symbol->type` |
| `CONSTANT_NODE` | Asigna `node->type = node->symbol->type` (definido durante el análisis léxico) |
| `METHOD_CALL_NODE` | Delega en `analyzeMethodCall(node, scope, true)` |
| `OPERATOR_NODE` | Evalúa recursivamente los operandos e invoca las reglas de operadores |
| `default` | Devuelve `TYPE_ERROR` |

### Validación de variables y llamadas a métodos

- **Identificadores (`ID_NODE`):** se reporta error si `lookupSymbol` devuelve `NULL` (`Variable no declarada: x`) o si el símbolo encontrado corresponde a un método usado sin invocar (`El identificador 'f' no es una variable.`).
- **Llamadas (`METHOD_CALL_NODE`):** `analyzeMethodCall` verifica en orden:
  1. Que el identificador exista en algún ámbito visible.
  2. Que su categoría sea `METHOD_SYMBOL`.
  3. Que no retorne `TYPE_VOID` cuando `isExpression` es verdadero.
  4. Que la cantidad de argumentos coincida con la de parámetros formales.
  5. Que cada argumento satisfaga `isAssignable` respecto del tipo de `method->parameters->children[i]->type`.

El chequeo de retorno `void` se ejecuta después de confirmar que el símbolo es un método, garantizando que `method->type` sea válido antes de consultarlo. Finalmente, `resolveNodeSymbol(node, method)` vincula el nodo con la declaración para que el generador de TAC pueda leer su firma.

### Propagación de `TYPE_ERROR`

Al evaluar un `OPERATOR_NODE`, el chequeo calcula primero los tipos de los operandos izquierdo y derecho (en operadores unarios, `GET_RIGHT(node)` es `NULL` y `rightType` queda en `TYPE_VOID`). Si cualquiera de los operandos produjo `TYPE_ERROR`, la función propaga `TYPE_ERROR` hacia la raíz sin emitir un nuevo mensaje:

```c
if (leftType == TYPE_ERROR || rightType == TYPE_ERROR) {
    node->type = TYPE_ERROR;
    return TYPE_ERROR;
}
```

Tratar `TYPE_ERROR` como un elemento absorbente del sistema de tipos evita errores en cascada: una expresión como `a = noexiste + 1 + 2 + 3;` reporta únicamente el error primario de variable no declarada.

---

## 4. Reglas de tipos y conversiones

### Tabla de operadores

| Operador | Tipos exigidos en operandos | Tipo resultante | Admite promoción `int` / `float` |
| --- | --- | --- | --- |
| `+` `-` `*` `/` | Ambos numéricos (`int` o `float`) | `float` si alguno es `float`, si no `int` | Sí |
| `%` | Ambos `int` | `int` | No |
| `<` `>` | Ambos numéricos (`int` o `float`) | `bool` | Sí |
| `==` | Ambos del mismo tipo básico (`int`, `float` o `bool`) | `bool` | No |
| `&&` `\|\|` | Ambos `bool` | `bool` | No |
| `-` (unario) | `int` o `float` | Mismo tipo del operando | No |
| `!` | `bool` | `bool` | No |

Resumen de combinaciones válidas e inválidas:

```text
int + int       -> OK (int)      int % int       -> OK (int)
int + float     -> OK (float)    float % int     -> ERROR
float + float   -> OK (float)    int % float     -> ERROR
bool + int      -> ERROR

int < float     -> OK (bool)     int == int      -> OK (bool)
float > int     -> OK (bool)     float == float  -> OK (bool)
bool < bool     -> ERROR         bool == bool    -> OK (bool)
                                 int == float    -> ERROR
bool && bool    -> OK (bool)     int == bool     -> ERROR
int && int      -> ERROR

-1              -> OK (int)      !true           -> OK (bool)
-true           -> ERROR         !1              -> ERROR
```

### Asimetría entre `<` / `>` y `==`

Los operadores relacionales `<` y `>` admiten mezclar `int` y `float`, mientras que `==` exige que ambos operandos tengan exactamente el mismo tipo básico:

```c
case OP_LT:
case OP_GT:
    if (isNumeric(left) && isNumeric(right)) return TYPE_BOOL;

case OP_EQ:
    if (left == right && isBasicType(left)) return TYPE_BOOL;
```

Esto responde a la especificación del lenguaje, cuya regla para `==` exige igualdad de tipos. Además, dado que `==` utiliza el predicado `isBasicType` (`INT`, `FLOAT`, `BOOL`) en lugar de `isNumeric` (`INT`, `FLOAT`), permite comparar expresiones booleanas (`true == false`), operación prohibida en `<` y `>`.

### Política de asignación y pasaje de parámetros (`isAssignable`)

```c
if (expected == actual) return true;
if (expected == TYPE_FLOAT && actual == TYPE_INT) return true;
return false;
```

La compatibilidad en asignaciones, sentencias `return` y argumentos de llamadas permite únicamente tipos idénticos o la conversión por ensanchamiento (*widening*) de `int` a `float`, rechazando el truncamiento (*narrowing*) de `float` a `int`:

```text
float f; f = 1;    -> OK     (int a float)
int n;   n = 1.0;  -> ERROR  (float a int)
int n;   n = true; -> ERROR
```

Aunque la regla 14 de la especificación menciona conversiones y truncamientos, la implementación adopta una política conservadora que solo acepta el ensanchamiento implícito sin pérdida de magnitud (ver [Alcance y Extensiones](../../decisiones/alcance-y-extensiones.md#1-alcance-de-float)).

### Representación en el volcado `.sem`

El formateador del AST (`describeNode`) imprime el operador de un `OPERATOR_NODE` sin mostrar su campo `node->type`. Por ello, el tipo resultante de las expresiones intermedias reside en la estructura en memoria aunque no se liste explícitamente en la línea de `Operacion:` del archivo `.sem`:

```text
  | Asignacion a: f #2 (Tipo FLOAT)
  |   | Operacion: +
  |   |   | ID: n #3 (Tipo INT)
  |   |   | Constante FLOAT: 1.500000
```

---

## 5. Conexión con las fases vecinas

| Módulo origen / destino | Interacción |
| --- | --- |
| `ast.c` (entrada) | Entrega el árbol con `type = TYPE_VOID` y símbolos provisorios |
| `symbol_table.c` (entrada) | Provee `lookupSymbol` y las listas de parámetros en `symbol->parameters` |
| `semantic_analyzer.c` (entrada/salida) | Invoca a este módulo en cada expresión y utiliza el `DataType` devuelto para validar sentencias |
| `intermediate_code.c` (salida) | Consume `node->type` y `node->symbol` ya resueltos en cada nodo de expresión |

Reparto de responsabilidades dentro del análisis semántico:

| Aspecto | `type_checker.c` | [Análisis Semántico](03-analisis-semantico.md) (`semantic_analyzer.c`) |
| --- | --- | --- |
| Alcance | Expresiones (`ID`, literales, llamadas, operadores) | Declaraciones, bloques y sentencias de control |
| Propósito | Inferir `DataType` y vincular usos con declaraciones | Gestionar ámbitos, verificar condiciones booleanas, retornos y `main` |

---

## 6. Cómo se extiende

### Un operador nuevo

Agregar el caso correspondiente en `resolveOperation`. Por ejemplo, para `!=`:

```c
case OP_NEQ:
    if (left == right && isBasicType(left)) return TYPE_BOOL;
    return TYPE_ERROR;
```

Si se omite el caso en `resolveOperation`, el operador cae en `default` y cualquier expresión que lo utilice reporta error de tipos incompatibles.

### Permitir el truncamiento de `float` a `int`

En el chequeo de tipos basta con agregar la condición inversa en `isAssignable`:

```c
if (expected == TYPE_INT && actual == TYPE_FLOAT) return true;
```

Para soportarlo en todo el compilador haría falta además emitir la conversión en el código intermedio y la instrucción de truncamiento escalar (`cvttss2si`) en el generador x86-64.

### Un tipo básico nuevo (`char`, `double`)

Requiere actualizar los predicados `isNumeric` y/o `isBasicType` y definir la jerarquía de promoción en `resolveNumericOperation` (que actualmente asume únicamente dos tipos numéricos donde `TYPE_FLOAT` prevalece sobre `TYPE_INT`).

### Conversiones explícitas (*casts*)

Requeriría un nuevo `NodeType` en el AST y una rama en `analyzeExpression` que valide que el tipo del operando sea convertible al tipo objetivo y anote este último en `node->type`.

---

## 7. Trampas y límites conocidos

- **Distinción entre operación unaria y binaria en diagnósticos:** `analyzeOperator` decide el texto del mensaje (`operacion unaria` vs. `operacion binaria`) verificando si `GET_RIGHT(node)` es `NULL`.
- **Comentario desactualizado en `OP_MOD`:** en `type_checker.c` el comentario previo a `case OP_MOD` menciona a la división, aunque el código aplica la restricción de operandos enteros exclusivamente al operador módulo (`%`).
- **Valor centinela `rightType = TYPE_VOID` en unarios:** en operadores de aridad 1, `rightType` se inicializa en `TYPE_VOID`. Cualquier regla nueva en `resolveOperation` para un operador unario debe inspeccionar únicamente `left`.
- **Propagación silenciosa de `TYPE_ERROR` (§3):** cuando una subexpresión falla, los operadores superiores devuelven `TYPE_ERROR` sin emitir mensajes adicionales; el primer error reportado es la causa raíz.
- **Política restrictiva de conversión (§4):** `isAssignable` admite `int` hacia `float` pero rechaza `float` hacia `int`.

---

## 8. Cómo se valida

- **Suite unitaria:** `tests/test_semantic.c` verifica cada regla de tipos con casos válidos e inválidos construyendo el AST directamente en memoria (sin lexer ni parser).
- **Ejecución manual:** los diagnósticos de tipos se verifican ejecutando el compilador hasta la etapa semántica:

```bash
./mi_compilador -target sem examples/if_not_equals_type.ctds
```

---

**Siguiente:** [Análisis Semántico](03-analisis-semantico.md)
