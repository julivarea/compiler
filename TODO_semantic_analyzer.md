# TODO List: Semantic Analyzer e Integración

- [ ] Validar declaraciones duplicadas en un mismo ámbito (Scope).
- [ ] Validar el uso de identificadores no declarados previamente (variables o métodos).
- [ ] Realizar chequeo de tipos (Type Checking) en operaciones aritméticas, lógicas y relacionales.
- [ ] Realizar chequeo de compatibilidad de tipos en sentencias de asignación.
- [ ] Validar llamadas a métodos (cantidad exacta de argumentos).
- [ ] Validar llamadas a métodos (compatibilidad de tipos en cada argumento).
- [ ] Verificar que las sentencias `return` devuelvan el tipo definido en la firma del método correspondiente.
- [ ] Crear el recorrido completo de verificación semántica sobre el AST.
- [ ] Integrar el paso de análisis semántico en el pipeline principal (después de construir el AST y la tabla de símbolos).
- [ ] Implementar un sistema de recolección y reporte de errores semánticos (idealmente con número de línea/columna).
- [ ] Escribir pruebas unitarias (o casos de prueba de integración) que fallen intencionalmente por errores semánticos.
