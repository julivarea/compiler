# Generación de Código Intermedio

**Archivos:** `include/intermediate_code.h`, `src/intermediate_code/intermediate_code.c` (392 líneas)

Recorre el [AST](../03-lexico-sintactico-y-ast/04-ast.md) anotado mediante traducción dirigida por la sintaxis para emitir la lista lineal de [cuádruplas TAC](01-representacion-tac.md).

!!! note "Esta etapa cubre tipos enteros y lógicos"
    El enunciado acota el código intermedio a tipos enteros y lógicos. Antes de emitir instrucciones, el generador inspecciona el AST y, si detecta el uso de `float`, rechaza el programa indicando la línea correspondiente (ver §7).

---

## 1. Qué problema resuelve

Traduce la representación jerárquica del AST a una secuencia lineal de instrucciones elementales:

- **Descompone expresiones anidadas** mediante un recorrido en postorden, alojando los resultados intermedios en temporales ilimitados.
- **Traduce el control de flujo estructurado** (`if`, `while`) y los operadores lógicos de corto circuito (`&&`, `||`) a etiquetas simbólicas y saltos condicionales o incondicionales.
- **Verifica la restricción de alcance de tipos** antes de iniciar la emisión (ver §7).

Al ejecutarse después de un [análisis semántico](../04-semantico/03-analisis-semantico.md) exitoso, el generador asume que el programa es válido y no repite chequeos de tipos ni de ámbitos.

---

## 2. Interfaz pública y estado del generador

```c
const TACList *generateIntermediateCode(NodeAST *root);
bool hasUnsupportedTypeError(void);
```

`buildIntermediateCode` en `driver.c` invoca a `generateIntermediateCode` una vez por compilación. El calificador `const` del retorno indica que el generador cede la lista terminada; cuando se activa la etapa de optimización, `driver.c` realiza un cast explícito a `(TACList *)` para que `optimizeIntermediateCode` pueda modificarla en el lugar.

### Estado interno del módulo

El generador mantiene cinco variables estáticas que se reinician al comienzo de cada invocación a `generateIntermediateCode`:

| Variable | Propósito |
| --- | --- |
| `currentCode` | Lista `TACList` en construcción |
| `tacMemoryError` | Bandera que registra fallos de reserva de memoria durante la emisión (§7) |
| `tacUnsupportedType` | Indica si la generación se abortó por presencia de `TYPE_FLOAT` (§7) |
| `temporaryCount` | Contador de temporales (`t0`, `t1`, ...); se reinicia a `0` al comenzar cada método |
| `labelCount` | Contador de etiquetas (`L0`, `L1`, ...); es global a todo el programa |

Los temporales se numeran por método porque su alcance es estrictamente local al [marco de pila](../07-codigo-objeto/01-marco-de-pila.md) de cada función. En cambio, las etiquetas se traducen a símbolos de ensamblador (`.Ln`) dentro del mismo archivo `.s`, por lo que `labelCount` nunca se reinicia entre métodos.

### Punto único de emisión (`emit`)

Todas las funciones generadoras agregan cuádruplas a través de `emit`:

```c
static void emit(TACOperation operation, TACOperand result,
                 TACOperand firstOperand, TACOperand secondOperand);
```

Los campos no utilizados por un opcode se completan con `tacNone()`. En las instrucciones que involucran etiquetas, la posición del operando depende del opcode: en `TAC_LABEL` la etiqueta ocupa `result`, en `TAC_GOTO` ocupa `firstOperand` y en `TAC_IF_FALSE` / `TAC_IF_TRUE` ocupa `secondOperand`.

---

## 3. Esquemas de traducción de sentencias y expresiones

El recorrido se divide en dos funciones despachadoras mutuamente recursivas:

### `generateStatementCode(node)`

Procesa construcciones que ejecutan efectos sin producir un valor:

| Nodo | Función delegada / Acción |
| --- | --- |
| `METHOD_DECLARATION_NODE` | `generateMethodDeclarationCode` |
| `ASSIGNMENT_NODE` | `generateAssignmentCode` |
| `METHOD_CALL_NODE` | `generateCallStatementCode` |
| `RETURN_NODE` | `generateReturnCode` |
| `IF_ELSE_NODE` | `generateIfElseCode` |
| `WHILE_NODE` | `generateWhileCode` |
| `VARIABLE_DECLARATION_NODE` | `generateVariableDeclarationCode` |
| `BLOCK_NODE` y `default` | `generateBlockCode` (recorre secuencialmente los hijos) |

Para distinguir declaraciones globales de locales sin depender de la profundidad en el árbol, `generateVariableDeclarationCode` consulta `node->symbol->isGlobal` y emite `TAC_GLOBAL_DECL` o `TAC_LOCAL_DECL` según corresponda (ver [ADR 002](../../decisiones/adr-002-resolucion-variables-globales-isglobal.md)).

### `generateExpressionCode(node)`

Evalúa una expresión y devuelve el `TACOperand` donde reside su resultado:

| Nodo | Operando devuelto | ¿Emite instrucciones? |
| --- | --- | --- |
| `ID_NODE` | `tacVariable(node->symbol)` | No |
| `CONSTANT_NODE` | `tacConstantInt` o `tacConstantBool` | No |
| `OPERATOR_NODE` | Temporal `tN` con el resultado | Sí |
| `METHOD_CALL_NODE` | Temporal `tN` con el valor de retorno | Sí |

Como los identificadores y literales devuelven operandos atómicos sin emitir instrucciones, una expresión como `x + 1` produce una única cuádrupla `t0 = x@3 + 1` sin copias intermedias a temporales.

### Aplanamiento de operadores en postorden

```c
TACOperand left = generateExpressionCode(GET_LEFT(node));        /* 1 */
TACOperand result = newTemporary(node->type);                    /* 2 */
NodeAST *rightNode = GET_RIGHT(node);
TACOperand right = (rightNode != NULL) ? generateExpressionCode(rightNode)  /* 3 */
                                       : tacNone();

emit(getTACOperation(node->operationType), result, left, right);  /* 4 */
return result;
```

La aridad se determina por la estructura del nodo (`GET_RIGHT(node) == NULL` en operadores unarios `-` y `!`). Como el temporal `result` se reserva en el paso 2 —después de evaluar el subárbol izquierdo pero antes del derecho—, cuando el operando derecho contiene subexpresiones compuestas los números de temporal reflejan el orden de reserva en la recursión y no el orden de aparición en el archivo `.ci`:

```text
param 1
t0 = call a, 1      # operando izquierdo (t0)
param 2
t2 = call b, 1      # operando derecho (t2, reservado después de t1)
t1 = t0 + t2        # suma (t1 fue reservado en el paso 2)
```

---

## 4. Evaluación de corto circuito (`&&` y `||`)

Los operadores `OP_AND` y `OP_OR` se interceptan antes del caso general y se traducen mediante código de salto (*jumping code*) en `generateShortCircuitCode`, evitando evaluar el segundo operando cuando el primero ya define el resultado:

```text
       result = a && b                          result = a || b
    ───────────────────────                  ───────────────────────
    result = a                               result = a
    if result false goto L_end               if result true goto L_end
    result = b                               result = b
    L_end:                                   L_end:
```

Ambas traducciones comparten implementación y solo difieren en el opcode de salto condicional:

```c
TACOperation jumpOp = (node->operationType == OP_AND) ? TAC_IF_FALSE : TAC_IF_TRUE;
emit(jumpOp, tacNone(), result, endLabel);
```

El resultado parcial se almacena siempre en un temporal nuevo `result` en lugar de reutilizar el operando izquierdo, dado que si el operando izquierdo fuera una variable del programa, sobrescribirla con el valor del operando derecho alteraría su estado.

---

## 5. Control de flujo sin *backpatching*

### Esquemas de traducción para `if`, `if-else` y `while`

```text
   if (cond) S1               if (cond) S1 else S2             while (cond) S1
────────────────────────   ──────────────────────────       ──────────────────────
   <cond>                     <cond>                        L_start:
   if t false goto L_end      if t false goto L_else           <cond>
   <S1>                       <S1>                             if t false goto L_end
L_end:                        goto L_end                       <S1>
                           L_else:                             goto L_start
                              <S2>                          L_end:
                           L_end:
```

En los compiladores de una sola pasada que emiten código durante el análisis sintáctico, los saltos hacia adelante requieren *backpatching* (emitir saltos con destino pendiente y completarlos al conocer la dirección objetivo). En este compilador no se requiere *backpatching* por dos motivos:

1. **Etiquetas simbólicas:** `newLabel()` genera un operando `Ln` que puede referenciarse en un salto antes de emitir la instrucción `TAC_LABEL` correspondiente; las direcciones relativas las resuelve recién el ensamblador.
2. **AST completo en memoria:** la estructura completa de cada sentencia compuesta ya está construida al iniciar la generación.

### Ejemplo con ciclos y condicionales anidados

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

```text
L0:                      # inicio while externo
t0 = i@2 < 3
if t0 false goto L1
j@3 = 0
L2:                      # inicio while interno
t1 = j@3 < 2
if t1 false goto L3
t2 = j@3 > 0
if t2 false goto L4      # if sin else
t3 = s@4 + 1
s@4 = t3
L4:
t4 = j@3 + 1
j@3 = t4
goto L2                  # cierre while interno
L3:
t5 = i@2 + 1
i@2 = t5
goto L0                  # cierre while externo
L1:
ret s@4
```

Como cada estructura solicita sus etiquetas de inicio y salida al comenzar su traducción, los pares quedan reservados de forma correlativa (`L0`/`L1` para el ciclo externo, `L2`/`L3` para el interno y `L4` para el condicional).

---

## 6. Métodos, llamadas y retorno implícito

### Estructura de un método

```c
temporaryCount = 0;
emit(TAC_BEGIN_FUNC, method, tacConstantInt(parameterCount), tacNone());
for (/* cada parámetro */) emit(TAC_PARAM_DECL, tacVariable(...), tacNone(), tacNone());
generateStatementCode(body);
if (node->type == TYPE_VOID && !lastInstructionIsReturn()) {
    emit(TAC_RETURN, tacNone(), tacNone(), tacNone());
}
emit(TAC_END_FUNC, method, tacNone(), tacNone());
```

Un método `void` puede carecer de sentencia `return` explícita en el código fuente, pero el generador de código objeto requiere una instrucción `TAC_RETURN` para emitir el epílogo de salida. `lastInstructionIsReturn()` consulta en $O(1)$ el puntero `currentCode->tail` y, si la última instrucción emitida no es `TAC_RETURN`, inserta un `ret` implícito antes de `end_func`. En los métodos no `void`, el análisis semántico ya garantizó mediante `guaranteesReturn` que todos los caminos de ejecución concluyen en un `return`.

### Llamadas a métodos

`generateCallArguments` evalúa los argumentos de izquierda a derecha y emite una instrucción `TAC_PARAM` por cada uno antes del `TAC_CALL`. La diferencia entre invocar un método como expresión o como sentencia radica únicamente en el operando `result`:

| Función | `result` en `TAC_CALL` | Instrucción emitida |
| --- | --- | --- |
| `generateCallExpressionCode` | `newTemporary(method->type)` | `t1 = call suma, 2` |
| `generateCallStatementCode` | `tacNone()` | `call nada, 0` |

---

## 7. Rechazo de tipos fuera de alcance y errores internos

### Rechazo de `TYPE_FLOAT` (`findUnsupportedType`)

Antes de generar instrucciones, `generateIntermediateCode` recorre el AST con `findUnsupportedType(root)`, que devuelve el primer nodo cuyo `node->type` o `node->symbol->type` sea `TYPE_FLOAT`:

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

Esta verificación se ubica al inicio de la generación de TAC y no en el análisis semántico porque `float` es válido a nivel de lenguaje (`-target sem` lo acepta), pero está excluido del alcance de las etapas de código intermedio y objeto. Rechazarlo explícitamente evita emitir instrucciones enteras sobre patrones de bits IEEE-754. La función `hasUnsupportedTypeError()` permite a `driver.c` saber que el diagnóstico específico ya fue impreso.

### Guardas de errores internos

El generador verifica tres condiciones de fallo interno:

| Ubicación | Condición | Comportamiento |
| --- | --- | --- |
| `generateIntermediateCode` | Fallo de `createTACList()` | Reporta en `stderr` y devuelve `NULL` |
| `emit` | `appendInstruction` devuelve `NULL` | Activa `tacMemoryError`; al finalizar se libera la lista parcial y se devuelve `NULL` |
| `getTACOperation` | `OperationType` no reconocido en el `switch` | Reporta error interno en `stderr` y devuelve `TAC_ASSIGN` como valor de resguardo |

---

## 8. Conexión con las fases vecinas

| Módulo origen / destino | Interacción |
| --- | --- |
| AST anotado (entrada) | Provee `node->type` y `node->symbol` resueltos y la garantía de validez semántica |
| `tac.c` (soporte) | Provee constructores de operandos y `appendInstruction` |
| `optimizer.c` (salida) | Recibe la `TACList` para aplicar transformaciones en el lugar |
| `object_code.c` (salida) | Traduce la secuencia de cuádruplas a ensamblador x86-64 |
| `driver.c` (salida) | Vuelca la lista al archivo `.ci` mediante `formatTACList` |

Invariantes que esta fase garantiza a las etapas posteriores:

- Toda expresión compuesta queda aplanada en cuádruplas con operandos atómicos.
- Todo método está delimitado por `begin_func` y `end_func` y posee al menos un `ret` en cada camino de salida.
- Toda etiqueta referenciada por `goto` o salto condicional tiene su definición `TAC_LABEL` en la lista.
- Todas las variables y parámetros llevan su nombre calificado `x@uid`.

---

## 9. Cómo se extiende

### Un operador binario o unario nuevo

Agregar la correspondencia entre `OperationType` y `TACOperation` en `getTACOperation`. El aplanamiento en `generateOperatorCode` ya maneja cualquier operador unario o binario de evaluación estricta.

### Una construcción de control nueva (`for`, `do while`)

Si se desazucara en el parser a combinaciones de `while` e `if`, este archivo no requiere modificaciones. Si posee un `NodeType` propio:

1. Añadir su `case` en `generateStatementCode`.
2. Implementar la función `generateXCode` reservando las etiquetas al inicio con `newLabel()` y emitiendo los saltos y etiquetas según el esquema de §5.

Omitir el paso 1 hace que el nodo caiga en el `default` de `generateStatementCode` (`generateBlockCode`), lo que ejecutaría sus hijos secuencialmente una sola vez sin emitir ningún salto ni etiqueta.

### Sentencias `break` y `continue`

Requieren mantener en el estado del módulo una pila de pares de etiquetas `(startLabel, endLabel)` para los ciclos activos, apilando al entrar en `generateWhileCode` y desapilando al salir, de modo que `break` y `continue` emitan un `TAC_GOTO` a la etiqueta del tope.

---

## 10. Trampas y límites conocidos

- **Cast de `const TACList *` en `driver.c` (§2):** el puntero retornado se convierte a `TACList *` cuando se invoca al optimizador.
- **Numeración de temporales no correlativa al orden de emisión (§3):** el temporal de una operación binaria se reserva antes de generar el subárbol derecho.
- **Reinicio de `temporaryCount` por método (§2):** cada función numera sus temporales desde `t0`, mientras que `labelCount` es global al archivo.
- **Ausencia deliberada de plegado de constantes:** expresiones como `2 + 3` emiten `t0 = 2 + 3`; la evaluación estática corresponde exclusivamente al [optimizador](../06-optimizador/02-optimizador.md).
- **`default` silencioso en `generateStatementCode` (§9):** cualquier nodo de sentencia sin caso explícito se procesa como un bloque común, recorriendo sus hijos sin emitir control de flujo.
- **Dependencia de `guaranteesReturn`:** los métodos con retorno no `void` no reciben `ret` implícito al final; su corrección en ensamblador depende de que el análisis semántico haya verificado que todos los caminos retornan.

---

## 11. Cómo se valida

- **Suite unitaria:** `tests/test_tac.c` verifica instrucción por instrucción las secuencias generadas para cada construcción del lenguaje empleando árboles anotados construidos con `tests/support/ast_builder.h`.
- **Inspección del archivo `.ci`:**

```bash
./mi_compilador -target codinter examples/programa.ctds
```

- **Propiedades estructurales verificables sobre el `.ci`:**

| Propiedad | Verificación |
| --- | --- |
| Definición de etiquetas | Todo destino de `goto L` o `if ... goto L` tiene una línea `L:` correspondiente |
| Balance de funciones | Todo `begin_func` tiene su correspondiente `end_func` |
| Cierre de métodos `void` | La instrucción previa a `end_func` en métodos sin retorno explícito es `ret` |

---

**Siguiente:** [Análisis de Variables Vivas](../06-optimizador/01-variables-vivas.md)
