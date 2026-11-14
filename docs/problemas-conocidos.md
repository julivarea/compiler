# Problemas Conocidos y Limitaciones

El enunciado del proyecto (`docs/spec/00-TDS-proyecto.pdf`, pág. 4) requiere documentar los problemas conocidos, **cómo se detectaron** y **qué alternativa** los resolvería, además de detallar cualquier caso de test fallido si existiera.

> **Estado de las pruebas:** Actualmente las **11 suites Unity (294 tests)** compilan y pasan en su totalidad (`0 Failures`, `0 Ignored`). No hay casos de test fallidos abiertos (ver [Primeros Pasos](wiki/01-introduccion/01-primeros-pasos.md)).

---

## 1. Ausencia de declaración adelantada y recursión mutua entre métodos

En C-TDS un método puede invocarse recursivamente a sí mismo (porque su símbolo se registra en el ámbito global antes de analizar su cuerpo; ver [Alcance y Extensiones](decisiones/alcance-y-extensiones.md#43-recursion-directa-y-ocultamiento-shadowing-entre-variables-y-metodos)), pero solo ve a los demás métodos declarados textualmente antes que él. Por lo tanto, `main() { g(); }` con `g` definido más abajo, o dos funciones mutuamente recursivas (`f` llama a `g` y `g` llama a `f`), producen `Método no declarado`.

- **Cómo se detectó:** escribiendo programas de prueba con métodos en distinto orden y casos de recursión cruzada.
- **Estado:** responde a la regla de alcance de la especificación del lenguaje (`01-TDS-spec-lenguaje.pdf`, pág. 3: *"una función puede ser invocada únicamente por código ubicado después de su declaración"*), por lo que es una restricción del lenguaje y del análisis en una sola pasada.
- **Alternativa si se quisiera permitir:** agregar una **pasada previa** en `analyzeSemantics` que recorra los hijos de la raíz y registre las firmas de todos los `METHOD_DECLARATION_NODE` en el ámbito global antes de analizar los cuerpos. No requeriría cambios en TAC ni en código objeto, ya que las llamadas se emiten por etiqueta simbólica.

---

## 2. El plegado de constantes desborda con signo en `+`, `-` y `*`

Al plegar en compilación expresiones como `2147483647 * 3`, el cálculo entero en `evaluateBinary` se realiza directamente sobre `int` de C, lo cual constituye comportamiento indefinido (*undefined behavior*) en caso de desborde.

- **Cómo se detectó:** compilando el proyecto con **UBSan** (*UndefinedBehaviorSanitizer*) y ejecutando la batería de ejemplos.
- **Por qué se mantiene así:** con GCC en x86-64 el resultado envuelve en complemento a dos coincidiendo con el código objeto generado (`addl`, `subl`, `imull`), y las expresiones dentro del rango de 32 bits con signo definido por la especificación (`[-2147483648, 2147483647]`) no desbordan. Las divisiones y el módulo (`/` y `%`) sí están protegidos contra divisor cero y contra `INT_MIN / -1` porque en x86-64 `idivl` abortaría en ejecución con `SIGFPE`.
- **Alternativa:** aplicar el mismo patrón que usan `/` y `%` en `evaluateBinary` ([Optimizador del Código Intermedio](wiki/06-optimizador/02-optimizador.md)), verificando el desborde antes de operar (por ejemplo en `int64_t` contra `[INT_MIN, INT_MAX]` o con `__builtin_*_overflow`) y retornando `false` para dejar la instrucción sin plegar, o bien calcular en `uint32_t` para envolver explícitamente módulo $2^{32}$.

---

## 3. El AST y la tabla de símbolos no se liberan al finalizar

El `driver` no invoca `freeAST` ni `freeScope` al terminar una compilación: ambas estructuras permanecen en memoria hasta que finaliza el proceso.

- **Cómo se detectó:** corriendo el compilador bajo **Valgrind**, que reporta los bloques alcanzables al salir correspondientes al árbol y sus ámbitos.
- **Por qué se mantiene así:** es una decisión deliberada y acotada, ya que el compilador procesa un único archivo por ejecución y el sistema operativo recupera la memoria al terminar el proceso.
- **Alternativa:** antes de llamar a `freeAST` y `freeScope` en `compile`, unificar el dueño de los `Symbol` de declaración —que hoy liberan tanto `freeAST` como `freeScope`— haciendo que los ámbitos guarden únicamente punteros prestados y el AST conserve la propiedad exclusiva (o viceversa).

*(La fuga que sí crecía con cada uso de variable o llamada —los símbolos provisorios creados por el parser en cada referencia— está resuelta mediante `resolveNodeSymbol`; ver [AST](wiki/03-lexico-sintactico-y-ast/04-ast.md)).*

---

## 4. Tres optimizaciones de mirilla pensadas sobre assembly no están implementadas

La pasada `peephole` actúa hoy sobre el TAC (eliminación de saltos redundantes a la etiqueta inmediata) y en el generador de código objeto mediante la fusión de comparación seguida de salto condicional ([ADR 003](decisiones/adr-003-fusion-comparacion-y-salto-condicional.md)). Los tres patrones pensados directamente sobre assembly x86-64 —pares `movl` redundantes, `movl $0` acortable a `xorl` y evitar recargar un valor ya presente en registro— no se implementaron.

- **Cómo se detectó:** al contrastar el catálogo final contra las optimizaciones inicialmente evaluadas.
- **Alternativa:** dado que el backend emite assembly como texto directo con `buildStringf`, implementar mirilla general a nivel máquina requeriría introducir una representación intermedia de instrucciones ensamblador (opcode y operandos) entre el TAC y la emisión textual, aplicar los patrones allí y recién luego serializar a texto (detallado en [Optimizador del Código Intermedio](wiki/06-optimizador/02-optimizador.md)).
