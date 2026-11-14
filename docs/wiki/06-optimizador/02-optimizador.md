# Optimización del Código Intermedio

**Archivos:** `include/optimizer.h`, `src/optimizer/optimizer.c` (674 líneas)

El módulo de optimización aplica hasta siete transformaciones que reducen y simplifican el código generado preservando la semántica del programa. Seis de las pasadas reescriben directamente la lista de [cuádruplas TAC](../05-codigo-intermedio/01-representacion-tac.md) apoyándose en el [análisis de variables vivas](01-variables-vivas.md), mientras que la reducción de fuerza y la fusión de comparación y salto actúan durante la emisión de código objeto.

---

## 1. Interfaz pública y contrato de `-opt`

### Comportamiento en la línea de comandos

| Invocación | Comportamiento |
| :--- | :--- |
| *(sin `-opt`)* | No se ejecuta ninguna pasada de optimización; el compilador genera exactamente el mismo código que sin esta etapa. |
| `-opt all` | Activa las siete optimizaciones soportadas (`OPTIMIZATION_ALL`). |
| `-opt <lista>` | Activa únicamente los flags indicados (separados por comas). |

`src/core/cli.c` analiza el argumento de `-opt` y almacena la máscara de bits resultante en `Options.optimizations`. Cuando se solicita `-target codinter` junto con `-opt`, la salida se escribe con extensión `.opt` en lugar de `.ci`. Si se especifica un nombre desconocido, el compilador informa el error por `stderr` y aborta antes de iniciar el pipeline.

### Flags de optimización soportados

| Flag | Transformación | Nivel | Función principal |
| :--- | :--- | :--- | :--- |
| `const-prop` | Propagación de constantes (§3.1) | TAC | `propagateConstants` |
| `const-fold` | Plegado de constantes (§3.1) | TAC | `foldConstants` |
| `const-branch` | Simplificación de ramas constantes (§3.2) | TAC | `simplifyConstantBranches` |
| `dead-code` | Eliminación de código inalcanzable y asignaciones muertas (§3.3) | TAC | `removeUnreachableCode`, `removeDeadStores`, `removeUnreferencedLabels` |
| `self-assign` | Eliminación de autoasignaciones (§3.4) | TAC | `removeSelfAssignments` |
| `peephole` | Eliminación de saltos redundantes y fusión de comparación y salto (§3.5) | TAC y Assembly | `removeRedundantJumps`, `canFuseConditionAndJump`, `emitFusedComparisonAndJump` |
| `strength-reduce` | Reducción de fuerza en multiplicaciones (§3.6) | Assembly | `emitStrengthReducedMultiplication` |

### Interfaz pública (`include/optimizer.h`)

```c
int  optimizeIntermediateCode(TACList *tacList, OptimizationSet enabled);
void setOptimizerDebug(bool enabled);
static inline bool isOptimizationEnabled(OptimizationSet enabled, Optimization optimization);
```

- `OptimizationSet` es un `unsigned int` utilizado como máscara de bits donde cada enum `Optimization` ocupa una potencia de dos (`1 << 0` a `1 << 6`).
- `optimizeIntermediateCode` modifica la lista `TACList` en el lugar y devuelve la cantidad total de transformaciones aplicadas.
- `setOptimizerDebug` habilita la impresión de depuración de las instrucciones eliminadas cuando se invoca con `-debug`.
- `isOptimizationEnabled` se define como `static inline` en el encabezado para que el generador de código objeto (`object_code.c`) pueda consultar si `strength-reduce` o `peephole` están activas sin depender de `optimizer.o` al enlazar pruebas aisladas.

---

## 2. Nivel de representación y bucle de punto fijo

### Por qué se optimiza sobre el TAC

El compilador dispone de tres representaciones posibles: el árbol sintáctico (`NodeAST`), el código intermedio (`TACList`) y el texto assembly emitido. El diseño concentra las transformaciones de flujo y datos sobre el TAC por tres razones:

1. **Estructura lineal:** el TAC es una lista doblemente enlazada donde insertar, reemplazar o desenlazar una instrucción no requiere actualizar punteros jerárquicos de nodos padres.
2. **Operandos tipados y desambiguados:** cada operando ya codifica su categoría (`OPERAND_TEMP`, `OPERAND_VARIABLE`, `OPERAND_CONSTANT`) y su identificador calificado con `@uid`, lo que evita colisiones entre variables homónimas de distintos ámbitos.
3. **Composición entre pasadas:** sobre el TAC, una condición como `if (2 > 1)` se reduce a `if true` tras el plegado y la propagación de constantes, permitiendo que la simplificación de ramas opere sin duplicar un evaluador de expresiones sobre el AST.

> **Nota sobre el descarte de `hasExecute`:** el diseño preliminar contemplaba un campo `bool hasExecute` en `NodeAST` para marcar subárboles que el generador de TAC debía ignorar. Al trasladar la eliminación de código inalcanzable, las ramas constantes y las autoasignaciones directamente al TAC, el campo quedó sin usuarios y fue eliminado de `NodeAST`.

### Optimización local frente a global

El módulo combina dos alcances de análisis sobre el TAC:

- **Local al bloque básico:** la propagación de constantes (`propagateConstants`) mantiene una tabla de valores conocidos que se reinicia en cada frontera de control (etiquetas, saltos y límites de función), garantizando corrección en una pasada lineal sin construir un grafo.
- **Global al procedimiento:** la eliminación de asignaciones muertas (`removeDeadStores`) construye el CFG de la función y consulta los conjuntos `LIVE_OUT` calculados por el [análisis de variables vivas](01-variables-vivas.md) sobre todos los caminos de ejecución.

### Bucle de punto fijo (`MAX_OPTIMIZATION_ROUNDS`)

Las pasadas sobre el TAC se habilitan mutuamente: plegar `1 + 2` genera una constante que `const-prop` propaga hacia una condición; `const-branch` convierte ese salto condicional en incondicional; `dead-code` elimina la rama que quedó inalcanzable y las asignaciones a temporales cuyo resultado dejó de leerse; finalmente, `peephole` limpia los saltos hacia la etiqueta inmediatamente siguiente. Por ello, `optimizeIntermediateCode` ejecuta las pasadas activas en un bucle hasta alcanzar un punto fijo:

```c
for (int round = 0; round < MAX_OPTIMIZATION_ROUNDS; round++) {
    int changes = 0;
    if (enabled & CONST_PROP)   changes += propagateConstants(tacList);
    if (enabled & CONST_FOLD)   changes += foldConstants(tacList);
    if (enabled & CONST_BRANCH) changes += simplifyConstantBranches(tacList);
    if (enabled & DEAD_CODE)  { changes += removeUnreachableCode(tacList);
                                changes += removeDeadStores(tacList); }
    if (enabled & SELF_ASSIGN)  changes += removeSelfAssignments(tacList);
    if (enabled & PEEPHOLE)     changes += removeRedundantJumps(tacList);

    if (changes == 0) break;
    totalChanges += changes;
}
```

- **Terminación:** ninguna pasada agrega instrucciones nuevas; cada transformación elimina nodos o sustituye variables por constantes en una lista finita, por lo que `changes` converge a `0`.
- **Cota de seguridad (`MAX_OPTIMIZATION_ROUNDS = 16`):** previene ciclos infinitos en caso de que una extensión futura introduzca dos transformaciones que se reviertan mutuamente.
- **Pasadas en código objeto:** `strength-reduce` y la fusión de comparación y salto de `peephole` no participan en este bucle porque dependen de la selección de instrucciones x86-64; se aplican en `generateAssembly`, que recibe el mismo `OptimizationSet`.

---

## 3. Catálogo de optimizaciones

### 3.1 Propagación y plegado de constantes (`const-prop`, `const-fold`)

Evalúan en tiempo de compilación las operaciones cuyos operandos son constantes conocidas y propagan el resultado hacia sus usos posteriores.

```text
// TAC sin optimizar          // Con const-prop + const-fold    // Con -opt all (incluye dead-code)
t0 = 1 + 2                    t0 = 3                            ret 6
t1 = t0 + 3                   t1 = 6
ret t1                        ret 6
```

#### Propagación (`propagateConstants` y `KnownConstantTable`)

Mientras recorre el TAC, `propagateConstants` mantiene el estado de las variables y temporales con valor constante conocido en la estructura `KnownConstantTable`:

```c
typedef struct {
    char name[TAC_NAME_LENGTH];
    bool isGlobal;
    TACOperand value;
} KnownConstant;

typedef struct {
    KnownConstant entries[MAX_KNOWN_CONSTANTS]; /* 256 */
    int count;
} KnownConstantTable;
```

- **Registro e invalidación:** ante `x = c` (con `c` constante), `rememberConstant` asocia el nombre de `x` con `c` y copia la marca `isGlobal` desde `x.symbol->isGlobal`. Si `x` recibe el resultado de una expresión no constante, `forgetConstant` elimina su entrada reemplazándola en $O(1)$ por el último elemento del arreglo.
- **Fronteras de flujo de control:** al encontrar `TAC_LABEL`, `TAC_GOTO`, `TAC_IF_FALSE`, `TAC_IF_TRUE`, `TAC_BEGIN_FUNC` o `TAC_END_FUNC`, la tabla se vacía por completo (`table.count = 0`), ya que a una etiqueta puede llegarse desde múltiples caminos con valores distintos.
- **Llamadas a funciones (`TAC_CALL`):** `forgetGlobalVariables` descarta únicamente las entradas con `isGlobal == true` y conserva las variables locales y temporales, dado que C-TDS pasa argumentos por valor y no posee punteros.

#### Plegado (`foldConstants`)

Cuando todos los operandos de una instrucción aritmética, lógica o relacional son `OPERAND_CONSTANT`, `evaluateBinary` o `evaluateUnary` calculan el resultado y convierten la instrucción en un `TAC_ASSIGN` (`destino = constante`) sin desenlazar el nodo de la lista.

Para preservar el comportamiento en tiempo de ejecución, `evaluateBinary` aborta el plegado (retorna `false`) en dos casos de división y módulo (`TAC_DIV`, `TAC_MOD`):

```c
bool isZeroDivisor = (rightValue == 0);
bool isSignedOverflow = (leftValue == INT_MIN && rightValue == -1);
if (isZeroDivisor || isSignedOverflow) {
    return false;
}
```

De este modo, una división por cero o el desborde de `INT_MIN / -1` no provocan `SIGFPE` durante la compilación, sino que se emiten en el ejecutable final.

---

### 3.2 Simplificación de ramas con condición constante (`const-branch`)

Cuando la condición de un salto condicional (`TAC_IF_FALSE` o `TAC_IF_TRUE`) es una constante literal, `simplifyConstantBranches` resuelve la decisión en tiempo de compilación:

| Instrucción | Condición constante | Transformación en el TAC |
| :--- | :--- | :--- |
| `if cond false goto L` | `cond == false` (0) | El salto siempre se toma: se reemplaza por `goto L`. |
| `if cond false goto L` | `cond == true` (1) | El salto nunca se toma: se elimina la instrucción. |
| `if cond true goto L` | `cond == true` (1) | El salto siempre se toma: se reemplaza por `goto L`. |
| `if cond true goto L` | `cond == false` (0) | El salto nunca se toma: se elimina la instrucción. |

```text
// TAC sin optimizar          // Con const-prop + const-fold + const-branch
x@2 = 0                       x@2 = 0
t0 = 2 > 1                    t0 = true
if t0 false goto L0           x@2 = 5
x@2 = 5                       goto L1
goto L1                       L0:
L0:                           x@2 = 9
x@2 = 9                       L1:
L1:                           ret x@2
ret x@2
```

En el ejemplo anterior, al eliminarse `if t0 false goto L0`, la etiqueta `L0:` queda sin referencias, lo que habilita a `dead-code` (§3.3) en la misma ronda a borrar `L0:` y la rama `x@2 = 9`.

---

### 3.3 Eliminación de código inalcanzable y asignaciones muertas (`dead-code`)

El [análisis semántico](../04-semantico/03-analisis-semantico.md) no considera un error la presencia de sentencias posteriores a un `return`, ya que la especificación del lenguaje no lo prohíbe. El flag `dead-code` aborda tanto ese código inalcanzable por flujo de control como las asignaciones muertas por flujo de datos mediante tres funciones:

1. **Eliminación de etiquetas huérfanas (`removeUnreferencedLabels`):** corre al inicio de `removeUnreachableCode`. Para cada `TAC_LABEL`, verifica con `isLabelReferenced` si algún `TAC_GOTO`, `TAC_IF_FALSE` o `TAC_IF_TRUE` de la lista la tiene como destino. Si nadie la referencia, se elimina antes del barrido de alcanzabilidad para que no reactive un flujo muerto.
2. **Eliminación de código inalcanzable (`removeUnreachableCode`):** recorre la lista manteniendo un booleano de alcanzabilidad. Al encontrar una instrucción que corta el flujo (`endsControlFlow`: `TAC_RETURN` o `TAC_GOTO`), toda instrucción ejecutable siguiente se descarta hasta alcanzar una etiqueta (`TAC_LABEL`) o un marcador de función. Las instrucciones estructurales (`isStructural`: `TAC_LABEL`, `TAC_BEGIN_FUNC`, `TAC_END_FUNC`, `TAC_GLOBAL_DECL`, `TAC_LOCAL_DECL`, `TAC_PARAM_DECL`) nunca se eliminan, preservando las declaraciones de ranuras del marco de pila y los delimitadores de función.
3. **Eliminación de asignaciones muertas (`removeDeadStores`):** invoca `createLiveVariableAnalysis` (ver [análisis de variables vivas](01-variables-vivas.md)) y recorre cada bloque básico consultando `isDeadDefinitionInBlock`. Antes de consultar la vivacidad, `isDeadStoreCandidate` excluye cuatro clases de instrucciones que no pueden borrarse aunque su destino no se lea:
   - Instrucciones cuyo `result` no es variable ni temporal (`!holdsValue`).
   - Llamadas a función (`TAC_CALL`), por sus posibles efectos laterales.
   - Instrucciones estructurales (`isStructural`).
   - Asignaciones a variables globales (`result.symbol->isGlobal`), dado que el análisis de vivacidad es intraprocedural y otra función podría leer la variable global.

```text
// TAC sin optimizar          // Con -opt dead-code
t0 = cond@2 > 0               t0 = cond@2 > 0
if t0 false goto L0           if t0 false goto L0
ret true                      ret true
x@3 = 1                       L0:
goto L1                       ret false
L0:                           L1:
ret false                     end_func f
y@4 = x@3
L1:
end_func f
```

---

### 3.4 Eliminación de autoasignaciones (`self-assign`)

`removeSelfAssignments` elimina toda instrucción `TAC_ASSIGN` cuyo destino (`result`) y origen (`firstOperand`) tengan el mismo tipo de operando y el mismo nombre (`x@2 = x@2`). Como los operandos en el TAC llevan anexado su identificador único de ámbito (`@uid`), esta comparación distingue sin ambigüedad variables homónimas declaradas en bloques anidados.

---

### 3.5 Optimización de mirilla (`peephole`)

Actúa sobre ventanas de dos instrucciones consecutivas tanto en el TAC como en el generador de código objeto:

- **Saltos redundantes en el TAC (`removeRedundantJumps`):** cuando una instrucción `TAC_GOTO L` está seguida inmediatamente por la etiqueta `TAC_LABEL L`, el salto se elimina. Si a raíz de ello la etiqueta `L` queda sin otras referencias en el programa (`!isLabelReferenced`), también se elimina en el mismo paso.
- **Fusión de comparación y salto en código objeto (`canFuseConditionAndJump` y `emitFusedComparisonAndJump`):** en `src/object_code/object_code.c`, cuando una comparación relacional (`TAC_LT`, `TAC_GT`, `TAC_EQ`) escribe en un temporal `t` y la instrucción inmediatamente siguiente es `TAC_IF_FALSE` o `TAC_IF_TRUE` leyendo `t`, el generador emite un único `cmpl` seguido del salto condicional correspondiente (`jl`, `jge`, `je`, `jne`, etc.), omitiendo las instrucciones `set<cc>` y `movzbl` de materialización del booleano. Las condiciones de seguridad y el detalle de esta transformación se documentan en el [ADR 003](../../decisiones/adr-003-fusion-comparacion-y-salto-condicional.md).

---

### 3.6 Reducción de fuerza en multiplicaciones (`strength-reduce`)

Implementada en `emitStrengthReducedMultiplication` dentro de `src/object_code/object_code.c`. Cuando uno de los operandos de `TAC_MUL` es una constante entera (en cualquiera de las dos posiciones), sustituye la instrucción `imull` por secuencias más simples:

| Multiplicación | Instrucción emitida |
| :--- | :--- |
| `x * 0` o `0 * x` | `xorl %eax, %eax` |
| `x * 1` o `1 * x` | Carga directa del operando en `%eax` (sin instrucción de multiplicación). |
| `x * 2^n` (`n > 0`, ej. `2, 4, 8, 16`) | `sall $n, %eax` (detectado con `(v & (v - 1)) == 0`). |
| Resto de los casos (o multiplicadores negativos) | `imull %edx, %eax` estándar. |

El desplazamiento aritmético a izquierda (`sall`) es equivalente a la multiplicación con signo en complemento a dos porque ambos truncan módulo $2^{32}$. Esta transformación no se aplica a la división entera (`TAC_DIV`) porque el desplazamiento a derecha con signo (`sarl`) redondea los números negativos hacia $-\infty$, mientras que la semántica de C-TDS trunca hacia cero.

---

## 4. Límites conocidos y detalles internos

- **Desborde de enteros en plegado de `+`, `-` y `*`:** `evaluateBinary` verifica el desborde en `INT_MIN / -1`, pero realiza la suma, resta y multiplicación directamente sobre el tipo `int` de C. Evaluar constantes que desborden 32 bits con signo recae en comportamiento indefinido del compilador anfitrión.
- **Contratos de `unlinkInstructionList`:** las pasadas que recolectan instrucciones a eliminar en un arreglo dinámico y delegan el borrado en `unlinkInstructionList` deben entregar el arreglo en el mismo orden en que las instrucciones aparecen en `tacList`, ya que el borrado se resuelve en una única pasada lineal. Además, `unlinkInstructionList` toma posesión del arreglo y ejecuta `free(instructions)` incluso cuando la cuenta es cero.
- **Costo cuadrático en `removeUnreferencedLabels`:** `isLabelReferenced` recorre toda la lista TAC por cada etiqueta evaluada, lo que resulta en complejidad $O(\text{etiquetas} \times \text{instrucciones})$. Para el tamaño de los programas de C-TDS el costo es despreciable y evita mantener contadores de referencias auxiliares.
- **Formato de depuración (`printTACWithDeletedInRed`):** cuando se ejecuta con `-debug`, cada pasada que elimina instrucciones invoca `printTACWithDeletedInRed` antes de desenlazarlas. A pesar del nombre de la función, no emite códigos de color ANSI (a diferencia del módulo de [errores y debug](../02-infraestructura/03-errores-y-debug.md)), sino que imprime la lista TAC completa prefijando con `[ELIMINADA]` y el motivo cada instrucción afectada.
- **Patrones de mirilla sobre assembly no implementados:** no se realiza posprocesamiento sobre el texto `.ass` para eliminar recargas redundantes (`movl %eax, -4(%rbp)` seguido de `movl -4(%rbp), %eax`) ni reemplazos generales de `movl $0, %eax` fuera de la multiplicación por cero.

---

## 5. Cómo se extiende

### Pasos para agregar una nueva optimización

1. **Definir el flag:** agregar el identificador en el enum `Optimization` de `include/optimizer.h` con el siguiente bit libre e incluirlo en la máscara `OPTIMIZATION_ALL`.
2. **Registrar el nombre en CLI:** agregar la entrada en la tabla de nombres de `src/core/cli.c` para habilitar `-opt <nombre>`.
3. **Implementar la pasada:** escribir una función `static int nuevaOptimizacion(TACList *tacList)` en `src/optimizer/optimizer.c` que devuelva la cantidad de instrucciones transformadas.
4. **Conectar en el bucle de punto fijo:** invocar la función dentro de `optimizeIntermediateCode` en el orden adecuado (las pasadas que exponen constantes antes de las que simplifican control o eliminan código muerto).
5. **Agregar pruebas:** incorporar casos unitarios sobre TAC en `tests/test_optimizer.c` y pruebas de equivalencia de ejecución en `tests/test_integration.c`.

### Optimizaciones candidatas futuras

| Optimización | Descripción | Requisitos de implementación |
| :--- | :--- | :--- |
| Mirilla sobre assembly | Eliminar cargas inmediatas tras un guardado al mismo slot (`movl %eax, X` + `movl X, %eax`). | Representar las instrucciones x86-64 en una lista antes de emitir texto, o filtrar las últimas líneas en el generador. |
| Propagación de copias | Sustituir lecturas de `x` por `y` después de una asignación `x = y`. | Adaptar `KnownConstantTable` para almacenar pares `(variable, operando)` e invalidar al modificar cualquiera de los dos. |
| Subexpresiones comunes locales (CSE) | Reutilizar el temporal `t0` si `a + b` vuelve a evaluarse en el mismo bloque básico sin cambios en `a` ni `b`. | Tabla de expresiones disponibles por bloque básico, invalidada al redefinir sus operandos. |
| Variables desvanecidas (*faint variables*) | Eliminar acumuladores de bucles cuyo resultado nunca se usa fuera del ciclo. | Extender el [análisis de variables vivas](01-variables-vivas.md) para que una variable solo genere vivacidad en los operandos que la calculan si ella misma está viva. |

---

## 6. Validación

- **Suite unitaria del optimizador:** `tests/test_optimizer.c` (951 líneas) evalúa cada flag de forma aislada y combinada a partir de listas TAC construidas con `buildTAC`, verificando tanto las instrucciones transformadas como los casos que deben permanecer intactos (por ejemplo, división por cero o variables modificadas en bucles).
- **Suite de código objeto e integración:** `tests/test_object_code.c` verifica el assembly emitido por `strength-reduce` y la fusión de `peephole`; `tests/test_integration.c` compila programas completos con y sin optimizaciones, ejecuta ambos binarios y comprueba que produzcan el mismo código de salida.
- **Verificación manual por consola:**

```bash
# Comparar TAC antes (.ci) y después de optimizar (.opt)
./mi_compilador -target codinter examples/valido_optimizacion_codigo_muerto.ctds
./mi_compilador -opt all -target codinter examples/valido_optimizacion_codigo_muerto.ctds

# Inspeccionar qué instrucciones elimina cada pasada
./mi_compilador -debug -opt all -target codinter examples/valido_optimizacion_codigo_muerto.ctds

# Verificar equivalencia de ejecución entre binario sin optimizar y optimizado
./mi_compilador -o /tmp/sin_opt examples/valido_optimizacion_codigo_muerto.ctds && /tmp/sin_opt; echo "sin opt -> $?"
./mi_compilador -opt all -o /tmp/con_opt examples/valido_optimizacion_codigo_muerto.ctds && /tmp/con_opt; echo "con opt -> $?"
```

---

**Siguiente:** [Marco de Pila](../07-codigo-objeto/01-marco-de-pila.md)
