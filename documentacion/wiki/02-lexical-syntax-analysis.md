# Análisis Sintáctico

Toma la secuencia de tokens que produce el [análisis léxico](02-lexical-analysis.md)
y la transforma en una representación arbórea estructurada (AST).

!!! note "Estado de esta página"
    Resumen de las fichas **F06** (gramática y parser, `src/bison.y`) y **F07**
    (acciones del parser, `src/parser_helpers.c`), todavía sin desarrollar al
    nivel de detalle de las demás. El plan está en `TODO_documentacion.md`.

    El análisis léxico, que antes se resumía acá, ahora tiene su propia página
    completa: [Análisis Léxico](02-lexical-analysis.md).

---

## 1. Análisis Sintáctico y Resolución de Conflictos

El analizador sintáctico procesa la secuencia de tokens mediante un algoritmo LALR(1) verificando la conformidad con la gramática de C-TDS:

- **Estructura del programa:** Permite declarar variables globales y métodos de forma intercalada en cualquier orden.
- **Resolución del conflicto Shift/Reduce:** En la especificación original, si una función aparecía tras una variable global, un parser LALR(1) con un solo token de lookahead no podía distinguir si el tipo inicial correspondía a una nueva variable o a un método, produciendo un conflicto de desplazamiento/reducción. Se unificaron las producciones bajo una regla común de declaraciones y se eliminaron reglas intermedias de retorno, permitiendo al parser discriminar inequívocamente al encontrar `;` (variable) o `(` (método).
- **Control de errores sintácticos:** El análisis sintáctico se detiene ante el primer error gramatical encontrado, emitiendo el mensaje correspondiente junto al número de línea del fallo.

---

## 2. Construcción del AST en el Parseo

A medida que las reglas gramaticales se reducen:
- Se instancian los nodos correspondientes a sentencias, expresiones y declaraciones.
- Los elementos puramente auxiliares de la sintaxis (paréntesis, llaves, palabras clave) se descartan, conservando únicamente la información semántica requerida por las etapas posteriores.
- El resultado final es un puntero a la raíz del árbol que representa el programa completo.

---

**Siguiente:** [Árbol de Sintaxis Abstracta (AST)](03-ast.md)
