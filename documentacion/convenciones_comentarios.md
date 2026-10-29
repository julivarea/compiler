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

Cada prototipo público en cabeceras y cada función de módulo en archivos `.c` se documenta con la estructura canónica de 3 secciones separadas por una línea vacía (` *`):

```c
/**
 * @brief <Resumen de acción en una oración terminada en punto.>
 *
 * <Párrafo(s) descriptivo(s) detallando responsabilidades, flujo, qué valida o construye,
 * y contratos. Se utiliza @p para referenciar parámetros en el texto.>
 *
 * @param <nombre> <Descripción del rol semántico y restricciones/centinelas entre paréntesis.>
 * @return <Descripción del valor devuelto (solo si la función NO es void).>
 */
```

### Reglas para las secciones:

1. **`@brief`:**
   - Una oración concisa que resume el propósito principal.
   - Siempre finaliza en punto (`.`).
   - Ejemplo: `@brief Crea e inicializa una nueva instrucción TAC cuádrupla en memoria dinámica.`

2. **Cuerpo explicativo:**
   - Separado de `@brief` y de `@param` por una línea vacía (` *`).
   - Explica el comportamiento interno, validaciones, efectos colaterales o algoritmos implementados.
   - Se utiliza `@p nombre_parametro` cuando se menciona un argumento en el texto (ej. `@p parent == NULL`).

3. **`@param <nombre>`:**
   - Detalla la función semántica de cada argumento.
   - Restricciones, valores especiales o valores centinela se especifican entre paréntesis al final (ej. `(acepta NULL)`, `(NULL para el scope global)`).
   - Siempre finaliza en punto (`.`).

4. **`@return` (Solo funciones con retorno):**
   - **Funciones `void`:** **NUNCA** llevan `@return` (se omite por completo).
   - **Funciones no-void:** Describe el resultado devuelto y qué significa en caso de error (ej. `NULL` o códigos especiales).
   - No se repite el tipo de dato redundante antes de la descripción.
   - Ejemplo: `@return Puntero a la instrucción creada, o NULL si falló la reserva de memoria.`

---

## 4. Ejemplos de Referencia

### 4.1. Función constructora con retorno y validación de memoria
```c
/**
 * @brief Crea e inicializa una nueva instrucción TAC cuádrupla en memoria dinámica.
 *
 * Reserva espacio para una estructura TACInstruction, configura su código de
 * operación y sus tres operandos (resultado, primer argumento y segundo argumento),
 * e inicializa el puntero de enlace siguiente en NULL.
 *
 * @param op Código de operación que define la instrucción.
 * @param result Operando de destino o etiqueta asociada.
 * @param arg1 Primer operando fuente o argumento principal.
 * @param arg2 Segundo operando fuente o argumento secundario.
 * @return Puntero a la instrucción creada, o NULL si falló la reserva de memoria.
 */
TACInstruction *createTACInstruction(TACOperation op, TACOperand result, TACOperand arg1, TACOperand arg2) {
    ...
}
```

### 4.2. Función destructora / `void` (sin `@return`)
```c
/**
 * @brief Libera la memoria ocupada por una instrucción TAC individual.
 *
 * Verifica que el puntero no sea nulo antes de invocar la liberación
 * de la estructura. No libera las instrucciones encadenadas en 'next'.
 *
 * @param instruction Puntero a la instrucción TAC a liberar (acepta NULL).
 */
void freeTACInstruction(TACInstruction *instruction) {
    if (instruction != NULL) {
        free(instruction);
    }
}
```

### 4.3. Función estática de mapeo / formateo
```c
/**
 * @brief Obtiene la representación textual del operador para instrucciones binarias o unarias.
 *
 * Mapea códigos de operación como TAC_ADD, TAC_SUB o TAC_EQ a su símbolo correspondiente
 * (+, -, ==) para su posterior formateo en texto. Retorna "?" para operaciones no reconocidas.
 *
 * @param op Código de operación del TAC.
 * @return Cadena constante con el símbolo del operador.
 */
static const char *getOperatorSymbol(TACOp op) {
    ...
}
```

### 4.4. Función pública con recorrido y emisión
```c
/**
 * @brief Vuelca la representación textual de una instrucción TAC al acumulador de cadenas.
 *
 * Extrae los nombres calificados de los operandos y formatea la instrucción
 * según su categoría sintáctica (etiquetas, declaraciones, asignaciones,
 * saltos condicionales, llamadas o expresiones binarias/unarias), escribiendo
 * el resultado mediante buildStringf.
 *
 * @param instruction Puntero a la instrucción TAC a imprimir (si es NULL no realiza acción).
 */
void printTACInstruction(const TACInstruction *instruction) {
    ...
}
```

---

## 5. Estructura y Comentarios en Archivos de Implementación (`.c`)

1. **Forward Declarations al inicio:**
   Todas las funciones estáticas (`static`) se declaran al inicio del archivo `.c`, facilitando una lectura ordenada de los componentes del módulo.
   ```c
   static const char *getOperatorSymbol(TACOp op);
   ```

2. **Comentarios de línea (`//`):**
   - Se reservan exclusivamente para aclaraciones internas puntuales.
   - Cuando se pasan valores literales o constantes como parámetros, el comentario aclara el nombre semántico del parámetro:
     ```c
     analyzeMethodCall(node, current_scope, 0); // isExpression = 0 (llamada como sentencia)
     ```

