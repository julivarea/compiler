# C-TDS Compiler

Compilador desarrollado como proyecto de la materia Taller de Diseño de Software (Cod. 3306)

El proyecto se aborda de forma incremental: cada entrega agrega una fase al
compilador. Hoy el camino está completo de punta a punta, desde el texto fuente
hasta un ejecutable x86-64, con un optimizador sobre el código intermedio.

## Autores

- Agustín Alieni
- Francisco Natale
- Julián Varea

## Estado actual

- **Análisis léxico**: reconoce todos los tokens válidos del lenguaje
- **Análisis sintáctico**: reconoce la gramática completa del lenguaje
- **AST**: las acciones semánticas de Bison construyen el árbol durante el parseo
- **Tabla de símbolos y ámbitos**:
  gestión de la jerarquía de `Scope`s (global, funciones, bloques), soporte de
  shadowing y asignación de identificadores únicos (`uid`) para cada entidad declarada.
- **Chequeo de tipos**:
  inferencia de tipos de expresiones, compatibilidad en asignaciones,
  coerción de `int` a `float`, y validación de llamadas a métodos.
- **Análisis semántico**:
  recorrido sobre el AST que valida identificadores no declarados o duplicados en el
  mismo ámbito, condiciones booleanas en `if`/`while`, consistencia en retornos
  (`return;` vs `return expr;`), firmas de métodos y la presencia del método `main()`.
- **Código intermedio (TAC)**:
  transforma el AST validado en instrucciones lineales de tres direcciones,
  representadas como cuádruplas con operandos tipados. Incluye evaluación de
  corto circuito para `&&` y `||`.
- **Código objeto x86-64**:
  traduce el TAC a assembly de GNU Assembler (sintaxis AT&T) y lo ensambla con
  `gcc` para producir el ejecutable. Enteros, booleanos y reales de precisión
  simple (SSE), respetando la convención de llamada System V AMD64.
- **Optimizador**:
  siete optimizaciones sobre el TAC, cada una activable por separado con
  `-opt <nombre>` o todas juntas con `-opt all`: plegado de constantes
  (`const-fold`), propagación de constantes (`const-prop`), eliminación de
  código inalcanzable y de asignaciones muertas (`dead-code`), simplificación de
  condiciones constantes (`const-branch`), eliminación de autoasignaciones
  (`self-assign`), mirilla sobre instrucciones contiguas (`peephole`) y
  reducción de fuerza en multiplicaciones (`strength-reduce`). La eliminación de
  asignaciones muertas se apoya en un análisis de variables vivas sobre el grafo
  de flujo de control.

Además hay un **intérprete** sobre el AST, accesible con `-target interp`. No
forma parte de la entrega: se mantiene porque sirve como oráculo de testing,
es decir una segunda implementación independiente contra la cual comparar el
resultado del backend.

## Por dónde empezar

- Si lo que querés es **compilar el proyecto y correrlo**, la guía de comandos
  está en [Primeros Pasos](wiki/00.5-getting-started.md): prerrequisitos, build,
  tests, opciones de la línea de comandos y prueba de humo.
- Si lo que querés es **entender cómo está hecho**, el recorrido es la
  [Wiki de Onboarding](wiki/00-index.md), que va de la visión general del
  compilador hasta cada fase en el orden en que se ejecutan.
- La sección **Documentación Técnica** del menú es material de consulta puntual
  (decisiones de diseño del AST, alcance del análisis semántico, el conflicto de
  Bison), no está pensada para leerse de corrido.

## Estructura del repositorio

```
.
├── include/              # archivos de cabecera (.h), uno por módulo
├── src/                  # implementaciones (.c), más lexer.l y bison.y
├── tests/                # tests unitarios con Unity, una suite por módulo
├── examples/             # programas .ctds de ejemplo, válidos e inválidos
├── documentacion/        # las páginas de este sitio
├── build/                # artefactos de Flex, Bison y los binarios de test
├── build.sh              # compilar el proyecto
├── test_suite.sh         # correr las diez suites de tests
└── mkdocs.yml            # configuración de este sitio
```

El pipeline está modularizado por fase, y cada módulo tiene su cabecera en
`include/` y su implementación en `src/`:

| Fase | Módulos |
| --- | --- |
| Léxico y sintáctico | `lexer.l`, `bison.y`, `parser_helpers` |
| AST | `ast`, `types` |
| Semántico | `symbol_table`, `type_checker`, `semantic_analyzer` |
| Código intermedio | `tac_operation`, `tac_operand`, `tac_instruction`, `tac_list`, `intermediate_code` |
| Optimización | `optimizer`, `live_variable_analysis` |
| Código objeto | `asm_instruction`, `function_frame`, `object_code` |
| Intérprete | `interpreter` |
| Infraestructura | `cli`, `driver`, `main`, `error_handler`, `string_builder`, `debug` |

`bison.tab.c`, `lex.yy.c`, el contenido de `build/` y el ejecutable
`mi_compilador` son artefactos generados y no se editan a mano.

## Problemas conocidos

- **Un método solo ve a los declarados antes que él.** No hay declaración
  adelantada en C-TDS, así que `main(){ g(); }` con `g` definido más abajo da
  `Método no declarado`. Es lo que pide la letra ("todo se declara antes de
  usarse"), pero conviene tenerlo presente al escribir ejemplos.
- **El plegado de constantes desborda con signo en `+`, `-` y `*`.** Al plegar
  una expresión como `2147483647 * 3` el cálculo se hace en `int`, lo que en C es
  comportamiento indefinido. Hoy no se nota, porque GCC envuelve en silencio y el
  resultado coincide con el que produciría el assembly generado, pero formalmente
  no está garantizado. La división y el módulo sí están protegidos, tanto contra
  el divisor cero como contra el único desborde posible (`INT_MIN / -1`). Se
  detectó compilando el proyecto con UBSan.
- **Los errores de ejecución del intérprete se reportan como errores
  semánticos y sin número de línea**, y `-target interp` termina con código 0
  aunque los haya reportado. Está asumido: el intérprete no es entregable, el
  mensaje igual sale por `stderr`, y arreglarlo pedía un contador de errores de
  ejecución propio que no se justifica.
