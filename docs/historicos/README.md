# Documentación Histórica (WIP)

Esta carpeta conserva material generado durante etapas tempranas e intermedias del desarrollo del compilador, preservado como respaldo histórico y de evolución del proyecto. **No refleja necesariamente el estado actual del código.**

La documentación vigente vive en:

- [`docs/wiki/`](../wiki/00-index.md): arquitectura y fichas detalladas de cada módulo y etapa actual.
- [`docs/decisiones/`](../decisiones/adr-001-desacoplamiento-tac-offsets-stack.md): ADRs y notas de diseño vigentes.
- [`docs/bugs/`](../bugs/asignacion_de_metodo.md): bugs resueltos vigentes.
- [`docs/spec/`](../spec/00-TDS-proyecto.pdf): enunciado del proyecto y especificación oficial de C-TDS.

## Contenido archivado

- `analisis-semantico.md`: informe inicial de análisis semántico, reemplazado por las fichas F09–F11 en `docs/wiki/`.
- `arbol_AST/`: notas iniciales de `NodeList` / constructores previos y boceto Excalidraw, reemplazados por las fichas F07–F08.
- `bugs/doble_return.md`: nota de una regla revertida (permitir múltiples `return` en ramas o código inalcanzable; reemplazado por `guaranteesReturn` en `semantic_analyzer.c`).
- `interprete/` y `optimization/instant_value.png`: diagrama del intérprete eliminado y boceto de una optimización no implementada.
- `lexico-sintactico/`: informes LaTeX/PDF de la etapa 1, nota original de diseño del AST (ya integrada en `docs/wiki/03-ast.md`) y el reproductor mínimo en Bison del conflicto histórico `shift/reduce` (`conflicto_ejecutable/`).
- `notas/redesign_compilator.pdf`: boceto temprano previo a la refactorización del compilador.
- `solve_scope.png`, `tac.png`: diagramas tempranos en Excalidraw, reemplazados por diagramas Mermaid en la wiki.
