# TODO List: Semantic Analyzer

De acuerdo a la nueva definición, el análisis semántico se limita exclusivamente a:

- [X] Validar que las variables que estamos llamando estén válidas (no usar identificadores no declarados, no re-declarar en el mismo scope).
- [ ] Validar que los tipos estén definidos (existencia de tipos).
- [ ] Validar que los tipos de las expresiones en una asignación/definición coincidan o sean compatibles con la variable.

Tareas transversales:
- [X] Crear el recorrido completo de verificación semántica sobre el AST.
- [X] Integrar el paso de análisis semántico en el pipeline principal.
- [X] Implementar un sistema de recolección y reporte de errores semánticos.
- [ ] Escribir pruebas unitarias que fallen intencionalmente por errores semánticos basados en estas tres reglas.
