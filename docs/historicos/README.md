# Documentación Histórica (WIP)

Esta carpeta conserva material generado durante etapas tempranas e intermedias del desarrollo del compilador, preservado como respaldo histórico y de evolución del proyecto. **No refleja necesariamente el estado actual del código.**

La documentación vigente vive en:

- [`docs/wiki/`](../wiki/index.md): arquitectura y fichas detalladas de cada módulo y etapa actual.
- [`docs/decisiones/`](../decisiones/alcance-y-extensiones.md): alcance, cambios a etapas anteriores, ADRs y notas de diseño vigentes.
- [`docs/problemas-conocidos.md`](../problemas-conocidos.md): problemas conocidos y limitaciones actuales.
- [`docs/spec/`](../spec/00-TDS-proyecto.pdf): enunciado del proyecto y especificación oficial de C-TDS.

## Contenido archivado

- `analisis-semantico.md`: informe inicial de análisis semántico, reemplazado por la sección de análisis semántico en `docs/wiki/04-semantico/`.
- `arbol_AST/`: notas iniciales de `NodeList` / constructores previos y boceto Excalidraw, reemplazados por `docs/wiki/03-lexico-sintactico-y-ast/`.
- `bugs/`: notas puntuales tempranas (`asignacion_de_metodo.md`, `tipos_casteados_en_op_bin.md`) y regla revertida (`doble_return.md`); el caso relevante de ámbitos de parámetros pasó a `docs/decisiones/adr-004-ambito-metodo-y-parametros.md`.
- `interprete/` y `optimization/instant_value.png`: diagrama del intérprete eliminado y boceto de una optimización no implementada.
- `lexico-sintactico/`: informes LaTeX/PDF de la etapa 1, nota original de diseño del AST (integrada en `docs/wiki/03-lexico-sintactico-y-ast/04-ast.md`) y el reproductor mínimo en Bison del conflicto histórico `shift/reduce` (`conflicto_ejecutable/`).
- `notas/redesign_compilator.pdf`: boceto temprano previo a la refactorización del compilador.
- `solve_scope.png`, `tac.png`: diagramas tempranos en Excalidraw, reemplazados por diagramas Mermaid en la wiki.
