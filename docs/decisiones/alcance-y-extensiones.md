# Alcance del Lenguaje y Extensiones al Enunciado

Decisiones de alcance e interfaz tomadas sobre la especificación del lenguaje (`docs/spec/01-TDS-spec-lenguaje.pdf`) y el enunciado del proyecto (`docs/spec/00-TDS-proyecto.pdf`).

---

## 1. Alcance de `float`

La **especificación del lenguaje** incluye `float` entre los tipos de C-TDS, y su regla 14 permite las conversiones entre `int` y `float`. El **enunciado del proyecto**, en cambio, acota las etapas de **código intermedio y código objeto** a **tipos enteros y lógicos**, y la descripción de las asignaciones menciona solo `int` y `boolean`.

El compilador respeta las dos definiciones, y el corte está exactamente en la frontera entre front-end y back-end:

| Fase | ¿Cubre `float`? |
| --- | --- |
| Léxico, sintáctico, [AST](../wiki/03-lexico-sintactico-y-ast/04-ast.md), [tabla de símbolos](../wiki/04-semantico/01-tabla-de-simbolos.md) | **Sí.** Es un tipo del lenguaje: se reconoce y se representa. |
| [Chequeo de tipos](../wiki/04-semantico/02-chequeo-de-tipos.md) | **Sí.** Se verifica la regla 14 (ensanchamiento implícito de `int` a `float`). |
| [Código intermedio](../wiki/05-codigo-intermedio/02-generacion-tac.md), optimizador y [código objeto](../wiki/07-codigo-objeto/02-generacion-codigo-objeto.md) | **No.** Un programa con `float` se rechaza explícitamente en generación de TAC, indicando su línea. |

```text
$ ./mi_compilador programa.ctds
Error en la linea 2: la generacion de codigo para el tipo 'float' no esta
soportada: esta etapa cubre tipos enteros y logicos.
```

Hasta `-target sem` un programa con reales se acepta sin errores: el front-end lo entiende completo. Lo que queda fuera de alcance es **generarle código**, y el rechazo ocurre en la primera fase que lo haría (detalle de implementación en [Generación de Código Intermedio](../wiki/05-codigo-intermedio/02-generacion-tac.md)).

**Por qué rechazar explícitamente y no ignorar.** Si el generador emitiera operaciones sin validar el tipo, el código objeto trataría el patrón de bits IEEE-754 como si fuera un entero (`1.0 + 1.0` se compilaría como una suma entera de dos patrones de bits sin reportar ningún error). Un error explícito con su número de línea es preferible a un ejecutable silenciosamente incorrecto.

---

## 2. Extensión sobre el enunciado: `-target sem`

El enunciado enumera `scan`, `parse`, `codinter` y `assembly` como argumentos de `-target`, pero también lista la extensión `.sem` entre las salidas esperadas del compilador.

Se incorporó `-target sem` en [CLI y Entrada](../wiki/02-infraestructura/01-cli-y-entrada.md) porque de otro modo la etapa de análisis semántico no tenía punto de corte propio y el archivo `.sem` (AST anotado con tipos y `uid`) no se podía producir de forma aislada.

---

## 3. Comportamiento por defecto sin `-target` (`CURRENT_TARGET`)

El enunciado establece que sin `-target` se compila *"hasta la etapa corriente"* generando el archivo de esa etapa, y que al completar el compilador se genera el ejecutable `nombreArchivo.out`.

Mientras el proyecto estuvo en desarrollo por entregas, ambas lecturas convivían mediante la constante `CURRENT_TARGET` en `include/cli.h`, que avanzaba al cerrar cada fase. Con el pipeline completo, `CURRENT_TARGET` vale `TARGET_ASSEMBLY`, de modo que sin `-target` se ejecutan todas las etapas y se produce el ejecutable `.out` (ver [CLI y Entrada](../wiki/02-infraestructura/01-cli-y-entrada.md)).
