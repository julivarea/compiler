# Análisis de Variables Vivas

**Archivos:** `include/live_variable_analysis.h`, `src/optimizer/live_variable_analysis.c` (484 líneas)

El análisis de variables vivas es un análisis de flujo de datos que opera sobre la lista de instrucciones [TAC](../05-codigo-intermedio/01-representacion-tac.md) para determinar qué variables pueden ser leídas en el futuro en cada punto del programa. Su propósito en el compilador es identificar definiciones muertas (asignaciones cuyo valor nunca se lee) para que el [optimizador](02-optimizador.md) pueda eliminarlas sin alterar la semántica.

---

## 1. Conceptos y alcance

- **Variable viva:** una variable está viva en un punto del programa si existe algún camino de ejecución desde ese punto hasta una lectura de la variable que no pase antes por una redefinición de la misma.
- **Bloque básico:** secuencia máxima de instrucciones consecutivas sin saltos intermedios: el flujo de control ingresa únicamente por la primera instrucción y sale únicamente por la última.
- **Líder (`isLeader`):** primera instrucción de un bloque básico. Una instrucción es líder si cumple alguna de estas condiciones:
  - Es la primera instrucción de la lista TAC.
  - Es una etiqueta (`TAC_LABEL`).
  - Es el inicio de una función (`TAC_BEGIN_FUNC`).
  - Sigue inmediatamente a un terminador de bloque.
- **Terminador de bloque (`endsBasicBlock`):** instrucción que transfiere o detiene el flujo de control al final de un bloque: `TAC_GOTO`, `TAC_IF_FALSE`, `TAC_IF_TRUE`, `TAC_RETURN` o `TAC_END_FUNC`.
- **Grafo de flujo de control (CFG):** grafo dirigido donde los nodos son los bloques básicos y existe una arista `B → S` si la ejecución puede pasar directamente del final de `B` al inicio del sucesor `S`.
- **Variable candidata (`holdsValue`, de `tac.h`):** solo se analizan operandos de tipo `OPERAND_VARIABLE` y `OPERAND_TEMP`. Las constantes y etiquetas se ignoran.
- **Alcance intraprocedural:** aunque todos los bloques del programa se almacenan en un mismo arreglo, ningún bloque ni arista cruza la frontera entre dos funciones. `TAC_BEGIN_FUNC` actúa como líder y `TAC_END_FUNC` como terminador sin arista de caída secuencial, manteniendo aisladas las funciones en el grafo.

---

## 2. Interfaz pública y estructuras de datos

### Funciones públicas

| Función | Descripción |
| :--- | :--- |
| `createLiveVariableAnalysis(tacList)` | Ejecuta las cinco etapas del análisis (§3) y devuelve la estructura convergida, o `NULL` si la lista es vacía o falla una reserva. |
| `freeLiveVariableAnalysis(analysis)` | Libera los bloques y la estructura del análisis. Acepta `NULL`. |
| `findInstructionBlock(analysis, instruction)` | Busca linealmente el bloque básico al que pertenece una instrucción. |
| `isDeadDefinitionInBlock(analysis, block, instruction)` | Evalúa si la asignación en `instruction` dentro de `block` es una definición muerta (§4). |
| `isDeadDefinition(analysis, instruction)` | Localiza primero el bloque con `findInstructionBlock` y delega en `isDeadDefinitionInBlock`. |
| `printLiveVariableAnalysis(analysis, output)` | Imprime el CFG completo y los conjuntos de cada bloque para la salida de `-debug` (usa `stdout` si `output` es `NULL`). |

La separación entre `isDeadDefinition` e `isDeadDefinitionInBlock` responde a su costo algorítmico: `removeDeadStores` en el optimizador recorre el programa bloque por bloque, por lo que ya conoce el bloque actual y consulta `isDeadDefinitionInBlock` en tiempo lineal, mientras que `isDeadDefinition` queda disponible para consultas puntuales en pruebas unitarias.

### `VariableSet` (conjunto de variables como *bitset*)

Cada conjunto de variables se representa mediante un `uint64_t` donde el bit `i` indica la presencia de la variable de índice `i`. Esto permite resolver las operaciones de conjuntos en una única instrucción de procesador:

| Operación de conjuntos | Expresión en C |
| :--- | :--- |
| Agregar variable `i` | `set \|= (1ULL << i)` |
| Pertenencia de `i` | `(set & (1ULL << i)) != 0` |
| Unión `A ∪ B` | `A \| B` |
| Diferencia `A − B` | `A & ~B` |
| Conjunto vacío | `0` |

### `BasicBlock` (bloque básico)

| Campo | Descripción |
| :--- | :--- |
| `id` | Identificador secuencial del bloque (comenzando en 0). |
| `firstInstruction` | Puntero a la primera instrucción TAC del bloque (líder). |
| `lastInstruction` | Puntero a la última instrucción TAC del bloque. |
| `successors[2]` / `successorCount` | Punteros a los bloques sucesores (máximo 2: salto condicional y caída secuencial). |
| `use` | `VariableSet` con `USE(B)`: variables leídas antes de ser definidas en el bloque. |
| `def` | `VariableSet` con `DEF(B)`: variables definidas antes de ser leídas en el bloque. |
| `liveIn` | `VariableSet` con las variables vivas al ingresar al bloque. |
| `liveOut` | `VariableSet` con las variables vivas al salir del bloque. |

### `LiveVariableAnalysis` (contexto del análisis)

| Campo | Descripción |
| :--- | :--- |
| `tacList` | Lista TAC analizada (referencia no propietaria). |
| `variableNames[]` / `variableCount` | Tabla que asocia cada nombre de variable o temporal con su índice de bit (`0` a `MAX_LIVENESS_VARIABLES - 1`). |
| `blocks[]` / `blockCount` | Arreglo dinámico de bloques básicos en el orden en que aparecen en el TAC. |

---

## 3. Etapas del algoritmo

`createLiveVariableAnalysis` construye y resuelve el análisis en cinco etapas secuenciales:

### 3.1 Recolección de variables (`collectVariables`)

Recorre todas las instrucciones del TAC y registra en `variableNames` cada operando candidato (`result`, `firstOperand`, `secondOperand`) mediante `registerVariable`. Si el nombre ya existe (búsqueda lineal con `getVariableIndex`), conserva su índice; de lo contrario, le asigna el siguiente bit libre hasta alcanzar `MAX_LIVENESS_VARIABLES` (64).

### 3.2 Particionado en bloques básicos (`partitionBasicBlocks`)

Recorre la lista TAC identificando líderes con `isLeader`. A partir de cada líder, extiende el bloque hasta que la instrucción actual sea un terminador (`endsBasicBlock`), la siguiente instrucción sea un líder o se alcance el final de la lista. El arreglo `blocks` crece dinámicamente duplicando su capacidad desde un tamaño inicial de 8 bloques.

### 3.3 Construcción del CFG (`buildControlFlowEdges`)

Inspecciona únicamente la última instrucción de cada bloque para agregar las aristas hacia sus sucesores:

| Última instrucción | Sucesores agregados |
| :--- | :--- |
| `TAC_GOTO` | El bloque cuya primera instrucción es la etiqueta destino (`findBlockByLabel`). |
| `TAC_IF_FALSE` / `TAC_IF_TRUE` | El bloque de la etiqueta destino y el bloque siguiente en el arreglo (caída secuencial). |
| `TAC_RETURN` / `TAC_END_FUNC` | Ninguno. |
| Cualquier otra | El bloque siguiente en el arreglo (caída secuencial). |

Si `findBlockByLabel` no encuentra una etiqueta destino, la arista correspondiente se omite de forma segura.

### 3.4 Cálculo de `USE` y `DEF` (`computeBlockUseAndDef`)

Recorre las instrucciones de cada bloque en orden secuencial, omitiendo las declaraciones (`TAC_GLOBAL_DECL`, `TAC_LOCAL_DECL`, `TAC_PARAM_DECL`) dado que reservar espacio no lee ni escribe valores. Si una declaración se contara como definición, incluiría la variable en `DEF(B)` e impediría propagar su vivacidad hacia atrás.

- **`USE(B)`:** variables leídas en `firstOperand` o `secondOperand` que aún no pertenecen a `DEF(B)`.
- **`DEF(B)`:** variables escritas en `result` que aún no pertenecen a `USE(B)`.

### 3.5 Resolución por punto fijo (`solveFixedPointLiveness`)

La vivacidad es un análisis hacia atrás (*backward data-flow*): el estado a la entrada de un bloque depende de lo que sucede después de él. En cada iteración se recorren los bloques en orden inverso (`blockCount - 1` hasta `0`), de modo que la información fluya en dirección opuesta al control y converja en menos pasadas:

$$\text{LIVE\_OUT}(B) = \bigcup_{S \in \text{succ}(B)} \text{LIVE\_IN}(S)$$

$$\text{LIVE\_IN}(B) = \text{USE}(B) \cup \bigl(\text{LIVE\_OUT}(B) - \text{DEF}(B)\bigr)$$

En código, gracias a la representación como `VariableSet`, la ecuación de transferencia se evalúa directamente como `newIn = block->use | (newOut & ~block->def)`. El bucle se repite hasta alcanzar el punto fijo (una pasada completa sin cambios en ningún `liveIn` ni `liveOut`). La terminación está garantizada porque las ecuaciones son monótonas crecientes sobre un conjunto finito acotado por 64 bits.

---

## 4. Criterio de definición muerta y ejemplo

### Evaluación intra-bloque (`isDeadDefinitionInBlock`)

Para determinar si una instrucción que define una variable `v` (en `instruction->result`) dentro del bloque `B` es una definición muerta, se recorren las instrucciones posteriores a ella dentro del mismo bloque:

1. Si una instrucción posterior lee `v` (`instructionReadsVariable`), la definición **no es muerta**.
2. Si una instrucción posterior sobrescribe `v` (`instructionWritesVariable`) sin haberla leído en esa misma instrucción, la definición anterior **es muerta**.
3. Si se alcanza el final del bloque `B` sin que ocurra 1 ni 2, la definición es muerta si y solo si `v ∉ LIVE_OUT(B)`.

### Ejemplo de ejecución

Dado el siguiente fragmento TAC:

```text
x = 1
y = 2
if_false x goto L1
z = x + y
L1:
w = y
return w
```

El particionado y la resolución producen los siguientes conjuntos:

| Bloque | Instrucciones | Sucesores | `USE` | `DEF` | `LIVE_OUT` | `LIVE_IN` |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| B0 | `x = 1`, `y = 2`, `if_false x goto L1` | B2, B1 | `{}` | `{x, y}` | `{x, y}` | `{}` |
| B1 | `z = x + y` | B2 | `{x, y}` | `{z}` | `{y}` | `{x, y}` |
| B2 | `L1:`, `w = y`, `return w` | ninguno | `{y}` | `{w}` | `{}` | `{y}` |

Al consultar `isDeadDefinitionInBlock` sobre `z = x + y` en `B1`: no hay instrucciones posteriores en `B1` y `z ∉ LIVE_OUT(B1) = {y}`, por lo que se identifica como definición muerta.

---

## 5. Limitaciones y comportamiento conservador

- **Tope de 64 variables (`MAX_LIVENESS_VARIABLES`):** si una función combinada con el resto del TAC supera las 64 variables o temporales distintos, `registerVariable` y `getOperandVariableIndex` devuelven `-1` para el excedente. Cuando `isDeadDefinitionInBlock` recibe un índice negativo, retorna `false`. De esta forma, superar el límite degrada la capacidad de optimización pero preserva la corrección del programa.
- **Capacidad fija en `successors[2]`:** `addSuccessor` inserta en el arreglo de dos elementos sin validar el tope, apoyándose en el invariante de que ningún terminador del TAC genera más de dos salidas.
- **Asignaciones muertas frente a ciclos auto-sustentados:** el análisis detecta definiciones cuyo valor nunca se lee, pero no elimina cómputos que se retroalimentan en un bucle sin uso externo. Por ejemplo, si dentro de un `while` se ejecuta `a = a + i` y al salir del ciclo `a` se pisa con `a = 10`, la lectura de `a` en `a + i` la incluye en `USE` del cuerpo del bucle y la mantiene viva a través de la arista de retorno. Eliminar ese patrón requeriría un análisis de variables desvanecidas (*faint variable analysis*).

---

## 6. Cómo se extiende

### Otros análisis de flujo de datos sobre el CFG

El particionado en bloques básicos (`partitionBasicBlocks`), el armado de aristas (`buildControlFlowEdges`) y el esquema iterativo de punto fijo son reutilizables para otros análisis sobre el TAC cambiando únicamente la dirección del recorrido y las ecuaciones de flujo:

| Propiedad | Variables vivas (actual) | Definiciones alcanzables (*reaching definitions*) |
| :--- | :--- | :--- |
| Dirección | Hacia atrás (*backward*) | Hacia adelante (*forward*) |
| Orden de recorrido | Del último bloque al primero | Del primer bloque al último |
| Confluencia | `LIVE_OUT(B) = ⋃ LIVE_IN(S)` sobre sucesores | `IN(B) = ⋃ OUT(P)` sobre predecesores |
| Transferencia | `IN = USE ∪ (OUT − DEF)` | `OUT = GEN ∪ (IN − KILL)` |

Para soportar análisis hacia adelante solo sería necesario almacenar las aristas de predecesores en `BasicBlock` durante `buildControlFlowEdges`.

### Soporte para más de 64 variables

Si se necesitara analizar programas con más de 64 variables simultáneas, `VariableSet` debería reemplazarse por un arreglo de palabras de 64 bits (`uint64_t words[N]`), encapsulando la unión, diferencia y pertenencia en funciones auxiliares que operen palabra por palabra.

---

## 7. Validación

- **Pruebas unitarias:** `tests/test_live_variable_analysis.c` (240 líneas) verifica el particionado en bloques, la construcción de aristas del CFG, la convergencia del punto fijo y la detección de definiciones muertas sobre listas TAC construidas con `buildTAC`.
- **Inspección del CFG con `-debug`:** al ejecutar el compilador con `-debug`, `printLiveVariableAnalysis` vuelca por salida estándar la lista de variables candidatas y cada bloque básico con sus instrucciones, sucesores y conjuntos `USE`, `DEF`, `LIVE_IN` y `LIVE_OUT` traducidos a nombres simbólicos:

```bash
./mi_compilador -debug -target codinter examples/dead_code.ctds
```

- **Equivalencia semántica:** la validación integral consiste en compilar los programas de prueba con y sin `-opt dead-code` (o `-opt all`), ejecutar ambos binarios y verificar que sus códigos de salida coincidan.

---

**Siguiente:** [Optimización del Código Intermedio](02-optimizador.md)
