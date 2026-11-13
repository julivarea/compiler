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

## 2. Modelado de Símbolos (`Symbol`) y Variantes de `SymbolType`

El compilador clasifica todas las entidades semánticas registradas en los entornos o referenciadas en el AST mediante el enumerado [`SymbolType`](https://github.com/julivarea/compiler/blob/main/include/symbol_table.h#L16-L20) definido en [`include/symbol_table.h`](https://github.com/julivarea/compiler/blob/main/include/symbol_table.h):

```c
typedef enum SymbolType {
    VARIABLE_SYMBOL,    /**< Identificador de variable o parámetro. */
    METHOD_SYMBOL,      /**< Identificador de método o función. */
    CONSTANT_SYMBOL     /**< Literal o valor constante primitivo. */
} SymbolType;
```

Cada entidad en memoria dinámica se modela a través de la estructura [`Symbol`](https://github.com/julivarea/compiler/blob/main/include/symbol_table.h#L25-L35):

```c
typedef struct Symbol {
    char *id;                   /**< Nombre textual del identificador en el código fuente. */
    int uid;                    /**< Identificador único asignado al entrar a un ámbito (0 si no está declarado). */
    DataType type;              /**< Tipo de dato semántico asociado al símbolo. */
    SymbolType symbolType;      /**< Categoría sintáctico-semántica del símbolo. */
    struct NodeAST *parameters; /**< Lista de parámetros formales (solo si symbolType es METHOD_SYMBOL). */
    union {
        int intValue;           /**< Valor numérico para constantes de tipo entero o booleano. */
        float floatValue;       /**< Valor numérico para constantes de punto flotante. */
    } value;
} Symbol;
```

### Las 3 Variantes de `SymbolType` y Uso de Cada Campo

A continuación se describen las 3 variantes de [`SymbolType`](https://github.com/julivarea/compiler/blob/main/include/symbol_table.h#L16-L20), su propósito semántico en el compilador y para qué se utiliza cada campo de [`Symbol`](https://github.com/julivarea/compiler/blob/main/include/symbol_table.h#L25-L35):

#### 1. `VARIABLE_SYMBOL` (Variables y Parámetros)
Representa identificadores de variables locales, variables globales y parámetros formales dentro de métodos.

- **`id` (`char*`):** Almacena el nombre textual del identificador en el código fuente (duplicado en el heap mediante `strdup`, por ejemplo `"x"`, `"total"`). Es utilizado por [`lookupSymbol`](https://github.com/julivarea/compiler/blob/main/include/symbol_table.h#L112) y [`lookupSymbolInScope`](https://github.com/julivarea/compiler/blob/main/include/symbol_table.h#L97) para la resolución de identificadores en expresiones y asignaciones, y para prevenir declaraciones duplicadas dentro del mismo ámbito.
- **`uid` (`int`):** Identificador numérico único incremental ($\ge 1$) asignado por [`appendSymbol`](https://github.com/julivarea/compiler/blob/main/include/symbol_table.h#L83) al registrar la variable en su [`Scope`](https://github.com/julivarea/compiler/blob/main/include/symbol_table.h#L65). Permite resolver colisiones por sombreado (*shadowing*) distinguiendo variables con el mismo nombre en distintos bloques. En etapas posteriores (código intermedio TAC y código objeto), el `uid` identifica unívocamente la variable y determina su ubicación en el marco de activación / stack (`FunctionFrame`). Si el símbolo es temporal o no ha sido declarado, se mantiene en `0`.
- **`type` (`DataType`):** Tipo de dato estático declarado para la variable (`TYPE_INT`, `TYPE_FLOAT` o `TYPE_BOOL`). Es consumido por [`type_checker.c`](https://github.com/julivarea/compiler/blob/main/src/type_checker.c) para chequear la validez de operaciones aritméticas/lógicas, verificar asignaciones mediante [`isAssignable`](https://github.com/julivarea/compiler/blob/main/include/type_checker.h#L40) y aplicar coerciones de tipo si corresponde.
- **`symbolType`:** Fijo en `VARIABLE_SYMBOL`. Permite al analizador semántico comprobar que el identificador usado en una asignación (`x = ...`) o expresión sea efectivamente una variable y no un método (evita que se use un método como valor o se intente asignar a una función).
- **`parameters` (`NodeAST*`):** No se utiliza (`NULL`), ya que las variables y parámetros no definen una firma de parámetros.
- **`value` (`union`):** No se utiliza durante el análisis semántico. Las variables obtienen su valor dinámico en tiempo de ejecución o mediante instrucciones TAC (`TAC_ASSIGN`).

#### 2. `METHOD_SYMBOL` (Métodos y Funciones)
Representa la firma y declaración de una función o método en el ámbito global (incluyendo el punto de entrada obligatorio `main`).

- **`id` (`char*`):** Nombre textual de la función (ej. `"factorial"`, `"main"`). Se usa para registrar el método en el ámbito global (`level = 0`) y resolver invocaciones (`METHOD_CALL_NODE`) en cualquier punto del programa mediante [`lookupSymbol`](https://github.com/julivarea/compiler/blob/main/include/symbol_table.h#L112).
- **`uid` (`int`):** Identificador único asignado al insertar el método en el ámbito global. Sirve de referencia unívoca para rotular etiquetas y saltos en la generación de código intermedio (`TAC_LABEL`, `TAC_CALL`) y en las directivas de ensamblador.
- **`type` (`DataType`):** Tipo de dato retornado por el método (`TYPE_INT`, `TYPE_FLOAT`, `TYPE_BOOL` o `TYPE_VOID`). Se usa para:
  - Validar que las sentencias `return` dentro de su cuerpo retornen expresiones de tipo compatible (`isAssignable`).
  - Prohibir que métodos `void` contengan sentencias `return` con valor.
  - Asignar el tipo resultante al nodo de invocación (`node->type = method->type`) y rechazar llamadas a métodos `void` dentro de expresiones aritméticas o condicionales.
- **`symbolType`:** Fijo en `METHOD_SYMBOL`. Permite distinguir llamadas a métodos de usos de variables, impidiendo invocar una variable como función (ej. `mi_variable()`) o asignar a un método.
- **`parameters` (`NodeAST*`):** Puntero directo al subárbol del AST (`PARAMETERS_NODE`) que contiene la lista de parámetros formales declarados. Se utiliza para:
  - **Validación de aridad:** Comprobar que la cantidad de argumentos provistos en la llamada (`node->childCount`) coincida exactamente con la cantidad de parámetros esperados (`parameters->childCount`).
  - **Chequeo de tipos en invocación:** Recorrer ordenadamente los argumentos reales e inspeccionar con [`isAssignable`](https://github.com/julivarea/compiler/blob/main/include/type_checker.h#L40) que cada uno sea compatible con el tipo del parámetro formal en esa posición (`parameters->children[i]->type`).
  - **Poblado del ámbito local:** Al abrir el [`Scope`](https://github.com/julivarea/compiler/blob/main/include/symbol_table.h#L65) del cuerpo del método, cada parámetro formal de este subárbol se inserta como una variable local (`VARIABLE_SYMBOL`).
- **`value` (`union`):** No se utiliza en esta variante.

#### 3. `CONSTANT_SYMBOL` (Literales y Constantes Primitivas)
Representa valores inmediatos o literales numéricos y booleanos presentes en las expresiones del código fuente (ej. `42`, `3.1415`, `true`, `false`).

- **`id` (`char*`):** Es `NULL`, dado que los literales son anónimos y no corresponden a identificadores con nombre.
- **`uid` (`int`):** Es `0`, ya que los literales no se insertan en las tablas de símbolos de los ámbitos léxicos (`Scope`).
- **`type` (`DataType`):** Tipo de dato primitivo del literal (`TYPE_INT`, `TYPE_FLOAT` o `TYPE_BOOL`). Permite al verificador de tipos inferir de forma directa e inmediata el tipo del nodo constante (`CONSTANT_NODE`) en expresiones.
- **`symbolType`:** Fijo en `CONSTANT_SYMBOL`. Identifica que el nodo AST encapsula un valor constante inmediato y no requiere búsqueda en la tabla de símbolos.
- **`parameters` (`NodeAST*`):** No se utiliza (`NULL`).
- **`value` (`union`):** Almacena el valor evaluado directamente en memoria binaria nativa:
  - `value.intValue`: Almacena el valor numérico entero para `TYPE_INT`, o `1`/`0` para constantes booleanas `true`/`false` (`TYPE_BOOL`).
  - `value.floatValue`: Almacena el número en punto flotante para `TYPE_FLOAT`.
  - **Uso posterior:** Este campo evita repetir conversiones de texto a número a lo largo del pipeline. En la etapa de código intermedio se mapea directamente a operandos inmediatos TAC (`tacConstantInt`, `tacConstantBool`) y en la generación de código objeto se convierte en valores inmediatos de ensamblador (`asmLoad(..., opImm(...))`).

### Resumen de Uso de Campos según la Variante

| Campo en [`Symbol`](https://github.com/julivarea/compiler/blob/main/include/symbol_table.h#L25-L35) | `VARIABLE_SYMBOL` | `METHOD_SYMBOL` | `CONSTANT_SYMBOL` |
| :--- | :--- | :--- | :--- |
| **`id`** | Nombre de la variable (`char*`) | Nombre de la función (`char*`) | `NULL` (anónimo) |
| **`uid`** | Secuencial único ($\ge 1$, $0$ si no declarada) | Secuencial único ($\ge 1$) | `0` (no registrado en `Scope`) |
| **`type`** | Tipo de la variable (`int`, `float`, `boolean`) | Tipo de retorno (`int`, `float`, `boolean`, `void`) | Tipo del literal (`int`, `float`, `boolean`) |
| **`symbolType`** | `VARIABLE_SYMBOL` | `METHOD_SYMBOL` | `CONSTANT_SYMBOL` |
| **`parameters`** | `NULL` (no aplica) | Puntero al AST de parámetros (`PARAMETERS_NODE`) | `NULL` (no aplica) |
| **`value`** | No utilizado en análisis semántico | No utilizado | `intValue` (int/bool) o `floatValue` (float) |

---

## 3. Sistema de Tipos y Expresiones

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

## 4. Sentencias de Control de Flujo

- **Condiciones en `if` y `while`:** La expresión condicional de un nodo `IF_ELSE_NODE` o `WHILE_NODE` debe inferirse obligatoriamente como `TYPE_BOOL`. Si se proporciona un valor numérico o incompatible, se emite un error semántico.

---

## 5. Métodos, Parámetros y Retornos

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

## 6. Estructura Global del Programa

- **Método `main()` Obligatorio:** Al finalizar el recorrido semántico del AST, `checkMainMethodExists()` valida que en el ámbito global exista un método llamado `"main"`, que no reciba parámetros. De no encontrarse, se aborta la compilación con un error semántico específico.

---

## 7. Manejo de Errores y Casos Anómalos

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
