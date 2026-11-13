# Bug: asignación a un método

## Problema

Se estaba aceptando la asignación a un identificador declarado sin verificar que el símbolo resuelto correspondiera a una variable (`VARIABLE_SYMBOL`) y no a un método (`METHOD_SYMBOL`).

## Código de Ejemplo (`examples/asignation_method.ctds`)

```c
int x(){

}
void main(){
  x = 5;
}
```

## Traza con Bug (Comportamiento Anterior)

```text
$ ./mi_compilador examples/asignation_method.ctds
--- Analisis sintactico sin errores formales. ---

--- Arbol de Sintaxis Abstracta (AST) ---
Bloque
  | Metodo: x (Retorna INT)
  |   | Parametros
  |   | Bloque
  | Metodo: main (Retorna VOID)
  |   | Parametros
  |   | Bloque
  |   |   | Asignacion a: x #1 (Tipo INT)
  |   |   |   | ID: x #1 (Tipo INT)
  |   |   |   | Constante INT: 5

--- Analisis semantico completado sin errores. ---

--- String Builder Buffer (TAC) ---
x:
main:
x = 5
```

## Solución

En `analyzeAssignmentNode` (`src/semantic/semantic_analyzer.c`), luego de resolver el símbolo en la tabla de símbolos con `lookupSymbol`, se verifica explícitamente `resolvedSymbol->symbolType == VARIABLE_SYMBOL`. Si el símbolo corresponde a un método, se reporta un error semántico (`"Ese nombre de simbolo corresponde al metodo '%s'."`) y se aborta la asignación antes de analizar la expresión o generar TAC.