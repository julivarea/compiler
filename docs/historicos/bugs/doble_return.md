# [HISTÓRICO — REVERTIDO] Bug: doble `return` en un método

> **Nota histórica:** Esta regla quedó **obsoleta y revertida**. El mensaje `"Ya hay un return definido"` ya no existe en el compilador (`examples/two_returns.ctds` es válido en C-TDS porque múltiples `return` en distintas ramas o código tras un `return` están permitidos). En el código actual, `analyzeMethodNode` y `guaranteesReturn` en `src/semantic/semantic_analyzer.c` validan que todo método no-`void` retorne un valor en **todos** sus caminos de ejecución, sin prohibir múltiples sentencias `return`.

Estado: REVERTIDO (histórico)

Problema: el analisis sintactico no verificaba si habia dos returns
Solucion: agregue un contador que en caso de que haya 2 returns, largue error

Traza:
Error semantico en la linea 3: Ya hay un return definido
Compilacion abortada: se encontraron 1 errores semanticos.

Codigo: 
int try_two_returns(){
    return 1;
    return 2;
}

void main(){

}
