Problema: Se estaba aceptando que podiamos hacer asignaciones, sin verificar que el simbolo
al que estabamos asignando sea una variable

Codigo:

int x(){

}
void main(){
  x = 5;
}



Traza con bug:
   /compiler  ./mi_compilador examples/asignation_method.txt
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


Solucion:
Al hacer una asignacion, validar que lo de la izquierda sea un nodo de ID