# Wiki del Compilador

Documentación técnica detallada de la arquitectura y de cada módulo del compilador, agrupada siguiendo el pipeline de ejecución y la estructura de directorios de `src/`.

---

## 1. Introducción

- [1.1 Primeros Pasos](01-introduccion/01-primeros-pasos.md) — prerrequisitos, compilación, tests con Unity y ejecución
- [1.2 Visión General y Arquitectura](01-introduccion/02-vision-general.md) — pipeline de 7 fases, flujo de datos y diagramas UML

## 2. Infraestructura (`src/core/` + `src/main.c`)

- [2.1 Entrada y Línea de Comandos](02-infraestructura/01-cli-y-entrada.md) — `main.c`, `cli.c`
- [2.2 Orquestación de Etapas](02-infraestructura/02-driver.md) — `driver.c`
- [2.3 Errores y Salida de Debug](02-infraestructura/03-errores-y-debug.md) — `error_handler.c`, `debug.h`
- [2.4 Utilidades y Tipos Base](02-infraestructura/04-utilidades-y-tipos.md) — `string_builder.c`, `types.h`

## 3. Léxico, Sintáctico y AST (`src/parser/`)

- [3.1 Análisis Léxico](03-lexico-sintactico-y-ast/01-analisis-lexico.md) — `lexer.l`
- [3.2 Análisis Sintáctico](03-lexico-sintactico-y-ast/02-analisis-sintactico.md) — `bison.y`
- [3.3 Acciones del Parser](03-lexico-sintactico-y-ast/03-acciones-del-parser.md) — `parser_helpers.c`
- [3.4 Árbol de Sintaxis Abstracta (AST)](03-lexico-sintactico-y-ast/04-ast.md) — `ast.c`

## 4. Análisis Semántico (`src/semantic/`)

- [4.1 Tabla de Símbolos y Ámbitos](04-semantico/01-tabla-de-simbolos.md) — `symbol_table.c`
- [4.2 Chequeo de Tipos](04-semantico/02-chequeo-de-tipos.md) — `type_checker.c`
- [4.3 Análisis Semántico](04-semantico/03-analisis-semantico.md) — `semantic_analyzer.c`

## 5. Código Intermedio (`src/intermediate_code/`)

- [5.1 Representación Intermedia (TAC)](05-codigo-intermedio/01-representacion-tac.md) — `tac.c`
- [5.2 Generación de Código Intermedio](05-codigo-intermedio/02-generacion-tac.md) — `intermediate_code.c`

## 6. Optimizador (`src/optimizer/`)

- [6.1 Análisis de Variables Vivas](06-optimizador/01-variables-vivas.md) — `live_variable_analysis.c`
- [6.2 Optimización del Código Intermedio](06-optimizador/02-optimizador.md) — `optimizer.c`

## 7. Código Objeto (`src/object_code/`)

- [7.1 Marco de Pila](07-codigo-objeto/01-marco-de-pila.md) — `function_frame.c`
- [7.2 Generación de Código Objeto](07-codigo-objeto/02-generacion-codigo-objeto.md) — `object_code.c`

---

## Secciones Complementarias

- **Decisiones de Diseño:** [Alcance y Extensiones](../decisiones/alcance-y-extensiones.md), [Cambios a Etapas Anteriores](../decisiones/cambios-etapas-anteriores.md), [ADRs](../decisiones/adr-001-desacoplamiento-tac-offsets-stack.md), [Conflicto en Bison](../decisiones/explicacion_conflicto_bison.md) y [Algoritmo de Sethi-Ullman](../decisiones/algoritmo_sethi_ullman.md).
- **Problemas Conocidos:** [Limitaciones y estado de pruebas](../problemas-conocidos.md).
