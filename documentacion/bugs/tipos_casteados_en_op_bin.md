Problema: para la comparacion booleana se estaba viendo que sean del mismo tipo, ignorando los casteos de int a float
por lo tanto, estaba marcando error cuando es totalmente valido

Salida:
Error semantico en la linea 6: Tipos incompatibles en la operacion binaria.

Compilacion abortada: se encontraron 1 errores semanticos.

Traza: 
void main(){
    int x;
    float y;
    x = 5;
    y = 5.1;
    if(x>y){
        return;
    }
}
