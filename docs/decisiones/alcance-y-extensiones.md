# Alcance, Aclaraciones y Extensiones sobre el Enunciado

Registro de las decisiones de diseño conscientes, correcciones de erratas del enunciado y extensiones implementadas respecto de la **Especificación del Lenguaje** (`docs/spec/01-TDS-spec-lenguaje.pdf`) y la **Descripción del Proyecto** (`docs/spec/00-TDS-proyecto.pdf`).

---

## 1. Alcance de `float`

La especificación del lenguaje incluye `float` entre los tres tipos básicos de C-TDS (`int`, `float`, `boolean`) y define reglas semánticas sobre él (Reglas 10, 11 y 14). Por su parte, el enunciado del proyecto (`00-TDS-proyecto.pdf`, pág. 2) acota las etapas de **generación de código intermedio y generación de código objeto** exclusivamente a **tipos enteros y lógicos**, por lo que no es necesario soportar `float` en el back-end.

El compilador implementa esa separación exacta entre front-end y back-end:

| Etapa | Soporte de `float` |
| --- | --- |
| [Análisis léxico](../wiki/03-lexico-sintactico-y-ast/01-analisis-lexico.md), [sintáctico](../wiki/03-lexico-sintactico-y-ast/02-analisis-sintactico.md), [AST](../wiki/03-lexico-sintactico-y-ast/04-ast.md) y [tabla de símbolos](../wiki/04-semantico/01-tabla-de-simbolos.md) | **Completo.** Se escanean literales reales, se construyen nodos tipados y se registran variables, parámetros y métodos `float`. |
| [Chequeo de tipos](../wiki/04-semantico/02-chequeo-de-tipos.md) y [análisis semántico](../wiki/04-semantico/03-analisis-semantico.md) | **Completo.** Se validan todas las expresiones, asignaciones, pasajes de parámetros y retornos que involucran `float`. |
| [Código intermedio](../wiki/05-codigo-intermedio/02-generacion-tac.md), [optimizador](../wiki/06-optimizador/02-optimizador.md) y [código objeto](../wiki/07-codigo-objeto/02-generacion-codigo-objeto.md) | **Fuera de alcance según el enunciado.** Al iniciar la generación de TAC se rechaza explícitamente cualquier programa que contenga nodos o símbolos `float`, informando la línea correspondiente. |

```text
$ ./mi_compilador examples/error_float_en_codegen.ctds
Error en la linea 2: la generacion de codigo para el tipo 'float' no esta
soportada: esta etapa cubre tipos enteros y logicos.
```

### Resolución de contradicciones de la especificación sobre `float` en el front-end

La especificación del lenguaje presenta algunas tensiones internas entre las páginas 4, 5 y 6 que se resolvieron mediante las siguientes decisiones en `src/semantic/type_checker.c`:

1. **Asignaciones a variables `float`:** En la pág. 4 (*Asignaciones*) el texto menciona únicamente variables `int` y `boolean`, pero `float` es un tipo básico (pág. 3) y la Regla 14 carecería de efecto si no pudiera asignarse. Se admiten asignaciones, parámetros y retornos de tipo `float`.
2. **Coerción `int` → `float` permitida; truncamiento `float` → `int` rechazado (Reglas 7, 13 y 14):** Mientras las Reglas 7 y 13 piden tipos iguales en `return` y `=`, la Regla 14 indica *"Se permiten coerciones o truncamientos entre `int` y `float`"*. En un lenguaje sin sintaxis de *cast* explícito, se optó por permitir la promoción implícita sin pérdida (`int` → `float`) en operaciones aritméticas (`+`, `-`, `*`, `/`), relacionales (`<`, `>`), asignaciones, argumentos y `return`, pero **rechazar el truncamiento implícito (`float` → `int`)** para no ocultar pérdidas de parte fraccionaria.
3. **Operador resto (`%`) exclusivo para `int` (Regla 10 vs. pág. 5):** Aunque la Regla 10 agrupa todos los `<arith_op>` para `int` o `float`, se priorizó la definición de la pág. 5 (*"`%` computa el resto de una división de números enteros"*), rechazando `%` con operandos `float`.
4. **Igualdad (`==`) entre operandos del mismo tipo (Regla 11):** Siguiendo literalmente la Regla 11, `==` exige que ambos operandos tengan exactamente el mismo tipo básico (`int == int`, `float == float` o `boolean == boolean`), sin promoción implícita entre `int` y `float`.

---

## 2. Ausencia de la etapa de Intérprete

En la tabla de calificación del enunciado (`00-TDS-proyecto.pdf`, pág. 3) figura el ítem *Intérprete (10%)*, el cual no aparece en el cronograma de entregas ni en la descripción de las etapas (págs. 1–2).

Se consultó con el profesor de la cátedra si correspondía implementar el intérprete y **confirmó que no era necesario**. En consecuencia, el pipeline compila directamente desde el código intermedio (TAC) hacia el optimizador y el generador de código objeto x86-64.

---

## 3. Correcciones respecto del enunciado (especificación del lenguaje)

1. **Paréntesis obligatorios en la condición de `while` (`01-TDS-spec-lenguaje.pdf`, pág. 2):**
   En la gramática de la especificación, la producción de `if` exige paréntesis (`if ( <expr> ) <block>`), mientras que la de `while` los omite por errata (`while <expr> <block>`). En `src/parser/bison.y` se corrigió la regla exigiendo paréntesis también en `while`:
   ```bison
   WHILE '(' Expression ')' Block
   ```
2. **Mención a operadores de incremento y decremento (`01-TDS-spec-lenguaje.pdf`, pág. 4):**
   Bajo la sección *Asignaciones* (pág. 4), el texto menciona *"Las asignaciones de incremento y decremento únicamente son permitidas para tipos numéricos"*. Sin embargo, la gramática formal (págs. 2–3) no define `+=`, `-=`, `++` ni `--`, sino únicamente la asignación simple `<id> = <expr> ;`. Se tomó la gramática formal como referencia normativa y no se incluyeron operadores de incremento/decremento.
3. **Programa de ejemplo con `return 1;` dentro de `void main()` (`01-TDS-spec-lenguaje.pdf`, pág. 3):**
   El código de ejemplo de la pág. 3 declara `void main()` pero ejecuta `return 1;` en su cuerpo, lo cual contradice la Regla 6 de la misma especificación (pág. 6: *"si el método no retorna un valor (es un método `void`) entonces la sentencia `return` no puede tener asociada ninguna expresión"*). El analizador semántico hace cumplir la Regla 6 y rechaza el retorno con valor en métodos `void`.

---

## 4. Decisiones semánticas y de diseño adicionales

### 4.1 Declaraciones globales y de métodos intercaladas

La gramática original (`<program> -> <var_decl>* <method_decl>*`) obligaba a declarar todas las variables globales antes del primer método y provocaba un conflicto *shift/reduce* en un parser LALR(1) al observar `Type ID` con un único token de *lookahead*.

Se unificaron las declaraciones de nivel superior en `Declarations` ([Análisis Sintáctico](../wiki/03-lexico-sintactico-y-ast/02-analisis-sintactico.md) y [Conflicto Shift/Reduce en Bison](explicacion_conflicto_bison.md)), permitiendo intercalar variables globales y métodos en cualquier orden. El alcance global se preserva en `Symbol.isGlobal` ([ADR 002](adr-002-resolucion-variables-globales-isglobal.md)), manteniendo compatibilidad total con todos los programas válidos bajo la gramática original.

### 4.2 Firma de `main()` y código de salida en `void main()`

- **En análisis semántico (`src/semantic/semantic_analyzer.c`):** La Regla 3 exige que exista un método `main` sin parámetros, sin restringir su tipo de retorno. `checkMainMethodExists` verifica que `main` esté declarado en el ámbito global, sea un método (`METHOD_SYMBOL`) y tenga `0` parámetros, admitiendo `void` o cualquier tipo de retorno válido.
- **En código objeto (`src/object_code/object_code.c`):** Cuando `main` se declara `void`, el lenguaje no retorna un entero explícito, pero el sistema operativo lee `%eax` al finalizar el proceso como código de salida (*exit status*). Para evitar códigos de salida arbitrarios por basura residual en `%eax`, `emitEpilogue` emite `movl $0, %eax` antes de `leave` / `ret` cuando la función es `void main()`.

### 4.3 Recursión directa y ocultamiento (*shadowing*) entre variables y métodos

Siguiendo las reglas de alcance y visibilidad (`01-TDS-spec-lenguaje.pdf`, págs. 3–4):

- **Recursión directa habilitada:** En `analyzeMethodNode`, el símbolo del método se inserta en el ámbito global antes de crear `methodScope` y analizar su cuerpo. Esto permite que un método se invoque recursivamente a sí mismo dentro de su propio cuerpo, manteniendo la restricción de que no puede invocar métodos declarados textualmente más abajo (ver [Problemas Conocidos](../problemas-conocidos.md#1-ausencia-de-declaracion-adelantada-y-recursion-mutua-entre-metodos)).
- **Ocultamiento de métodos por variables locales o parámetros:** La especificación establece que *"una variable local puede ocultar tanto un identificador de una variable como de una función"*. Como cada `Scope` utiliza un único espacio de nombres (`lookupSymbol` busca desde el ámbito actual hacia la raíz), declarar un parámetro o variable local con el mismo nombre de una función global oculta a dicha función dentro de ese alcance. Si luego se intenta invocar ese nombre como método dentro del mismo alcance, `analyzeMethodCall` detecta que el símbolo resuelto es `VARIABLE_SYMBOL` y reporta el error semántico correspondiente.

---

## 5. Extensiones y decisiones en la línea de comandos (`cli.c`)

1. **Extensión `-target sem`:**
   El enunciado (`00-TDS-proyecto.pdf`, pág. 3–4) lista la extensión `.sem` entre las salidas por etapa, pero en la Tabla 1 solo enumera `scan`, `parse`, `codinter` y `assembly` para `-target`. Se incorporó `-target sem` en [CLI y Entrada](../wiki/02-infraestructura/01-cli-y-entrada.md) para permitir detener la compilación tras el análisis semántico e inspeccionar el AST anotado en `.sem`.
2. **Comportamiento por defecto sin `-target` (`CURRENT_TARGET`):**
   El enunciado establece que sin `-target` se compila hasta la etapa corriente o se genera `nombreArchivo.out` al completar el compilador. Esto se modeló con la constante `CURRENT_TARGET` en `include/cli.h` (actualmente `TARGET_ASSEMBLY`), de modo que sin `-target` se ejecuta el pipeline completo y se genera el ejecutable `.out`.
3. **Argumento opcional y listas en `-opt [optimización]`, y extensión `.opt`:**
   - Si se pasa `-opt` sin argumento explícito antes de la siguiente opción o del archivo `.ctds` (por ejemplo `./mi_compilador -opt prog.ctds`), se interpreta como `-opt all`.
   - Se admite pasar múltiples optimizaciones separadas por comas (`-opt const-fold,dead-code`) o repetir el flag `-opt`, acumulando la máscara de bits en `Options.optimizations`.
   - Cuando se compila hasta ejecutable (sin `-target`), la extensión de salida por defecto se mantiene en `.out`; cuando se combina un `-target` explícito con `-opt` (por ejemplo `-target codinter -opt all`), la extensión por defecto pasa a `.opt`.
4. **Uso general de `-o <salida>` y protección del archivo fuente:**
   - Aunque la Tabla 1 describe `-o <salida>` como *"Renombra el archivo ejecutable a `<salida>`"*, en `src/core/cli.c` se habilitó `-o` para renombrar el archivo de salida de cualquier etapa (`-target`).
   - Además, `validateOutputNotInput` verifica tanto por cadena (`strcmp`) como por dispositivo e inodo (`stat`: `st_dev` y `st_ino`) que la ruta de salida no apunte al mismo archivo `.ctds` de entrada, evitando sobrescribir accidentalmente el programa fuente.
