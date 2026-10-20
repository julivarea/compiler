# Checklist y Estado: Análisis Semántico

Documento de seguimiento del estado del analizador semántico (`src/semantic_analyzer.c` y `src/type_checker.c`), especificando lo pendiente de implementar y el registro consolidado de tareas resueltas según la especificación de **C-TDS** (`documentacion/spec/01-TDS-spec-lenguaje.pdf`).

**Estado:** las 14 reglas semánticas del enunciado están implementadas y verificadas a mano, una por una. Lo único pendiente es la suite de pruebas automatizadas.

---

## 1. Tareas Pendientes (Lo que nos queda)

### 1.1 Pruebas Unitarias y Automatización
- [ ] **Crear suite de pruebas semánticas (`tests/test_semantic.c`):**
  - [ ] Pruebas de variables no declaradas y redeclaración en el mismo scope.
  - [ ] Pruebas de expresiones inválidas (`bool + int`, `3.5 % 2`, `true < false`).
  - [ ] Pruebas de `==` con operandos de distinto tipo (`1 == 1.5`, `true == 1`), que deben fallar, y del mismo tipo, que deben pasar.
  - [ ] Pruebas de condiciones no booleanas en `if` y `while` (`if (10) ...`).
  - [ ] Pruebas de llamadas a métodos con argumentos de más, de menos o tipos incompatibles.
  - [ ] Pruebas de llamadas a identificadores que son variables (`x()`).
  - [ ] Pruebas de asignaciones inválidas a métodos (`foo = 5`).
  - [ ] Pruebas de control de retorno: método no-void sin ningún `return`, `if` sin `else` como única salida, `while` como única salida, `return` con expresión en un método `void`, `return` sin expresión en un método no-void, y tipo de retorno incompatible.
  - [ ] Pruebas de retorno **válidas** que no deben fallar: `return` en ambas ramas de un `if/else`, `return` dentro de un bloque anidado, y `return` duplicado (ver decisión de diseño 3).
  - [ ] Pruebas de presencia y firma del método `main` (sin argumentos, existencia obligatoria).
- [ ] **Integración en `test_suite.sh`:**
  - [ ] Agregar compilación de `test_semantic.c` vinculando `unity.c`, `semantic_analyzer.c`, `type_checker.c`, `symbol_table.c`, `ast.c`, `error_handler.c`, etc.
  - [ ] Ejecutar el binario generado (`run_semantic_tests`) dentro de la suite de pruebas automatizadas.

> **Nota para quien escriba los tests:** `analyzeSemantics(raiz, NULL, TYPE_VOID)` valida todo, incluida la presencia y firma de `main`; no hace falta llamar nada más. Como `semantic_errors` es una global, hay que resetearla a 0 en el `setUp()` de cada caso.

---

## 2. Estado de Tareas Previas Completadas

### 2.1 Ámbitos y declaraciones
- [x] **Validación de variables y ámbitos:** Detección de variables no declaradas, símbolos fuera de alcance, prohibición de redeclaraciones en el mismo scope y unificación de ámbito entre parámetros formales y variables locales en el cuerpo de la función (Reglas 1 y 2).
- [x] **Validación de tipos de símbolo en asignaciones:** Exigencia de que el lado izquierdo de una asignación sea una variable o parámetro (`ID_SYMBOL`) y no un método (Regla 8).
- [x] **Validación de tipos de símbolo en llamadas:** Verificación de que el identificador llamado corresponda a un método (`METHOD_SYMBOL`) y no a una variable.
- [x] **Integración de `checkMainMethodExists` en el ciclo semántico (Regla 3):** `analyzeSemantics()` la invoca sola cuando `parent == NULL`, que es lo que distingue al scope global, y después de recorrer los hijos para que `main` ya esté registrado en la tabla. La función pasó a ser `static` y se quitó de `semantic_analyzer.h`: si quedaba pública, llamarla de nuevo duplicaba el error. El llamado manual desde `main()` de `src/bison.y` se eliminó.

### 2.2 Tipos y expresiones
- [x] **Deducción de tipos en expresiones:** Inferencia y tipado en expresiones binarias matemáticas, relacionales y lógicas básicas.
- [x] **Restricción del operador módulo (`%`):** Restringido exclusivamente a operandos enteros (`TYPE_INT`). Antes usaba `isNumeric()`, que incluye `TYPE_FLOAT`, así que `3.5 % 2` se aceptaba y encima quedaba tipado como `INT`. Ver decisión de diseño 2.
- [x] **Restricción de operadores relacionales (`<`, `>`):** Exclusivo para tipos numéricos (`TYPE_INT` y `TYPE_FLOAT`), rechazando operandos booleanos (Regla 10).
- [x] **Operaciones relacionales mixtas:** Soporte de coerción/promoción para comparaciones relacionales entre `int` y `float` (Reglas 10 y 14).
- [x] **Igualdad (`==`) con tipos idénticos (Regla 11):** Ambos operandos deben ser del mismo tipo básico. No se aplica la coerción `int`/`float`, a diferencia de los relacionales. Ver decisión de diseño 1.
- [x] **Operadores lógicos y negación (Regla 12):** `&&`, `||` y `!` exigen operandos `boolean`.
- [x] **Compatibilidad en asignaciones (Reglas 13 y 14):** Validación de tipos asignables vía `isAssignable`, con coerción de `int` a `float`.
- [x] **Validación de llamadas a métodos (Reglas 4 y 5):** Chequeo de aridad, correspondencia de tipos esperados vs reales en argumentos, y prohibición de usar un método `void` como expresión.
- [x] **Estructuras de control (Regla 9):** Validación de tipo booleano en las condiciones de `if` y `while`.

### 2.3 Retornos
- [x] **Validación de `return` (Reglas 6 y 7):** Concordancia entre el tipo devuelto y el tipo del método actual, `return` sin expresión solo en métodos `void`, `return` con expresión solo en métodos no-void, y detección de sentencias `return` fuera del cuerpo de una función.
- [x] **Completitud de retornos en todos los caminos:** La función `guaranteesReturn()` verifica recursivamente que un método no-void no pueda alcanzar su fin sin retornar, como exige la sección "Invocación y Retorno de Métodos" del enunciado. Un `return` garantiza; un bloque garantiza si alguna de sus sentencias garantiza; un `if` garantiza solo si tiene `else` y ambas ramas garantizan; un `while` nunca garantiza, porque su condición puede ser falsa en la primera evaluación. Antes solo se buscaba un `RETURN_NODE` entre los hijos directos del cuerpo, lo que rechazaba programas válidos (un `if/else` que retorna en ambas ramas) y aceptaba inválidos (un `if` sin `else` como única salida).

### 2.4 Mantenimiento
- [x] **Eliminación de `analyzeIdNode()`:** Era código inalcanzable. Un `ID_NODE` llega a `analyzeIdentifier()` si está en una expresión, lo resuelve `analyzeAssignmentNode()` si es el destino de una asignación, y se convierte en `VARIABLE_DECLARATION_NODE` si viene de una lista de declaración; ninguno pasa por `analyzeNodeSemantics()`. Se verificó instrumentando la función y corriendo 56 programas sin una sola ejecución. Además no anotaba `node->symbol` ni `node->type`, así que de haber sido alcanzable habría dejado nodos sin resolver para la generación de código.
- [x] **Eliminación del chequeo "Ya hay un return definido":** Ver decisión de diseño 3.

---

## 3. Decisiones de diseño sobre ambigüedades del enunciado

Estas tres resuelven puntos donde la letra no es unívoca. Conviene reflejarlas en la documentación de la etapa y confirmarlas con la cátedra.

1. **`==` exige tipos idénticos, los relacionales no.** La regla 11 y la sección Expresiones dicen que únicamente se pueden comparar expresiones del mismo tipo, mientras que la regla 14 permite coerciones `int`/`float` en general. Interpretamos que la regla específica de `==` gana sobre la general, pero que para `<` y `>` (regla 10, que pide operandos "int o float" sin exigir que coincidan) la coerción sí aplica. Consecuencia visible: `x > y` con un `int` y un `float` se acepta, `x == y` no.

2. **`%` acepta solo enteros.** La regla 10 dice que los operandos de los ⟨arith_op⟩ —y `%` figura entre ellos— pueden ser `int` o `float`, pero la sección Expresiones dice que `%` computa el resto de una división de números enteros. Elegimos la segunda por ser la más específica, y porque la primera obligaría a definir qué significa truncar un `float` al generar código. **Es la más riesgosa de las tres:** si los tests no públicos siguen la regla 10 al pie de la letra, un `3.5 % 2` que nosotros rechazamos contaría como falso positivo.

3. **No se reportan `return` duplicados ni código inalcanzable.** Ninguna de las 14 reglas los prohíbe, y el enunciado cierra esa sección diciendo que si el compilador no detecta ninguna violación no debe generar ningún informe. El chequeo anterior, además de reportar algo que no es violación, se disparaba con cualquier sentencia posterior a un `return` —no solo con otro `return`— y dejaba de analizarla, de modo que un error real ubicado después de un `return` quedaba invisible. `examples/two_returns.txt` pasó a ser un programa válido.

> **Advertencia:** el programa de ejemplo del propio enunciado (página 3) tiene un `void main()` que ejecuta `return 1;`, lo que viola la regla 6. No sirve como caso de test.
