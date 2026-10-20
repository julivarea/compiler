# Checklist de Investigación y Pendientes: Generación de Código Intermedio (TAC)

Este documento detalla el estado actual, los errores detectados en la implementación (`src/intermediate_code.c` y `src/intermediate_code.h`), las discrepancias con la especificación oficial de **C-TDS** (`documentacion/spec/01-TDS-spec-lenguaje.pdf`) y las decisiones de diseño necesarias para las siguientes etapas del compilador (Intérprete, Optimizador y Generador x86-64 según `documentacion/spec/00-TDS-proyecto.pdf`).

---

## 0. Orden de trabajo propuesto

Con la entrega de código objeto (enteros) el **lunes 26 de Octubre**, el orden que maximiza el avance es:

1. ~~**Los 4 bugs de generación de la sección 1.**~~ **HECHO.** Son acotados y todo lo demás se construye encima de un TAC correcto.
2. ~~**La migración a cuádruplas (sección 3.1).**~~ **HECHO.** Antes de agregar funcionalidad, no después: el 45% de la nota (Intérprete 10% + Código Objeto 20% + Optimizador 15%) consume el IR, y cualquier feature que escribamos en texto plano habrá que reescribirla.
3. ~~**Corto circuito `&&` / `||` (sección 2.1).**~~ **HECHO.** Lo exige el enunciado de forma explícita.
4. **Lo que cada método debe exponer (secciones 2.2, 3.2 y 3.3):** `begin_func`/`end_func`, parámetros formales y `ret` implícito en métodos `void`. Es un solo trabajo. ← **siguiente.**
5. **Desambiguación de variables por alcance (sección 2.3).**
6. **Declaraciones de variables, globales y locales (sección 3.4).**
7. **La CLI (sección 4).** Es independiente del diseño del TAC y puede hacerse en paralelo.
8. **Coerción `int`/`float` (sección 2.4),** si se decide mantener `float` en el TAC. El enunciado solo exige TAC para enteros y lógicos, así que es opcional.
9. **Pruebas unitarias (sección 5).**

---

## 1. Errores Críticos e Inmediatos (Bugs en la Generación de TAC)

> **Sección completa.** Los cuatro primeros se reprodujeron sobre `examples/factorial.txt` y
> `examples/programa_sin_error.txt`, y se corrigieron en el commit
> `fix TAC generation: call temporaries, call statements, ret newline and if labels`.
> El quinto lo resolvió la migración de la sección 3.1.
>
> Verificación: sobre los 15 programas del corpus de prueba no queda ninguna etiqueta
> referenciada sin definir, ninguna etiqueta muerta, ni ninguna instrucción truncada.

- [x] **Llamadas a funciones en expresiones generan código incompleto / nulo**
  - **Ubicación:** `src/intermediate_code.c` (`generateExpressionCode`, caso `METHOD_CALL_NODE`).
  - **Problema:** Se emite `buildStringf("%s = call %s, %d\n", result.place, ...)`, pero `result.place` nunca se inicializa con un nuevo temporal (`newTemp()`). Queda como cadena vacía `""`.
  - **Efecto:** Se emiten instrucciones rotas como ` = call factorial, 1` y las expresiones que usan el resultado quedan con operandos truncados (ej. `t4 = n * `, `res = `).
  - **Acción:** Asignar `strcpy(result.place, newTemp());` antes de emitir la instrucción `call`.

- [x] **Llamadas a funciones como sentencias son ignoradas completamente**
  - **Ubicación:** `src/intermediate_code.c` (`generateStatementCode`).
  - **Problema:** No existe `case METHOD_CALL_NODE:`. Cae al bloque `default:`, que itera sobre los hijos llamando a `generateStatementCode` sobre expresiones (argumentos), por lo que no emite ni los parámetros ni la instrucción `call`.
  - **Efecto:** Invocaciones independientes como `reiniciar();` o `factorial(a);` desaparecen silenciosamente del código intermedio generado.
  - **Acción:** Implementar `case METHOD_CALL_NODE:` en `generateStatementCode` que evalúe los argumentos con `generateExpressionCode`, emita `param ...` y luego emita `call nombre, N` (sin asignar a temporal).

- [x] **Falta de salto de línea en retorno `void` (`ret`)**
  - **Ubicación:** `src/intermediate_code.c` (`generateStatementCode`, caso `RETURN_NODE`).
  - **Problema:** En retornos sin expresión (`GET_EXPRESSION(node) == NULL`), se emite `buildStringf("ret");` sin el carácter `\n`.
  - **Efecto:** La instrucción `ret` se concatena directamente con la etiqueta del siguiente método (ej. `retfactorial:`).
  - **Acción:** Cambiar por `buildStringf("ret\n");`.

- [x] **Lógica de etiquetas y saltos rotos en condicionales `if-else`**
  - **Ubicación:** `src/intermediate_code.c` (`generateStatementCode`, caso `IF_ELSE_NODE`).
  - **Problemas detectados:**
    1. Si no hay bloque `else` (`GET_ELSE_BLOCK(node) == NULL`), el salto condicional es `if <cond> false goto elseIf`, pero la etiqueta `elseIf:` nunca se imprime en el código porque está dentro del `if (GET_ELSE_BLOCK != NULL)`. El salto apunta a una etiqueta inexistente.
    2. Se emite una etiqueta inicial `startIf:` previa a la condición a la cual ninguna instrucción salta (código muerto innecesario).
    3. Si hay bloque `else`, se emite `goto endIf` al final de la rama `else`, inmediatamente seguido de `endIf:`, produciendo un salto redundante a la siguiente línea.
  - **Acción:**
    - Si no hay bloque `else`: emitir `if <cond> false goto endIf`, generar el bloque `then`, y luego emitir `endIf:`.
    - Si hay bloque `else`: emitir `if <cond> false goto elseIf`, generar bloque `then`, emitir `goto endIf`, emitir `elseIf:`, generar bloque `else`, y emitir `endIf:`.
    - Eliminar la etiqueta no referenciada `startIf`.

- [x] **Fragilidad en la gestión de temporales y fuga de memoria en etiquetas**
  - **Ubicación:** `src/intermediate_code.c` (`newTemp` y `newLabel`).
  - **Problemas:**
    1. `newTemp()` devuelve un puntero a un buffer estático (`static char buffer[32]`). Múltiples llamadas concurrentes o anidadas sobrescriben el mismo espacio de memoria.
    2. `newLabel()` utiliza `strdup()`, reservando memoria dinámica en el heap que nunca es liberada (`free()`).
  - **Acción:** Diseñar un manejo seguro de memoria (ej. copiar cadenas en estructuras con tamaño fijo de buffer o un pool de asignación reutilizable). La migración a cuádruplas (sección 3.1) resuelve esto de raíz, porque los nombres pasan a vivir en campos de tamaño fijo dentro de cada instrucción.

---

## 2. Requisitos de la Especificación de C-TDS No Contemplados

### 2.1
- [x] **Evaluación de corto circuito en operadores lógicos (`&&`, `||`)**
  - **Referencia:** `documentacion/spec/01-TDS-spec-lenguaje.pdf` (Página 5, "Expresiones"):
    > *"Los operadores lógicos && y || deben ser evaluados usando evaluación de corto circuito. El segundo operador no es evaluado si el primer operador determina el valor de toda la expresión, es decir, si el resultado es false para && o true para ||."*
  - **Problema actual:** En `generateExpressionCode`, para `OP_AND` y `OP_OR` se evalúa incondicionalmente el operando derecho y se emite una única instrucción `TAC_AND` / `TAC_OR`. Si el lado derecho contiene llamadas a métodos con efectos colaterales o expresiones con guardas (ej. `n != 0 && 10 / n > 1`), la semántica del lenguaje se rompe.
  - **Acción:** Diseñar la evaluación booleana con control de flujo (saltos condicionales) para asegurar que el operando derecho solo se evalúe cuando sea estrictamente necesario. Se resuelve en una sola pasada con etiquetas, sin backpatching, porque el AST ya está completo.

  - **RESUELTO.** `generateShortCircuitCode()` en `src/intermediate_code.c`. Los dos operadores
    usan el mismo esquema, "resultado en un temporal con salto de salida":

    ```
      result = a && b              result = a || b
      --------------------         --------------------
      result = a                   result = a
      if result false goto L       if result true goto L
      result = b                   result = b
      L:                           L:
    ```

    Si el operando izquierdo ya decide el valor de la expresión, ese valor es justamente el
    que hay que devolver y ya quedó en `result`, así que el salto saltea por completo la
    evaluación del derecho. Si no lo decide, el resultado de la expresión es exactamente el
    del operando derecho. Una sola pasada, sin backpatching.

  - **Opcode nuevo:** se agregó `TAC_IF_TRUE` (`if <x> true goto <L>`). Se podría haber
    evitado negando la condición, pero habría costado una instrucción extra por cada `||` y
    en x86-64 los dos saltos mapean directo a `jz` / `jnz`.

  - **Decisión de diseño:** el resultado va siempre a un temporal nuevo, aunque el operando
    izquierdo ya sea un temporal reutilizable. Reutilizarlo ahorraría una copia, pero el
    operando izquierdo puede ser una variable del programa, y asignarle el valor del operando
    derecho la corrompería. La copia redundante (`t1 = t0`) es candidata directa para la
    propagación de copias en la etapa de optimización.

  - **Verificación:** se comprobó que el operando derecho queda **después** del salto
    condicional en `false && efecto()` (con `efecto()` modificando una global), en
    `n > 0 && 10 / n > 1` y en `n == 0 || 10 / n > 1`. También se probó el anidamiento
    (`a && b && c`, `a || b && c` respetando precedencia) y el uso dentro de condiciones de
    `if` y de `while`. Los programas del corpus que no usan operadores lógicos generan un TAC
    byte a byte idéntico al anterior.

  - **Pendiente menor:** `TAC_AND` y `TAC_OR` quedaron en el enum pero ya no los emite nadie.
    Se conservan por si el optimizador quiere plegar una expresión sin efectos colaterales a
    una única instrucción; si se decide que no, hay que borrarlos.

### 2.2
- [ ] **Retorno implícito en métodos `void` al alcanzar el fin del bloque**
  - **Referencia:** `documentacion/spec/01-TDS-spec-lenguaje.pdf` (Página 4, "Invocación y Retorno de Métodos"):
    > *"Un método que no tiene declarado un tipo de retorno (un método void) retorna con una sentencia return (sin expresión) o cuando el fin del método es alcanzado."*
  - **Problema actual:** Si una función `void` (como `main`) no incluye una sentencia `return;` explícita, el generador no emite ninguna instrucción `ret`. En la posterior generación de código máquina x86-64, la función no tendrá epílogo de retorno y caerá en las instrucciones de la función contigua (fall-through caótico).
  - **Acción:** Al finalizar la generación del cuerpo de un método `void`, verificar si la última sentencia emitida fue un retorno; si no lo fue, emitir `ret\n`. Para los métodos no-void no hace falta: el análisis semántico ya garantiza que todos sus caminos terminan en un `return`.

### 2.3
- [ ] **Diferenciación de variables por alcance (Shadowing y variables globales vs locales)**
  - **Problema:** En el TAC actual, las variables se emiten con su nombre de identificador crudo (`x = 5`). Si existe una variable global `x` y una función declara una variable local `x` (o en bloques anidados), en el código intermedio ambas reciben el mismo nombre.
  - **Impacto:** En las fases posteriores (generador x86-64 e Intérprete), no será posible distinguir si `x` corresponde a una posición en el segmento `.data`/`.bss` o a un desplazamiento en la pila local (`-8(%rbp)`).
  - **Acción:** Desambiguar las variables asociándolas a su identificador de símbolo único o prefijándolas con su ámbito (ej. `g_x`, `x_1`, `x_2`).
  - **Nota:** La infraestructura ya existe. `struct Symbol` tiene el campo `uid`, que `appendSymbol()` asigna a toda entidad declarada (un `uid` distinto por declaración, incluso si comparten nombre), y vale `0` para lo que no es una entidad declarada. Desde la migración de la sección 3.1, los operandos de tipo `OPERAND_VARIABLE` guardan el puntero al `Symbol`, así que el `uid` ya está disponible en el punto donde se imprime: alcanza con usarlo al emitir el nombre.

### 2.4
- [ ] **Conversión / Coerción de tipos entre `int` y `float`**
  - **Referencia:** Regla semántica 14 de la especificación: *"Se permiten coerciones o truncamientos entre int y float"*.
  - **Acción:** Analizar si el TAC requiere instrucciones explícitas de conversión de tipo (ej. `t0 = int_to_float x`) para facilitar la posterior traducción a instrucciones de punto flotante SSE/AVX en x86-64.
  - **Prioridad:** Baja. El enunciado del proyecto acota esta etapa a *"las operaciones con tipos enteros y lógicos y para las operaciones de control de flujo"*, así que el `float` en el TAC es una extensión nuestra.
  - **Menor asociado:** las constantes `float` se emiten con `%f` (`3.5` sale como `3.500000`). Si se mantiene `float`, hay que fijar el formato.

---

## 3. Decisiones de Diseño y Arquitectura para Próximas Etapas

### 3.1
- [x] **Migración de texto plano (`string_builder`) a Estructura de Datos de Instrucciones (Cuádruplas / TAC)**
  - **Contexto:** La cátedra exige implementar posteriormente:
    1. **Intérprete (10%)**: Ejecución directa del código intermedio.
    2. **Generador de Código Objeto x86-64 (20%)**: Traducción del TAC a ensamblador.
    3. **Optimizador (15%)**: Análisis y transformación del código intermedio (ej. propagación de constantes, eliminación de código muerto).
  - **Problema de la arquitectura actual:** Si el código intermedio solo existe como una cadena de texto formateada en `string_builder`, el Intérprete y el Generador x86-64 deberán volver a parsear texto con `sscanf` o expresiones regulares, lo cual duplica el trabajo y multiplica los errores.
  - **Propuesta de diseño:**
    Definir una lista enlazada o arreglo de instrucciones TAC estructuradas:
    ```c
    typedef enum {
        TAC_ADD, TAC_SUB, TAC_MUL, TAC_DIV, TAC_MOD,
        TAC_NEG, TAC_NOT,
        TAC_ASSIGN,
        TAC_LABEL,
        TAC_GOTO,
        TAC_IF_FALSE,
        TAC_PARAM,
        TAC_CALL,
        TAC_RETURN
    } TACOp;

    typedef struct TACInstruction {
        TACOp op;
        char result[32];
        char arg1[32];
        char arg2[32];
        struct TACInstruction *next;
    } TACInstruction;
    ```
    - **Ventajas:**
      - La visualización en texto se resuelve con un recorrido simple que llama a `buildStringf`.
      - El **Intérprete** puede evaluar las instrucciones directamente mediante un `switch (inst->op)`.
      - El **Optimizador** puede reescribir o eliminar nodos de la lista enlazada.
      - El **Generador x86-64** procesa cada cuádrupla de forma limpia sin parseo textual.

  - **RESUELTO.** La estructura vive en `src/tac.h` / `src/tac.c` y el recorrido del AST que la
    llena quedó en `src/intermediate_code.c`. `getIntermediateCode()` devuelve la lista;
    `printTACList()` es solo una de sus salidas posibles.

  - **Decisión de diseño: operandos tipados.** El borrador de arriba proponía
    `char result[32]; char arg1[32]; char arg2[32];`. Se implementó en cambio un `TACOperand`
    con la naturaleza del operando explícita:

    ```c
    typedef enum {
        OPERAND_NONE, OPERAND_TEMP, OPERAND_VARIABLE,
        OPERAND_CONSTANT, OPERAND_LABEL, OPERAND_METHOD
    } OperandKind;

    typedef struct {
        OperandKind kind;
        DataType type;
        char name[TAC_NAME_LENGTH];
        Symbol *symbol;                                  /* solo OPERAND_VARIABLE */
        union { int int_val; float float_val; } value;   /* solo OPERAND_CONSTANT */
    } TACOperand;
    ```

    *Motivo:* con cadenas planas, el generador x86-64 tendría que mirar `"5"` y deducir si es
    una constante o una variable con ese nombre, y mirar `"x"` sin poder saber si va al
    segmento `.data` o a un desplazamiento en la pila. Con `kind` eso es un `switch`. Además
    el campo `symbol` hace que el `uid` viaje dentro del operando, lo que resuelve la
    sección 2.3 sin volver a tocar la estructura. Costo: unas 40 líneas más.

  - **Verificación:** la migración es **equivalente en comportamiento**. El TAC generado es
    byte a byte idéntico al anterior en los 15 programas del corpus (se guardó la salida
    previa y se comparó con `diff`). Por eso se conservó una rareza del generador viejo: en
    una operación binaria el temporal del resultado se pide antes de generar el operando
    derecho, lo que produce `t0 = n + t2`. Es inofensivo; normalizarlo queda como cambio
    cosmético aparte.

  - **Estado de los opcodes:** `TAC_BEGIN_FUNC` ya se emite (imprime `nombre:`, igual que
    antes). `TAC_END_FUNC` existe en el enum pero todavía no se emite: lo cubre la sección 3.3.
    Los relacionales `TAC_LT`/`TAC_GT`/`TAC_EQ` están. `TAC_AND` y `TAC_OR` también, aunque
    el corto circuito de la sección 2.1 va a dejar de emitirlos. Faltan los de declaración de
    variables de la sección 3.4.

### 3.2
- [ ] **Los parámetros formales de un método no aparecen en el TAC**
  - **Ubicación:** `src/intermediate_code.c` (`generateStatementCode`, caso `METHOD_DECLARATION_NODE`).
  - **Problema:** Solo se emite la etiqueta del método y se genera el cuerpo (`node->children[1]`). El nodo de parámetros (`node->children[0]`) se ignora por completo, así que el TAC de `int duplicar(int n)` es simplemente `duplicar:` y no registra que existe un parámetro llamado `n`.
  - **Impacto:** El generador x86-64 necesita saber cuántos parámetros formales tiene el método y en qué orden, para asociarlos a `%rdi, %rsi, %rdx, %rcx, %r8, %r9` (convención System V AMD64) y al stack a partir del séptimo. Hoy esa información solo existe en el AST y en la tabla de símbolos, no en el código intermedio.
  - **Acción:** Emitir una cabecera por método del tipo `begin_func <nombre>, <cantidad_de_parametros>` y una instrucción por parámetro formal, o bien equivalente en la estructura de cuádruplas.
  - **Avance parcial:** el opcode `TAC_BEGIN_FUNC` ya existe y se emite, pero por ahora solo lleva el nombre del método y se imprime como `nombre:`. Falta agregarle la cantidad de parámetros y emitir los formales.

### 3.3
- [ ] **No existe marca de fin de método**
  - **Problema:** El final de un método se infiere por la aparición de la siguiente etiqueta en el texto. Eso obliga a cualquier consumidor a mirar hacia adelante para saber dónde termina una función.
  - **Impacto:** El generador x86-64 necesita el punto exacto donde emitir el epílogo (`leave` / `ret`), y el Intérprete necesita saber dónde cortar la ejecución de un cuerpo.
  - **Acción:** Emitir `end_func <nombre>` al terminar cada método. Va junto con la sección 3.2.
  - **Avance parcial:** el opcode `TAC_END_FUNC` ya está definido en `src/tac.h` y contemplado en la impresión, pero todavía no lo emite nadie.

### 3.4
- [ ] **Las declaraciones de variables no emiten ninguna instrucción**
  - **Problema:** `VARIABLE_DECLARATION_NODE` cae al `default` de `generateStatementCode`, que recorre sus hijos (no tiene), de modo que no emite nada. Ni las variables globales ni las locales aparecen en el TAC.
  - **Impacto:** El generador x86-64 necesita el listado de variables globales para emitir el segmento `.data`/`.bss`, y el de locales (más los temporales usados) para calcular el tamaño del stack frame de cada función.
  - **Acción:** Decidir entre dos alternativas y documentarla:
    1. Que el TAC lleve instrucciones de declaración (`global x_1, int` / `local y_4, int`).
    2. Que el generador de código objeto consulte la tabla de símbolos.
  - **Relacionado:** ver la sección 3.5, porque la alternativa 2 hoy no es posible.

### 3.5
- [ ] **El árbol de scopes se descarta después del análisis semántico**
  - **Problema:** `analyzeSemantics()` devuelve el `Scope` global, pero `main()` en `src/bison.y` ignora el valor de retorno. Una vez terminado el análisis, ninguna estructura viva contiene la tabla de símbolos.
  - **Impacto:** Si el generador de código objeto va a necesitar los scopes (para distinguir global de local, para calcular offsets, o para la alternativa 2 de la sección 3.4), hoy no los tiene.
  - **Acción:** Decidir si se conserva el `Scope` global en una variable accesible por las etapas siguientes, o si el TAC se autocontiene con toda la información necesaria. Condiciona las secciones 2.3, 3.2 y 3.4.

### 3.6
- [ ] **Los contadores de temporales son globales a todo el programa**
  - **Problema:** `resetTempAndLabelCounters()` se invoca una sola vez en `generateIntermediateCode()`, así que la numeración `t0..tN` y `L0..LN` es continua a lo largo de todos los métodos.
  - **Impacto:** No es incorrecto, pero para dimensionar el stack frame de cada función hay que saber cuántos temporales usa *esa* función, lo que obliga a recorrer su rango de instrucciones.
  - **Acción:** Decidir si los temporales se reinician por método (lo que simplifica el cálculo del frame) o si se lleva la cuenta por función de otra forma. Las etiquetas conviene que sigan siendo únicas globalmente, para no colisionar entre funciones.

### 3.7
- [ ] **El Intérprete no está planificado y vale más que esta etapa**
  - **Contexto:** En la tabla de calificación de `documentacion/spec/00-TDS-proyecto.pdf` el **Intérprete vale 10%**, mientras que el **Generador de Código Intermedio vale 5%**. Sin embargo, el Intérprete no figura en el cronograma de entregas.
  - **Acción:** Consultar a la cátedra si corresponde implementarlo, con qué alcance y para qué fecha. Si la respuesta es que sí, es un argumento más para ejecutar la migración de la sección 3.1 cuanto antes, porque el Intérprete se implementa como un `switch` sobre las cuádruplas.

---

## 4. Interfaz de Línea de Comandos (CLI) y Especificación del Compilador

- [ ] **Soporte de argumentos y modos de salida según Tabla 1 de `00-TDS-proyecto.pdf`**
  - **Requisitos de la cátedra:**
    - Sintaxis: `c-tds [opcion] nombreArchivo.ctds`
    - Opción `-target <etapa>`: con etapas `scan`, `parse`, `codinter`, `assembly`.
    - Opción `-o <salida>`: renombra el archivo generado.
    - Opción `-debug`: imprime información de depuración.
    - **Comportamiento por defecto:** Si no se especifica `-debug`, cuando la compilación es exitosa **no debe imprimirse nada por salida estándar**; la salida debe escribirse en un archivo con la extensión correspondiente a la etapa (`.ci` para código intermedio).
  - **Problema actual:** `src/bison.y` imprime encabezados y trazas fijas por `stdout` de forma incondicional (`printf("--- Analisis sintactico ...")`), y no procesa los argumentos de línea de comandos requeridos. Los ejemplos de `examples/` usan extensión `.txt` en lugar de `.ctds`.
  - **Ambigüedad a resolver:** el enunciado dice que por defecto se compila *"hasta la etapa corriente"* generando un archivo con la extensión de esa etapa, y también que sin `-target` se produce el ejecutable `nombreArchivo.out`. Mientras el compilador esté incompleto ambas cosas no pueden valer a la vez; hay que elegir una y documentarla.
  - **Acción:** Adecuar la función `main()` de `bison.y` para respetar los flags y generar el archivo `.ci`.

---

## 5. Pruebas Unitarias y Automatización

- [ ] **Crear suite de pruebas unitarias `tests/test_intermediate_code.c`:**
  - Pruebas de expresiones aritméticas compuestas y generación correcta de temporales.
  - Pruebas de expresiones lógicas con corto circuito (`&&`, `||`): verificar que el operando derecho no se evalúa cuando el izquierdo ya determina el resultado.
  - Pruebas de llamadas a métodos como expresiones (validar asignación de temporales destino).
  - Pruebas de llamadas a métodos como sentencias (validar emisión de `param` y `call`).
  - Pruebas de control de flujo (`if-else` con y sin rama `else`, ciclos `while`): verificar que toda etiqueta referenciada por un salto esté definida.
  - Pruebas de métodos `void` con y sin `return`.
  - Pruebas de ocultamiento de scopes: dos variables con el mismo nombre en ámbitos distintos deben recibir nombres distintos en el TAC.
- [ ] **Integrar en `test_suite.sh`:**
  - Compilación y ejecución automática de las pruebas de código intermedio junto con las de lexer y parser.
