# Alcance y Reglas del Análisis Semántico

El análisis semántico recorre el Árbol de Sintaxis Abstracta (AST) tras un parseo sintáctico exitoso. Sus responsabilidades están organizadas en los módulos `src/semantic_analyzer.c`, `src/symbol_table.c` y `src/type_checker.c`.

---

## 1. Gestión de Ámbitos y Declaraciones (Scopes)


- **Declaración antes del uso:** Cualquier variable o método debe haber sido declarado antes de ser referenciado en una expresión, asignación o llamada.
- **Sin duplicados en el mismo ámbito:** No se permite declarar dos identificadores con el mismo nombre dentro del mismo nivel de `Scope` (variables locales en el mismo bloque o funciones en el scope global).
- **Sombreado permitido (*Shadowing*):** Un bloque interno o método puede declarar una variable con el mismo nombre que otra en un ámbito padre, ocultando la visibilidad del padre dentro del bloque local.
- **Asignación de identificador único (`UID`):** Al insertarse un símbolo en su `Scope`, se le asigna un `uid` autoincremental que persiste en el AST. Esto permite distinguir de forma inmediata variables homónimas en distintos niveles.

---

## 2. Sistema de Tipos y Expresiones

El chequeo de tipos se implementa en `src/type_checker.c`:

- **Compatibilidad en Asignaciones (`isAssignable`):**
  - Tipos idénticos (`int` a `int`, `float` a `float`, `boolean` a `boolean`).
  - Coerción implícita de `int` a `float`.
  - Se rechazan incompatibilidades como asignar `boolean` a variables numéricas o `float` a variables `int`.
- **Operaciones Aritméticas (`+`, `-`, `*`, `/`, `%`):**
  - Enteros con enteros resultan en `TYPE_INT`.
  - Operaciones con al menos un `float` resultan en `TYPE_FLOAT` (con coerción del operando entero).
  - El operador `%` calcula el resto y requiere operandos enteros.
- **Operaciones Lógicas (`&&`, `||`, `!`):**
  - Requieren estrictamente operandos de tipo `boolean` y retornan `TYPE_BOOL`.
- **Operaciones Relacionales y de Igualdad (`==`, `<`, `>`):**
  - Retornan siempre `TYPE_BOOL`.
  - Comparan operandos compatibles.

---

## 3. Sentencias de Control de Flujo

- **Condiciones en `if` y `while`:** La expresión condicional de un nodo `IF_ELSE_NODE` o `WHILE_NODE` debe inferirse obligatoriamente como `TYPE_BOOL`. Si se proporciona un valor numérico o incompatible, se emite un error semántico.

---

## 4. Métodos, Parámetros y Retornos

- **Firmas e Invocaciones:**
  - Toda llamada a método (`METHOD_CALL_NODE`) verifica que la función exista en la tabla de símbolos.
  - La cantidad de argumentos suministrados debe coincidir exactamente con la cantidad de parámetros formales declarados.
  - Cada argumento debe ser compatible en tipo con el parámetro esperado según `isAssignable()`.
  - Un método de tipo `void` no puede ser utilizado como parte de una expresión.
- **Sentencias `return`:**
  - Una sentencia `return` fuera del cuerpo de cualquier función genera un error semántico.
  - En funciones `void`, un `return` no debe contener ninguna expresión (`return;`).
  - En funciones con tipo de retorno (`int`, `float`, `boolean`), el `return` debe contener una expresión cuyo tipo sea compatible con el declarado en la firma del método.

---

## 5. Estructura Global del Programa

- **Método `main()` Obligatorio:** Al finalizar el recorrido semántico del AST, `checkMainMethodExists()` valida que en el ámbito global exista un método llamado `"main"`, que no reciba parámetros. De no encontrarse, se aborta la compilación con un error semántico específico.

---

## 6. Manejo de Errores

A diferencia del análisis sintáctico que interrumpe la ejecución en el primer fallo:
- El análisis semántico **no corta en el primer error**.
- Recorre todo el árbol reportando cada inconsistencia con su respectivo número de línea.
- Si se acumula al menos un error (`semantic_errors > 0`), la compilación se aborta con código de salida `1` impidiendo la generación de código intermedio.
