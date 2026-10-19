# Checklist de Investigación y Pendientes: Generación de Código Intermedio (TAC)

Este documento detalla el estado actual, los errores detectados en la implementación (`src/intermediate_code.c` y `src/intermediate_code.h`), las discrepancias con la especificación oficial de **C-TDS** (`documentacion/spec/01-TDS-spec-lenguaje.pdf`) y las decisiones de diseño necesarias para las siguientes etapas del compilador (Intérprete, Optimizador y Generador x86-64 según `documentacion/spec/00-TDS-proyecto.pdf`).

---

## 1. Errores Críticos e Inmediatos (Bugs en la Generación de TAC)

- [ ] **Llamadas a funciones en expresiones generan código incompleto / nulo**
  - **Ubicación:** `src/intermediate_code.c` (`generateExpressionCode`, caso `METHOD_CALL_NODE`).
  - **Problema:** Se emite `buildStringf("%s = call %s, %d\n", result.place, ...)`, pero `result.place` nunca se inicializa con un nuevo temporal (`newTemp()`). Queda como cadena vacía `""`.
  - **Efecto:** Se emiten instrucciones rotas como ` = call factorial, 1` y las expresiones que usan el resultado quedan con operandos truncados (ej. `t4 = n * `, `promedio = `).
  - **Acción:** Asignar `strcpy(result.place, newTemp());` antes de emitir la instrucción `call`.

- [ ] **Llamadas a funciones como sentencias son ignoradas completamente**
  - **Ubicación:** `src/intermediate_code.c` (`generateStatementCode`).
  - **Problema:** No existe `case METHOD_CALL_NODE:`. Cae al bloque `default:`, que itera sobre los hijos llamando a `generateStatementCode` sobre expresiones (argumentos), por lo que no emite ni los parámetros ni la instrucción `call`.
  - **Efecto:** Invocaciones independientes como `reiniciar();` o `factorial(a);` desaparecen silenciosamente del código intermedio generado.
  - **Acción:** Implementar `case METHOD_CALL_NODE:` en `generateStatementCode` que evalúe los argumentos con `generateExpressionCode`, emita `param ...` y luego emita `call nombre, N` (sin asignar a temporal).

- [ ] **Falta de salto de línea en retorno `void` (`ret`)**
  - **Ubicación:** `src/intermediate_code.c` (`generateStatementCode`, caso `RETURN_NODE`).
  - **Problema:** En retornos sin expresión (`GET_EXPRESSION(node) == NULL`), se emite `buildStringf("ret");` sin el carácter `\n`.
  - **Efecto:** La instrucción `ret` se concatena directamente con la etiqueta del siguiente método (ej. `retfactorial:`).
  - **Acción:** Cambiar por `buildStringf("ret\n");`.

- [ ] **Lógica de etiquetas y saltos rotos en condicionales `if-else`**
  - **Ubicación:** `src/intermediate_code.c` (`generateStatementCode`, caso `IF_ELSE_NODE`).
  - **Problemas detectados:**
    1. Si no hay bloque `else` (`GET_ELSE_BLOCK(node) == NULL`), el salto condicional es `if <cond> false goto elseIf`, pero la etiqueta `elseIf:` nunca se imprime en el código porque está dentro del `if (GET_ELSE_BLOCK != NULL)`. El salto apunta a una etiqueta inexistente.
    2. Se emite una etiqueta inicial `startIf:` previa a la condición a la cual ninguna instrucción salta (código muerto innecesario).
    3. Si hay bloque `else`, se emite `goto endIf` al final de la rama `else`, inmediatamente seguido de `endIf:`, produciendo un salto redundante a la siguiente línea.
  - **Acción:**
    - Si no hay bloque `else`: emitir `if <cond> false goto endIf`, generar el bloque `then`, y luego emitir `endIf:`.
    - Si hay bloque `else`: emitir `if <cond> false goto elseIf`, generar bloque `then`, emitir `goto endIf`, emitir `elseIf:`, generar bloque `else`, y emitir `endIf:`.
    - Eliminar la etiqueta no referenciada `startIf`.

- [ ] **Fragilidad en la gestión de temporales y fuga de memoria en etiquetas**
  - **Ubicación:** `src/intermediate_code.c` (`newTemp` y `newLabel`).
  - **Problemas:**
    1. `newTemp()` devuelve un puntero a un buffer estático (`static char buffer[32]`). Múltiples llamadas concurrentes o anidadas sobrescriben el mismo espacio de memoria.
    2. `newLabel()` utiliza `strdup()`, reservando memoria dinámica en el heap que nunca es liberada (`free()`).
  - **Acción:** Diseñar un manejo seguro de memoria (ej. copiar cadenas en estructuras con tamaño fijo de buffer o un pool de asignación reutilizable).

---

## 2. Requisitos de la Especificación de C-TDS No Contemplados

- [ ] **Evaluación de corto circuito en operadores lógicos (`&&`, `||`)**
  - **Referencia:** `documentacion/spec/01-TDS-spec-lenguaje.pdf` (Página 5, "Expresiones"):
    > *"Los operadores lógicos && y || deben ser evaluados usando evaluación de corto circuito. El segundo operador no es evaluado si el primer operador determina el valor de toda la expresión, es decir, si el resultado es false para && o true para ||."*
  - **Problema actual:** En `generateExpressionCode`, para `OP_AND` y `OP_OR` se evalúa incondicionalmente el operando derecho antes de emitir la operación binaria. Si el lado derecho contiene llamadas a métodos con efectos colaterales o expresiones con guardas (ej. `n != 0 && 10 / n > 1`), la semántica del lenguaje se rompe.
  - **Acción:** Diseñar la evaluación booleana con control de flujo (saltos condicionales) para asegurar que el operando derecho solo se evalúe cuando sea estrictamente necesario.

- [ ] **Retorno implícito en métodos `void` al alcanzar el fin del bloque**
  - **Referencia:** `documentacion/spec/01-TDS-spec-lenguaje.pdf` (Página 4, "Invocación y Retorno de Métodos"):
    > *"Un método que no tiene declarado un tipo de retorno (un método void) retorna con una sentencia return (sin expresión) o cuando el fin del método es alcanzado."*
  - **Problema actual:** Si una función `void` (como `main`) no incluye una sentencia `return;` explícita, el generador no emite ninguna instrucción `ret`. En la posterior generación de código máquina x86-64, la función no tendrá epílogo de retorno y caerá en las instrucciones de la función contigua (fall-through caótico).
  - **Acción:** Al finalizar la generación del cuerpo de un método `void`, verificar si la última sentencia emitida fue un retorno; si no lo fue, emitir `ret\n`.

- [ ] **Diferenciación de variables por alcance (Shadowing y variables globales vs locales)**
  - **Problema:** En el TAC actual, las variables se emiten con su nombre de identificador crudo (`x = 5`). Si existe una variable global `x` y una función declara una variable local `x` (o en bloques anidados), en el código intermedio ambas reciben el mismo nombre.
  - **Impacto:** En las fases posteriores (generador x86-64 e Intérprete), no será posible distinguir si `x` corresponde a una posición en el segmento `.data`/`.bss` o a un desplazamiento en la pila local (`-8(%rbp)`).
  - **Acción:** Desambiguar las variables asociándolas a su identificador de símbolo único o prefijándolas con su ámbito (ej. `g_x`, `x_1`, `x_2`).

- [ ] **Conversión / Coerción de tipos entre `int` y `float`**
  - **Referencia:** Regla semántica 14 de la especificación: *"Se permiten coerciones o truncamientos entre int y float"*.
  - **Acción:** Analizar si el TAC requiere instrucciones explícitas de conversión de tipo (ej. `t0 = int_to_float x`) para facilitar la posterior traducción a instrucciones de punto flotante SSE/AVX en x86-64.

---

## 3. Decisiones de Diseño y Arquitectura para Próximas Etapas

- [ ] **Migración de texto plano (`string_builder`) a Estructura de Datos de Instrucciones (Cuádruplas / TAC)**
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

---

## 4. Interfaz de Línea de Comandos (CLI) y Especificación del Compilador

- [ ] **Soporte de argumentos y modos de salida según Tabla 1 de `00-TDS-proyecto.pdf`**
  - **Requisitos de la cátedra:**
    - Sintaxis: `c-tds [opcion] nombreArchivo.ctds`
    - Opción `-target <etapa>`: con etapas `scan`, `parse`, `codinter`, `assembly`.
    - Opción `-o <salida>`: renombra el archivo generado.
    - Opción `-debug`: imprime información de depuración.
    - **Comportamiento por defecto:** Si no se especifica `-debug`, cuando la compilación es exitosa **no debe imprimirse nada por salida estándar**; la salida debe escribirse en un archivo con la extensión correspondiente a la etapa (`.ci` para código intermedio).
  - **Problema actual:** `src/bison.y` imprime encabezados y trazas fijas por `stdout` de forma incondicional (`printf("--- Analisis sintactico ...")`), y no procesa los argumentos de línea de comandos requeridos.
  - **Acción:** Adecuar la función `main()` de `bison.y` para respetar los flags y generar el archivo `.ci`.

---

## 5. Pruebas Unitarias y Automatización

- [ ] **Crear suite de pruebas unitarias `tests/test_intermediate_code.c`:**
  - Pruebas de expresiones aritméticas compuestas y generación correcta de temporales.
  - Pruebas de expresiones lógicas con corto circuito (`&&`, `||`).
  - Pruebas de llamadas a métodos como expresiones (validar asignación de temporales destino).
  - Pruebas de llamadas a métodos como sentencias (validar emisión de `param` y `call`).
  - Pruebas de control de flujo (`if-else` con y sin rama `else`, ciclos `while`).
  - Pruebas de métodos `void` con y sin `return`.
- [ ] **Integrar en `test_suite.sh`:**
  - Compilación y ejecución automática de las pruebas de código intermedio junto con las de lexer y parser.
