Estado: FIXEADO 

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
