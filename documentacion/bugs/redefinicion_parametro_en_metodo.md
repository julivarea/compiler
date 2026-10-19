# ADR / Reporte de Bug: Redefinición de Parámetro Formal en el Cuerpo del Método

## Problema

Se permitía que una variable local declarada en el cuerpo principal de una función tuviera el mismo identificador que un parámetro formal de dicha función, tratándolo erróneamente como un *shadowing* (ocultamiento) válido en lugar de emitir un error semántico de redeclaración en el mismo ámbito.

## Código de Ejemplo (`examples/aliasing_parameters_in_method.txt`)

```c
int aliasing_parameters_with_assignment(int x){
    int x; 
    return x;
}

void main(){
    
}
```

## Traza con Bug (Comportamiento Anterior)

El analizador semántico aceptaba el programa sin emitir errores y creaba dos entidades con distinto UID para la misma variable `x` (`#2` para el parámetro y `#3` para la variable local):

```text
$ ./mi_compilador examples/aliasing_parameters_in_method.txt
--- Analisis sintactico sin errores formales. ---

--- Arbol de Sintaxis Abstracta (AST) ---
Bloque
  | Metodo: aliasing_parameters_with_assignment (Retorna INT)
  |   | Parametros
  |   |   | Declaracion Variable: x #2 (Tipo INT)
  |   | Bloque
  |   |   | Declaracion Variable: x #3 (Tipo INT)
  |   |   | Return
  |   |   |   | ID: x #3 (Tipo INT)
  | Metodo: main (Retorna VOID)
  |   | Parametros
  |   | Bloque

--- Analisis semantico completado sin errores. ---

--- String Builder Buffer (TAC) ---
aliasing_parameters_with_assignment:
ret x
main:
```

## Causa Raíz

En `src/semantic_analyzer.c`, la función `analyzeMethodNode` creaba un `Scope` para el método (nivel 1) donde registraba los parámetros formales, pero delegaba el cuerpo de la función al nodo `BLOCK_NODE` mediante `analyzeBlockNode`, el cual creaba un **segundo `Scope` anidado** (nivel 2).

Al procesar la declaración local `int x;`, `analyzeVariableDeclarationNode` consultaba `lookupVariableCurrentScope(current_scope, "x")`. Como `current_scope` era el ámbito del bloque (nivel 2) y el parámetro formal residía en el ámbito del método (nivel 1), la búsqueda no hallaba colisión y permitía la redeclaración asignándole un nuevo UID.

## Alternativas Evaluadas y Decisión de Diseño (ADR)

### Alternativa 1: Validar colisiones mediante UIDs o recorriendo ámbitos padres (Descartada)
- **Idea:** Al declarar una variable en un bloque, verificar hacia arriba si existe un símbolo en el padre con flag de parámetro o comparar identificadores/UIDs.
- **Descarte:** Es un parche frágil que acopla el manejo de UIDs a la semántica de scopes y añade casos especiales artificiales. Los UIDs existen únicamente como identificadores únicos de entidad para generación de código intermedio.

### Alternativa 2: Unificar el ámbito del método con el bloque principal (Aceptada)
- **Fundamento según la Especificación Oficial (C-TDS 2026, pág. 4):**
  > *"El scope de la función está conformado por los parámetros formales y los identificadores de las variables declaradas en el cuerpo de la función. Se pueden definir scope locales adicionales al introducir bloques (`<block>`) de código."*
- **Decisión:** El cuerpo principal de una función **no constituye un sub-ámbito separado**, sino que comparte el mismo `Scope` creado para el método.
- **Preservación de Sub-bloques:** Si dentro del método existen bloques anidados adicionales (por ejemplo en `if`, `while` o bloques `{ ... }` internos), estos sí continúan invocando `analyzeBlockNode` y creando sub-ámbitos hijos legítimos con soporte de shadowing según la especificación.

## Solución Técnica

Se modificó `analyzeMethodNode` en `src/semantic_analyzer.c` para que:
1. Cree el `method_scope` con el tipo de retorno correspondiente.
2. Analice los parámetros formales directamente en `method_scope`.
3. Recorra e itere las declaraciones y sentencias del bloque cuerpo (`node->children[1]`) dentro de ese **mismo** `method_scope` sin crear un ámbito intermedio.

Al compartir el mismo ámbito, la función estándar `lookupVariableCurrentScope` detecta inmediatamente cualquier identificador duplicado (tanto si una variable local repite el nombre de un parámetro, como si dos parámetros formales se llaman igual).

## Traza Corregida (Comportamiento Actual)

```text
$ ./mi_compilador examples/aliasing_parameters_in_method.txt
--- Analisis sintactico sin errores formales. ---
Error semantico en la linea 2: La variable 'x' ya fue declarada previamente en este ambito.

Compilacion abortada: se encontraron 1 errores semanticos.
```
