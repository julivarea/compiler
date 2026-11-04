# Alcance y Reglas del Análisis de Variables Vivas

El análisis de variables vivas opera sobre la lista de instrucciones TAC (código de tres direcciones) generada tras el análisis semántico. Su objetivo es detectar **definiciones muertas** (asignaciones cuyo valor nunca se leerá) para que una etapa posterior pueda eliminarlas. Sus responsabilidades están organizadas en los módulos `src/live_variable_analysis.c` y `include/live_variable_analysis.h`.

---

## 1. Conceptos Fundamentales

- **Variable viva:** una variable está *viva* en un punto del programa si su valor actual puede ser leído más adelante por algún camino de ejecución, sin ser sobrescrita antes.
- **Bloque básico:** secuencia máxima de instrucciones que se ejecutan siempre de corrido: se entra únicamente por la primera y se sale únicamente por la última.
- **Líder:** primera instrucción de un bloque básico. Una instrucción es líder si cumple alguna de estas condiciones (`isLeader`):
  - Es la primera instrucción del programa.
  - Es una etiqueta (`TAC_LABEL`).
  - Es el inicio de una función (`TAC_BEGIN_FUNC`).
  - Sigue inmediatamente a un terminador de bloque.
- **Terminador de bloque:** instrucción que cierra un bloque: `TAC_GOTO`, `TAC_IF_FALSE`, `TAC_IF_TRUE`, `TAC_RETURN` o `TAC_END_FUNC`.
- **CFG (Grafo de Flujo de Control):** los bloques son nodos y existe una arista `B → S` si la ejecución puede pasar de `B` a `S`. Los `S` se denominan **sucesores** de `B`.
- **Variable candidata (`isCandidate`):** solo se analizan operandos de tipo `OPERAND_VARIABLE` y `OPERAND_TEMP`. Las constantes y etiquetas se ignoran.

---

## 2. Modelado de Datos

### `VariableSet` (conjunto de variables como *bitset*)

Un conjunto de variables se representa como un entero de 64 bits donde **cada bit corresponde a una variable**: el bit `i` está encendido si la variable de índice `i` pertenece al conjunto.

| Operación de conjuntos | Expresión en C |
| :--- | :--- |
| Agregar variable `i` | `set \|= (1ULL << i)` |
| ¿Contiene `i`? | `(set & (1ULL << i)) != 0` |
| Unión `A ∪ B` | `A \| B` |
| Diferencia `A − B` | `A & ~B` |
| Conjunto vacío | `0` |

> **Limitación:** al usar 64 bits, el análisis admite como máximo 64 variables distintas (`MAX_LIVENESS_VARIABLES`).

### `BasicBlock` (bloque básico)

| Campo | Descripción |
| :--- | :--- |
| `id` | Número secuencial del bloque, empezando en 0. |
| `firstInstruction` | Puntero a la primera instrucción TAC del bloque (el líder). |
| `lastInstruction` | Puntero a la última instrucción TAC del bloque. |
| `successors[]` / `successorCount` | Bloques a los que puede saltar la ejecución desde este. |
| `liveIn` | `VariableSet` con las variables vivas **al entrar** al bloque. |
| `liveOut` | `VariableSet` con las variables vivas **al salir** del bloque. |

### `LiveVariableAnalysis` (contexto del análisis)

| Campo | Descripción |
| :--- | :--- |
| `tacList` | Lista TAC original sobre la que se trabaja (no es propietario de ella). |
| `variableNames[]` / `variableCount` | Tabla que asigna un índice (0, 1, 2, ...) a cada variable o temporal distinto. El índice es la posición del bit en el `VariableSet`. |
| `blocks[]` / `blockCount` | Arreglo dinámico de bloques básicos, en el orden en que aparecen en el TAC. |

---

## 3. Flujo General (`createLiveVariableAnalysis`)

La función principal ejecuta las etapas en este orden:

1. **`collectVariables`:** recorre todo el TAC y registra cada variable/temporal en `variableNames` mediante `registerVariable`.
2. **`partitionBasicBlocks`:** divide el TAC en bloques básicos.
3. **`buildControlFlowEdges`:** calcula los sucesores de cada bloque.
4. **`computeBlockUseAndDef`:** calcula `USE(B)` y `DEF(B)` para cada bloque.
5. **`solveFixedPointLiveness`:** resuelve las ecuaciones de liveness por punto fijo.

Si la lista TAC es `NULL`/vacía, o si no se genera ningún bloque, la función retorna `NULL`. La memoria se libera con `freeLiveVariableAnalysis`.

---

## 4. Etapas del Análisis

### 4.1 Registro de Variables (`collectVariables`, `registerVariable`, `getVariableIndex`)

- Por cada instrucción se registran, si son candidatas, `result`, `firstOperand` y `secondOperand`.
- `registerVariable` primero busca el nombre con `getVariableIndex` (búsqueda lineal con `strcmp`). Si ya existe devuelve su índice; si no, lo agrega al final.
- Si se supera `MAX_LIVENESS_VARIABLES`, devuelve `-1` y esa variable queda **fuera del análisis**.

### 4.2 Particionado en Bloques Básicos (`partitionBasicBlocks`)

- Se recorre la lista TAC saltando instrucciones hasta hallar un líder (`isLeader`).
- Desde cada líder, el bloque se extiende hasta que ocurra una de estas situaciones:
  - La instrucción actual es un terminador (`goto`, `if_false`, `if_true`, `return`, `end_func`).
  - La siguiente instrucción es un líder.
  - Se acaba la lista.
- Cada bloque guarda su `firstInstruction` y `lastInstruction`, y se agrega al arreglo `blocks`, que se duplica con `realloc` cuando se llena (capacidad inicial: 8).

### 4.3 Construcción del CFG (`buildControlFlowEdges`)

Se analiza **únicamente la última instrucción** de cada bloque:

| Última instrucción | Sucesores |
| :--- | :--- |
| `TAC_GOTO` | Uno: el bloque cuya etiqueta es el destino del salto. |
| `TAC_IF_FALSE` / `TAC_IF_TRUE` | Dos: el bloque destino del salto y el bloque siguiente en el orden del TAC (caída secuencial). |
| `TAC_RETURN` / `TAC_END_FUNC` | Ninguno. |
| Cualquier otra | Uno: el bloque siguiente (caída secuencial). |

`findBlockByLabel` localiza el bloque cuya primera instrucción es una `TAC_LABEL` con el nombre buscado.

### 4.4 Cálculo de `USE` y `DEF` (`computeBlockUseAndDef`)

Se recorren las instrucciones del bloque en orden, **ignorando las declaraciones** (`TAC_GLOBAL_DECL`, `TAC_LOCAL_DECL`, `TAC_PARAM_DECL`), ya que declarar no lee ni escribe un valor.

- **`USE(B)`:** variables **leídas** (`firstOperand`, `secondOperand`) en el bloque **antes de ser definidas** dentro del mismo bloque. Son las que deben venir vivas desde antes.
- **`DEF(B)`:** variables **asignadas** (`result`) en el bloque antes de haber sido utilizadas. Una escritura "mata" el valor previo.

### 4.5 Resolución por Punto Fijo (`solveFixedPointLiveness`)

Es un análisis **hacia atrás** (*backward*): se recorren los bloques del último al primero, y se repite hasta que ningún conjunto cambie.

**Ecuación 1:**
```
LIVE_OUT(B) = ⋃ LIVE_IN(S)   para todo sucesor S de B
```

**Ecuación 2:**
```
LIVE_IN(B) = USE(B) ∪ (LIVE_OUT(B) − DEF(B))
```

- Lo que está vivo al salir de `B` es lo que cualquier sucesor necesite.
- Lo que está vivo al entrar es lo que `B` lee, más lo que sigue vivo después y `B` no sobrescribe.
- La iteración es necesaria por los **bucles**: el resultado de un bloque puede depender de otro que aún no fue calculado correctamente. Al terminar una pasada sin cambios, se alcanzó el punto fijo.

---

## 5. Ejemplo Completo

**Código TAC:**
```
x = 1
y = 2
if_false x goto L1
z = x + y
L1:
w = y
return w
```

**Bloques y sucesores:**

| Bloque | Instrucciones | Sucesores |
| :--- | :--- | :--- |
| B0 | `x = 1`, `y = 2`, `if_false x goto L1` | B2 (salto), B1 (caída) |
| B1 | `z = x + y` | B2 |
| B2 | `L1:`, `w = y`, `return w` | ninguno |

**`USE` / `DEF`:**

| Bloque | USE | DEF |
| :--- | :--- | :--- |
| B0 | `{}` (`x` se lee pero ya fue definida en el bloque) | `{x, y}` |
| B1 | `{x, y}` | `{z}` |
| B2 | `{y}` | `{w}` |

**Cálculo hacia atrás:**

| Bloque | LIVE_OUT | LIVE_IN |
| :--- | :--- | :--- |
| B2 | `{}` | `{y}` |
| B1 | `{y}` | `{x, y}` |
| B0 | `{x, y}` | `{}` |

**Resultado:** en `z = x + y`, `z` no está en `LIVE_OUT(B1) = {y}` y nadie la lee después dentro del bloque, por lo tanto **es una definición muerta**.

---

## 6. Funciones de Consulta

- **`findInstructionBlock(analysis, instruction)`:** devuelve el bloque que contiene a la instrucción, o `NULL` si no se encuentra.
- **`isVariableLiveIn(analysis, block, name)`:** indica si la variable pertenece a `LIVE_IN(block)`.
- **`isVariableLiveOut(analysis, block, name)`:** indica si la variable pertenece a `LIVE_OUT(block)`.
- **`isDeadDefinition(analysis, instruction)`:** localiza el bloque de la instrucción y delega en `isDeadDefinitionInBlock`.

### Criterio de Definición Muerta (`isDeadDefinitionInBlock`)

Dada una instrucción que **define** una variable `v` (su `result`), se examinan las instrucciones siguientes **dentro del mismo bloque**:

1. Si alguna instrucción **lee** `v` → **no es muerta** (el valor se usa).
2. Si alguna instrucción **reescribe** `v` sin haberla leído antes → **es muerta** (el valor fue pisado sin usarse).
3. Si se llega al final del bloque sin ocurrir lo anterior → es muerta si y solo si `v ∉ LIVE_OUT(B)`.

---

## 7. Salida de Depuración (`printLiveVariableAnalysis`)

Imprime, por cada bloque: su identificador, sus instrucciones (formateadas con `formatTACInstruction`), sus sucesores, y los conjuntos `USE`, `DEF`, `LIVE_IN` y `LIVE_OUT`. Al inicio muestra la lista de variables candidatas y el total de bloques. Si `output` es `NULL`, escribe en `stdout`.

---

## 8. Manejo de Errores y Casos Anómalos

1. **Entrada nula o vacía:** si `tacList` o `tacList->head` es `NULL`, se retorna `NULL`.
2. **Sin bloques:** si el particionado no genera ningún bloque, se retorna `NULL`.
3. **Fallo de memoria:** si `calloc`, `malloc` o `realloc` fallan, se libera lo reservado y se retorna `NULL` (o se detiene el particionado con los bloques construidos hasta ese punto).
4. **Parámetros `NULL` en consultas:** todas las funciones públicas de consulta devuelven `false`/`NULL` de forma segura ante punteros nulos.
5. **Etiqueta inexistente:** si `findBlockByLabel` no encuentra el destino de un salto, simplemente no se agrega esa arista.

### Limitaciones y Puntos de Atención

- **Máximo 64 variables:** `registerVariable` devuelve `-1` en silencio al superar el límite. Las variables excedentes no se analizan y no se emite ningún aviso.
- **Fuga de memoria potencial:** cuando `blockCount == 0`, se hace `free(analysis)` sin liberar `analysis->blocks`. Conviene usar `freeLiveVariableAnalysis(analysis)`.
- **Tamaño de `successors[]`:** si es un arreglo de tamaño fijo, debe tener al menos 2 posiciones (máximo de sucesores: los dos de un salto condicional).
- **Análisis intra-bloque en `isDeadDefinitionInBlock`:** no considera las propias operaciones de la instrucción evaluada; solo las posteriores dentro del bloque.
- **Cast de `const`:** `printLiveVariableAnalysis` recalcula `USE`/`DEF` y descarta el `const` del puntero. Guardar `use` y `def` dentro de `BasicBlock` evitaría tanto el recálculo como el cast.