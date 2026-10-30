# Convenciones de Documentación y Comentarios

Este documento formaliza las reglas y patrones de documentación en código C utilizados en el compilador. Se basa en las convenciones establecidas en [`include/semantic_analyzer.h`](file:///home/francisco/compiler/include/semantic_analyzer.h), [`src/semantic_analyzer.c`](file:///home/francisco/compiler/src/semantic_analyzer.c) y [`src/tac_instruction.c`](file:///home/francisco/compiler/src/tac_instruction.c).

---

## 1. Estándar General

- **Formato:** Bloques de documentación compatibles con **Doxygen** (`/** ... */`).
- **Idioma y ortografía:** Español formal técnico con correcta acentuación (*"análisis"*, *"árbol"*, *"ámbito"*, *"código"*, *"instrucción"*).
- **Tono y redacción:** 
  - Verbos en tercera persona singular del presente o infinitivo (*"Crea..."*, *"Libera..."*, *"Realiza..."*, *"Verifica..."*, *"Vuelca..."*).
  - **Punto final obligatorio:** Todas las oraciones, resúmenes (`@brief`), párrafos explicativos y descripciones de parámetros (`@param`) deben terminar siempre en punto (`.`).
- **Líneas delimitadoras:**
  - Apertura con `/**` en su propia línea.
  - Cada línea intermedia comienza con ` * `.
  - Cierre con ` */` alineado.
  - La firma de la función comienza inmediatamente en la línea siguiente a ` */` (sin líneas en blanco intermedias).

---

## 2. Estructura de Archivos de Cabecera (`.h`)

### 2.1. Guardas de Inclusión (*Header Guards*)
Se nombran en mayúsculas con el sufijo `_H`. La guarda de cierre lleva un comentario de línea indicando la macro que finaliza:

```c
#ifndef SEMANTIC_ANALYZER_H
#define SEMANTIC_ANALYZER_H

...

#endif // SEMANTIC_ANALYZER_H
```

### 2.2. Bloque de Módulo / Archivo (`@file`)
Ubicado inmediatamente después de `#define <GUARD_H>`, antes de los `#include`:

```c
/**
 * @file semantic_analyzer.h
 * @brief Pase de análisis semántico sobre el Árbol de Sintaxis Abstracta (AST).
 */
```

- `@file <nombre_archivo>`: Nombre del archivo de cabecera.
- `@brief <descripción_concisa>`: Una única oración clara que sintetiza la responsabilidad del módulo, terminada en punto.

### 2.3. Bloque de Includes
Agrupados de manera lógica, sin comentarios redundantes entre líneas:

```c
#include "types.h"
#include "ast.h"
#include "symbol_table.h"
```

---

## 3. Documentación de Funciones

Se documenta el contrato público y lo que el código no puede decir. Si el nombre, la firma y los tipos ya lo explican, no se documenta.

### Reglas para las secciones:

1. **`@brief`:**
   - Una oración concisa que resume el propósito o contrato principal.
   - Siempre finaliza en punto (`.`).
   - Obligatorio en las declaraciones públicas del `.h`.
   - Ejemplo: `@brief Crea e inicializa una nueva instrucción TAC cuádrupla en memoria dinámica.`

2. **Cuerpo explicativo (opcional):**
   - Solo si hay un contrato, flujo o invariante que la firma no revela.
   - Separado de `@brief` y de `@param` por una línea vacía (` *`).

3. **`@param <nombre>` (solo si aporta):**
   - Solo para restricciones no evidentes: `NULL`, ownership, centinelas, unidades, rangos o alias.
   - Restricciones y valores centinela entre paréntesis al final (ej. `(acepta NULL)`, `(NULL para el scope global)`).
   - Siempre finaliza en punto (`.`).
   - Si el nombre del parámetro ya lo explica, se omite.

4. **`@return` (solo si aporta):**
   - **Funciones `void`:** **NUNCA** llevan `@return`.
   - **Funciones no-void:** solo si el significado no es obvio (ej. qué implica `NULL` o un código de error).
   - No se repite el tipo de dato redundante antes de la descripción.

---

## 4. Ejemplos de Referencia

### 4.1. Declaración pública con contrato en el `.h`
```c
/**
 * @brief Crea una instrucción TAC y enlaza sus operandos.
 *
 * @param result Operando de destino (no NULL).
 * @param arg1 Primer operando fuente (acepta NULL si la operación es unaria).
 * @return Puntero a la instrucción creada, o NULL si falló la reserva de memoria.
 */
TACInstruction *createTACInstruction(TACOperation operation, TACOperand result, TACOperand arg1, TACOperand arg2);
```

### 4.2. Función `static` autoexplicativa en el `.c`
```c
static const char *getOperatorSymbol(TACOperation operation) {
    ...
}
```

Solo si el mapeo no fuera evidente se añadiría un `//` de una línea explicando el porqué.

---

## 5. Estructura y Comentarios en Archivos de Implementación (`.c`)

1. **Forward Declarations cuando hagan falta:**
   Solo se declaran al inicio las funciones `static` que lo necesiten (recursión mutua u orden de definición). No se duplican firmas por rutina.
   ```c
   static const char *getOperatorSymbol(TACOperation operation);
   ```

2. **Comentarios de línea (`//`):**
   - Se reservan exclusivamente para aclaraciones internas puntuales.
   - Cuando se pasan valores literales o constantes como parámetros, el comentario aclara el nombre semántico del parámetro:
     ```c
     analyzeMethodCall(node, current_scope, 0); // isExpression = 0 (llamada como sentencia)
     ```

3. **Intenciones con variables booleanas en lugar de bloques de comentarios:**
   En vez de preceder un `if` con un bloque de comentario explicativo, se extrae la condición a una o más variables booleanas bien nombradas:
   ```c
   bool needsFloatCoercion = (node->symbol->type == TYPE_FLOAT);
   if (needsFloatCoercion) {
       value = coerceToFloat(value);
   }
   ```

