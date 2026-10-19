# Análisis Léxico y Sintáctico

La etapa de entrada del compilador transforma la secuencia de caracteres crudos del código fuente en una representación arbórea estructurada (AST).

---

## 1. Análisis Léxico

El analizador léxico escanea el código fuente reconociendo la familia de tokens del lenguaje C-TDS:

- **Palabras reservadas:** Tipos primitivos (`int`, `boolean`, `float`, `void`), estructuras de control (`if`, `else`, `while`, `return`) y valores booleanos (`true`, `false`). Son sensibles a mayúsculas y minúsculas.
- **Identificadores:** Secuencias alfanuméricas que comienzan con letra o guión bajo, utilizadas para variables y métodos.
- **Constantes numéricas:** Literales enteros (`int`) y de punto flotante (`float`).
- **Operadores y delimitadores:** Operaciones aritméticas (`+`, `-`, `*`, `/`, `%`), lógicas (`&&`, `||`, `!`), de comparación (`==`, `<`, `>`), asignación (`=`) y símbolos de puntuación (`;`, `,`, paréntesis y llaves).
- **Comentarios:** Soporta comentarios de una línea (`//...`) y multilínea (`/* ... */`), descartándolos automáticamente.
- **Rastreo de líneas:** Registra la línea de origen de cada token para que cualquier mensaje de error posterior indique con exactitud la ubicación en el código fuente.

---

## 2. Análisis Sintáctico y Resolución de Conflictos

El analizador sintáctico procesa la secuencia de tokens mediante un algoritmo LALR(1) verificando la conformidad con la gramática de C-TDS:

- **Estructura del programa:** Permite declarar variables globales y métodos de forma intercalada en cualquier orden.
- **Resolución del conflicto Shift/Reduce:** En la especificación original, si una función aparecía tras una variable global, un parser LALR(1) con un solo token de lookahead no podía distinguir si el tipo inicial correspondía a una nueva variable o a un método, produciendo un conflicto de desplazamiento/reducción. Se unificaron las producciones bajo una regla común de declaraciones y se eliminaron reglas intermedias de retorno, permitiendo al parser discriminar inequívocamente al encontrar `;` (variable) o `(` (método).
- **Control de errores sintácticos:** El análisis sintáctico se detiene ante el primer error gramatical encontrado, emitiendo el mensaje correspondiente junto al número de línea del fallo.

---

## 3. Construcción del AST en el Parseo

A medida que las reglas gramaticales se reducen:
- Se instancian los nodos correspondientes a sentencias, expresiones y declaraciones.
- Los elementos puramente auxiliares de la sintaxis (paréntesis, llaves, palabras clave) se descartan, conservando únicamente la información semántica requerida por las etapas posteriores.
- El resultado final es un puntero a la raíz del árbol que representa el programa completo.

---

**Siguiente:** [Árbol de Sintaxis Abstracta (AST)](03-ast.md)
