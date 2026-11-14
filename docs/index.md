# C-TDS Compiler

Compilador completo para el lenguaje **C-TDS (TDS26)**, desde el código fuente hasta código máquina ejecutable **x86-64**, con optimizador sobre código de tres direcciones (TAC).

## Contexto Académico y Autores

Proyecto desarrollado en el marco de la materia **Taller de Diseño de Software (Cod. 3306)** — Profesor **Francisco Bavera** — Departamento de Computación, **Facultad de Ciencias Exactas, Físico-Químicas y Naturales**, **Universidad Nacional de Río Cuarto (UNRC)**, año **2026**.

### Autores

- Agustín Alieni
- Francisco Natale
- Julián Lucas Varea Grosso

### División del trabajo

La organización del equipo se realizó sin repartirse etapas de antemano. Al comienzo de cada etapa se definieron en conjunto las ideas principales y las decisiones de diseño importantes, avanzando luego de forma progresiva e incremental hasta completar la etapa sin un reparto explícito de tareas. Por ese motivo, todos los integrantes participaron activamente en el desarrollo y en las discusiones de todas las etapas del compilador.

---

## Cómo Funciona el Compilador

El compilador sigue una arquitectura modular multipasada centrada en dos representaciones intermedias principales: el **Árbol de Sintaxis Abstracta (AST)** y el **Código de Tres Direcciones (TAC)**.

```text
Fuente (.ctds) ──▶ Lexer (Flex) ──▶ Parser (Bison) ──▶ AST ──▶ Semántico ──▶ TAC ──▶ [Optimizador -opt] ──▶ Assembly x86-64 ──▶ Ejecutable (.out)
```

1. **Análisis Léxico (`Flex`):** escanea tokens del lenguaje, descarta comentarios y espacios en blanco, y registra números de línea.
2. **Análisis Sintáctico (`Bison`):** valida la gramática LALR(1) y aborta ante el primer error formal.
3. **Construcción del AST:** construye la representación arbórea directamente durante las reducciones sintácticas.
4. **Análisis Semántico:** resuelve ámbitos léxicos y *shadowing* (`Scope`, `Symbol`, `uid`), infiere/chequea tipos (incluyendo coerción `int` → `float`), valida retornos, condiciones booleanas y la presencia de `main()`. Reporta todos los errores encontrados antes de abortar.
5. **Generación de Código Intermedio (TAC):** linealiza el AST validado a cuádruplas tipadas, con evaluación en cortocircuito para `&&` y `||`.
6. **Variables Vivas y Optimización (`-opt`):** opcionalmente construye el grafo de flujo de control (CFG), calcula conjuntos de vivacidad por punto fijo y aplica transformaciones iterativas sobre el TAC hasta alcanzar convergencia.
7. **Generación de Código Objeto:** asigna offsets en pila (`FunctionFrame`) respetando la convención **System V AMD64 ABI**, emite assembly GNU (`AT&T` x86-64) y enlaza con `gcc`.

### Uso Rápido y Pruebas

```bash
bash build.sh                             # compila Flex, Bison y los módulos C -> ./mi_compilador
./mi_compilador examples/valido_factorial_recursivo.ctds   # genera el ejecutable examples/valido_factorial_recursivo.out
./mi_compilador -opt all archivo.ctds     # compila con todas las optimizaciones activas
bash test_suite.sh                        # ejecuta las 11 suites Unity (294 tests)
```

Opciones de línea de comandos: `-o <salida>`, `-target <scan|parse|sem|codinter|assembly>`, `-opt [all|<lista>]` y `-debug`. Los detalles completos de compilación y testing están en [Primeros Pasos](wiki/01-introduccion/01-primeros-pasos.md).

---

## Estado Actual por Etapa

| Etapa | Estado | Alcance y detalles |
| --- | --- | --- |
| **Análisis Léxico** | Completo | Reconoce palabras clave, identificadores, literales (`int`, `float`, `boolean`), operadores y comentarios multilínea con reporte de error ante EOF sin cerrar. |
| **Análisis Sintáctico** | Completo | Gramática LALR(1) libre de conflictos (`0` *shift/reduce*, `0` *reduce/reduce*). |
| **AST** | Completo | Construcción directa en acciones de Bison y anotación posterior in-place. |
| **Tabla de Símbolos** | Completo | Jerarquía de `Scope` en árbol, soporte de *shadowing* e identificador único `uid` por entidad. |
| **Chequeo de Tipos** | Completo | Inferencia de expresiones, compatibilidad de asignaciones, llamadas a métodos y promoción implícita `int` → `float`. |
| **Análisis Semántico** | Completo | Validación de declaraciones, no duplicidad por ámbito, condiciones booleanas, retornos garantizados en todo camino de ejecución y firma de `main()`. |
| **Código Intermedio (TAC)** | Completo | Cuádruplas con operandos tipados y cortocircuito para `&&` y `||` (enteros y lógicos; rechazo explícito de `float`). |
| **Optimizador** | Completo | Análisis de variables vivas sobre CFG + 7 pasadas: `const-fold`, `const-prop`, `dead-code`, `const-branch`, `self-assign`, `peephole` y `strength-reduce`. |
| **Código Objeto x86-64** | Completo | Emisión de assembly AT&T y ensamblado final con `gcc` (enteros y lógicos). |

---

## Organización del Repositorio

```text
.
├── include/                        # Cabeceras públicas (.h) documentadas con Doxygen
│   ├── cli.h                       # Opciones de línea de comandos y banderas -opt
│   ├── driver.h                    # Orquestación del pipeline según -target
│   ├── debug.h                     # Salida de diagnóstico controlada por -debug
│   ├── error_handler.h             # Reporte y contadores de errores léxicos/sintácticos/semánticos
│   ├── string_builder.h            # Búfer dinámico de cadenas para salidas textuales
│   ├── types.h                     # Tipos de datos y operadores compartidos
│   ├── ast.h                       # Estructura y constructores del AST
│   ├── parser_helpers.h            # Arreglos temporales (NodeArray) usados por Bison
│   ├── symbol_table.h              # Ámbitos léxicos (Scope) y símbolos (Symbol)
│   ├── type_checker.h              # Inferencia y reglas de tipos
│   ├── semantic_analyzer.h         # Recorrido semántico sobre el AST
│   ├── tac.h                       # Estructuras y constructores de cuádruplas TAC
│   ├── intermediate_code.h         # Generador AST -> TAC
│   ├── live_variable_analysis.h    # CFG y análisis de vivacidad por punto fijo
│   ├── optimizer.h                 # Máscaras y pasadas de optimización sobre TAC
│   ├── function_frame.h            # Dimensionamiento de stack frame y slots
│   └── object_code.h               # Emisión de assembly x86-64 AT&T
├── src/                            # Implementaciones agrupadas por etapa
│   ├── main.c                      # Punto de entrada del compilador
│   ├── core/                       # cli.c, driver.c, error_handler.c, string_builder.c
│   ├── parser/                     # lexer.l, bison.y, parser_helpers.c, ast.c
│   ├── semantic/                   # symbol_table.c, type_checker.c, semantic_analyzer.c
│   ├── intermediate_code/          # tac.c, intermediate_code.c
│   ├── optimizer/                  # live_variable_analysis.c, optimizer.c
│   └── object_code/                # function_frame.c, object_code.c
├── tests/                          # 11 suites Unity (10 unitarias + 1 de integración) y tests/support/
├── examples/                       # Programas .ctds de ejemplo válidos e inválidos
├── docs/                           # Fuente de esta documentación (MkDocs + specs + archivo histórico)
├── build/                          # Artefactos generados de Flex, Bison y tests
├── build.sh                        # Script de compilación completo
├── test_suite.sh                   # Script de ejecución de las 11 suites
└── mkdocs.yml                      # Configuración del sitio MkDocs Material
```

---

## Navegación de la Documentación

- **[Wiki](wiki/index.md):** documentación detallada paso a paso de cada módulo, espejando las etapas del compilador y la estructura de `src/` (empezando por [Primeros Pasos](wiki/01-introduccion/01-primeros-pasos.md) y [Visión General y Arquitectura](wiki/01-introduccion/02-vision-general.md)).
- **[Decisiones de Diseño](decisiones/alcance-y-extensiones.md):** alcance de `float`, extensión `-target sem`, ADRs de arquitectura, resolución del conflicto en Bison, comparación con Sethi-Ullman y registro de [Cambios a Etapas Anteriores](decisiones/cambios-etapas-anteriores.md).
- **[Problemas Conocidos](problemas-conocidos.md):** limitaciones conocidas del compilador actual, cómo fueron detectadas y alternativas propuestas de mejora.
