# Problemas Conocidos y Limitaciones

El enunciado del proyecto (`docs/spec/00-TDS-proyecto.pdf`, pág. 4) requiere documentar los problemas conocidos, **cómo se detectaron** y **qué alternativa** los resolvería, además de detallar cualquier caso de test fallido si existiera.

> **Estado de las pruebas:** Actualmente las **11 suites Unity (294 tests)** compilan y pasan en su totalidad (`0 Failures`, `0 Ignored`). No hay casos de test fallidos abiertos (ver [Primeros Pasos](wiki/01-introduccion/01-primeros-pasos.md)).

---

## 1. Un método solo ve a los declarados antes que él

No existe declaración adelantada en C-TDS, por lo que `main() { g(); }` con `g` definido más abajo produce `Método no declarado`.

- **Cómo se detectó:** escribiendo programas de ejemplo con los métodos en distinto orden.
- **Estado:** responde a la especificación del lenguaje (*"todo identificador debe ser declarado antes de su uso"*), por lo que no es un defecto sino una restricción a tener en cuenta al escribir casos de prueba.
- **Alternativa si se quisiera permitir:** agregar una **pasada previa** en `analyzeSemantics` que recorra los hijos de la raíz y registre todos los `METHOD_DECLARATION_NODE` en el ámbito global antes de analizar los cuerpos. No afecta al backend, ya que las llamadas TAC se resuelven por nombre.

---

## 2. El plegado de constantes desborda con signo en `+`, `-` y `*`

Al plegar en compilación expresiones como `2147483647 * 3`, el cálculo entero se realiza sobre `int` de C, lo cual constituye comportamiento indefinido (*undefined behavior*) en caso de desborde.

- **Cómo se detectó:** compilando el proyecto con **UBSan** (*UndefinedBehaviorSanitizer*) y ejecutando la batería de ejemplos.
- **Comportamiento observado:** con GCC en x86-64 el resultado envuelve en complemento a dos coincidiendo con el código objeto generado, aunque formalmente no está garantizado por el estándar C.
- **Divisiones y módulo:** `/` y `%` sí están protegidos contra divisor cero y contra `INT_MIN / -1`.
- **Alternativa:** aplicar el mismo patrón que ya usan `/` y `%` en `evaluateBinary` ([Optimizador del Código Intermedio](wiki/06-optimizador/02-optimizador.md)). Verificar el posible desborde antes de operar (por ejemplo con `__builtin_add_overflow`, `__builtin_sub_overflow`, `__builtin_mul_overflow` o comparando contra `INT_MAX`/`INT_MIN`) y retornar `false` para dejar la instrucción sin plegar y resolverla en tiempo de ejecución.

---

## 3. El AST y la tabla de símbolos no se liberan al finalizar

El `driver` no invoca `freeAST` ni `freeScope` al terminar una compilación: ambas estructuras permanecen en memoria hasta que finaliza el proceso.

- **Cómo se detectó:** corriendo el compilador bajo **Valgrind**, que reporta un único bloque retenido por corrida (el árbol y sus ámbitos).
- **Por qué se mantiene así:** es una decisión deliberada y acotada, ya que el compilador procesa un único archivo por ejecución y el sistema operativo recupera la memoria al terminar el proceso.
- **Alternativa:** antes de llamar a `freeAST` y `freeScope` en `compile`, resolver el doble dueño actual de los `Symbol` de declaración —que hoy liberan tanto `freeAST` como `freeScope`— haciendo, por ejemplo, que los ámbitos guarden únicamente punteros prestados y el AST conserve la propiedad exclusiva.

*(La fuga que sí crecía proporcionalmente con el tamaño del programa —los símbolos provisorios creados por el parser en cada referencia— está resuelta mediante `resolveNodeSymbol`; ver [AST](wiki/03-lexico-sintactico-y-ast/04-ast.md)).*

---

## 4. Tres optimizaciones de mirilla pensadas sobre assembly no están implementadas

La pasada `peephole` actúa hoy sobre el TAC (eliminación de saltos redundantes a la etiqueta inmediata), y el generador de código objeto fusiona una comparación seguida de salto condicional. Los tres patrones pensados directamente sobre assembly x86-64 —pares `movl` redundantes, `movl $0` acortable a `xorl` y evitar recargar un valor ya presente en registro— no se implementaron.

- **Cómo se detectó:** al contrastar el catálogo final contra las optimizaciones inicialmente evaluadas.
- **Alternativa:** dado que el backend emite assembly como texto directo con `buildStringf`, implementar mirilla a nivel máquina requeriría introducir una representación intermedia de instrucciones ensamblador (opcode y operandos) entre el TAC y la emisión textual, aplicar los patrones allí y recién luego serializar a texto (desarrollado en [Optimizador del Código Intermedio](wiki/06-optimizador/02-optimizador.md)).
