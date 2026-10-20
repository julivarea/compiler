# Checklist y Estado: Análisis Semántico

Documento de seguimiento del estado del analizador semántico (`src/semantic_analyzer.c` y `src/type_checker.c`), especificando lo pendiente de implementar y el registro consolidado de tareas resueltas según la especificación de **C-TDS** (`documentacion/spec/01-TDS-spec-lenguaje.pdf`).

---

## 1. Tareas Pendientes (Lo que nos queda)

### 1.1 Estructura Global y Control de Flujo
- [x] **Integración de `checkMainMethodExists` en el ciclo semántico**
  - **Ubicación:** `src/semantic_analyzer.c` y `src/bison.y`.
  - **Problema:** La verificación del método `main` está implementada en `checkMainMethodExists()`, pero se invoca manualmente desde `main()` de `bison.y`.
  - **Acción:** Integrarla de forma automática en el scope global (`parent == NULL`) dentro de `analyzeSemantics()`, para que cualquier invocación del análisis (compilador o tests unitarios) valide automáticamente la presencia y firma de `main`.
  - **Resuelto:** `analyzeSemantics()` llama a `checkMainMethodExists()` cuando `parent == NULL`, que es lo que distingue al scope global. La función pasó a ser `static` y se quitó de `semantic_analyzer.h`, para que no pueda invocarse dos veces y duplicar el error.


### 1.2 Pruebas Unitarias y Automatización
- [ ] **Crear suite de pruebas semánticas (`tests/test_semantic.c`):**
  - [ ] Pruebas de variables no declaradas y redeclaración en el mismo scope.
  - [ ] Pruebas de expresiones inválidas (`bool + int`, `3.5 % 2`, `true < false`).
  - [ ] Pruebas de condiciones no booleanas en `if` y `while` (`if (10) ...`).
  - [ ] Pruebas de llamadas a métodos con argumentos de más, de menos o tipos incompatibles.
  - [ ] Pruebas de llamadas a identificadores que son variables (`x()`).
  - [ ] Pruebas de asignaciones inválidas a métodos (`foo = 5`).
  - [ ] Pruebas de control de retorno (función con tipo sin return, return duplicado, tipos incompatibles, return en void).
  - [ ] Pruebas de presencia y firma del método `main` (sin argumentos, existencia obligatoria).
- [ ] **Integración en `test_suite.sh`:**
  - [ ] Agregar compilación de `test_semantic.c` vinculando `unity.c`, `semantic_analyzer.c`, `type_checker.c`, `symbol_table.c`, `ast.c`, `error_handler.c`, etc.
  - [ ] Ejecutar el binario generado (`run_semantic_tests`) dentro de la suite de pruebas automatizadas.

---

## 2. Estado de Tareas Previas Completadas

- [x] **Validación de variables y ámbitos:** Detección de variables no declaradas, símbolos fuera de alcance, prohibición de redeclaraciones en el mismo scope y unificación de ámbito entre parámetros formales y variables locales en el cuerpo de la función.
- [x] **Validación de tipos de símbolo en asignaciones:** Exigencia de que el lado izquierdo de una asignación sea una variable o parámetro (`ID_SYMBOL`) y no un método (Regla 8).
- [x] **Validación de tipos de símbolo en llamadas:** Verificación de que el identificador llamado corresponda a un método (`METHOD_SYMBOL`) y no a una variable.
- [x] **Restricción del operador módulo (`%`):** Restringido exclusivamente a operandos enteros (`TYPE_INT`).
- [x] **Restricción de operadores relacionales (`<`, `>`):** Exclusivo para tipos numéricos (`TYPE_INT` y `TYPE_FLOAT`), rechazando operandos booleanos (Regla 10).
- [x] **Operaciones relacionales mixtas:** Soporte de coerción/promoción para comparaciones relacionales entre `int` y `float` (Reglas 10 y 14).
- [x] **Deducción de tipos en expresiones:** Inferencia y tipado en expresiones binarias matemáticas, relacionales y lógicas básicas.
- [x] **Compatibilidad en asignaciones:** Validación estricta de tipos asignables (`isAssignable`).
- [x] **Validación de llamadas a métodos:** Chequeo de aridad y correspondencia de tipos esperados vs reales en argumentos.
- [x] **Estructuras de control:** Validación de tipo booleano en las condiciones de `if` y `while`.
- [x] **Validación de `return`:** Concordancia entre tipo devuelto y tipo del método actual, y detección de sentencias `return` fuera de funciones.
- [x] **Completitud y unicidad de retornos:** Detección de falta de `return` en funciones no-void y control de sentencias `return` duplicadas en el bloque principal del método.
