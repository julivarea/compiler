# Comparación con el Algoritmo de Sethi-Ullman

Documento de análisis sobre la estrategia de evaluación de expresiones y asignación de almacenamiento adoptada en el compilador frente al algoritmo clásico de Sethi-Ullman (números de Ershov).

---

## 1. Problema de diseño

Al traducir una expresión aritmética representada en el [AST](../wiki/03-lexico-sintactico-y-ast/04-ast.md), como `x = (a + b) * (c - (d * e))`, el generador de código debe definir:

1. El orden de evaluación de los subárboles (izquierdo vs. derecho).
2. La asignación de ubicaciones para los resultados intermedios (registros de CPU vs. ranuras en el marco de pila).

---

## 2. Estrategia adoptada en el compilador

En C-TDS la traducción se divide en dos etapas desacopladas ([Generación de TAC](../wiki/05-codigo-intermedio/02-generacion-tac.md) y [Generación de Código Objeto](../wiki/07-codigo-objeto/02-generacion-codigo-objeto.md)):

| Aspecto | Implementación actual (`intermediate_code.c` + `object_code.c`) | Alternativa con Sethi-Ullman |
| :--- | :--- | :--- |
| **Orden de evaluación** | Recorrido postorden fijo (siempre hijo izquierdo antes que hijo derecho). | Reordena la evaluación empezando por el subárbol que demanda más registros ($L$ mayor). |
| **Temporales en TAC** | Un temporal nuevo (`t0, t1, ...`) por cada operación evaluada. | Reutilización acotada de nombres/registros virtuales. |
| **Almacenamiento en x86-64** | Asignación directa en pila: cada temporal recibe un slot de 4 bytes en `FunctionFrame` y cada cuádrupla opera usando `%eax`/`%edx` como registros de paso. | *Spilling* a memoria únicamente cuando la etiqueta $L(N)$ supera el número $K$ de registros físicos disponibles. |
| **Complejidad de implementación** | Una sola pasada de emisión; cada cuádrupla se traduce de forma independiente. | Dos pasadas (etiquetado *bottom-up* y emisión *top-down* con seguimiento de registros libres). |

### Por qué se eligió asignación en pila con recorrido fijo

- **Desacoplamiento entre etapas:** El TAC mantiene temporales ilimitados sin asumir cuántos registros físicos tiene la arquitectura destino (ver [ADR 001](adr-001-desacoplamiento-tac-offsets-stack.md)).
- **Simplicidad y corrección en el backend:** Al volcar cada temporal a su offset `-k(%rbp)` al terminar cada instrucción TAC, los registros `%eax` y `%edx` quedan libres antes de la instrucción siguiente. Esto evita conflictos con instrucciones x86-64 que usan registros fijos (`idivl` sobre `%edx:%eax`, desplazamientos con `%cl` y llamadas que pisan registros *caller-saved*).
- **Reducción previa en el optimizador:** Con `-opt all`, las pasadas de plegado y propagación de constantes junto con la eliminación de asignaciones muertas (`dead-code`) reducen las expresiones constantes antes de dimensionar el marco de pila.

---

## 3. Resumen del algoritmo de Sethi-Ullman (para una futura extensión a registros)

Si se quisiera reemplazar el volcado sistemático a la pila por una asignación de registros en expresiones sin llamadas con efectos laterales, el algoritmo opera en dos pasadas:

### Pasada 1: Etiquetado *bottom-up* ($L(N)$)

A cada nodo $N$ se le asocia la cantidad mínima de registros necesarios para evaluarlo sin accesos intermedios a pila:

- **Hojas:** $L(N) = 1$ si es hijo izquierdo (debe cargarse en registro) y $L(N) = 0$ si es hijo derecho (en x86-64 puede operarse directamente desde memoria, por ejemplo `addl -8(%rbp), %eax`).
- **Nodo binario con hijos $T_1, T_2$ de etiquetas $L_1, L_2$:**
  $$L(N) = \begin{cases} \max(L_1, L_2) & \text{si } L_1 \neq L_2 \\ L_1 + 1 & \text{si } L_1 = L_2 \end{cases}$$

Cuando $L_1 \neq L_2$, evaluar primero el subárbol más pesado consume $\max(L_1, L_2)$ registros, deja su resultado en un único registro y permite evaluar el otro subárbol con los registros restantes. Cuando $L_1 = L_2$, el primer subárbol retiene un registro para su resultado mientras el segundo demanda otros $L_1$ registros adicionales ($L_1 + 1$).

### Pasada 2: Emisión *top-down*

En cada operador binario:
1. Se evalúa primero el hijo con mayor etiqueta $L$.
2. Si ambos hijos requieren al menos los $K$ registros disponibles ($L_1 \ge K$ y $L_2 \ge K$), se evalúa el más pesado, se guarda su resultado en un slot temporal de la pila, se evalúa el segundo reutilizando los $K$ registros y luego se recupera el valor de la pila para aplicar el operador.
