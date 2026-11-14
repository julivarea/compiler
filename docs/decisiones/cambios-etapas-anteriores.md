# Cambios a Etapas Anteriores

Este documento reúne las modificaciones realizadas sobre fases ya entregadas del compilador durante el desarrollo de etapas posteriores, detallando qué se modificó en cada caso y qué requerimiento técnico motivó el cambio.

La mayoría de las correcciones sobre el front-end surgieron al conectar el back-end: condiciones que pasaban inadvertidas cuando el compilador solo construía el árbol o validaba reglas aisladas quedaron expuestas al generar código intermedio, optimizar o emitir assembly.

---

## Resumen

| Etapa modificada | Cambios | Cuándo se introdujeron |
| :--- | :---: | :--- |
| Léxico y sintáctico (entregada 23/09) | 4 | Durante código intermedio, optimizador y entrega final |
| AST y tabla de símbolos (entregada 30/09) | 4 | Durante el optimizador |
| Análisis semántico (entregado 07/10) | 3 | Durante código objeto y optimizador |
| Código objeto (entregado 26/10) | 2 | Durante el optimizador |
| Transversal a cuatro etapas (§5) | 1 | Recorte de alcance en la entrega final |

---

## 1. Léxico y sintáctico

### 1.1 Contabilización de errores léxicos para abortar el pipeline

**Cambio realizado:** `lexer.l` pasó a reportar los caracteres inválidos a través de `error_handler`, incrementando un contador consultado por el driver antes de avanzar a las etapas siguientes.

**Motivación:** inicialmente un error léxico imprimía un mensaje por consola pero no detenía la compilación. Al incorporar la generación de TAC, un archivo con errores léxicos continuaba ejecutando el pipeline hasta emitir código. Esto motivó además la separación explícita entre `lexical_errors`, `syntax_errors` y `semantic_errors` descrita en la [documentación de manejo de errores](../wiki/02-infraestructura/03-errores-y-debug.md).

### 1.2 Detección de comentarios multilínea sin cerrar al llegar a `EOF`

**Cambio realizado:** la regla `<<EOF>>` dentro de la condición de arranque exclusiva `%x COMMENT` reporta un error léxico si el archivo termina antes de encontrar `*/`.

**Motivación:** cuando un comentario de bloque quedaba abierto al final del archivo, el analizador léxico finalizaba sin reportar error y el parser recibía un flujo de tokens truncado.

### 1.3 Recursión a izquierda en producciones de listas

**Cambio realizado:** todas las reglas de listas de la gramática pasaron de recursión a derecha a recursión a izquierda, y se eliminó la función auxiliar `prependNode`.

**Motivación:** en un analizador ascendente LALR(1) como Bison, la recursión a derecha obliga a apilar todos los elementos de la lista antes de ejecutar la primera reducción, haciendo crecer la pila de estados proporcionalmente al tamaño de la entrada. Con recursión a izquierda cada elemento se reduce de inmediato en espacio constante y la lista se construye en orden natural.

### 1.4 Extracción del no terminal `Assignment`

**Cambio realizado:** la regla de asignación se separó de `Statement` en un no terminal propio:

```text
Assignment : ID '=' Expression
Statement  : Assignment ';' | ...
```

**Motivación:** contar con una producción de asignación sin el `;` delimitador facilita extender la gramática con estructuras como `for (init; cond; step)`, donde el paso de actualización es una asignación sin punto y coma final (ver [análisis sintáctico](../wiki/03-lexico-sintactico-y-ast/02-analisis-sintactico.md)).

---

## 2. AST y tabla de símbolos

### 2.1 Renombrado de `BINARYOPERATOR_NODE` a `OPERATOR_NODE`

**Cambio realizado:** se unificaron los nombres de tipos y constructores de operadores y se factorizaron las macros de acceso a hijos:

| Antes | Ahora |
| :--- | :--- |
| `BINARYOPERATOR_NODE` | `OPERATOR_NODE` |
| `newBinaryOperatorNode` | `newOperatorNode` |
| `OP_NEGATIVE`, `OP_NEGATION` | `OP_UMINUS`, `OP_NOT` |
| Siete macros `GET_*` con validación de rango duplicada | Las mismas siete definidas sobre `GET_CHILD(node, i)` |

**Motivación:** ese mismo tipo de nodo representaba también a los operadores unarios (`-x`, `!b`), que tienen un único hijo, por lo que el nombre `BINARYOPERATOR_NODE` resultaba impreciso al leer los generadores posteriores. Asimismo, `OP_UMINUS` y `OP_NOT` distinguen con claridad la negación aritmética de la lógica.

### 2.2 Transferencia de propiedad del identificador en `newSymbol`

**Cambio realizado:** `newSymbol` dejó de invocar `strdup` sobre el parámetro `char *id` y pasó a tomar posesión directa del puntero recibido.

**Motivación:** el analizador sintáctico ya reservaba memoria para el lexema con `strdup`, por lo que volver a duplicarlo dentro de la tabla de símbolos provocaba una copia redundante cuya primera reserva quedaba sin liberar.

### 2.3 Incorporación y posterior remoción de constructores `make*Symbol` por valor

**Cambio realizado:** se añadieron en `symbol_table` los constructores `makeIntSymbol`, `makeFloatSymbol`, `makeBoolSymbol` y `makeVoidSymbol` (que retornaban estructuras `Symbol` por valor) y posteriormente se eliminaron.

**Motivación:** fueron creados para representar valores temporales durante la evaluación en el intérprete del AST. Al retirar el intérprete del alcance del proyecto (§5), las cuatro funciones quedaron sin referencias en el resto de los módulos y se suprimieron.

### 2.4 Incorporación y posterior remoción del campo `hasExecute` en `NodeAST`

**Cambio realizado:** se agregó el campo `bool hasExecute` en `NodeAST` al comenzar la etapa de optimización y se eliminó una vez completadas las pasadas.

**Motivación:** el diseño preliminar preveía marcar con `hasExecute = false` los subárboles del AST descartados por optimizaciones (como ramas de condición constante o sentencias posteriores a un `return`) para que el generador de TAC los omitiera sin necesidad de desenlazar nodos del árbol. Sin embargo, todas las optimizaciones de flujo y constantes se implementaron finalmente sobre la lista lineal de cuádruplas TAC (ver [optimizador](../wiki/06-optimizador/02-optimizador.md)), dejando el campo sin uso.

---

## 3. Análisis semántico

### 3.1 Rechazo del nombre de un método utilizado como variable

**Cambio realizado:** el chequeador de tipos rechaza toda expresión donde un identificador declarado como método (`SYMBOL_METHOD`) se utilice fuera de una invocación (por ejemplo, `x = f;`).

**Motivación:** previamente, al buscar `f` en la tabla de símbolos se recuperaba la entrada del método y se tomaba su tipo de retorno como si fuera el tipo de una variable. El programa superaba el análisis semántico y fallaba recién en el generador de código objeto al buscar una ranura de memoria para `f`.

### 3.2 Supresión de errores de tipos en cascada

**Cambio realizado:** cuando un operando de una expresión ya tiene tipo `TYPE_ERROR`, el chequeador propaga `TYPE_ERROR` hacia el nodo padre sin emitir un nuevo mensaje de error.

**Motivación:** un único identificador no declarado o incompatible dentro de una expresión compuesta disparaba múltiples errores secundarios en cada operador ascendente. La propagación silenciosa de `TYPE_ERROR` asegura que cada error primario produzca un solo diagnóstico (ver [chequeo de tipos](../wiki/04-semantico/02-chequeo-de-tipos.md)).

### 3.3 Eliminación de validaciones duplicadas entre módulos semánticos

**Cambio realizado:** se removieron de `semantic_analyzer.c` las comprobaciones de tipos que ya ejecutaba `type_checker.c`.

**Motivación:** mantener la misma validación en ambos archivos duplicaba la lógica y podía generar mensajes repetidos o inconsistentes. La división de responsabilidades quedó fijada en [chequeo de tipos](../wiki/04-semantico/02-chequeo-de-tipos.md) y [análisis semántico](../wiki/04-semantico/03-analisis-semantico.md): `semantic_analyzer.c` administra los ámbitos, las declaraciones y las reglas estructurales (`main`, `extern`, caminos de `return`), mientras que `type_checker.c` resuelve la compatibilidad de tipos en expresiones y llamadas.

---

## 4. Código objeto

### 4.1 Eliminación del campo obsoleto `paramCount` en `FunctionFrame`

**Cambio realizado:** se quitó el campo `paramCount` de `FunctionFrame` junto con funciones auxiliares de consulta de registros que habían quedado en desuso, conservando `integerParamCount` y `stackParamCount`.

**Motivación:** la convención de llamada requiere distinguir cuántos parámetros formales se recibieron en los primeros seis registros (`integerParamCount`) y cuántos ingresaron por el marco del llamador (`stackParamCount`), haciendo redundante un contador único.

### 4.2 Modularización de `object_code.c` por opcode

**Cambio realizado:** el bloque `switch` principal de `generateInstruction` en `object_code.c` se descompuso en funciones específicas por familia de opcodes (`emitBinaryOp`, `emitDivisionOp`, `emitRelationalOp`, `emitCall`, etc.).

**Motivación:** extraer cada traducción a una función dedicada redujo la complejidad de `generateInstruction`, facilitó la incorporación de la reducción de fuerza y la fusión de saltos condicionales, y clarificó la estructura documentada en [código objeto](../wiki/07-codigo-objeto/02-generacion-codigo-objeto.md).

---

## 5. Recorte de alcance: eliminación del intérprete y de `float` en el back-end

Para ajustar el compilador estrictamente a los requerimientos del enunciado —que acota la generación de código a tipos enteros y lógicos y no solicita un intérprete—, se realizó una limpieza transversal sobre cuatro etapas:

| Componente eliminado | Archivos afectados |
| :--- | :--- |
| Intérprete del AST | `interpreter.c/h`, suite de pruebas y opción `-target interp` |
| Constructores `make*Symbol` | `symbol_table.c/h` (§2.3) |
| Emisión x86-64 de punto flotante | `object_code.c`: registros `%xmm`, instrucciones `movss`/`comiss` y sección `.rodata` |
| Coerciones implícitas `int` → `float` | `intermediate_code.c`: `coerceToFloat`, `coerceBinaryOperands` y `TAC_INT_TO_FLOAT` |
| Plegado de constantes `float` | `optimizer.c`: `asFloat` y ramas para `TYPE_FLOAT` |
| Operando constante `float` en TAC | `tac.h/c`: `tacConstantFloat` y campo `floatValue` |
| Contador `floatParamCount` | `function_frame.h` |

### Rechazo explícito de `float` al ingresar al generador de TAC

Al retirar las instrucciones SSE del generador de código objeto, fue necesario impedir que un programa con variables o literales `float` continuara hacia el back-end y fuera traducido erróneamente con instrucciones enteras (`movl`, `addl`).

Al mismo tiempo, `float` no podía eliminarse del front-end (léxico, sintáctico, AST y chequeo de tipos) porque forma parte de la especificación de C-TDS y de las reglas evaluadas en la etapa semántica. La frontera se estableció al inicio de `generateIntermediateCode` (ver [generación de TAC](../wiki/05-codigo-intermedio/02-generacion-tac.md)): el front-end analiza y valida `float` de acuerdo con la gramática y el sistema de tipos, y el generador de código intermedio rechaza con un error explícito cualquier programa que intente generar código para ese tipo.

---

## 6. Aspectos que permanecieron estables

- **Especificación sintáctica:** salvo la factorización interna del no terminal `Assignment` (§1.4) y el paso a recursión a izquierda (§1.3) —que reconocen exactamente el mismo lenguaje—, la gramática no sufrió cambios.
- **Reglas semánticas:** las catorce reglas definidas para el analizador semántico se mantuvieron sin cambios de criterio; los ajustes de §3 corrigieron omisiones de implementación.
- **Representación de cuádruplas TAC:** la estructura `TACInstruction` y su catálogo de opcodes permanecieron estables entre el generador de código intermedio, el optimizador, el generador de código objeto y las suites de pruebas.
