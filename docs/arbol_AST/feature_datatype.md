# Notas sobre la Feature de Data Types en Bison

Este documento resume la implementación y manejo de los tipos de datos (Data Types) durante la construcción del Árbol de Sintaxis Abstracta (AST) usando Bison, según el registro de audio transcrito.

## Retornos de las Producciones en Bison
- Se define que cuando Bison resuelve producciones estructurales mayores (como `programa`, `statement`, `expresion`, etc.), estas devuelven una estructura de tipo **nodo**.
- **Novedad para Data Types:** Se configuró Bison para que, al resolver una producción de especificador de tipo (`type`), esta devuelva una etiqueta `type` que mapea directamente a una **enumeración de tipo de dato** (`data type`).
  - Ejemplo: Si el token es `int`, la producción devuelve un enumerado tipo `TYPE_INT`.
  - Si el token es `boolean`, devuelve un enumerado tipo `TYPE_BOOLEAN`.

## Manejo de la Declaración de Variables
Este mecanismo es fundamental cuando el analizador sintáctico se encuentra con declaraciones de múltiples variables en una sola línea, por ejemplo: `int x, y, z;`.

### Flujo de Resolución:
1. **Captura:** El parser obtiene el tipo base (que ya viene procesado como el enumerado de `data type`) y captura la lista completa de identificadores (en el ejemplo: `x`, `y`, `z`).
2. **Aplanamiento (Flatten):** Se llama a una función auxiliar denominada `flatten_variables_declarations`.
   - **Argumentos:** Se le pasa el tipo de dato y la lista de identificadores.
3. **Creación de Nodos:** 
   - La función se encarga de crear un nodo de declaración **individual** para cada variable (crea un nodo para `x`, otro para `y`, y otro para `z`).
   - A cada uno de estos nodos individuales, **le asigna el tipo de dato** que se pasó como argumento.
   - **Retorno:** Finalmente, la producción devuelve la lista completa de estos nuevos nodos, logrando que cada variable quede declarada con su tipo correspondiente en el AST.
