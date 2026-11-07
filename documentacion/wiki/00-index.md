# Wiki de Onboarding

Índice estructurado para recorrer el diseño y la implementación del compilador por etapas.

---

## Ruta de Lectura

1. [Primeros Pasos](00.5-getting-started.md)
2. [Visión General y Arquitectura](01-overview.md)
3. [Análisis Léxico](02-lexical-analysis.md) — `lexer.l`
4. [Análisis Sintáctico](02.5-syntax-analysis.md) — `bison.y`
5. [Acciones del Parser](02.7-parser-actions.md) — `parser_helpers.c`
6. [Árbol de Sintaxis Abstracta (AST)](03-ast.md) — `ast.c`
7. [Tabla de Símbolos y Ámbitos](04-symbol-table.md) — `symbol_table.c`
8. [Chequeo de Tipos](04.5-type-checker.md) — `type_checker.c`
9. [Análisis Semántico](04-semantic-analysis.md) — `semantic_analyzer.c`
10. [La Representación Intermedia (TAC)](05-tac-representation.md) — `tac_*.c`
11. [Generación de Código Intermedio](05.5-tac-generation.md) — `intermediate_code.c`
12. [Análisis de Variables Vivas](05.7-live-variables.md) — `live_variable_analysis.c`
13. [Optimización del Código Intermedio](05.9-optimizer.md) — `optimizer.c`
14. [Instrucciones x86-64](06-asm-instructions.md) — `asm_instruction.c`
15. [Generación de Código Objeto y Gestión del Stack](06-object-code.md) — `function_frame.c`, `object_code.c`

---

## Informes y Decisiones de Diseño

- [Alcance y Reglas Semánticas](../analisis-semantico.md)
- [Decisiones de Diseño del AST](../arbol_AST/decisiones_diseno_ast.md)
- [Resolución del Conflicto en Bison](../lexico-sintactico/notas/explicacion_conflicto_bison.md)
- [Criterio de Nodos en el AST](../lexico-sintactico/notas/notes_to_ast_definition.md)
