# Bug: casteos implícitos ignorados en una comparación

## Problema

Para las comparaciones relacionales (`<`, `>`), el verificador de tipos exigía que ambos operandos tuvieran exactamente el mismo tipo (`left == right`), rechazando comparaciones entre `int` y `float` que son válidas según la regla de promoción implícita del lenguaje C-TDS.

## Código y Salida con Bug

```c
void main(){
    int x;
    float y;
    x = 5;
    y = 5.1;
    if(x>y){
        return;
    }
}
```

```text
Error semantico en la linea 6: Tipos incompatibles en la operacion binaria.

Compilacion abortada: se encontraron 1 errores semanticos.
```

## Solución

En `resolveOperation` (`src/semantic/type_checker.c`), los operadores relacionales `OP_LT` y `OP_GT` pasaron a validar `isNumeric(left) && isNumeric(right)` (aceptando cualquier combinación de `TYPE_INT` y `TYPE_FLOAT`) y a retornar `TYPE_BOOL`, mientras que `OP_EQ` conserva `left == right && isBasicType(left)`.
