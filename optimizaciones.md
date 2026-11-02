1. propagacion de valores constantes

si tenes return 1 + 2 + 3;
en lugar de hacer:
t1 = 1 + 2;
t2 = t1 + 3;
return t2;

directamente resolver:
t1 = 1 + 2 + 3;
return t1;

2. codigo muerto

para cada vez que aparece:
if(cond){
    return true;
    x = 1;
} else {
    return false;
    y = x; 
    x = y;
    x = y + 1;
}

directamente cortar lo que haya despues del return en las ramas

3. optimizaciones de if(true){ // value expresion -> true

directamente deberia ejecutarse siempre, y evitar preguntar la condicion

}

4. optimizar en assembly instrucciones y demas

5. operaciones para constantes * 2

si aparece * 2 generar una instruccion y si no otra
