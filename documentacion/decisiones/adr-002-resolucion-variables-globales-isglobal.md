# ADR 002: Detección y Resolución O(1) de Variables Globales mediante Flag en Symbol

- **Estado:** Aceptado
- **Fecha:** 2026-11-13 (formalización de cambios del 2026-11-12)
- **Contexto:** Detección de variables globales en la generación de TAC, optimizador y generador de código x86-64.

---

## 1. Contexto del Problema

Originalmente, la condición de si una variable era global o local no estaba modelada explícitamente como un atributo intrínseco en su entidad semántica (`Symbol`), sino que se infería mediante **heurísticas posicionales y escaneos lineales**:

1. **Generación de TAC (`src/intermediate_code.c`):** Asumía implícitamente que las declaraciones globales ocurrían únicamente al inicio de la raíz del AST antes de los métodos.
2. **Optimizador (`src/optimizer.c`) y Generador de Código Objeto (`src/object_code.c`):** Para determinar si un operando era una variable global (por ejemplo, para emitir `movl var(%rip), %reg` en lugar de un offset de stack, o para descartar candidatos en eliminación de asignaciones muertas), la función `isGlobalVariable` realizaba un escaneo lineal de la lista enlazada de instrucciones TAC buscando `TAC_GLOBAL_DECL` por comparación de cadenas (`strcmp`), o se detenía ante el primer `TAC_BEGIN_FUNC`.

Este esquema presentaba serias deficiencias:
* **Acoplamiento posicional frágil:** Dependía de la ubicación física de las instrucciones o nodos en lugar de su ámbito semántico.
* **Sobrecarga de rendimiento:** Búsquedas lineales $O(N)$ repetidas sobre cadenas en cada consulta de operandos en el backend y en cada ronda del optimizador.
* **Inconsistencia ante casos de prueba sintéticos:** Los tests que construían cuádruplas TAC de forma manual sin un AST completo podían fallar o dar falsos positivos.

---

## 2. Decisión

Se toma la decisión arquitectónica de **establecer a la Tabla de Símbolos (`SymbolTable`) como la única fuente de verdad autoritativa para el ámbito de las variables**, propagando esta información hacia las fases posteriores:

1. **Atributo `isGlobal` en `Symbol`:** Se agrega un campo booleano `bool isGlobal` en la estructura `Symbol` (`include/symbol_table.h`).
2. **Determinación automática en inserción:** En `src/symbol_table.c` (`appendSymbol`), se asigna automáticamente:
   ```c
   newSymbol->isGlobal = (scope->parent == NULL);
   ```
   Toda variable registrada en el ámbito raíz (sin padre) queda marcada irrevocablemente como global.
3. **Desacoplamiento en Código Intermedio:** En `src/intermediate_code.c`, la emisión de declaraciones de variables se unifica en `generateVariableDeclarationCode`, discriminando la cuádrupla a emitir (`TAC_GLOBAL_DECL` vs. `TAC_LOCAL_DECL`) consultando directamente `node->symbol->isGlobal`, sin depender de si la declaración se procesa en el bucle principal de la raíz o dentro de un bloque.
4. **Propagación a `TACOperand` y Resolución $O(1)$:** El puntero al `Symbol` se preserva dentro de `TACOperand`. Tanto el optimizador como el generador de código x86-64 consultan `operand->symbol->isGlobal` en tiempo constante $O(1)$.
5. **Fallback para cuádruplas sintéticas:** Se mantiene un escaneo de respaldo en `isGlobalVariable` únicamente para cuádruplas aisladas construidas en tests unitarios que carezcan de referencia al símbolo (`operand->symbol == NULL`).

---

## 3. Fundamentación Arquitectónica

* **Autoridad Semántica de la Tabla de Símbolos:** Según la teoría estándar de diseño de compiladores (*Dragon Book*, Capítulo 2 y 4), la Tabla de Símbolos es el repositorio central de los atributos léxicos y semánticos (tipo, ámbito, tamaño, visibilidad). El ámbito no debe ser re-deducido heurísticamente por etapas posteriores analizando la sintaxis.
* **Eliminación de la Heurística de `TAC_BEGIN_FUNC`:** Al consultar el símbolo directamente, se eliminó la restricción arbitraria que asumía que ninguna variable global podía aparecer o declararse después de una función.
* **Eficiencia en Backend y Optimizador:** Consultar un flag booleano en un puntero preexistente ($O(1)$) evita traversals innecesarios sobre listas enlazadas y comparaciones de strings en operaciones de alta frecuencia (como `resolveOperandLocation` y rondas de optimización).

---

## 4. Consecuencias

### Positivas
1. **Rendimiento:** Resolución de variables globales en tiempo $O(1)$ en optimizador y backend.
2. **Robustez y Simplicidad:** Código de emisión de declaraciones en `intermediate_code.c` más limpio y desacoplado de la estructura posicional del AST.
3. **Mantenibilidad:** Un único punto de verdad (`appendSymbol`) define si un símbolo es global.
4. **Compatibilidad:** La representación textual externa de las cuádruplas TAC (`.ci`) no se alteró, manteniendo invariantes los contratos de prueba existentes.

### Negativas / Compromisos Aceptados
* Se mantiene una pequeña función de fallback lineal en `object_code.c` y `optimizer.c` que solo se ejecuta cuando `operand->symbol` es nulo, pensada exclusivamente para soportar suites de pruebas sintéticas donde se crean instrucciones TAC "a mano".
