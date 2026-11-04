# Wiki de Onboarding

Índice estructurado para recorrer el diseño y la implementación del compilador por etapas.

---

## Ruta de Lectura

1. [Primeros Pasos](00.5-getting-started.md)
2. [Visión General y Arquitectura](01-overview.md)
3. [Análisis Léxico](02-lexical-analysis.md) — `lexer.l`
4. [Análisis Sintáctico](02.5-syntax-analysis.md) — `bison.y`
5. [Acciones del Parser](02.7-parser-actions.md) — `parser_helpers.c`
6. [Árbol de Sintaxis Abstracta (AST)](03-ast.md)
7. [Análisis Semántico y Tabla de Símbolos](04-semantic-analysis.md)
8. [Generación de Código Intermedio (TAC)](05-intermediate-code.md)
9. [Generación de Código Objeto y Gestión del Stack](06-object-code.md)
10. [Live Variable Analysis](../live_variable_analysis.md)

---

## Informes y Decisiones de Diseño

- [Alcance y Reglas Semánticas](../analisis-semantico.md)
- [Decisiones de Diseño del AST](../arbol_AST/decisiones_diseno_ast.md)
- [Resolución del Conflicto en Bison](../lexico-sintactico/notas/explicacion_conflicto_bison.md)
- [Criterio de Nodos en el AST](../lexico-sintactico/notas/notes_to_ast_definition.md)
