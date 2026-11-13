# ADR 003: Fusión de Comparaciones Relacionales y Saltos Condicionales (Condition Code & Branch Folding)

- **Estado:** Aceptado (En diseño / Pendiente de implementación)
- **Fecha:** 2026-11-13
- **Contexto:** Generación de código x86-64 para estructuras de control (`if`, `while`) a partir de cuádruplas TAC en `src/object_code.c`.

---

## 1. Contexto del Problema

En el lenguaje fuente, las estructuras condicionales e iterativas (`if`, `if-else`, `while`) evalúan condiciones relacionales como `a < b`, `a > b` o `a == b`. 

Durante la etapa de generación de código intermedio (`src/intermediate_code.c`), estas construcciones se aplanan canónicamente en dos cuádruplas TAC consecutivas:
1. Una operación relacional que almacena el valor de verdad en un operando temporal virtual:
   ```
   t0 = a < b
   ```
2. Un salto condicional que inspecciona dicho temporal:
   ```
   ifFalse t0 goto .L1
   ```

Actualmente, el generador de código objeto (`src/object_code.c`) traduce cada cuádrupla de forma **aislada y sin contexto**, mediante plantillas fijas de macro-expansión. Esto provoca una severa sobrecarga de instrucciones y accesos redundantes a memoria.

---

## 2. Estado Actual (Cómo lo tenemos hoy)

Cada cuádrupla se traduce independientemente, forzando la materialización numérica del booleano (0 o 1) en memoria:

### Traducción de `t0 = a < b` ([emitRelationalOp](file:///home/francisco/compiler/src/object_code.c#L641-L648))
```assembly
    movl a(%rbp), %eax       # 1. Carga primer operando
    movl b(%rbp), %edx       # 2. Carga segundo operando
    cmpl %edx, %eax          # 3. Compara operandos (fija flags en CPU)
    setl %al                 # 4. Materializa condición a byte (1 si menor, 0 si no)
    movzbl %al, %eax         # 5. Extiende byte a entero de 32 bits en %eax
    movl %eax, -12(%rbp)     # 6. Vuelca el booleano al stack slot de t0
```

### Traducción de `ifFalse t0 goto .L1` ([emitIfFalse](file:///home/francisco/compiler/src/object_code.c#L429-L433))
```assembly
    movl -12(%rbp), %eax     # 7. Vuelve a leer t0 desde la pila a %eax
    cmpl $0, %eax            # 8. Compara el booleano contra cero
    je .L1                   # 9. Salta si es cero (falso)
```

### Diagnóstico del Estado Actual
* **Costo por condición:** **8 a 9 instrucciones** de Assembly x86-64.
* **Sobrecarga de memoria:** 2 accesos a memoria (un `store` y un `load` consecutivo sobre el mismo slot de pila `-12(%rbp)`).
* **Desperdicio de hardware:** La instrucción `cmpl` inicial ya dejó en el registro de estados (*EFLAGS*) la información exacta del resultado (`ZF`, `SF`, `OF`). La secuencia `setcc` $\to$ `movzbl` $\to$ `store` $\to$ `load` $\to$ `cmpl $0` no hace más que destruir y reconstruir un estado que el procesador ya tenía listo.

---

## 3. Diseño Objetivo (Cómo quedaría)

Aplicar **Condition Code and Branch Folding** mediante reconocimiento de patrones en ventana deslizante (*Peephole / Lookahead*) durante la emisión de la función en `src/object_code.c`.

Cuando una cuádrupla relacional (`TAC_LT`, `TAC_GT`, `TAC_EQ`) sea sucedida inmediatamente por un salto condicional (`TAC_IF_FALSE` o `TAC_IF_TRUE`) que consume ese mismo temporal, **se fusionan ambas operaciones**.

### Código Assembly resultante (3 instrucciones)
```assembly
    movl a(%rbp), %eax       # 1. Carga primer operando
    movl b(%rbp), %edx       # 2. Carga segundo operando
    cmpl %edx, %eax          # 3. Compara operandos
    jge .L1                  # 4. Salto condicional directo a la etiqueta
```

### Tabla de Mapeo de Condiciones y Saltos Directos

| Comparación TAC | Mnemónico de comparación | Salto con `ifFalse` (Condición Invertida) | Salto con `ifTrue` (Condición Directa) |
|---|---|---|---|
| `TAC_LT` (`<`) | `cmpl %edx, %eax` | **`jge .label`** ($\ge$) | **`jl .label`** ($<$) |
| `TAC_GT` (`>`) | `cmpl %edx, %eax` | **`jle .label`** ($\le$) | **`jg .label`** ($>$) |
| `TAC_EQ` (`==`) | `cmpl %edx, %eax` | **`jne .label`** ($\ne$) | **`je .label`** ($==$) |

*(Nota: en sintaxis AT&T, `cmpl %edx, %eax` evalúa `%eax - %edx`, por lo que el orden relativo de los operandos se preserva de forma canónica).*

---

## 4. Fundamentación Teórica (Dragon Book)

En *Compilers: Principles, Techniques, and Tools* (Aho, Lam, Sethi, Ullman):
* **Capítulo 8, Sección 8.7 (Peephole Optimization — Flow-of-Control Optimizations):**
  El texto destaca que las secuencias donde se evalúa un valor lógico para luego bifurcar deben colapsarse eliminando la prueba redundante contra cero y utilizando directamente las bifurcaciones condicionales que provee el juego de instrucciones (`jcc`).
* **Capítulo 8, Sección 8.9 (Instruction Selection & Condition Codes):**
  Se formaliza que los códigos de condición (*condition codes*) son un recurso de hardware efímero. Generar código para ramas bifurcadas mediante la materialización de valores de verdad (*jumping code* vs. *value code*) es un antipatrón en backends de arquitecturas CISC modernas, a menos que el valor booleano escape de la estructura de control (e.g. asignado a una variable de usuario o retornado).

---

## 5. Criterios de Implementación y Casos Borde

1. **Temporales efímeros vs. Variables persistentes:**
   * La fusión aplica cuando el operando resultado de la comparación es un `OPERAND_TEMP` consumido por el salto contiguo.
   * Si la comparación asigna directamente a una variable de usuario (e.g. `flag = a < b;`), se mantiene la emisión con `setcc` y `movzbl` porque el valor numérico persistente sí es necesario.
2. **Compatibilidad con pruebas unitarias aisladas:**
   * La suite de tests unitarios (`tests/test_object_code.c`) evalúa `generateInstruction` con cuádruplas individuales (`TAC_IF_FALSE` sin contexto previo).
   * La implementación debe mantener el comportamiento actual como *fallback* por defecto, activando la fusión cuando el generador recorre la secuencia de instrucciones dentro de `emitFunction`.

---

## 6. Beneficios Medibles

* **Reducción de tamaño del código:** Reducción de **~60% de instrucciones** en cada bloque de control condicional e iterativo (de 8-9 a 3 instrucciones).
* **Eliminación de presión sobre el Stack Frame:** El temporal intermedio de la condición ya no necesita un slot físico en la pila si no se materializa.
* **Menor latencia de CPU:** Se eliminan dos accesos a memoria (store-forwarding) y una instrucción de extensión de ceros (`movzbl`).
