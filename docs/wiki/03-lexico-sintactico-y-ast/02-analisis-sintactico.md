# Análisis Sintáctico

**Archivo:** `src/parser/bison.y` (162 líneas)

Toma la secuencia de tokens que produce el [análisis léxico](01-analisis-lexico.md)
y verifica que forme un programa válido según la gramática de C-TDS, construyendo
el AST en el camino.

---

## 1. Qué problema resuelve

Decidir si la secuencia de tokens pertenece al lenguaje, y mientras lo hace,
armar el árbol que representa su estructura. Es la fase que entiende de
**jerarquía**: que en `2 + 3 * 4` la multiplicación va adentro de la suma, que un
`while` tiene una condición y un cuerpo, que un método tiene parámetros.

El léxico sabe qué es cada pieza; el sintáctico sabe cómo se arman.

---

## 2. Interfaz pública

Bison genera `build/bison.tab.c` y `build/bison.tab.h` a partir de este archivo.

| Símbolo | Qué es | Quién lo usa |
| --- | --- | --- |
| `yyparse()` | Parsea la entrada completa. Devuelve `0` si la aceptó, `1` si hubo error | `runParse` en `driver.c` |
| `yyerror(const char *)` | Callback que Bison invoca al detectar un error | Lo llama Bison, lo define este archivo |
| `astRoot` | Puntero global a la raíz del AST construido | Lo escribe la acción de `Program`; lo leen todas las fases siguientes |
| `yylex()` | Declarado `extern`: lo provee el léxico | Lo llama Bison internamente |
| `bison.tab.h` | Los `#define` de cada token y la declaración de `yylval` | `lexer.l` lo incluye para poder devolver tokens |

La dependencia es circular a propósito: el parser necesita `yylex()` del léxico,
y el léxico necesita los números de token del parser. Se corta generando primero
el header con `bison -d` y después compilando el léxico contra él. Por eso
`build.sh` corre `bison` **antes** que `flex`.

---

## 3. El prólogo

### El bloque `%{ ... %}`

Se copia tal cual al principio del `.c` generado. Contiene los `#include`, las
declaraciones `extern` y `yyerror`.

```c
static void yyerror(const char *s) {
    (void)s; /* el mensaje de Bison esta en ingles y no aporta nada mas que la linea */
    syntaxError(yylineno, "error de sintaxis");
}
```

**Por qué se descarta el mensaje de Bison.** Bison pasa un texto en inglés del
estilo `syntax error, unexpected '='`. El proyecto reporta todos los errores en
español y con el mismo formato, así que se ignora el parámetro y se arma el
mensaje propio con `syntaxError`, que es el de `error_handler`. El `(void)s`
existe solo para que GCC no avise por parámetro sin usar.

### El `#ifdef UNITY_TESTING`

```c
#ifdef UNITY_TESTING
NodeAST *astRoot = NULL;
#else
extern NodeAST *astRoot;
#endif
```

En la build normal `astRoot` se define en `main.c` y acá solo se declara. Pero
las suites de tests no linkean `main.c` (tienen su propio `main`), así que sin
este `#ifdef` el enlazado fallaría por símbolo indefinido. Es una concesión al
testing, y está comentada como tal en el código.

### El bloque `%code requires`

```
%code requires {
#include "ast.h"
#include "parser_helpers.h"
}
```

A diferencia de `%{ %}`, esto se copia al **header generado**, no al `.c`. Hace
falta porque la `%union` menciona `struct NodeAST`, `struct NodeArray` y
`DataType`: sin esos tipos a la vista, `bison.tab.h` no compilaría cuando
`lexer.l` lo incluye.

---

## 4. Declaraciones

### La unión de valores semánticos

Cada símbolo de la gramática puede llevar un valor. Como los tipos son distintos
según el símbolo, Bison usa una unión:

| Campo | Lo llevan | Quién lo pone |
| --- | --- | --- |
| `int intval` | `NUMBER`, `BOOL_CONST` | El léxico |
| `float floatval` | `FLOAT_CONST` | El léxico |
| `char *strval` | `ID` | El léxico (`strdup`) |
| `struct NodeAST *node` | Los no terminales que producen **un** nodo | Las acciones |
| `struct NodeArray *array` | Los no terminales que producen una **lista** de nodos | Las acciones |
| `DataType dataType` | `Type` | Las acciones |

La distinción `node` / `array` es la decisión de diseño central de las acciones:
un `Block` es un nodo, pero `Statements` es una lista que después se cuelga de un
nodo. El detalle está en [acciones del parser](03-acciones-del-parser.md).

### Tokens

```
%token VOID INT FLOAT BOOLEAN IF ELSE WHILE RETURN
%token EQ AND OR NOT
%token <strval> ID
%token <intval> NUMBER BOOL_CONST
%token <floatval> FLOAT_CONST
```

Los dos primeros grupos no llevan valor: la palabra misma es toda la información.
Los tres últimos lo declaran con `<campo>`, y es lo que permite escribir `$1` en
una acción y que Bison sepa de qué campo de la unión leer.

Los operadores de un solo carácter (`+`, `<`, `;`, `(`…) **no se declaran**: se
usan directamente como `'+'`, y su número de token es su código ASCII.

### Tipos de los no terminales

```
%type <node>     Program Statement Expression MethodCall Block MethodDeclaration
                 ParameterList Assignment
%type <dataType> Type
%type <array>    VariableDeclaration IdentifierList ArgumentList VariableDeclarations
                 Statements Parameters Declarations Declaration
```

Vale la pena mirar quién está en cada grupo, porque explica la forma del AST:
`Block` y `ParameterList` son nodos (van a ser un `BLOCK_NODE` y un
`PARAMETERS_NODE`), mientras que `Statements` y `Parameters` son listas que se
resuelven dentro de esos nodos.

---

## 5. Precedencia y asociatividad

```
%left OR                /* || */
%left AND               /* && */
%left EQ                /* == */
%left '<' '>'
%left '+' '-'
%left '*' '/' '%'
%precedence NOT UMINUS  /* ! y menos unario */
```

**El orden de las declaraciones es el de menor a mayor precedencia.** La primera
línea es la que liga más flojo. La especificación del lenguaje lista la
precedencia de mayor a menor (`-` unario, `!`, `* / %`, `+ -`, `< >`, `==`, `&&`,
`||`), así que este bloque es exactamente esa lista al revés, y conviene tenerlo
claro porque invertirlo es un error que compila sin quejas y cambia la semántica.

`%left` además fija asociatividad **izquierda**: `10 - 3 - 2` se agrupa
`(10 - 3) - 2`.

Verificado ejecutando los programas y mirando el código de salida:

| Expresión | Resultado | Confirma |
| --- | --- | --- |
| `2 + 3 * 4` | 14 | `*` liga más fuerte que `+` |
| `(2 + 3) * 4` | 20 | Los paréntesis la fuerzan |
| `10 - 3 - 2` | 5 | Asociatividad izquierda (si fuera derecha daría 9) |
| `100 / 10 / 2` | 5 | Ídem con división |
| `17 % 5 * 2` | 4 | `%` y `*` tienen la misma precedencia, izquierda |
| `0 - 3 * 4 + 20` | 8 | `*` antes que `-` y `+` |
| `a < b == c < d` | igual que `(a<b) == (c<d)` | Relacionales ligan más que `==` |
| `!false && false` | `(!false) && false` | `!` liga más que `&&` |
| `true \|\| false && false` | `true \|\| (false && false)` | `&&` liga más que `\|\|` |

### El pseudo-token `UMINUS`

```
| '-' Expression %prec UMINUS   { $$ = newOperatorNode(OP_UMINUS, $2, NULL); }
```

El problema: `-` aparece en dos reglas, como resta binaria y como negación
unaria, y necesitan precedencias distintas. `-3 * 4` tiene que ser `(-3) * 4`,
pero la precedencia del token `'-'` es más baja que la de `'*'`.

La solución estándar es declarar un token que **el léxico nunca devuelve**,
`UMINUS`, cuyo único propósito es tener precedencia alta, y marcar la regla con
`%prec UMINUS` para que use esa en lugar de la del `'-'` que contiene.

### Por qué `%precedence` y no `%left` para los unarios

`%precedence` (Bison 3.0+) declara precedencia **sin** asociatividad. Para un
operador prefijo la asociatividad no significa nada: no hay dos operandos entre
los cuales decidir. Usar `%left` funcionaría pero declararía algo falso.

No impide anidarlos, porque eso lo resuelve la forma de la regla
(`NOT Expression`, con la expresión a la derecha). Verificado: `- -a` con `a = 7`
devuelve 7, y `!!true` es verdadero.

---

## 6. Inventario de producciones

Los 17 no terminales, en el orden del archivo. La columna *Acción* resume qué construye;
el detalle de cada función auxiliar está en [acciones del parser](03-acciones-del-parser.md).

### Estructura del programa

| No terminal | Producciones | Acción |
| --- | --- | --- |
| `Program` | `Declarations` | Crea el `BLOCK_NODE` raíz, le cuelga las declaraciones y lo asigna a `astRoot` |
| `Declarations` | `%empty` \| `Declarations Declaration` | Acumula en una lista; vacío es `NULL` |
| `Declaration` | `VariableDeclaration` \| `MethodDeclaration` | Unifica los dos casos en una lista |

La recursión **izquierda** (`Declarations Declaration`) no es casual: es la forma
natural para un parser ascendente, porque permite reducir cada elemento a medida
que llega sin acumular la pila. Con recursión derecha el parser tendría que
apilar todo el programa antes de empezar a reducir.

### Declaraciones de variables

| No terminal | Producciones | Acción |
| --- | --- | --- |
| `VariableDeclaration` | `Type IdentifierList ';'` | `resolveVariableDefinition`: reparte el tipo entre todos los identificadores |
| `Type` | `INT` \| `BOOLEAN` \| `FLOAT` | Devuelve el `DataType` |
| `IdentifierList` | `ID` \| `IdentifierList ',' ID` | Un `ID_NODE` por identificador (`newIdentifierNode`) |

**`VOID` no está en `Type`.** Es deliberado: `void` solo es válido como tipo de
retorno, no como tipo de variable. Al dejarlo afuera de `Type`, `void x;` es un
error **sintáctico** y no hace falta ninguna regla semántica que lo prohíba.
Verificado: da `error de sintaxis`.

**`int a, b, c;` declara tres variables del mismo tipo.** El tipo se parsea una
vez y `resolveVariableDefinition` lo propaga, porque en el momento de reducir
`IdentifierList` el tipo todavía no se conoce: por eso los `ID_NODE` se crean con
`TYPE_VOID` provisorio.

### Métodos

| No terminal | Producciones | Acción |
| --- | --- | --- |
| `MethodDeclaration` | `Type ID '(' ParameterList ')' Block` \| `VOID ID '(' ParameterList ')' Block` | `newMethodDeclaration` |
| `ParameterList` | `%empty` \| `Parameters` | `newParametersNode`: siempre produce un `PARAMETERS_NODE`, vacío o con hijos |
| `Parameters` | `Type ID` \| `Parameters ',' Type ID` | `newParameterDeclaration` por parámetro |

**`ParameterList` existe para que el nodo siempre esté.** Un método sin
parámetros igual tiene su `PARAMETERS_NODE` vacío, en lugar de un `NULL`. Eso
permite que todas las fases siguientes accedan a `children[0]` sin chequear, y es
la razón de que el layout de `METHOD_DECLARATION_NODE` sea siempre
`[parameters, body]`.

**Cada parámetro repite su tipo.** `int f(int a, int b)` y no `int f(int a, b)`:
la producción es `Parameters ',' Type ID`, con `Type` obligatorio.

### Bloques y sentencias

| No terminal | Producciones | Acción |
| --- | --- | --- |
| `Block` | `'{' VariableDeclarations Statements '}'` | `newBlockNode`: un `BLOCK_NODE` con las declaraciones seguidas de las sentencias |
| `VariableDeclarations` | `%empty` \| `VariableDeclarations VariableDeclaration` | Lista |
| `Statements` | `%empty` \| `Statements Statement` | Lista |

**La regla de `Block` es la que impone que las variables se declaren al
principio.** No es una regla semántica: está en la gramática. `VariableDeclarations`
viene antes de `Statements` y no se pueden intercalar, así que
`{ int a; a=1; int b; }` es un error **sintáctico**. Verificado.

Es una decisión defendible: lo que se puede atrapar en la gramática no necesita
código de chequeo después.

| `Statement` | Produce |
| --- | --- |
| `Assignment ';'` | El nodo que armó `Assignment`, ver abajo |
| `MethodCall ';'` | El propio nodo de llamada, como sentencia |
| `RETURN Expression ';'` | `newReturnNode($2)`: `RETURN_NODE` con la expresión |
| `RETURN ';'` | `newReturnNode(NULL)`: `RETURN_NODE` sin hijos |
| `Block` | Un bloque anidado es una sentencia válida |
| `WHILE '(' Expression ')' Block` | `newWhileNode($3, $5)`: `WHILE_NODE` |
| `IF '(' Expression ')' Block` | `newIfElseNode($3, $5, NULL)`: `IF_ELSE_NODE` sin rama else |
| `IF '(' Expression ')' Block ELSE Block` | `newIfElseNode($3, $5, $7)`: `IF_ELSE_NODE` con las dos ramas |
| `';'` | `NULL`: la sentencia vacía no genera nodo |

Cada acción llama a un **constructor con nombre** de [`ast.c`](04-ast.md) en lugar
de armar el nodo con `newNode` y sus tres hijos posicionales: la acción dice qué
construcción es, y el orden de los hijos queda en un solo lugar.

| No terminal | Producción | Acción |
| --- | --- | --- |
| `Assignment` | `ID '=' Expression` | `newAssignmentNode($1, $3)`: `ASSIGNMENT_NODE` con el símbolo provisorio del destino |

**Por qué la asignación es un no terminal aparte.** La producción podría estar
escrita directamente dentro de `Statement` como `ID '=' Expression ';'`, y así
estaba antes. Separarla no cambia el lenguaje aceptado ni el árbol que se
construye: `Statement : Assignment ';'` reconoce exactamente lo mismo.

Lo que gana es **extensibilidad**. Una asignación sin el punto y coma es la
pieza que hace falta para un `for`:

```
FOR '(' Assignment ';' Expression ';' Assignment ')' Block
```

Con la producción embebida en `Statement` eso no se puede escribir, porque
`Statement` ya incluye el `';'`. Teniendo `Assignment` como no terminal propio,
la cabecera del `for` lo reusa tal cual, y la acción puede desazucararlo a un
`while` con los nodos que ya existen (ver §10). Es una preparación para esa
extensión, hecha al costo de una producción trivial.

**La sentencia vacía devuelve `NULL` y desaparece.** No hace falta un caso
especial en ninguna fase posterior porque `appendNode` ignora los nodos `NULL`:

```c
NodeArray *appendNode(NodeArray *array, NodeAST *node) {
    if (node == NULL) return array;
    ...
```

Esa misma función crea el arreglo si recibe `NULL`, que es lo que hace funcionar
a los `%empty` que devuelven `NULL`. Las dos guardas juntas son las que permiten
que las acciones de la gramática no tengan ni un `if`.

### Llamadas y expresiones

| No terminal | Producciones | Acción |
| --- | --- | --- |
| `MethodCall` | `ID '(' ')'` \| `ID '(' ArgumentList ')'` | `newMethodCallNode($1, NULL)` o `newMethodCallNode($1, $3)`: `METHOD_CALL_NODE` con los argumentos como hijos |
| `ArgumentList` | `Expression` \| `ArgumentList ',' Expression` | Lista de expresiones |

| `Expression` | Produce |
| --- | --- |
| `ID` | `ID_NODE` (`newIdentifierNode`) |
| `MethodCall` | Una llamada también es una expresión: por eso se puede usar su resultado |
| `FLOAT_CONST` / `BOOL_CONST` / `NUMBER` | `CONSTANT_NODE` del tipo correspondiente |
| `'-' Expression %prec UMINUS` | `OP_UMINUS` |
| `NOT Expression` | `OP_NOT` |
| `'(' Expression ')'` | **El mismo nodo de adentro**: los paréntesis no dejan rastro en el AST |
| `Expression OP Expression` × 10 | Un nodo de operación binaria por cada operador |

Los diez binarios son `+ - * / % < > == && ||`, todos con la misma forma
`newOperatorNode(OP_X, $1, $3)`.

**Los paréntesis desaparecen.** `(2 + 3) * 4` y `2 + 3 * 4` producen árboles
distintos, pero en ninguno de los dos hay un nodo "paréntesis": su efecto ya
quedó capturado en la forma del árbol. Es la diferencia entre un árbol de
parseo y un **árbol de sintaxis abstracta**.

**Que `MethodCall` sea a la vez `Statement` y `Expression`** es lo que permite
invocar un método con resultado e ignorarlo (`f(10);`), que es un requisito
explícito del lenguaje. No hace falta ninguna regla extra: se desprende de que
el mismo no terminal aparezca en los dos lugares.

---

## 7. Cómo parsea Bison

Bison genera un parser **LALR(1)**: ascendente, con un token de anticipación.

- **Ascendente** (*bottom-up*): no arranca del símbolo inicial tratando de
  derivar la entrada, sino de la entrada tratando de reducirla al símbolo
  inicial. Va apilando tokens y, cuando el tope de la pila coincide con el lado
  derecho de una producción, lo reemplaza por el izquierdo.
- **Dos operaciones**: *shift* (apilar el token siguiente) y *reduce* (aplicar
  una producción). El autómata, precalculado en una tabla, decide cuál según el
  estado y el token de anticipación.
- **`(1)`**: un solo token de anticipación. Toda la gramática tiene que ser
  decidible mirando un token hacia adelante, y es la restricción que explica casi
  todas las decisiones de forma de este archivo.

**La acción de una producción se ejecuta en el momento de reducir.** De ahí sale
la propiedad que hace que todo esto funcione: cuando se reduce
`Expression '+' Expression`, las dos subexpresiones **ya fueron reducidas**, así
que `$1` y `$3` ya son punteros a sus nodos terminados. El AST se construye de
las hojas hacia la raíz, y `astRoot` se asigna en la última reducción de todas.

Esto es también lo que vuelve imposible decidir algo que dependa de código que
todavía no se leyó, y es la razón de que un método solo pueda llamar a los
declarados antes que él.

### Ejemplo: `a = 2 + 3 * 4`

La salida real de `-target parse` muestra el árbol construido:

```
Bloque
  | Metodo: main (Retorna INT)
  |   | Parametros
  |   | Bloque
  |   |   | Declaracion Variable: a #0 (Tipo INT)
  |   |   | Asignacion a: a #0 (Tipo VOID)
  |   |   |   | Operacion: +
  |   |   |   |   | Constante INT: 2
  |   |   |   |   | Operacion: *
  |   |   |   |   |   | Constante INT: 3
  |   |   |   |   |   | Constante INT: 4
```

La multiplicación quedó **adentro** del hijo derecho de la suma: eso es la
precedencia materializada en la forma del árbol. El `#0` es el `uid` todavía sin
asignar y `Tipo VOID` el tipo todavía sin inferir: los pone el análisis semántico,
y por eso el `.sint` es el árbol *sin anotar* y el `.sem` el mismo árbol anotado.

---

## 8. Decisiones de diseño

### La gramática no tiene conflictos

Verificado con `bison -Wall`: **0 conflictos, 0 warnings**. Eso no es gratis en
una gramática LALR(1), y hay dos lugares donde podría haberlos y no los hay.

### El conflicto histórico: variable contra método

Las dos declaraciones empiezan igual, con `Type ID`. Un parser con un token de
anticipación, al terminar de leer el identificador, tiene que decidir si reducir
`ID` a `IdentifierList` (camino de variable) o dejarlo como está (camino de
método), y en la formulación original de la gramática eso producía un conflicto
*shift/reduce*.

Se resolvió **unificando las dos ramas bajo `Declaration`** y eliminando reglas
intermedias, de modo que el token de anticipación alcance para decidir:

| Anticipación después de `Type ID` | Decisión |
| --- | --- |
| `;` o `,` | Reducir: es una declaración de variable |
| `(` | Shift: es una declaración de método |

Un solo token basta, así que entra en LALR(1). El efecto visible es que se
pueden intercalar globales y métodos en cualquier orden. Verificado con un
programa que alterna `int g1; int f(...){...} int g2; boolean g3, g4; void main(){...}`.
El paso a paso de por qué ocurría el conflicto original y cómo se reformuló está desarrollado en [Explicación del conflicto en Bison](../../decisiones/explicacion_conflicto_bison.md).

### No hay problema del `else` colgante

El *dangling else* es el conflicto clásico de estas gramáticas: en
`if (a) if (b) S1 else S2`, el `else` podría pertenecer a cualquiera de los dos
`if`, y la gramática es ambigua.

Acá **no aparece**, y la razón es la forma de las reglas: el cuerpo de un `if` es
un `Block`, no un `Statement`, y `Block` obliga a llaves. Entonces el anidamiento
que genera la ambigüedad es **inexpresable**:

```c
if (a > 0) if (a > 5) { a=2; } else { a=3; }   // error de sintaxis
if (a > 0) { if (a > 5) { a=2; } else { a=3; } } // valido y sin ambiguedad
```

Verificado: el primero da `error de sintaxis`, el segundo parsea. Las llaves
delimitan sin lugar a dudas a quién pertenece el `else`, así que Bison no tiene
nada que desambiguar. **Obligar las llaves le sale gratis a la gramática y le
ahorra el conflicto más famoso del análisis sintáctico.**

### El análisis sintáctico corta en el primer error

`yyparse()` llama a `yyerror` y aborta; no hay reglas de recuperación (`error`
en las producciones). Un archivo con dos errores de sintaxis reporta uno solo.
Verificado.

Es distinto del léxico y del semántico, que acumulan y siguen. La razón es
práctica: después de un error de sintaxis el parser no sabe en qué estado está, y
seguir produce cascadas de errores inventados que confunden más de lo que ayudan.

### Un programa vacío es sintácticamente válido

`Declarations : %empty` lo permite. El archivo vacío pasa `-target parse` con
código 0 y falla en `-target sem` con `El programa no contiene la definicion del
metodo 'main'`. Verificado. Es el reparto correcto: la gramática describe la
forma, el semántico los requisitos.

---

## 9. Cómo se extiende

### Un operador binario nuevo (`!=`)

1. `lexer.l`: la regla que devuelve el token.
2. **Acá:** `%token NEQ`, agregarlo a la línea de precedencia de `EQ`
   (`%left EQ NEQ`), y una producción
   `| Expression NEQ Expression { $$ = newOperatorNode(OP_NEQ, $1, $3); }`.
3. `types.h`: `OP_NEQ` en el enum.
4. Semántico, TAC y código objeto.

Si se lo mete en una línea de precedencia equivocada compila igual y da
resultados mal agrupados, que es el bug más difícil de ver de este archivo.

### Un `for`

La pregunta central es si hace falta un nodo nuevo o alcanza con **azúcar
sintáctica** sobre lo que ya existe.

Un `for (i = 0; i < n; i = i + 1) Block` es equivalente a una asignación seguida
de un `while` cuyo cuerpo termina con el incremento. Entonces se puede escribir
la producción y, en la acción, armar ese árbol con los nodos existentes.

**Y la producción ya tiene la pieza que necesita:** el no terminal `Assignment`
de §6 es una asignación **sin** el punto y coma, que es exactamente la forma que
la cabecera de un `for` requiere en sus posiciones primera y tercera.

```
| FOR '(' Assignment ';' Expression ';' Assignment ')' Block
```

y construir un `BLOCK_NODE` que contenga `[init, WHILE_NODE(cond, [body..., incr])]`.
**Ninguna fase posterior se entera**: no hay nodo nuevo, no hay opcode nuevo, no
hay nada que tocar en el semántico, el TAC ni el generador de código. Es la
respuesta más fuerte que se puede dar a "¿cómo agregarías un `for`?", y el costo
está todo concentrado en esta ficha.

El camino alternativo es un `FOR_NODE` propio, que da mejores mensajes de error y
permite optimizaciones específicas de ciclos, a cambio de tocar las cinco fases.

### `break` y `continue`

Acá es al revés: no se pueden desazucarar, porque son saltos no locales. Hacen
falta nodos nuevos, una pila de etiquetas de ciclo en la generación de TAC, y un
chequeo semántico de que aparezcan dentro de un ciclo.

### Permitir declaraciones en cualquier parte del bloque

Cambiar la regla de `Block` por una lista única que mezcle declaraciones y
sentencias:

```
Block : '{' BlockItems '}'
BlockItems : %empty | BlockItems VariableDeclaration | BlockItems Statement
```

Es un cambio chico acá, pero mueve el problema al semántico: habría que verificar
que una variable no se use antes de su declaración dentro del mismo bloque, que
hoy es imposible por construcción.

---

## 10. Trampas y límites conocidos

- **El orden de las líneas de precedencia es de menor a mayor**, al revés de como
  lo lista la especificación del lenguaje. Invertirlo compila sin un solo aviso y
  rompe la semántica de todas las expresiones.
- **`UMINUS` no existe para el léxico.** Buscarlo en `lexer.l` no da nada, y es
  correcto: solo está declarado para colgarle una precedencia a la regla del menos
  unario.
- **Las acciones se ejecutan al reducir, no al leer.** Cualquier cosa que una
  acción quiera saber tiene que estar ya reducida. Es la razón de fondo de que no
  haya declaración adelantada de métodos en el lenguaje.
- **Los `ID_NODE` nacen con `TYPE_VOID` y `uid` 0.** No es un bug: en el momento
  de reducir todavía no hay tipo ni ámbito resuelto. Los completa el análisis
  semántico, y comparar el `.sint` con el `.sem` del mismo programa es la forma
  de verlo.
- **El mensaje de error sintáctico es siempre el mismo.** `error de sintaxis` con
  el número de línea, porque se descarta el texto de Bison. Es consistente con el
  resto del compilador pero menos informativo que el original; activar
  `%define parse.error verbose` lo mejoraría a costa de mezclar idiomas.
- **La gramática no tiene recuperación de errores.** No hay producciones con el
  token especial `error`, así que no existe la posibilidad de reportar dos
  errores de sintaxis en una corrida.

---

## 11. La teoría detrás

- **Gramática libre de contexto**: el formalismo que describe la estructura de un
  lenguaje de programación. Cada producción es una regla de reescritura. El
  léxico usa expresiones regulares porque los tokens no necesitan recursión; la
  sintaxis necesita una gramática porque sí (expresiones anidadas, bloques
  anidados).
- **Análisis ascendente** (*bottom-up*) frente a descendente (*top-down*): Bison
  genera el primero, que reduce de la entrada hacia el símbolo inicial. Las
  familias habituales son LR(0), SLR(1), **LALR(1)** y LR(1), en orden creciente
  de poder y de tamaño de tabla. LALR(1) es el punto de equilibrio clásico y es
  lo que generan `yacc` y Bison por defecto.
- **Shift/reduce y reduce/reduce**: los dos tipos de conflicto. Un *shift/reduce*
  significa que el autómata puede apilar o reducir y la tabla no alcanza para
  decidir; se resuelve con precedencias, o reformulando la gramática como se hizo
  acá con `Declaration`.
- **Precedencia y asociatividad como desambiguación**: la gramática de
  expresiones `E : E '+' E | E '*' E | ...` es **ambigua**. Se podría
  desambiguar estratificándola en varios no terminales (`Expression`, `Term`,
  `Factor`), que es la forma de un libro de texto. Acá se eligió dejarla plana y
  resolverla con declaraciones de precedencia: menos reglas, menos reducciones
  en tiempo de parseo, y la precedencia escrita en un solo lugar legible.
- **Árbol de parseo contra árbol de sintaxis abstracta**: el primero tiene un
  nodo por cada símbolo de cada producción, incluidos paréntesis y palabras
  clave; el segundo conserva solo lo que las fases siguientes necesitan. Las
  acciones de este archivo construyen directamente el segundo.
- **Gramática de atributos**: `$$` es el atributo sintetizado del lado izquierdo
  y `$1`, `$2`… los de los símbolos del lado derecho. Acá todos los atributos son
  **sintetizados** (se calculan de los hijos hacia el padre), que es lo que
  encaja naturalmente con un parser ascendente.

---

## 12. Cómo se valida

- **Suite:** `tests/test_parser.c` (356 líneas). Programas que la gramática debe
  aceptar y programas que debe rechazar, incluidos los casos de este documento.
- **Conflictos:** la ausencia de conflictos es parte de lo que hay que mantener.
  Se chequea con:

```bash
bison -Wall -d src/parser/bison.y -o /tmp/check.tab.c
```

  Si no imprime nada, la gramática sigue sin conflictos ni advertencias.

- **A mano:** `-target parse` escribe el AST sin anotar en un `.sint`, que es la
  forma de verificar agrupamiento y precedencia:

```bash
./mi_compilador -target parse examples/programa.ctds
```

---

**Siguiente:** [Acciones del Parser](03-acciones-del-parser.md)
