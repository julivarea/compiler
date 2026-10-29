# Algoritmo de Sethi-Ullman (Generación Óptima de Código para Expresiones)

## 1. Introducción y Contexto Histórico

El **Algoritmo de Sethi-Ullman** fue publicado en 1970 por **Ravi Sethi** y **Jeffrey D. Ullman** en su artículo fundamental:
> *"The Generation of Optimal Code for Arithmetic Expressions"* (Journal of the ACM, 1970).

De forma paralela e independiente, el matemático soviético **Andrei Ershov** describió un concepto equivalente en 1958, por lo que a las etiquetas calculadas en este algoritmo a menudo se las conoce en la literatura como **Números de Ershov**.

### El Problema que Resuelve
En un compilador, una expresión aritmética como:
$$x = (a + b) \times (c - (d \times e))$$
se representa internamente como un árbol de sintaxis abstracta (**AST**).

Para calcular el resultado en una CPU con un número finito de registros (o de posiciones de almacenamiento temporal), surgen dos preguntas críticas:
1. **¿En qué orden conviene evaluar los subárboles (hijo izquierdo vs. hijo derecho)?**
2. **¿Cuál es la cantidad mínima estricta de registros necesarios para calcular la expresión sin tener que guardar datos intermedios en la memoria RAM (*spilling*)?**

Si se evalúa el árbol en un orden ingenuo (por ejemplo, siempre de izquierda a derecha), se pueden mantener ocupados registros innecesariamente, agotándolos y forzando accesos lentos a memoria. Sethi-Ullman encuentra el orden **matemáticamente óptimo**.

---

## 2. Las Dos Fases del Algoritmo

El algoritmo opera sobre un árbol binario de expresión en dos pasadas:

```
          [AST]
            │
            ▼ (Pasada 1: Post-orden Bottom-Up)
  [Árbol con Etiquetas L(n)]
            │
            ▼ (Pasada 2: Top-Down)
 [Código Assembly / TAC Óptimo]
```

### Fase 1: Etiquetado de Nodos (Bottom-Up)
A cada nodo $N$ del árbol se le calcula un entero $L(N)$, llamado **número de Sethi-Ullman** o **etiqueta de registro**, que indica *cuántos registros se necesitan como mínimo para evaluar ese subárbol*.

Las reglas de etiquetado (para una arquitectura estándar registro-registro) son:

1. **Hojas (Operandos base):**
   - Si una hoja es el hijo izquierdo: $L(N) = 1$ (se carga en un registro).
   - Si una hoja es el hijo derecho: $L(N) = 0$ (en muchas arquitecturas puede usarse directamente como operando de memoria en la instrucción, ej. `addl b, %eax`).
   *(Nota: En máquinas puras donde ambos operandos deben estar sí o sí en registros, todas las hojas se etiquetan con 1).*

2. **Nodos Interiores (Operadores binarios con hijos $T_1$ y $T_2$):**
   Sean $L_1 = L(T_1)$ y $L_2 = L(T_2)$:
   - **Caso 1: $L_1 \neq L_2$**
     $$L(N) = \max(L_1, L_2)$$
     *Explicación:* Evaluamos primero el subárbol que requiere más registros. Cuando termina, su resultado queda en **1 solo registro**. Luego evaluamos el otro subárbol con los registros restantes sin sobrepasar el máximo.
   
   - **Caso 2: $L_1 = L_2$**
     $$L(N) = L_1 + 1$$
     *Explicación:* Como ambos subárboles son igual de complejos, evaluemos el que evaluemos primero, consumirá $L_1$ registros y dejará su resultado retenido en 1 registro. Para evaluar el segundo subárbol necesitaremos otros $L_1$ registros libres a la vez. Total necesario: $L_1 + 1$.

---

### Fase 2: Generación de Código (Top-Down)

Una vez etiquetado el árbol, se recorre desde la raíz hacia las hojas siguiendo una regla fundamental:

> **Regla de Oro:** Para cualquier operador binario, **se evalúa primero el hijo que tenga la etiqueta $L$ más alta**.

- Si $L(hijo\_izq) > L(hijo\_der)$: Evaluar primero el hijo izquierdo, guardar su resultado en un registro $R$, y luego evaluar el hijo derecho.
- Si $L(hijo\_der) > L(hijo\_izq)$: Evaluar primero el hijo derecho, guardar su resultado en un registro $R$, y luego evaluar el hijo izquierdo.
- Si $L(hijo\_izq) == L(hijo\_der)$: Da igual cuál primero, pero el resultado ocupará un registro adicional durante la evaluación del segundo.

---

## 3. Ejemplo Práctico Paso a Paso

Consideremos la siguiente expresión:
$$((a + b) \times c) + ((d - e) \times (f + g))$$

### Representación en Árbol:

```text
                  (+)  [Nodo Raíz]
                /     \
             (*)       (*)
            /   \     /   \
          (+)    c  (-)   (+)
          / \       / \   / \
         a   b     d   e f   g
```

### Paso 1: Etiquetar las hojas y subárboles inferiores
- Hoja `a` (izq) = 1, Hoja `b` (der) = 0 $\implies$ Nodo `(+)` de `(a + b)`:
  $L_1 = 1, L_2 = 0 \implies \max(1, 0) = 1$.
- Multiplicación `((a+b) * c)`:
  Hijo izq tiene $L=1$, hoja `c` (der) tiene $L=0 \implies \max(1, 0) = 1$.

Ahora el subárbol derecho:
- `(d - e)`: $L(d)=1, L(e)=0 \implies L = 1$.
- `(f + g)`: $L(f)=1, L(g)=0 \implies L = 1$.
- Multiplicación `((d-e) * (f+g))`:
  Ambos hijos tienen $L = 1$.
  Como son iguales: $L = 1 + 1 = \mathbf{2}$.

### Paso 2: Etiquetar la raíz
- Hijo izquierdo `((a+b) * c)` tiene $L = 1$.
- Hijo derecho `((d-e) * (f+g))` tiene $L = 2$.
- Raíz `(+)`: $\max(1, 2) = \mathbf{2}$.

**Conclusión del etiquetado:** Esta expresión puede calcularse con un mínimo estricto de **2 registros**, ¡sin tocar la memoria!

### Comparación de Orden de Evaluación:

#### Orden Naïve (Izquierda a Derecha):
1. Calcula `a + b` $\to$ usa 1 registro (`R1`).
2. Multiplica por `c` $\to$ resultado en `R1`.
3. Ahora debe calcular el lado derecho, pero `R1` ya está ocupado:
4. Calcula `d - e` $\to$ necesita otro registro (`R2`).
5. Ahora necesita calcular `f + g`, pero `R1` y `R2` están ocupados.
   **¡Se queda sin registros si solo tenía 2!** Está obligado a hacer *spill* de `R1` o `R2` a memoria stack.

#### Orden Sethi-Ullman (Evaluar primero el lado más pesado):
Como el lado derecho tiene $L=2$ y el izquierdo $L=1$, **evalúa primero el lado derecho**:
1. Evalúa `(d - e)` $\to$ resultado en `R1`.
2. Evalúa `(f + g)` $\to$ usa `R2`.
3. Multiplica `R1 = R1 * R2` $\to$ el lado derecho completo queda comprimido en **`R1`** (se libera `R2`).
4. Ahora evalúa el lado izquierdo:
   - Carga `a` en `R2`, suma `b`.
   - Multiplica `R2` por `c`.
5. Suma final: `R1 = R2 + R1`.
**Resultado:** Se evaluó la expresión completa utilizando únicamente 2 registros, sin ningún *spill* al stack.

---

## 4. ¿Qué pasa cuando la CPU tiene menos registros que $L(N)$? (Spilling)

Si una expresión compleja requiere $L(N) = 4$ registros pero nuestra máquina solo nos permite usar $K = 2$ registros (por ejemplo, `%eax` y `%edx` en un compilador didáctico):

El algoritmo de Sethi-Ullman se extiende de forma elegante:
1. Si ambos subárboles requieren más registros de los disponibles ($L_1 \ge K$ y $L_2 \ge K$):
   - Evalúa el subárbol más pesado.
   - Emite una instrucción explícita de guardado en stack: `pushq` (o `movl %reg, -offset(%rbp)`).
   - Evalúa el segundo subárbol reutilizando los mismos $K$ registros.
   - Recupera el valor guardado: `popq` (o `movl -offset(%rbp), %reg`).
   - Aplica la operación binaria.

Esto garantiza que la cantidad de lecturas y escrituras al stack sea la **mínima matemática posible**.

---

## 5. Relación con Nuestro Compilador (C-TDS)

| Característica | Sethi-Ullman | Nuestro Compilador Actual (`intermediate_code.c` + `object_code.c`) |
| :--- | :--- | :--- |
| **Generación de TAC** | Reordena la evaluación según el peso del subárbol. | Recorrido canónico post-orden fijo (siempre izquierdo $\to$ derecho). |
| **Temporales** | Reutiliza un número mínimo de nombres/registros. | Emite un temporal único `t0, t1, t2...` por cada operación. |
| **Manejo en Stack** | *Spill* mínimo solo cuando se agotan los registros disponibles. | *Naive Spilling*: cada temporal tiene su propio slot reservado en el frame. |
| **Complejidad** | Requiere dos pasadas sobre el AST y gestión de registros libres. | Muy simple, determinista y fácil de auditar. Cumple la premisa: *"Corrección sobre optimización"*. |

---

## 6. Referencias para Profundizar

- **Aho, Lam, Sethi, Ullman.** *Compilers: Principles, Techniques, and Tools* (Dragon Book), 2da Edición, Sección 8.10: *"Code Generation from DAGs / Tree-Evaluation with Registers"*.
- **Sethi, R., & Ullman, J. D. (1970).** *The Generation of Optimal Code for Arithmetic Expressions.* Journal of the ACM (JACM), 17(4), 715-728.
- **Ershov, A. P. (1958).** *On programming of arithmetic operations.* Communications of the ACM, 1(8), 3-6.
