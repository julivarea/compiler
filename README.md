# C-TDS Compiler

Compilador desde cero para C-TDS (TDS26), un lenguaje de programación
imperativo simple, desarrollado como proyecto de la materia Taller de Diseño
de Software (Cod. 3306).

El proyecto se aborda de forma incremental: esta primera entrega cubre el
análisis léxico y sintáctico del lenguaje, usando Flex y Bison, junto con la
interfaz del árbol de sintaxis abstracta (AST) que van a usar las próximas
etapas.

## Autores

- Agustín Alieni
- Fran Natale
- Julián Varea

## Estado actual

- **Análisis léxico** (`lexer.l`): reconoce todos los tokens del lenguaje
  (palabras reservadas, identificadores, constantes numéricas y booleanas,
  operadores, delimitadores y comentarios), con número de línea en los
  mensajes de error.
- **Análisis sintáctico** (`bison.y`): reconoce la gramática completa de
  C-TDS (declaraciones de variables y métodos intercaladas, bloques,
  condicionales, ciclos, llamadas a método, expresiones con precedencia de
  operadores), sin conflictos shift/reduce ni reglas inalcanzables.
- **AST** (`ast.h`, `ast.c`): por ahora solo está definida la interfaz. Las
  firmas y la documentación de cada función están en `ast.h`; los cuerpos en
  `ast.c` están vacíos a propósito, y las acciones semánticas de `bison.y`
  están escritas pero comentadas, a la espera de cerrar en equipo algunas
  decisiones de diseño antes de implementarlas.

Los detalles y las decisiones de diseño de esta etapa (el conflicto
shift/reduce que resolvimos, por qué separamos el AST de la gramática, el
criterio para decidir cuándo un nodo se representa en el árbol, etc.) están
documentados en
[`documentation/syntactic analyzer/analizador_sintactico.pdf`](documentation/syntactic%20analyzer/analizador_sintactico.pdf).

## Estructura del repositorio

```
.
├── lexer.l                                          # Analizador léxico (Flex)
├── bison.y                                          # Analizador sintáctico (Bison)
├── ast.h                                            # Interfaz del AST
├── ast.c                                            # Implementación del AST (pendiente)
└── documentation/
    └── syntactic analyzer/
        └── analizador_sintactico.pdf                # Documentación de esta etapa
```

## Compilación

```bash
to do julian varea
```

## Uso

```bash
to do julian varea
```
