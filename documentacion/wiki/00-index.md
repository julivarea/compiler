# Wiki de Onboarding

Índice estructurado para recorrer el diseño y la implementación del compilador por etapas.

---

## Ruta de Lectura

**Infraestructura** — lo que no es ninguna fase, pero todas usan. Contesta qué
pasa entre que escribís `./mi_compilador programa.ctds` y que el lexer empieza a
leer.

1. [Primeros Pasos](00.5-getting-started.md)
2. [Visión General y Arquitectura](01-overview.md)
3. [Entrada y Línea de Comandos](01.2-cli.md) — `main.c`, `cli.c`
4. [Orquestación de las Etapas](01.4-driver.md) — `driver.c`
5. [Errores y Salida de Debug](01.6-errors.md) — `error_handler.c`, `debug.h`
6. [Utilidades y Tipos Base](01.8-utilities.md) — `string_builder.c`, `types.h`

**Las fases**, en el orden en que se ejecutan.

7. [Análisis Léxico](02-lexical-analysis.md) — `lexer.l`
8. [Análisis Sintáctico](02.5-syntax-analysis.md) — `bison.y`
9. [Acciones del Parser](02.7-parser-actions.md) — `parser_helpers.c`
10. [Árbol de Sintaxis Abstracta (AST)](03-ast.md) — `ast.c`
11. [Tabla de Símbolos y Ámbitos](04-symbol-table.md) — `symbol_table.c`
12. [Chequeo de Tipos](04.5-type-checker.md) — `type_checker.c`
13. [Análisis Semántico](04-semantic-analysis.md) — `semantic_analyzer.c`
14. [La Representación Intermedia (TAC)](05-tac-representation.md) — `tac.c`
15. [Generación de Código Intermedio](05.5-tac-generation.md) — `intermediate_code.c`
16. [Análisis de Variables Vivas](05.7-live-variables.md) — `live_variable_analysis.c`
17. [Optimización del Código Intermedio](05.9-optimizer.md) — `optimizer.c`
18. [Marco de Pila](06.5-stack-frame.md) — `function_frame.c`
19. [Generación de Código Objeto](06-object-code.md) — `object_code.c`

---

## Informes y Decisiones de Diseño

- [Alcance y Reglas Semánticas](../analisis-semantico.md)
- [Decisiones de Diseño del AST](../arbol_AST/decisiones_diseno_ast.md)
- [Resolución del Conflicto en Bison](../lexico-sintactico/notas/explicacion_conflicto_bison.md)
- [Criterio de Nodos en el AST](../lexico-sintactico/notas/notes_to_ast_definition.md)
