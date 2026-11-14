# Análisis Sintáctico

**Archivo:** `src/parser/bison.y` (162 líneas)

Consume la secuencia de tokens producida por el [análisis léxico](01-analisis-lexico.md), verifica que conforme un programa válido según la gramática libre de contexto de C-TDS y construye el árbol de sintaxis abstracta (AST).

---

## 1. Qué problema resuelve

Determinar si la secuencia de tokens respeta la gramática del lenguaje y materializar su estructura jerárquica (precedencia de operadores, anidamiento de bloques, firma y cuerpo de métodos) en un AST cuya raíz queda en `astRoot`.

---

## 2. Interfaz pública

Bison genera `build/bison.tab.c` y `build/bison.tab.h` a partir de `src/parser/bison.y`:

| Símbolo | Qué es | Quién lo usa |
| --- | --- | --- |
| `yyparse()` | Ejecuta el análisis sintáctico. Devuelve `0` si acepta la entrada o `1` ante error | `runParse` en `driver.c` |
| `yyerror(const char *)` | Callback invocado por Bison al detectar un error sintáctico | Definido en `bison.y`, llamado internamente por `yyparse()` |
| `astRoot` | Puntero global a la raíz del AST construido | Escrito por la producción `Program`; leído por las fases posteriores |
| `yylex()` | Declarado `extern`, provisto por Flex | Llamado internamente por `yyparse()` |
| `bison.tab.h` | Definiciones numéricas de tokens y declaración de `yylval` | Incluido por `lexer.l` |

La dependencia mutua entre `lexer.l` y `bison.y` se resuelve ejecutando `bison -d` antes de `flex` en `build.sh`, de modo que `bison.tab.h` exista al compilar el escáner.

---

## 3. Prólogo y configuración

### Bloque `%{ ... %}`

Se copia al inicio de `bison.tab.c` con los `#include`, declaraciones `extern` y `yyerror`:

```c
static void yyerror(const char *s) {
    (void)s;
    syntaxError(yylineno, "error de sintaxis");
}
```

El texto en inglés que genera Bison (`s`) se descarta para mantener todos los diagnósticos del compilador en español y bajo el formato unificado de `error_handler`.

### Definición condicional de `astRoot` (`#ifdef UNITY_TESTING`)

```c
#ifdef UNITY_TESTING
NodeAST *astRoot = NULL;
#else
extern NodeAST *astRoot;
#endif
```

En el binario del compilador, `astRoot` se define en `main.c`. Como los ejecutables de tests unitarios definen su propio `main` y no enlazan `main.c`, `UNITY_TESTING` provee la definición del símbolo directamente en `bison.tab.c`.

### Bloque `%code requires`

```
%code requires {
#include "ast.h"
#include "parser_helpers.h"
}
```

A diferencia de `%{ ... %}`, su contenido se inserta en `bison.tab.h`. Es necesario porque la `%union` referencia `struct NodeAST`, `struct NodeArray` y `DataType`, tipos que deben estar visibles cuando `lexer.l` incluye el encabezado.

---

## 4. Declaraciones de símbolos y tipos

### Unión de valores semánticos (`%union`)

| Campo | Símbolos asociados | Origen |
| --- | --- | --- |
| `int intval` | `NUMBER`, `BOOL_CONST` | Analizador léxico |
| `float floatval` | `FLOAT_CONST` | Analizador léxico |
| `char *strval` | `ID` | Analizador léxico (`strdup`) |
| `struct NodeAST *node` | No terminales que producen un único nodo del AST | Acciones sintácticas |
| `struct NodeArray *array` | No terminales que acumulan una lista de nodos | Acciones sintácticas ([acciones del parser](03-acciones-del-parser.md)) |
| `DataType dataType` | `Type` | Acciones sintácticas |

### Tokens y no terminales

```
%token VOID INT FLOAT BOOLEAN IF ELSE WHILE RETURN
%token EQ AND OR NOT
%token <strval> ID
%token <intval> NUMBER BOOL_CONST
%token <floatval> FLOAT_CONST

%type <node>     Program Statement Expression MethodCall Block MethodDeclaration
                 ParameterList Assignment
%type <dataType> Type
%type <array>    VariableDeclaration IdentifierList ArgumentList VariableDeclarations
                 Statements Parameters Declarations Declaration
```

Los operadores y delimitadores de un carácter (`+`, `<`, `;`, `(`, etc.) se referencian directamente por su literal de carácter en las reglas sin declaración `%token`. Los no terminales se dividen entre aquellos que construyen un `NodeAST` (`Block`, `ParameterList`) y aquellos que acumulan listas intermedias en un `NodeArray` (`Statements`, `Parameters`, `Declarations`).

---

## 5. Precedencia y asociatividad

En lugar de estratificar la gramática de expresiones en múltiples no terminales (`Term`, `Factor`, etc.), `Expression` se define con reglas planas (`Expression OP Expression`) y la ambigüedad se resuelve mediante declaraciones explícitas de precedencia. Esto reduce la cantidad de producciones y evita reducciones intermedias de un solo hijo.

En Bison, el orden de las directivas va de **menor a mayor precedencia** (inverso al orden de mayor a menor en que suele presentarse en la especificación del lenguaje):

```
%left OR                /* || (menor precedencia) */
%left AND               /* && */
%left EQ                /* == */
%left '<' '>'
%left '+' '-'
%left '*' '/' '%'
%precedence NOT UMINUS  /* ! y menos unario (mayor precedencia) */
```

- **Asociatividad izquierda (`%left`):** expresiones como `10 - 3 - 2` o `100 / 10 / 2` se agrupan como `(10 - 3) - 2` y `(100 / 10) / 2`.
- **Pseudo-token `UMINUS`:** el token `'-'` participa tanto en la resta binaria como en la negación unaria, pero `-3 * 4` debe agruparse como `(-3) * 4`. Se declara el token ficticio `UMINUS` (que el léxico nunca emite) en el nivel más alto y se anota la regla unaria con `%prec UMINUS`:
  ```
  | '-' Expression %prec UMINUS   { $$ = newOperatorNode(OP_UMINUS, $2, NULL); }
  ```
- **`%precedence` para operadores unarios:** asigna precedencia sin definir asociatividad, ya que los operadores prefijos no tienen dos operandos adyacentes que desambiguar, pero siguen admitiendo anidamiento (`- -a`, `!!true`) por la recursión derecha de sus producciones (`NOT Expression`).

---

## 6. Producciones de la gramática

El detalle de los acumuladores de listas se documenta en [acciones del parser](03-acciones-del-parser.md) y el de los constructores de nodos en [ast.c](04-ast.md).

### Estructura del programa y declaraciones globales

| No terminal | Producciones | Acción |
| --- | --- | --- |
| `Program` | `Declarations` | Crea el `BLOCK_NODE` raíz con `newBlockNode($1)` y lo asigna a `astRoot` |
| `Declarations` | `%empty` \| `Declarations Declaration` | Devuelve `NULL` o concatena con `mergeNodeArrays($1, $2)` |
| `Declaration` | `VariableDeclaration` \| `MethodDeclaration` | Pasa la lista de variables o envuelve el método con `appendNode(NULL, $1)` |

Las listas usan **recursión izquierda** (`Declarations Declaration`), que en un parser ascendente permite reducir cada elemento a medida que se consume sin apilar la totalidad de la entrada en la pila del autómata.

### Declaraciones de variables

| No terminal | Producciones | Acción |
| --- | --- | --- |
| `VariableDeclaration` | `Type IdentifierList ';'` | `resolveVariableDefinition($1, $2)`: asigna el tipo a todos los identificadores de la lista |
| `Type` | `INT` \| `BOOLEAN` \| `FLOAT` | Devuelve el `DataType` correspondiente |
| `IdentifierList` | `ID` \| `IdentifierList ',' ID` | Crea un `ID_NODE` provisorio (`TYPE_VOID`) por cada identificador |

- **`VOID` no pertenece a `Type`:** `void` solo es válido en el encabezado de `MethodDeclaration`. Una declaración como `void x;` se rechaza directamente como error sintáctico.
- **Declaraciones múltiples (`int a, b, c;`):** cuando se reduce `IdentifierList`, el parser aún no redujo `VariableDeclaration` y la acción de `IdentifierList` no tiene acceso al `Type`. Por eso cada identificador se instancia inicialmente como `ID_NODE` con `TYPE_VOID`, y `resolveVariableDefinition` los transforma en nodos `VARIABLE_DECLARATION_NODE` tipados al reducir la declaración completa.

### Declaraciones de métodos

| No terminal | Producciones | Acción |
| --- | --- | --- |
| `MethodDeclaration` | `Type ID '(' ParameterList ')' Block` \| `VOID ID '(' ParameterList ')' Block` | `newMethodDeclaration` |
| `ParameterList` | `%empty` \| `Parameters` | `newParametersNode(NULL)` o `newParametersNode($1)` |
| `Parameters` | `Type ID` \| `Parameters ',' Type ID` | `newParameterDeclaration` por cada parámetro |

`ParameterList` construye siempre un `PARAMETERS_NODE` (incluso con 0 hijos cuando no hay parámetros), garantizando que `METHOD_DECLARATION_NODE` tenga invariablemente dos hijos `[parameters, body]`.

### Bloques y sentencias

| No terminal | Producciones | Acción |
| --- | --- | --- |
| `Block` | `'{' VariableDeclarations Statements '}'` | `newBlockNode(mergeNodeArrays($2, $3))` |
| `VariableDeclarations` | `%empty` \| `VariableDeclarations VariableDeclaration` | Acumula declaraciones con `mergeNodeArrays` |
| `Statements` | `%empty` \| `Statements Statement` | Acumula sentencias con `appendNode` |
| `Assignment` | `ID '=' Expression` | `newAssignmentNode($1, $3)` |

La producción `Block : '{' VariableDeclarations Statements '}'` impone por gramática que todas las declaraciones locales precedan a las sentencias del bloque; intercalar una declaración luego de una sentencia (`{ int a; a = 1; int b; }`) es un error sintáctico.

| Producción de `Statement` | Resultado |
| --- | --- |
| `Assignment ';'` | Nodo `ASSIGNMENT_NODE` construido por `Assignment` |
| `MethodCall ';'` | Nodo `METHOD_CALL_NODE` usado como sentencia |
| `RETURN Expression ';'` | `newReturnNode($2)` (`RETURN_NODE` con 1 hijo) |
| `RETURN ';'` | `newReturnNode(NULL)` (`RETURN_NODE` con 0 hijos) |
| `Block` | `BLOCK_NODE` anidado |
| `WHILE '(' Expression ')' Block` | `newWhileNode($3, $5)` |
| `IF '(' Expression ')' Block` | `newIfElseNode($3, $5, NULL)` (2 hijos) |
| `IF '(' Expression ')' Block ELSE Block` | `newIfElseNode($3, $5, $7)` (3 hijos) |
| `';'` | `NULL` (descartado por `appendNode` sin generar nodo en el AST) |

Separar `Assignment` (sin el `';'` final) como no terminal propio no altera el lenguaje aceptado por `Statement : Assignment ';'`, pero permite reutilizar la asignación en construcciones como el encabezado de un ciclo `for` (ver §9).

### Llamadas y expresiones

| No terminal | Producciones | Acción |
| --- | --- | --- |
| `MethodCall` | `ID '(' ')'` \| `ID '(' ArgumentList ')'` | `newMethodCallNode($1, NULL)` o `newMethodCallNode($1, $3)` |
| `ArgumentList` | `Expression` \| `ArgumentList ',' Expression` | Acumula las expresiones de los argumentos |
| `Expression` | `ID` | `newIdentifierNode($1)` |
| | `MethodCall` | Permite usar el valor de retorno de un método dentro de una expresión |
| | `FLOAT_CONST` / `BOOL_CONST` / `NUMBER` | `newFloatLiteralNode` / `newBoolLiteralNode` / `newIntLiteralNode` |
| | `'-' Expression %prec UMINUS` / `NOT Expression` | `newOperatorNode(OP_UMINUS, $2, NULL)` / `newOperatorNode(OP_NOT, $2, NULL)` |
| | `'(' Expression ')'` | `$$ = $2` (los paréntesis agrupan pero no generan nodo en el AST) |
| | `Expression OP Expression` (10 operadores binarios) | `newOperatorNode(OP_X, $1, $3)` para `+`, `-`, `*`, `/`, `%`, `<`, `>`, `==`, `&&`, `\|\|` |

La producción `'(' Expression ')' { $$ = $2; }` refleja la diferencia entre árbol de parseo y AST: los paréntesis determinan qué reducción se ejecuta primero, pero no se representan como nodos porque la jerarquía resultante ya codifica el agrupamiento.

---

## 7. Construcción ascendente del AST

Bison genera un analizador **LALR(1)**: ascendente (*bottom-up*, mediante transiciones *shift* y *reduce*) con un único token de anticipación (*lookahead*).

Las acciones semánticas se ejecutan en el instante de cada **reducción**, calculando atributos sintetizados (`$$`) a partir de los valores de los símbolos del lado derecho (`$1`, `$2`, …). Cuando se reduce una producción como `Expression '+' Expression`, ambas subexpresiones ya fueron reducidas previamente y sus punteros `NodeAST *` están construidos. El AST se ensambla desde las hojas hacia la raíz, asignando `astRoot` en la reducción final de `Program`.

Para `a = 2 + 3 * 4`, la salida de `-target parse` muestra cómo la mayor precedencia de `*` reduce primero `3 * 4` y lo ubica como hijo derecho de `+`:

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

En el archivo `.sint`, los símbolos todavía tienen `#0` (`uid` sin asignar) y las expresiones `Tipo VOID`; ambos campos son completados luego por el análisis semántico en `.sem`.

---

## 8. Decisiones de diseño

### Gramática libre de conflictos

`bison -Wall` compila `src/parser/bison.y` con **0 conflictos *shift/reduce* y 0 conflictos *reduce/reduce***.

### Resolución del prefijo común entre variables y métodos (`Type ID`)

Tanto `VariableDeclaration` como `MethodDeclaration` comienzan con `Type ID`. En un parser con 1 token de anticipación, obligar a reducir `Type` o `ID` a un no terminal específico antes de ver el token siguiente provocaba un conflicto *shift/reduce*. La gramática actual evita reducciones prematuras antes de consumir `ID`, de modo que el token de anticipación posterior a `Type ID` desambigua el camino en LALR(1):

| Token de anticipación tras `Type ID` | Acción del autómata |
| --- | --- |
| `;` o `,` | Reduce `ID` a `IdentifierList` (declaración de variable) |
| `(` | Hace *shift* de `'('` (declaración de método) |

Esto permite además intercalar declaraciones de variables globales y métodos en cualquier orden. El análisis detallado del conflicto original y su resolución se encuentra en [Explicación del conflicto en Bison](../../decisiones/explicacion_conflicto_bison.md).

### Ausencia de ambigüedad *dangling else*

En gramáticas donde las ramas de `if` son `Statement` arbitrarios, `if (a) if (b) S1 else S2` es ambiguo porque el `else` puede asociarse a cualquiera de los dos `if`. En C-TDS las ramas de `IF` y `WHILE` son obligatoriamente `Block` (delimitados por llaves `{ ... }` según la gramática):

```c
if (a > 0) if (a > 5) { a = 2; } else { a = 3; }       // error de sintaxis
if (a > 0) { if (a > 5) { a = 2; } else { a = 3; } }   // válido y sin ambigüedad
```

Al exigir llaves en la gramática, la asociación del `else` queda determinada por los delimitadores de bloque sin necesidad de directivas de desambiguación.

### Corte en el primer error y programa vacío

- **Sin recuperación de errores:** no se definen reglas con el token especial `error`; ante el primer error sintáctico, `yyparse()` invoca `yyerror` y termina devolviendo `1`, evitando cascadas de diagnósticos espurios.
- **Programa vacío sintácticamente válido:** `Declarations : %empty` acepta un archivo sin declaraciones en `-target parse`; la exigencia del método `main` se verifica en el análisis semántico (`-target sem`).

---

## 9. Cómo se extiende

### Agregar un operador binario (`!=`)

1. `lexer.l`: devolver el token `NEQ`.
2. `bison.y`: declarar `%token NEQ`, agregarlo en el nivel de precedencia correspondiente (`%left EQ NEQ`) y añadir la producción `| Expression NEQ Expression { $$ = newOperatorNode(OP_NEQ, $1, $3); }`.
3. `types.h`, `ast.c`, semántico, TAC y código objeto: incorporar `OP_NEQ`.

### Agregar un ciclo `for`

Puede implementarse de dos formas:

- **Desazucarado sintáctico (*syntactic sugar*) en `bison.y`:** reutilizando el no terminal `Assignment` de §6 (que no consume `';'`), se define la producción:
  ```
  | FOR '(' Assignment ';' Expression ';' Assignment ')' Block
  ```
  La acción construye un `BLOCK_NODE` con `[init, WHILE_NODE(cond, [body..., incr])]` usando los constructores existentes. No requiere nuevos tipos de nodo ni cambios en el análisis semántico, TAC o generación de código objeto.
- **Nodo dedicado (`FOR_NODE`):** preserva la construcción original para diagnósticos u optimizaciones específicas de bucle, pero requiere extender `NodeType`, el analizador semántico, el generador de TAC y el backend.

### Agregar `break` y `continue`

No pueden desazucararse localmente en el AST porque representan saltos hacia etiquetas del ciclo envolvente. Requieren nuevos valores en `NodeType`, validación en el analizador semántico de que aparezcan dentro de un ciclo y una pila de etiquetas de inicio/salida de bucle durante la generación de TAC.

### Permitir declaraciones intercaladas con sentencias dentro de un bloque

Reemplazar la regla de `Block` por una lista unificada:

```
Block : '{' BlockItems '}'
BlockItems : %empty | BlockItems VariableDeclaration | BlockItems Statement
```

Esto traslada al análisis semántico la responsabilidad de verificar el orden declaración-antes-de-uso dentro del bloque.

---

## 10. Consideraciones y límites

- **Orden de precedencia ascendente:** las directivas `%left` y `%precedence` van de menor a mayor precedencia; ubicar un operador nuevo en la fila incorrecta compila sin advertencias pero altera el árbol generado.
- **Reducción de izquierda a derecha:** las acciones sintácticas solo disponen de subárboles ya reducidos, en línea con la regla semántica de C-TDS que exige declarar un método antes de invocarlo.
- **Nodos `ID_NODE` sin anotar tras el parseo:** en la etapa sintáctica aún no existe resolución de ámbitos ni inferencia de tipos, por lo que las referencias nacen con `TYPE_VOID` y `uid = 0`.
- **Mensaje único de error sintáctico:** `yyerror` emite siempre `"error de sintaxis"` con el número de línea actual.

---

## 11. Cómo se valida

- **Tests unitarios:** `tests/test_parser.c` prueba programas válidos e inválidos (precedencia, bloques sin llaves, `void` en variables, declaraciones intercaladas, estructuras de control).
- **Verificación de conflictos en la gramática:**

```bash
bison -Wall -d src/parser/bison.y -o /tmp/check.tab.c
```

- **Inspección manual del árbol sin anotar (`.sint`):**

```bash
./mi_compilador -target parse examples/valido_completo.ctds
```

---

**Siguiente:** [Acciones del Parser](03-acciones-del-parser.md)
