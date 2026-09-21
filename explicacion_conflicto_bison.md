# Análisis del Problema de Conflicto en Bison (Shift/Reduce)

## El Problema Original

El código original presentaba un problema donde el parser no reconocía la declaración de una función si había variables declaradas antes, lanzando un error de sintaxis:

```c
int a;
int sumar(int a, int b){} // Error sintactico: syntax error
```

El problema subyacente era un **conflicto de desplazamiento/reducción (shift/reduce conflict)** en la gramática de `bison.y` debido a que el parser no podía ver suficientes tokens hacia adelante (lookahead).

Específicamente, la gramática original exigía que **todas** las variables globales se declararan antes que los métodos:
```bison
Program : VariableDeclarations MethodDeclarations ;
```

## ¿Por qué ocurría el error?

El problema ocurre porque Bison genera un parser LALR(1), lo que significa que solo mira **un token hacia adelante**. Cuando el parser encontraba la palabra `int` (en la línea de `int sumar`), se daba esta situación:

1. Las declaraciones de variables empiezan por un tipo (`int`).
2. Las declaraciones de métodos también empiezan por un tipo (`int`).

Como Bison siempre prefiere "desplazar" (continuar con la regla que está evaluando actualmente) en lugar de "reducir" (dar por terminada la lista de `VariableDeclarations`), **asumía automáticamente que `int sumar` era una nueva declaración de variable**. 

Al hacer esa suposición, esperaba que después de `int sumar` viniera un punto y coma `;` o una coma `,`. Pero en su lugar se encontraba con un paréntesis `(`, lo que producía el fallo, ya que no contemplaba la apertura de paréntesis dentro de la declaración de una variable.

## La Solución

Para solucionar esto, aplicamos dos cambios clave en el archivo `bison.y`:

### 1. Unificación de las Declaraciones (Declarations)
En lugar de forzar que todas las variables vayan primero y luego los métodos, creamos una regla genérica `Declarations` que permite que vengan variables y métodos de forma intercalada en el scope global.

```bison
Program
    : Declarations
    ;

Declarations
    : /* empty */
    | Declaration Declarations
    ;

Declaration
    : VariableDeclaration
    | MethodDeclaration
    ;
```

### 2. Eliminación de la regla intermedia (`ReturnType`)
Integremos directamente `Type` y `VOID` en la regla de `MethodDeclaration` (eliminando la regla intermedia `ReturnType`) para evitar que el parser haga reducciones prematuras que confundan el estado interno.

```bison
MethodDeclaration
    : Type ID '(' ParameterList ')' Block
    | VOID ID '(' ParameterList ')' Block
    ;
```

## Resultado

Con esta nueva estructura, el parser lee `int sumar` (es decir, la estructura `Type ID`) de forma neutra y **espera al siguiente token**. 
- Si el siguiente token es un `;` o `,`, sabe inmediatamente que está frente a una declaración de variable (`VariableDeclaration`). 
- Si el siguiente token es un `(`, sabe sin ambigüedades que se trata de un método (`MethodDeclaration`).

De esta manera se elimina el conflicto de Shift/Reduce logrando que ambas sentencias se parseen correctamente.
