# Notas sobre la Optimización de Listas Transitorias en Bison

Este documento resume la decisión de diseño y la optimización algorítmica para el manejo de listas dinámicas (`NodeList`) durante la construcción del Árbol de Sintaxis Abstracta (AST).

## Problema de la Construcción de Listas
- Se define que cuando Bison resuelve producciones que construyen listas usando **recursión por la izquierda** (como la lista de parámetros o identificadores), va encontrando los elementos uno a uno.
- **Limitación anterior:** Para que la lista mantuviera el orden correcto de los elementos, cada nuevo nodo debía insertarse al final. La función encargada de esto debía recorrer toda la lista con un bucle `while` para encontrar el último nodo.
  - Al costar esto O(N) por cada inserción, construir una lista completa de N elementos terminaba costando O(N²).
- **Incompatibilidad de inversión:** Se descartó la idea de insertar siempre en la cabecera (lo cual es instantáneo, O(1)) y luego invertir la lista al final. Esto se debió a que la gramática combina listas con recursión por la izquierda (que quedarían al revés) con listas por la derecha como los `Statements` (que se arman al derecho). Invertir el arreglo rompería el orden lógico del código.

## Manejo del Puntero a la Cola (Tail Pointer)
Este mecanismo es fundamental para lograr inserciones instantáneas sin modificar la gramática en Bison ni romper el orden de las sentencias.

### Flujo de Resolución:
1. **Modificación de Estructura:** Se le agregó a la estructura `NodeList` un puntero auxiliar denominado `tail`. Este puntero registra directamente cuál es el último elemento y es mantenido **únicamente por el nodo cabecera (head)**.
2. **Inicialización (`initializeTemporaryList`):** 
   - Cuando se crea el primer elemento de la lista (caso base), la función se encarga de que el nodo se apunte a sí mismo como su propio `tail`.
3. **Agregación Individual (`appendToTemporaryList`):** 
   - Cuando el parser captura un nuevo nodo y necesita agregarlo, la función accede directo al último nodo usando el `tail` y engancha el nuevo ahí, sin bucles.
   - **Actualización:** A continuación, actualiza el `tail` de la cabecera para que apunte a este nuevo elemento.
   - **Retorno:** Finalmente, devuelve la lista actualizada en tiempo constante O(1).
4. **Fusión de Listas (`mergeNodeLists`):**
   - Cuando Bison necesita unir dos listas completas (ej: juntar bloques de sentencias), la función simplemente engancha el `tail` de la primera lista con el inicio de la segunda, y actualiza el `tail` resultante en tiempo O(1).
