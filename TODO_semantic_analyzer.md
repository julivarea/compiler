# Checklist de Investigación y Pendientes: Análisis Semántico

Este documento reúne el estado actual, las discrepancias encontradas frente a la especificación oficial de **C-TDS** (`docs/spec/01-TDS-spec-lenguaje.pdf`), y los puntos pendientes a investigar e implementar en el analizador semántico (`semantic_analyzer.c` y `type_checker.c`).

---

## 1. Reglas Semánticas y Bugs a Investigar / Corregir

- [ X ] **Validación de tipos de símbolo en invocaciones (`METHOD_CALL_NODE`)**
  - **Ubicación:** `src/type_checker.c` (`analyzeMethodCall`).
  - **Problema:** Si se llama a una variable como si fuera una función (ej. `int x; x(1, 2);`), `lookupVariable` encuentra el símbolo pero no comprueba que `method->symbolType == METHOD_SYMBOL`.
  - **Acción:** Validar `symbolType == METHOD_SYMBOL` antes de leer parámetros; emitir error semántico claro si no es un método.

- [X] **Validación de tipos de símbolo en asignaciones (`ASSIGNMENT_NODE`)**
  - **Ubicación:** `src/semantic_analyzer.c` (`analyzeAssignmentNode`).
  - **Problema:** Si se asigna a un nombre de función (ej. `void foo() {} ... foo = 5;`), no se valida que el destino sea una variable o parámetro (`symbolType == ID_SYMBOL`).
  - **Acción:** Exigir que la variable a la izquierda de la asignación sea `ID_SYMBOL` (Regla 8 de la especificación).

- [X] **Restricción de tipos en operadores relacionales (`<`, `>`)**
  - **Ubicación:** `src/type_checker.c` (`resolveBinaryOperation`).
  - **Problema:** `OP_LT` y `OP_GT` están agrupados con `OP_EQ`, lo que permite comparar booleanos con desigualdad (ej. `true < false`).
  - **Acción:** Según la Regla 10 de la especificación, los operandos de `<` y `>` deben ser únicamente numéricos (`TYPE_INT` o `TYPE_FLOAT`), nunca `TYPE_BOOL`.

- [X] **Operaciones relacionales mixtas (`int` con `float`)**
  - **Ubicación:** `src/type_checker.c` (`resolveBinaryOperation`).
  - **Problema:** Actualmente se exige igualdad estricta `left == right`, rechazando comparaciones válidas como `3 < 4.5`.
  - **Acción:** Permitir comparación relacional entre `int` y `float` aplicando coerción/promoción (Regla 10 y 14).

- [X] **Restricción del operador módulo (`%`) a enteros**
  - **Ubicación:** `src/type_checker.c` (`resolveBinaryOperation`).
  - **Problema:** `OP_MOD` está agrupado con las operaciones aritméticas generales y permite `float % float -> float`.
  - **Acción:** La especificación define que `%` solo computa el resto entre números enteros (`TYPE_INT`).

- [X] **Conflicto entre parámetros formales y variables locales en el cuerpo de la función**
  - **Ubicación:** `src/semantic_analyzer.c` (`analyzeMethodNode`).
  - **Problema:** El cuerpo del método (`BLOCK_NODE`) creaba un nuevo sub-ámbito, permitiendo redeclarar como variable local un identificador idéntico al de un parámetro formal (tratándolo como shadowing).
  - **Acción:** Se unificó el ámbito del método: según la especificación ("el scope de la función está conformado por los parámetros formales y las variables declaradas en el cuerpo"), el cuerpo principal comparte el `method_scope`, detectándose la redeclaración automáticamente con `lookupVariableCurrentScope`. Los bloques anidados (`if`, `while`, `{}`) siguen creando sub-ámbitos con shadowing permitido.

---

## 2. Control de Flujo y Estructura Global

- [ ] **Detección de falta de `return` en funciones no-void (Control Flow / Missing Return)**
  - **Ubicación:** `src/semantic_analyzer.c`.
  - **Problema:** Métodos con tipo de retorno distinto de `TYPE_VOID` pueden alcanzar el fin de su bloque sin una sentencia `return` válida.
  - **Acción:** Investigar si se requiere análisis de retorno estático (comprobar que la última sentencia del bloque sea `return`, o análisis de todos los caminos de ejecución en `if/else`).

- [ ] **Integración de `checkMainMethodExists` en el ciclo semántico**
  - **Ubicación:** `src/semantic_analyzer.c` y `src/bison.y`.
  - **Problema:** La verificación del método `main` está expuesta como función auxiliar y se invoca desde `main()` de `bison.y`, pero no corre automáticamente dentro de `analyzeSemantics()`.
  - **Acción:** Integrar o asegurar que cualquier invocación de análisis semántico (incluyendo futuros tests unitarios) valide la existencia y firma del método `main`.

---

## 3. Estado de Tareas Previas

- [x] Validar variables no declaradas y redeclaraciones en el mismo scope.
- [x] Deducción de tipos en expresiones binarias matemáticas y lógicas básicas.
- [x] Validación de firmas en llamadas a funciones (cantidad y tipos de parámetros).
- [x] Validación de compatibilidad de tipos en asignaciones (`isAssignable`).
- [x] Validación de tipo booleano en condiciones de `if` y `while` (`analyzeIfElseNode`, `analyzeWhileNode`).
- [x] Validación de sentencias `return` (`analyzeReturnNode`: tipo devuelto vs tipo esperado del método, y detección de return fuera de función).

---

## 4. Pruebas Unitarias y Reportes

- [ ] **Crear `tests/test_semantic.c`:**
  - Pruebas de variables no declaradas y redeclaración en el mismo scope.
  - Pruebas de expresiones inválidas (`bool + int`, `3.5 % 2`, `true < false`).
  - Pruebas de condiciones de `if` y `while` no booleanas (`if (10) ...`).
  - Pruebas de llamadas a métodos con argumentos de más, de menos o tipos incompatibles.
  - Pruebas de llamadas a variables no-función (`x()`).
  - Pruebas de retornos inconsistentes (devolver valor en función `void`, no devolver valor en función con tipo, tipos incompatibles).
  - Pruebas de presencia y firma del método `main`.
- [ ] **Integrar en `test_suite.sh`:**
  - Agregar la compilación y ejecución de `test_semantic` junto a los tests de lexer y parser.
