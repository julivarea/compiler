# Etapa de Optimización — C-TDS

Este documento especifica las optimizaciones a implementar sobre el compilador C-TDS: qué hace
cada una, sobre qué representación trabaja, bajo qué condiciones es segura y cómo se verifica.

El enunciado del proyecto (`documentacion/spec/00-TDS-proyecto.pdf`) deja el conjunto de
optimizaciones a criterio de cada grupo, acordado con la cátedra, y fija únicamente la interfaz:

> `-opt [optimización]` — Realiza la lista de optimizaciones. `all` realiza todas las
> optimizaciones soportadas.

La etapa vale **15% de la nota final** y su entrega es el **miércoles 12 de noviembre**.

---

## 1. Contrato de la línea de comandos

| Invocación | Comportamiento |
| --- | --- |
| *(sin `-opt`)* | No se aplica ninguna optimización. **La salida debe ser idéntica a la del compilador sin esta etapa.** |
| `-opt all` | Se aplican todas las optimizaciones soportadas |
| `-opt <nombre>` | Se aplica únicamente la indicada |

`src/cli.c` parsea la opción y la deja en `Options.opt` (`NULL` si no se pasó, `"all"` si se
pasó sin nombre), y la extensión de salida pasa a `.opt`. `parseOptimizations()` en
`src/optimizer.c` traduce ese valor al conjunto de optimizaciones activas, y un nombre
desconocido se informa por stderr.

**Requisito no negociable:** sin `-opt`, el compilador tiene que producir exactamente lo mismo
que antes. Es la propiedad que permite aislar una regresión: si un programa falla con `-opt` y
anda sin él, el problema está en la optimización y no en el resto del compilador.

### Nombres propuestos para los flags

| Flag | Optimización |
| --- | --- |
| `const-fold` | Plegado de constantes (§3.1) — **implementada** |
| `const-prop` | Propagación de constantes (§3.1) — **implementada** |
| `dead-code` | Eliminación de código inalcanzable (§3.2) — **implementada** |
| `const-branch` | Condición constante en `if` / `while` (§3.3) — **implementada** |
| `self-assign` | Asignaciones redundantes y mirilla (§3.4) |
| `strength-reduce` | Reducción de fuerza en multiplicaciones (§3.5) |

---

## 2. Dónde se aplica cada optimización

El compilador tiene tres representaciones sobre las que se puede transformar, y la elección no
es indistinta:

| Nivel | Representación | Conviene para |
| --- | --- | --- |
| **AST** | `NodeAST`, antes de generar el TAC | Decisiones que dependen de la estructura del programa: ramas con condición constante, sentencias que no hay que ejecutar |
| **TAC** | `TACList`, lista de cuádruplas | La mayoría: plegado y propagación de constantes, código inalcanzable, copias redundantes. Es lineal, tipada y fácil de reescribir |
| **Assembly** | Texto emitido por `object_code.c` | Solo lo que depende de la máquina: elección de instrucción, patrones de mirilla |

**Criterio general: preferir el TAC.** Es la representación para la que la lista de
instrucciones ya está enlazada (`getIntermediateCode()`), cada operando lleva su tipo y su
naturaleza (`OPERAND_TEMP`, `OPERAND_CONSTANT`, `OPERAND_VARIABLE`), y una transformación se
reduce a recorrer y reescribir nodos. Optimizar en el AST obliga a razonar sobre la estructura;
optimizar en assembly obliga a razonar sobre registros.

### El mecanismo `hasExecute`

El AST tiene un campo `hasExecute` en `NodeAST` pensado para marcar nodos que no deben
generar código. `generateStatementCode()` en `src/intermediate_code.c` ya lo respeta: si está en
`false`, el nodo se saltea sin emitir nada.

Es el mecanismo natural para todas las optimizaciones que trabajan a nivel de AST: en vez de
eliminar nodos del árbol —con el riesgo de romper punteros de los padres— se los marca y el
generador los ignora. Sirve tanto para las asignaciones redundantes (§3.4) como para el código
inalcanzable (§3.2) y las ramas muertas de un `if` con condición constante (§3.3).

---

## 3. Catálogo de optimizaciones

### 3.1 Plegado y propagación de constantes

**Flags:** `const-fold`, `const-prop` · **Nivel:** TAC

Cuando los operandos de una operación son constantes, el resultado se calcula en tiempo de
compilación y la operación desaparece.

```
// Fuente
return 1 + 2 + 3;

// TAC sin optimizar          // TAC optimizado
t1 = 1 + 2                    t1 = 6
t2 = t1 + 3                   ret t1
ret t2
```

Son dos transformaciones distintas que se potencian y conviene implementar juntas:

- **Plegado (`const-fold`):** reemplazar una operación entre constantes por su resultado.
  `t1 = 1 + 2` pasa a `t1 = 3`.
- **Propagación (`const-prop`):** si un temporal o una variable tiene un valor constante
  conocido en ese punto, reemplazar sus usos por la constante. Eso convierte `t2 = t1 + 3` en
  `t2 = 3 + 3`, que el plegado vuelve a reducir.

Aplicadas de forma alternada hasta que no haya más cambios, la cadena completa colapsa a una
sola instrucción.

**Precondiciones y cuidados:**
- La división y el módulo por cero **no se pliegan**: hay que dejar la operación en el código
  para que el comportamiento en ejecución no cambie.
- El plegado tiene que respetar la semántica de tipos del lenguaje: `int` de 32 bits con signo,
  `%` solo entre enteros, y la coerción `int` → `float` de la regla 14.
- La propagación solo vale mientras la variable no haya sido reasignada ni pueda haber sido
  modificada por una llamada a método (si es global).

**Verificación:** comparar el `.ci` con y sin el flag sobre un programa con aritmética
constante, y comprobar que el ejecutable devuelve el mismo resultado en ambos casos.

#### Estado: implementada

En `src/optimizer.c`. El ejemplo del principio de la sección produce hoy:

```
// sin -opt                   // con -opt all
t0 = 1 + 2                    t0 = 3
t1 = t0 + 3                   t1 = 6
ret t1                        ret 6
```

Se ve la cadena completa: se pliega `1 + 2`, se propaga el `3` al operando de la suma
siguiente, se pliega `3 + 3`, y se propaga el `6` hasta el `ret`. Las dos pasadas se repiten
hasta que ninguna introduce cambios, con un tope de rondas por si una transformación futura no
convergiera.

Las dos asignaciones que quedan ya no las lee nadie. Eliminarlas es trabajo de la eliminación
de código muerto, que todavía no está: con solo estas dos optimizaciones el TAC no se acorta,
se simplifica.

**Qué hace cada flag por separado.** Ninguna de las dos rinde mucho sola, y es esperable:
`const-fold` solo actúa cuando los dos operandos ya son literales, y `const-prop` no tiene nada
que propagar si antes no se plegó algo. Es el caso concreto de lo que anticipa §4.

**Decisiones de implementación:**
- El plegado **no elimina la instrucción**, la convierte en `TAC_ASSIGN`. Así el destino
  conserva su valor y cualquier uso posterior sigue siendo válido, sin tener que reescribir la
  lista enlazada mientras se la recorre.
- La propagación **descarta todo lo que sabía en cada etiqueta y en cada salto**. A una
  etiqueta se puede llegar desde cualquier parte, así que lo que valía antes del salto no vale
  necesariamente después. Es conservador —dentro de un ciclo no se propaga nada— pero nunca
  incorrecto.
- En un `call` se olvidan las variables y se conservan los temporales. El callee puede escribir
  cualquier global, pero no puede alcanzar los temporales de quien lo invoca: C-TDS pasa por
  valor y no tiene punteros.
- La tabla de valores conocidos se indexa por el nombre del operando, que ya viene calificado
  con el `uid`, así que dos variables homónimas de scopes distintos no se confunden.

**Verificado:** 22 programas compilados **dos veces** —con y sin `-opt all`— ejecutando ambos
binarios y comparando el código de salida entre sí y contra el resultado esperado: 22 de 22
idénticos. Incluye el caso de **división por cero**, donde ambas versiones abortan igual con
SIGFPE, que es la prueba de que no se plegó. Los 40 tests siguen pasando y los 12 ejemplos
conservan su código de salida con y sin el flag.

---

### 3.2 Eliminación de código inalcanzable

**Flag:** `dead-code` · **Nivel:** AST o TAC

Todo lo que aparece después de un `return` dentro de un mismo bloque es inalcanzable y no debe
generar código.

```c
if (cond) {
    return true;
    x = 1;          // inalcanzable
} else {
    return false;
    y = x;          // inalcanzable
    x = y;
    x = y + 1;
}
```

La transformación es cortar las sentencias posteriores al `return` en cada rama. En el TAC, lo
equivalente es descartar las instrucciones entre un `ret` y la siguiente etiqueta o `end_func`.

**Relación con el análisis semántico:** el analizador semántico **no** reporta esto como error,
y es deliberado — está documentado como decisión de diseño 3 en `TODO_semantic_analyzer.md`,
porque el enunciado no lo prohíbe y pide no informar lo que no es una violación. Que sea válido
es justamente lo que lo convierte en caso de optimización: el programa es correcto, solo que
parte de su código no puede ejecutarse nunca.

**Extensión posible (mismo flag, otro alcance):** además del código inalcanzable, está la
familia de *código muerto* propiamente dicho — asignaciones cuyo resultado nunca se lee. Es más
costosa porque requiere análisis de vivacidad; conviene dejarla para después de que funcione lo
anterior.

**Verificación:** un programa con sentencias después de un `return` en ambas ramas de un
`if/else` no debe generar ninguna instrucción para ellas, y debe devolver el mismo resultado.

#### Estado: implementada

`removeUnreachableCode()` en `src/optimizer.c`. Sobre el ejemplo de arriba:

```
// sin -opt                  // con -opt dead-code
t0 = cond@2 > 0               t0 = cond@2 > 0
if t0 false goto L0           if t0 false goto L0
ret true                      ret true
x@3 = 1                       L0:
goto L1                       ret false
L0:                           L1:
ret false                     end_func f
y@4 = x@3
x@3 = y@4
t1 = y@4 + 1
x@3 = t1
L1:
end_func f
```

**Cómo funciona.** Una sola pasada que lleva un estado de "el flujo está vivo". Tras un `ret` o
un `goto` pasa a muerto y se descarta toda instrucción ejecutable, hasta que una etiqueta lo
reanima.

**Se extendió a `goto`, no solo a `return`.** El enunciado habla de lo que sigue a un `return`,
pero el argumento es idéntico —después de un salto incondicional la siguiente instrucción solo
es alcanzable por una etiqueta— y es donde aparece la mayor parte del código muerto real: en el
ejemplo, el `goto L1` del final de la rama `then` también desaparece.

**Qué nunca se elimina.** Las instrucciones *estructurales*, que no se ejecutan sino que
describen una propiedad del programa:
- Las **etiquetas**, porque son destino de saltos y reanudan el flujo.
- Los **marcadores de método** (`begin_func` / `end_func`), que delimitan las funciones.
- Las **declaraciones** (`global`, `local`, `param_decl`). Borrar un `local` porque quedó
  después de un `return` le sacaría su lugar en el frame a una variable, sin ganar nada en
  tiempo de ejecución. El costo de conservarlas es un slot sin usar; el de borrarlas, un bug.

**Combinación con §3.1.** Las tres juntas sobre un `if (2 > 1) { return 1+2+3; c = 9; }`
eliminan el `c = 9` y reducen la aritmética a `ret 6`, dejando la condición como `if true`, que
es exactamente lo que habilita la optimización de §3.3.

**Lo que todavía no hace.** Las asignaciones que el plegado deja atrás (`t0 = 3`, `t1 = 6`) no
se eliminan: no son *inalcanzables*, son *muertas* —su resultado no lo lee nadie—, y detectarlo
requiere el análisis de vivacidad que esta misma sección menciona como extensión. Por eso en
los ejemplos del repositorio el conteo de instrucciones casi no baja: solo `two_returns.ctds`
pierde una, su segundo `return`.

**Verificado:** 20 programas compilados dos veces y ejecutados, comparando el código de salida
entre sí y contra el esperado: 20 de 20. Entre ellos, los siete casos específicos de esta
optimización —código tras `return`, ambas ramas retornando, `return` dentro de un bloque
anidado, `goto` intermedio de un `if/else`, `return` dentro de un `while`, y etiquetas que sí
son alcanzables—. Los 40 tests siguen pasando y los 12 ejemplos conservan su código de salida.

---

### 3.3 Condición constante en `if` y `while`

**Flag:** `const-branch` · **Nivel:** TAC *(ver la nota sobre el cambio de nivel más abajo)*

Si la condición de una estructura de control se reduce a una constante, no hace falta evaluarla
ni emitir el salto.

```c
if (true) {
    // este bloque se ejecuta siempre: va directo, sin preguntar la condición
}
```

| Caso | Transformación |
| --- | --- |
| `if (true) { A }` | Se emite `A` sin condición ni etiquetas |
| `if (false) { A }` | No se emite nada |
| `if (true) { A } else { B }` | Se emite `A`; `B` se descarta |
| `if (false) { A } else { B }` | Se emite `B`; `A` se descarta |
| `while (false) { A }` | No se emite nada |
| `while (true) { A }` | El salto de salida se elimina, queda el ciclo infinito |

Se apoya en §3.1: una condición como `if (2 > 1)` recién se vuelve constante después del
plegado, así que conviene correr esta optimización **después** de la de constantes.

La rama descartada se marca con `hasExecute = false` en vez de desenganchar el nodo del árbol.

**Verificación:** el `.ci` de un `if (true)` no debe contener ninguna instrucción `if ... goto`
ni etiquetas asociadas a esa condición.

#### Estado: implementada

`simplifyConstantBranches()` en `src/optimizer.c`. Sobre `if (2 > 1) { x = 5; } else { x = 9; }`:

```
// sin -opt                  // con -opt all
x@2 = 0                       x@2 = 0
t0 = 2 > 1                    t0 = true
if t0 false goto L0           x@2 = 5
x@2 = 5                       goto L1
goto L1                       L1:
L0:                           ret x@2
x@2 = 9
L1:
ret x@2
```

**Cambio de nivel respecto de lo planificado: se implementó sobre el TAC y no sobre el AST.**
El motivo es que en el AST las condiciones todavía no están plegadas: `if (2 > 1)` sigue siendo
una operación entre dos literales, así que a nivel AST solo se reconocería un `if (true)`
escrito literalmente. Para el resto habría que implementar un segundo evaluador de constantes
sobre el árbol, duplicando la lógica de §3.1. Sobre el TAC, en cambio, §3.1 ya dejó la
condición reducida a `true` o `false` y la transformación se vuelve trivial. Resultó entonces a
la vez más simple y más efectiva.

**Consecuencia:** el mecanismo `hasExecute` no lo usa esta optimización. Queda disponible para
§3.4, que sí trabaja sobre el AST.

**Cómo funciona.** Un `if <cond> false goto L` salta cuando la condición es falsa y un
`if <cond> true goto L` cuando es verdadera; si la condición es literal, se sabe de antemano
cuál de las dos cosas ocurre:

- El salto **nunca** se toma: la instrucción no hace nada y se elimina. El bloque que seguía
  pasa a ejecutarse siempre, sin preguntar la condición.
- El salto **siempre** se toma: se reemplaza por un `goto` incondicional, y lo que venía
  después queda inalcanzable.

**Hizo falta una limpieza extra: eliminar las etiquetas sin referencias.** Sin eso, la
optimización quedaba a medias y vale la pena explicar por qué. Al resolver el `if` desaparece el
salto, pero la etiqueta a la que apuntaba sobrevive; y como una etiqueta reanuda el flujo, §3.2
seguía considerando alcanzable toda la rama muerta que la seguía. En el ejemplo, el `x@2 = 9` no
se iba. Con `removeUnreferencedLabels()` corriendo antes del análisis de alcance, la rama queda
expuesta y se elimina. El programa ya era correcto sin esa limpieza —el `goto L1` saltaba por
encima de la rama muerta—, pero el código no se reducía.

**Orden dentro de la ronda:** esta pasada corre **antes** que §3.2, para que el `goto` que
genera deje su cola inalcanzable en la misma vuelta.

**Lo que queda sin limpiar.** El `goto L1` seguido inmediatamente de `L1:` es un salto
redundante, y lo aborda la mirilla de §3.4. Y los `t0 = true` que quedan son asignaciones
muertas, que necesitan el análisis de vivacidad mencionado en §3.2.

**Verificado:** 25 programas compilados dos veces y ejecutados: 25 de 25 con resultados
idénticos y correctos. Cubre los seis casos de la tabla de arriba, condicionales constantes
anidados, y la mezcla de condición constante con condición sobre variable. Y especialmente los
casos que **no** deben tocarse: `if` y `while` cuya condición depende de una variable,
`while` anidados, `factorial` y `fib`. Sobre los ejemplos del repositorio,
`if_not_equals_type.ctds` baja de 12 a 11 instrucciones y `two_returns.ctds` de 7 a 6.

---

### 3.4 Asignaciones redundantes y mirilla sobre el assembly

**Flag:** `self-assign` · **Nivel:** AST (asignaciones) y assembly (mirilla)

Una asignación de una variable a sí misma no tiene efecto y no debe generar código:

```c
x = x;      // se ignora por completo
```

En el TAC, `x@3 = x@3` se traduce a un `movl` desde un slot al mismo slot, o sea dos
instrucciones de máquina que no hacen nada.

**Mecanismo:** es el caso para el que `hasExecute` fue pensado. El chequeo es, sobre un
`ASSIGNMENT_NODE`, comparar el símbolo del destino con el de la expresión asignada: si la
expresión es un `ID_NODE` y ambos refieren a la **misma entidad declarada**, se marca el nodo
como no ejecutable.

> **Cuidado con la comparación.** Hay que comparar la identidad de la entidad, no la cadena del
> nombre: dos variables homónimas en scopes distintos son variables distintas. El compilador ya
> resuelve esto con el campo `uid` del `Symbol`, que es único por declaración; comparar
> `uid` es correcto, y comparar `id` con `==` compara punteros de cadenas, que es un error.

**Mirilla (peephole) sobre el assembly.** El mismo criterio se extiende a patrones que solo son
visibles en el código generado:

| Patrón | Reemplazo |
| --- | --- |
| `movl X, %eax` seguido de `movl %eax, X` | Se elimina la segunda |
| `movl $0, %eax` | `xorl %eax, %eax` (más corta) |
| Salto a la etiqueta inmediatamente siguiente | Se elimina el salto |
| Carga de un valor que ya está en el registro | Se elimina la carga |

La mirilla trabaja sobre una ventana de dos o tres instrucciones consecutivas y es la única de
las optimizaciones de este catálogo que necesariamente va a nivel de assembly, porque los
patrones que reconoce no existen en el TAC.

**Verificación:** un programa con `x = x;` no debe generar ninguna instrucción para esa
sentencia, y los patrones de mirilla deben reducir el conteo de instrucciones del `.ass` sin
cambiar el resultado de la ejecución.

---

### 3.5 Reducción de fuerza en multiplicaciones por constante

**Flag:** `strength-reduce` · **Nivel:** assembly

Cuando uno de los operandos de una multiplicación es una constante conocida, se puede emitir una
instrucción más barata que `imull`: si aparece `* 2` se genera una instrucción, y si no, otra.

| Expresión | Instrucción |
| --- | --- |
| `x * 2` | `addl %eax, %eax` o `sall $1, %eax` |
| `x * 4`, `x * 8`, … potencias de dos | `sall $n, %eax` con `n = log2(constante)` |
| `x * 1` | Ninguna: el resultado es el operando |
| `x * 0` | `xorl %eax, %eax` |
| Resto de los casos | `imull` como hasta ahora |

**Precondiciones:**
- Solo aplica a enteros. En reales, `mulss` no tiene equivalente por desplazamiento.
- El desplazamiento aritmético es correcto para la multiplicación con signo; para la **división**
  por potencias de dos **no** alcanza con `sarl`, porque el redondeo de los negativos difiere del
  que exige la división entera. Si se quisiera extender a división habría que agregar la
  corrección de signo, o directamente dejarla afuera.

**Verificación:** el `.ass` de `x * 2` no debe contener `imull`, y el ejecutable debe devolver el
mismo valor que sin el flag, incluyendo el caso de operandos negativos.

---

## 4. Orden de aplicación e interacciones

Las optimizaciones no son independientes: unas habilitan a otras. El orden recomendado es:

1. **Plegado y propagación de constantes** (§3.1), alternadas hasta que no haya más cambios.
2. **Condición constante** (§3.3), que necesita que el paso anterior haya reducido las
   condiciones a constantes.
3. **Código inalcanzable** (§3.2), que puede aparecer recién después de descartar una rama.
4. **Asignaciones redundantes** (§3.4), que la propagación de copias puede haber creado.
5. **Reducción de fuerza y mirilla** (§3.5 y §3.4), al final y sobre el assembly ya emitido.

Con `-opt all` se corre la secuencia completa. Con un flag individual se corre solo esa
transformación, lo que puede dar menos mejora que la cadena completa: es esperable y conviene
documentarlo en el informe.

---

## 5. Cómo agregar una optimización nueva

1. Elegir el nivel según §2 (en la duda, TAC).
2. Darle un nombre de flag y registrarlo en la tabla de §1.
3. Implementarla como una pasada propia, que reciba la representación y la devuelva
   transformada, sin mezclarse con la generación.
4. Garantizar que sin su flag el resultado no cambia.
5. Documentar en este archivo: qué hace, el ejemplo antes/después, las precondiciones y cómo se
   verifica.

---

## 6. Estado de la implementación

- **La infraestructura está completa.** `src/optimizer.c` y `include/optimizer.h` contienen el
  enumerado de optimizaciones, el parseo del valor de `-opt` y el ciclo que aplica las pasadas
  hasta converger. El driver las invoca desde `buildIntermediateCode()`, entre la generación
  del TAC y su emisión, de modo que las aprovechan tanto `-target codinter` como la generación
  de assembly y el ejecutable.
- **El mecanismo `hasExecute` quedó funcionando.** El campo está declarado en `NodeAST`,
  `src/ast.c` lo inicializa en `true` y `src/intermediate_code.c` lo consulta antes de traducir
  cada sentencia. Está inerte: mientras ninguna optimización lo ponga en `false`, el
  comportamiento no cambia. El esqueleto que había en `src/semantic_analyzer.c` se quitó porque
  no compilaba y porque el analizador semántico no debe conocer los flags de optimización; la
  transformación va en la pasada de optimización, según §3.4.
- **Implementadas:** plegado y propagación de constantes (§3.1), eliminación de código
  inalcanzable (§3.2) junto con la limpieza de etiquetas sin referencias, y condición constante
  en `if` / `while` (§3.3).
- **Pendientes:** asignaciones redundantes y mirilla (§3.4), reducción de fuerza (§3.5).

---

## 7. Criterio de aceptación de la etapa

Para dar la etapa por terminada, cada optimización implementada debe cumplir:

- Se activa individualmente con su flag y también con `-opt all`.
- Sin su flag, el código generado es **idéntico** al que producía el compilador antes.
- Existe al menos un programa de prueba que la dispara, y su ejecutable devuelve el mismo
  resultado con y sin la optimización.
- Está documentada en este archivo con su ejemplo antes/después.

El punto que más peso tiene en la corrección es el segundo: una optimización que cambia el
resultado de un programa es peor que no tener la optimización.

---

## Apéndice: correspondencia con la lista original

Este documento formaliza la lista de ideas que se había anotado al abrir la etapa. La
correspondencia es:

| Idea original | Sección |
| --- | --- |
| 1. Propagación de valores constantes (`return 1 + 2 + 3`) | §3.1 |
| 2. Código muerto (cortar lo que sigue a un `return` en cada rama) | §3.2 |
| 3. Optimización de `if(true)` (ejecutar siempre, sin preguntar la condición) | §3.3 |
| 4. Optimizar instrucciones en assembly, `x = x`, y aprovechar el flag `hasExecute` del nodo AST | §3.4 y §2 |
| 5. Operaciones para constantes `* 2` (si aparece `* 2` generar una instrucción y si no otra) | §3.5 |
