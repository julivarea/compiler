# ADR 001: Desacoplamiento de Código Intermedio (TAC) y Gestión de Memoria del Stack (Offsets)

- **Estado:** Aceptado (Rechazo de acoplamiento)
- **Fecha:** 2026-11-13
- **Contexto:** Evaluación de cálculo anticipado de offsets y slots de stack en `intermediate_code.c`.

---

## 1. Contexto del Problema

Se evaluó la posibilidad de calcular los desplazamientos (*offsets*) relativos a `%rbp` y la reserva de ranuras de memoria (*slots*) para variables locales y temporales directamente durante la fase de generación de código intermedio (`src/intermediate_code/intermediate_code.c`). 

La propuesta sugería utilizar contadores/variables globales que se reiniciaran al comienzo de cada función (aprovechando que `temporaryCount` ya se reinicia por método) para prefijar el tamaño del marco de pila (*stack frame*) o preasignar la posición de cada operando en el stack, evitando así la pasada previa de escaneo (`buildFrameLayout` / `allocateLocalSlots`) que actualmente realiza el backend en `src/object_code/object_code.c`.

---

## 2. Decisión

**Se rechaza trasladar la lógica de offsets y reserva de memoria a la generación de código intermedio (TAC).**

El cálculo de offsets, el mapeo de variables/temporales a la pila y el dimensionamiento del marco de activación permanecen como responsabilidad exclusiva del **Backend** (`src/object_code/object_code.c` y `src/object_code/function_frame.c`). 

El código intermedio (TAC) continuará operando bajo el modelo de **registros virtuales ilimitados y nombres simbólicos**, completamente agnóstico de la memoria física y del hardware de ejecución.

---

## 3. Fundamentación Teórica y Respaldo en la Bibliografía (Dragon Book)

La decisión se fundamenta en los principios canónicos de diseño de compiladores formalizados en *Compilers: Principles, Techniques, and Tools* (Aho, Lam, Sethi, Ullman — *Dragon Book*):

### 3.1. Separación estricta entre IR y Entorno de Ejecución
* En el **Capítulo 6 (Intermediate-Code Generation)**, el *Dragon Book* establece que la Representación Intermedia (IR) debe modelar la semántica computacional del programa fuente de manera independiente del hardware.
* En los **Capítulos 7 (Run-Time Environments)** y **8 (Code Generation)**, se define que la organización del almacenamiento en tiempo de ejecución (*Storage Organization*), el diseño del registro de activación (*Activation Record / Stack Frame*), la dirección de crecimiento de la pila y las reglas de alineación de datos pertenecen estrictamente a la arquitectura de la máquina objetivo (*target machine*).
* Introducir offsets como `-4(%rbp)` o conteos de slots físicos en TAC acopla la fase independiente de la máquina con la máquina destino, destruyendo la modularidad de la arquitectura en capas.

### 3.2. Ruptura del Pipeline de Optimización (Capítulo 9)
En nuestro compilador, el pipeline de ejecución es:
$$\text{AST} \longrightarrow \text{TAC} \longrightarrow \textbf{Optimizador} \longrightarrow \text{Código Objeto}$$

El optimizador (`src/optimizer/optimizer.c`) aplica transformaciones como propagación de constantes, plegado algebraico (*constant folding*) y eliminación de código muerto (*dead-code elimination*):
* Si TAC calcula los offsets de antemano basándose en `temporaryCount` o declaraciones, cualquier temporal o variable muerta eliminada por el optimizador deja huecos inutilizados en el stack frame o infla artificialmente el prólogo de la función (`subq $N, %rsp`).
* Si se pretendiera recalcular o compactar offsets dentro del optimizador, se contaminaría una fase puramente algebraica con detalles de gestión de memoria de bajo nivel.

### 3.3. Confusión entre Nombres Lógicos y Espacio Físico (Liveness & Register Allocation)
* En el código intermedio, los temporales (`t0`, `t1`, `t2`, ...) son infinitos y representan **valores** intermedios.
* Como señala el *Dragon Book* (Secciones 8.4 y 8.6), los temporales tienen rangos de vida (*live ranges*) sumamente breves: nacen en una operación y mueren poco después.
* Asignar un slot de pila a cada temporal desde TAC asume una estrategia de *stack spilling* total e indiscriminado. En un compilador real, la mayoría de los temporales deben residir en registros de la CPU (`%eax`, `%edx`, etc.) o reutilizar un conjunto mínimo de ranuras compartidas mediante análisis de variables vivas (`live_variable_analysis`).

### 3.4. Detalles de ABI y Alineación de Pila
El tamaño final de la pila en x86-64 no depende únicamente de las variables locales y temporales, sino también de:
* La alineación obligatoria del stack a 16 bytes previa a cualquier instrucción `call` (System V AMD64 ABI).
* El espacio necesario para parámetros salientes que no entran en los 6 registros de la ABI (`pendingParamSlots`).
El generador de código intermedio no tiene por qué (ni debe) conocer estas restricciones de la plataforma destino.

---

## 4. Consecuencias

### Positivas
1. **Modularidad y portabilidad:** TAC permanece limpio, legible y agnóstico de x86-64.
2. **Optimizaciones seguras:** El optimizador puede insertar, reordenar y eliminar cuádruplas libremente sin invalidar mapas de memoria.
3. **Evolución hacia asignación de registros:** Permite en el futuro implementar asignación de registros real (e.g. Sethi-Ullman o coloreo de grafos) en el backend sin modificar la generación de TAC.
4. **Ausencia de estado mutable global:** Se evita introducir variables globales mutables de control en `intermediate_code.c`.

### Costo Aceptado
* El backend debe realizar un escaneo lineal previo por función (`allocateLocalSlots` en `src/object_code/object_code.c`) para registrar los operandos que efectivamente sobrevivieron a las optimizaciones y calcular el frame alineado. Este costo es de orden $O(N)$ sobre unas pocas decenas de cuádruplas en memoria, ejecutándose en microsegundos y resultando despreciable frente a los beneficios de diseño.
