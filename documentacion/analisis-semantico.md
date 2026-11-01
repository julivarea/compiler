# Alcance y Reglas del Análisis Semántico

El análisis semántico recorre el Árbol de Sintaxis Abstracta (AST) tras un parseo sintáctico exitoso. Sus responsabilidades están organizadas en los módulos `src/semantic_analyzer.c`, `src/symbol_table.c` y `src/type_checker.c`.

---

## 1. Gestión de Ámbitos y Declaraciones (Scopes)

La resolución léxica se apoya en un árbol de ámbitos (`Scope`) donde cada bloque y función posee su propio entorno:

- **Búsqueda Jerárquica (`lookupSymbol`):**
  - Al evaluar el uso de una variable en expresiones (`ID_NODE`), sentencias de asignación (`ASSIGNMENT_NODE`) o llamadas (`METHOD_CALL_NODE`), la búsqueda inicia en el `Scope` actual mediante `lookupSymbolInScope`.
  - Si el identificador no se encuentra localmente, asciende recursivamente a través del puntero `scope->parent` hasta alcanzar el ámbito global (`level = 0`).
  - Al hallarlo, el nodo del AST actualiza su puntero `node->symbol` hacia el `Symbol` resuelto, asociando su `uid` y tipo.
- **Búsqueda Local y Validación de Duplicados (`lookupSymbolInScope`):**
  - Al declarar una variable o método, se utiliza exclusivamente `lookupSymbolInScope` para verificar colisiones en el nivel actual sin consultar a los padres.
  - No se permite declarar dos identificadores con el mismo nombre dentro del mismo nivel de `Scope`.
- **Sombreado Léxico (*Shadowing*):**
  - Un ámbito anidado puede declarar una variable con el mismo nombre que otra en un ámbito ancestro. Como `lookupSymbol` consulta primero el ámbito local, la variable interna oculta a la externa dentro de su bloque.
- **Asignación de Identificador Único (`UID`):**
  - Al insertarse un símbolo en su `Scope` con `appendSymbol()`, se le asigna un `uid` secuencial único ($\ge 1$). Los identificadores no declarados mantienen `uid = 0`. Esto garantiza la distinción inequívoca de variables homónimas en las etapas de código intermedio (TAC) y código objeto.

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
  - **Garantía de Retorno:** Todo método no-void debe asegurar mediante `guaranteesReturn` que todos sus caminos de ejecución terminen en un `return`.

---

## 5. Estructura Global del Programa

- **Método `main()` Obligatorio:** Al finalizar el recorrido semántico del AST, `checkMainMethodExists()` valida que en el ámbito global exista un método llamado `"main"`, que no reciba parámetros. De no encontrarse, se aborta la compilación con un error semántico específico.

---

## 6. Manejo de Errores y Casos Anómalos

El compilador aplica un modelo de diagnóstico acumulativo y robusto:

### Casos Anómalos de Resolución de Variables ("Casos Malos")
1. **Variable no declarada en expresión:** Si `lookupSymbol` retorna `NULL` al evaluar un identificador (`ID_NODE`), se reporta error (`"Variable no declarada: <id>"`) y se asigna el tipo `TYPE_ERROR` al nodo. Los operadores que reciben este tipo propagan el error sin generar reportes falsos redundantes.
2. **Asignación a variable no declarada:** Si en `ASSIGNMENT_NODE` el identificador no existe en ningún scope ancestro, se reporta `"Asignacion a variable no declarada '<id>'"`.
3. **Colisión de categoría de símbolo:** Si el identificador existe pero corresponde a un método y se intenta utilizar como variable, o viceversa, se reporta la discrepancia categórica (`VARIABLE_SYMBOL` vs `METHOD_SYMBOL`).
4. **Redeclaración en el mismo ámbito:** Si `lookupSymbolInScope` encuentra que el símbolo ya existía en el nivel actual, se emite `"La variable '<id>' ya fue declarada previamente en este ambito"` y se descarta la inserción duplicada.
5. **Punteros nulos:** Las funciones de búsqueda manejan entradas `NULL` (`scope == NULL` o `id == NULL`) retornando `NULL` de forma segura.

### Política de Terminación del Compilador
- A diferencia del análisis sintáctico que interrumpe la ejecución en el primer fallo, el análisis semántico **no corta en el primer error**, sino que recorre todo el AST acumulando los diagnósticos con su número de línea.
- Si se registra al menos un error (`semantic_errors > 0`), el módulo `driver.c` emite el mensaje `"Compilacion abortada: se encontraron X errores semanticos."` y aborta la compilación con código de salida `1`, impidiendo la generación de código intermedio (TAC) y código ensamblador.
