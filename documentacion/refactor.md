# Reglas de Organización, Estilo y Documentación

Estándar del proyecto para refactorización, basado en la **Metáfora del Periódico**, la **Regla del Paso Descendente** (*Step-Down Rule*) y las convenciones de [`convenciones_comentarios.md`](convenciones_comentarios.md) (formato Doxygen y ejemplos canónicos).

---

## 1. Principio: de lo principal a lo auxiliar

Todo archivo se lee de arriba abajo, bajando el nivel de abstracción:

1. **Primero el propósito y las operaciones principales:** el lector debe entenderlos en las primeras líneas.
2. **Después los detalles:** submétodos, helpers, mapeos y funciones estáticas.
3. **Tipos canónicos:** se definen en su lugar; sin `typedef` adelantados ni aliases artificiales.

La regla se aplica al escribir o modificar código; no obliga a reordenar archivos existentes solo para cumplirla.

---

## 2. Archivos de cabecera (`.h`)

### 2.1. Guardas e includes

Se usa include guard con sufijo `_H` (no `#pragma once`). Después del `#define` va el bloque `@file`/`@brief`, y luego los `#include` agrupados:

```c
#ifndef SEMANTIC_ANALYZER_H
#define SEMANTIC_ANALYZER_H

/**
 * @file semantic_analyzer.h
 * @brief Pase de análisis semántico sobre el Árbol de Sintaxis Abstracta (AST).
 */

#include "types.h"
#include "ast.h"

...

#endif // SEMANTIC_ANALYZER_H
```

### 2.2. Orden del contenido

**A. Objetos / estructuras de datos:**

1. **Definición canónica del tipo principal:** `typedef struct X { ... } X;`
2. **Constructor:** `createX(...)`.
3. **Ciclo de vida y operaciones públicas de primer nivel:** `freeX(...)`, `printX(...)`.
4. **Tipos auxiliares extensos:** si un enum o estructura auxiliar es grande, va a su propia cabecera para no tapar el tipo principal.

**B. Procesos / servicios / fases:**

1. **Función pública de entrada:** `analyzeSemantics(...)`, `generateIntermediateCode(...)`.
2. **Consultas y sub-operaciones públicas**, si existen.

---

## 3. Archivos de implementación (`.c`)

1. **Includes** al inicio.
2. **Prototipos `static`:** solo cuando se necesiten (recursión mutua u orden de definición).
3. **Función principal primero:** en objetos el constructor; en procesos la entrada pública.
4. **Submétodos directos:** en el orden en que los usa la función principal.
5. **Helpers de bajo nivel al final:** formateo, búsquedas, conversiones y mapeos.
6. **Comentarios de línea (`//`):** solo aclaraciones puntuales, por ejemplo el nombre semántico de un literal:
   ```c
   analyzeMethodCall(node, current_scope, 0); // isExpression = 0 (llamada como sentencia)
   ```

---

## 4. Nombres significativos

- **Sin abreviaturas opacas:** `op` → `operation`, `arg1`/`arg2` → `firstOperand`/`secondOperand`, `ptr` → `pointer`, `tmp` → `temporary`, `val` → `value`, `buf` → `buffer`, `idx` → `index`, `len` → `length`, `ctx` → `context`, `err` → `error`.
- **Nombres pronunciables y buscables:** identificadores en inglés, comentarios en español.
- **Booleanos** con prefijo `is`, `has`, `can` o `needs` (`isExpression`, `hasReturn`, `needsImplicitReturn`).
- **Expresión de intención mediante booleanos (en vez de bloques de comentarios):** Extraer condiciones a variables booleanas con nombres descriptivos (`bool var = (cond); if (var)`) para documentar la intención directamente con el código, evitando bloques de comentarios gigantes:
  ```c
  bool needsImplicitReturn = (node->type == TYPE_VOID && !lastInstructionIsReturn());
  if (needsImplicitReturn) {
      emit(TAC_RETURN, tacNone(), tacNone(), tacNone());
  }
  ```
- **Verbos consistentes** con el módulo: `create`, `free`, `print`, `analyze`, `generate`, `get`.
- **Excepciones:** contadores cortos (`i`, `j`, `k`) y acrónimos de dominio ya establecidos (`AST`, `TAC`, `TACOp`).

> Los ejemplos de [`convenciones_comentarios.md`](convenciones_comentarios.md) usan nombres heredados (`op`, `arg1`); se renombran solo en el código que se modifique, nunca en un barrido masivo.

---

## 5. Documentación de funciones

Referencia: [`convenciones_comentarios.md`](convenciones_comentarios.md). Se documenta solo lo que el código no puede decir: el contrato, el porqué y las restricciones.

- `@brief` de una línea en cada declaración pública del `.h`. Es la única fuente de documentación de la API.
- Cuerpo explicativo y `@param` solo cuando aporten algo no evidente: `NULL`, ownership, centinelas, unidades, rangos o errores.
- `@return` solo en funciones no-`void` y solo si el significado no es obvio.
- En el `.c` no se repite la documentación del `.h`; las `static` triviales no llevan Doxygen ni bloques de comentarios explicativos.
- La firma de la función empieza en la línea siguiente al ` */`, sin línea en blanco.

---

## 6. Checklist de refactor

- [ ] Orden descendente en el código nuevo o modificado: principal → submétodos → helpers.
- [ ] Cabecera con guard `_H`, bloque `@file`/`@brief` e includes agrupados.
- [ ] Prototipos `static` solo donde el orden de definición o la recursión mutua lo exijan.
- [ ] Nombres significativos en el código que se toca, sin renombrados masivos.
- [ ] Intenciones expresadas con booleanos descriptivos (`bool var = (cond); if (var)`) en vez de bloques de comentarios.
- [ ] Documentación solo de contratos y restricciones no evidentes; sin repetir el código.
- [ ] Sin comentarios redundantes; `//` solo para aclaraciones puntuales.
