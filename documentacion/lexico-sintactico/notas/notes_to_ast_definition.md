### Consideracion para diseñar el AST

**Si una construcción de la gramática no aporta información propia, no conviene representarla como un nodo del AST.**

Hay que distinguir entre:

* **Construcciones que tienen significado propio** → se convierten en nodos del AST.
* **Construcciones que solamente agrupan, organizan o derivan hacia otras construcciones** → no necesitan un nodo propio; simplemente delegan en sus hijos.

Por ejemplo, `Statement` no aporta información por sí mismo. Es solamente una categoría que agrupa distintas instrucciones:

```text
Statement
├── Assignment
├── MethodCall
├── If
├── While
├── Return
└── Block
```

Por lo tanto, no necesitamos un:

```c
STATEMENT_NODE
```

Cuando el parser reconoce:

```yacc
Statement:
    Assignment
    { $$ = $1; }
```

el `Statement` simplemente **delega la construcción del AST al nodo concreto**.

Lo mismo ocurre con `Expression`:

```text
Expression
├── ID
├── Literal
├── MethodCall
├── Binary
├── Unary
└── ...
```

No necesitamos un `EXPRESSION_NODE`. El resultado de `Expression` ya es directamente el nodo concreto correspondiente.

### Caso particular: Block

Aunque `Block` aparece dentro de `Statement`, **sí tiene información propia**, porque representa una estructura concreta del programa: contiene declaraciones y sentencias y además delimita un ámbito.

Por eso:

```text
Statement → Block
```

puede simplemente hacer:

```yacc
{ $$ = $1; }
```

pero `Block` crea su propio:

```c
BLOCK_NODE
```

Es decir:

> **No importa que una construcción aparezca como una alternativa de otra. Lo importante es si aporta información semántica propia al AST.**

La pregunta mental es:

> **¿Esta construcción representa algo que necesito conservar, o solamente me ayuda a llegar a algo que sí representa algo?**

Si solamente ayuda a llegar al nodo real, **desaparece del AST y delega**.

Si tiene información propia que será útil para las siguientes etapas del compilador, **se conserva como nodo**.

