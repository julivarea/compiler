# ADR 002: Resolución O(1) de Variables Globales mediante `Symbol.isGlobal`

- **Estado:** Aceptado
- **Contexto:** Identificación del alcance global o local de las variables en el generador de TAC, el optimizador y el generador de código objeto.

---

## 1. Problema

Inicialmente, la distinción entre variable global y variable local no estaba almacenada en `Symbol`, sino que se infería por posición o búsqueda lineal:

1. **En `intermediate_code.c`:** Se asumía que las declaraciones globales aparecían únicamente al comienzo de la raíz del AST, antes del primer método.
2. **En `optimizer.c` y `object_code.c`:** Para saber si un operando requería direccionamiento relativo a `%rip` (`var(%rip)`) o si debía excluirse de la eliminación de asignaciones muertas, se recorría linealmente la lista TAC buscando una instrucción `TAC_GLOBAL_DECL` con el mismo nombre (`strcmp`).

Esto acoplaba la semántica al orden físico de las instrucciones, repetía búsquedas $O(N)$ en cada consulta de operando y fallaba en pruebas unitarias que construían listas TAC sin encabezados `TAC_GLOBAL_DECL`.

---

## 2. Decisión

La tabla de símbolos (`Symbol`) actúa como única fuente de verdad sobre el alcance de cada variable:

1. **Campo `bool isGlobal` en `Symbol` (`include/symbol_table.h`):** En `appendSymbol` (`src/semantic/symbol_table.c`) se inicializa automáticamente según si el ámbito tiene padre:
   ```c
   newSymbol->isGlobal = (scope->parent == NULL);
   ```
2. **Emisión uniforme en TAC (`src/intermediate_code/intermediate_code.c`):** `generateVariableDeclarationCode` decide entre `TAC_GLOBAL_DECL` y `TAC_LOCAL_DECL` leyendo `node->symbol->isGlobal`, permitiendo declaraciones globales e intercaladas en cualquier orden dentro del ámbito raíz.
3. **Propagación a `TACOperand`:** `tacVariable(symbol)` conserva el puntero `Symbol *` dentro del operando TAC. Tanto el optimizador (`KnownConstant`, `removeDeadStores`) como el generador de código objeto (`formatOperand`) consultan `operand->symbol->isGlobal` en $O(1)$.
4. **Eliminación de búsquedas de respaldo:** Se removieron los escaneos lineales por nombre sobre la lista de cuádruplas.

---

## 3. Consecuencias

- **Ventajas:** Consulta en tiempo constante $O(1)$, soporte directo para variables globales declaradas entre métodos y código más simple en las tres fases finales.
- **Requisito en tests:** Todo `TACOperand` de tipo `OPERAND_VARIABLE` que represente una variable global en pruebas unitarias debe construirse asociándole su `Symbol` con `isGlobal = true`.
