# Notas sobre la Refactorización del Nodo AST

Este documento resume los cambios realizados durante la refactorización de la estructura de los nodos del Árbol de Sintaxis Abstracta (AST), según el registro de audio transcrito.

## Situación Anterior
- Las estructuras que requerían una cantidad variable de elementos (como bloques o listas de parámetros) utilizaban estructuras de tipo lista, ya que la cantidad de parámetros no era fija.
- Existían otros casos donde la aridad (cantidad de hijos) era fija y conocida, como:
  - Operadores binarios (suma, resta) con nodos `left` y `right`.
  - Estructuras de control condicionales (`if then`, `if then else`).

## Cambios Aplicados
El objetivo principal fue **unificar** la forma en que se almacenan y acceden los hijos de un nodo, logrando un tratamiento homogéneo.

### 1. Unificación de Nodos de Aridad Fija
Para los nodos con una cantidad fija de hijos (como los binarios o condicionales), ahora se utilizan posiciones genéricas (parámetros):
- En un operador binario, el operando izquierdo (`left`) se asigna al parámetro `0` y el derecho (`right`) al parámetro `1`.
- Este mismo esquema se utiliza para mapear la condición y los bloques de un `if` o `if then else`.

### 2. Manejo de Listas y Nodos de Aridad Variable
Para los casos donde la cantidad de hijos es dinámica (no se sabe de antemano cuántos habrá):
- Se levanta el parámetro y se guarda en una estructura temporal.
- Luego, se inicializa una lista que se guarda en la estructura principal del nodo.
- El "nodo lista" básicamente reserva el lugar para la lista y permite ir guardando los nodos hijos iterativamente.

## Beneficios
- **Iteración y Limpieza Uniforme:** Al aplicar esta refactorización, queda una **única forma** de iterar y limpiar las listas en los nodos. Ya no hay distinción estructural a la hora de recorrerlos entre los nodos de los que sabíamos exactamente cuántos hijos tendrían y los que no.
