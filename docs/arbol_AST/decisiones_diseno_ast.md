# Decisiones de Diseño: Árbol de Sintaxis Abstracta (AST)

Este documento unifica las decisiones de diseño, optimizaciones y convenciones aplicadas sobre la estructura y generación del AST.

## 1. Estructura Unificada de Nodos (Aridad Fija y Dinámica)
Se refactorizó el almacenamiento de hijos (`children`) para utilizar un arreglo dinámico genérico, permitiendo un tratamiento homogéneo al iterar y liberar memoria, sin importar la cantidad de hijos.

- **Nodos de aridad fija** (operadores binarios, condicionales, asignaciones): Se mapean a posiciones fijas del arreglo `children`.
  - Para legibilidad, se utilizan macros en la etapa semántica: `GET_LEFT(node)`, `GET_RIGHT(node)`, `GET_CONDITION(node)`, `GET_IF_BLOCK(node)`, `GET_ELSE_BLOCK(node)`.
- **Nodos de aridad dinámica** (bloques, parámetros, argumentos de métodos): El `childCount` determina su longitud. Al parsear, se utiliza una estructura temporal de lista ("nodo lista") que reserva el espacio y permite agregar sub-nodos progresivamente.

## 2. Optimización de Inserción en Listas (O(1))
Las listas construidas mediante recursión por la izquierda en Bison requerían insertar elementos al final, lo cual tenía costo O(N) y O(N²) para construir la lista completa.

- Se introdujo un puntero `tail` en la estructura `NodeList`, mantenido únicamente por el nodo cabecera (head).
- **`initializeTemporaryList`**: El nodo cabecera se apunta a sí mismo como `tail`.
- **`appendToTemporaryList`**: Se enlaza el nuevo elemento directamente usando el `tail` actual, y se actualiza el `tail` de la cabecera en tiempo constante O(1).
- **`mergeNodeLists`**: Une dos listas enlazando el `tail` de la primera con la cabeza de la segunda en tiempo constante O(1).
- Se descartó la alternativa de insertar en cabecera e invertir al final porque la gramática combina listas construidas por la izquierda y por la derecha.

## 3. Manejo de Tipos de Datos y Declaraciones
Las producciones en Bison de especificadores de tipo (ej. `int`, `boolean`) devuelven directamente un enumerado `DataType` (ej. `TYPE_INT`, `TYPE_BOOLEAN`).

- **Aplanamiento (Flatten) de variables:** Al parsear la declaración de múltiples variables (ej. `int x, y, z;`), se utiliza `resolveVariableDefinition`.
- Esta función recibe el `DataType` y la lista de identificadores.
- Crea un nodo de declaración **individual** para cada variable y le asigna el tipo correspondiente.
- Retorna una lista aplanada de nodos independientes, simplificando la distribución de tipos en las etapas semánticas.

## 4. Almacenamiento de Símbolos y Literales
La estructura `Symbol` asocia metadatos de texto a los nodos, gestionándose en memoria con `newSymbol` (que duplica la cadena) y `freeSymbol`.

- **`id`**: Almacena el nombre de identificadores como variables o métodos.
- **`value`**: Almacena el valor en crudo de constantes/literales (creados mediante `newLiteralNode`).

## 5. Operadores y Rastreo de Código Fuente
- **Tipos de Operaciones:** El enumerado `OperationType` define las operaciones aritméticas, lógicas, relacionales y unarias soportadas. Se inyectan en los nodos binarios mediante `newBinaryOperatorNode`.
- **Rastreo de Líneas (`yylineno`):** La función constructora base `newNode` captura automáticamente la línea actual de código desde la variable global `yylineno` de Flex/Bison. Este valor se persiste en `node->line`, garantizando precisión al reportar errores semánticos.
