# ADR 001: Desacoplamiento de Código Intermedio (TAC) y Offsets del Stack

- **Estado:** Aceptado
- **Contexto:** Evaluación de calcular los desplazamientos (*offsets*) relativos a `%rbp` durante la generación de código intermedio (`src/intermediate_code/intermediate_code.c`) en lugar de hacerlo en el backend (`src/object_code/`).

---

## 1. Problema

Durante el diseño del generador de código se planteó la alternativa de aprovechar los contadores por método en `intermediate_code.c` para preasignar allí el offset de pila (`-4(%rbp)`, `-8(%rbp)`, etc.) de cada variable local y cada temporal, evitando la pasada previa `allocateLocalSlots` que realiza `src/object_code/object_code.c`.

---

## 2. Decisión

El cálculo de offsets, la asignación de ranuras (`StackSlot`) y el dimensionamiento del marco de activación (`FunctionFrame`) se mantienen exclusivamente en el **backend** (`src/object_code/function_frame.c` y `src/object_code/object_code.c`).

El código intermedio (TAC) opera únicamente con nombres simbólicos (`x@1`) y temporales ilimitados (`t0`, `t1`, ...), sin información sobre direcciones ni tamaño de pila.

---

## 3. Motivos técnicos

1. **Independencia de la arquitectura destino:** La representación intermedia modela las operaciones del programa sin depender de convenciones de x86-64. El tamaño de cada tipo en pila, la dirección de crecimiento del stack y los registros disponibles pertenecen a la generación de código objeto.
2. **Interacción con el optimizador (`-opt`):** El pipeline ejecuta el optimizador entre la generación de TAC y la emisión de código objeto:
   $$\text{AST} \longrightarrow \text{TAC} \longrightarrow \text{Optimizador} \longrightarrow \text{Código Objeto}$$
   Las pasadas de plegado/propagación de constantes y eliminación de código muerto eliminan temporales y asignaciones inútiles. Si los offsets se fijaran al crear el TAC, los temporales eliminados dejarían huecos sin usar en el marco de pila (inflando el `subq $N, %rsp`), o bien obligarían al optimizador a recalcular posiciones de memoria.
3. **Restricciones de la System V AMD64 ABI:** El tamaño final del marco depende también de los argumentos pasados por pila a partir del séptimo parámetro (`pendingParamSlots`) y del redondeo a múltiplo de 16 bytes previo a `call`.

---

## 4. Consecuencias

- **Ventajas:** El TAC permanece simple de inspeccionar (`.ci` / `.opt`), el optimizador puede eliminar o reescribir instrucciones sin invalidar offsets, y `allocateLocalSlots` reserva espacio únicamente para los operandos que sobreviven a la optimización.
- **Costo aceptado:** `emitFunction` realiza un recorrido lineal previo sobre las instrucciones de cada función para poblar `FunctionFrame` antes de emitir el prólogo.
