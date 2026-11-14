# La Representación Intermedia (Cuádruplas TAC)

**Archivos:** `include/tac.h`, `src/intermediate_code/tac.c` (~610 líneas)

La estructura de datos del **Código de Tres Direcciones**: el puente entre el
front-end y el back-end.

!!! note "El TAC cubre tipos enteros y lógicos"
    El enunciado acota esta etapa a **tipos enteros y lógicos**. El operando
    constante guarda un `int` y nada más, y un programa que use `float` se
    rechaza al empezar a generar, con su línea. Ver la
    [generación de TAC](02-generacion-tac.md).

---

## 1. Qué problema resuelve

El [AST](../03-lexico-sintactico-y-ast/04-ast.md) es una estructura **arbórea**, buena para representar la
sintaxis del lenguaje. El assembly es una secuencia **lineal** de instrucciones
que operan sobre registros y memoria. Pasar de uno al otro de un salto es
difícil, porque hay que resolver al mismo tiempo el orden de evaluación, los
resultados intermedios, el control de flujo y la asignación de registros.

El TAC es el paso intermedio: **lineal como el assembly, pero todavía sin
máquina**. No hay registros ni direcciones; hay operandos con nombre y
temporales ilimitados. Eso convierte el problema en dos problemas chicos:
aplanar el árbol ([generación TAC](02-generacion-tac.md)) y asignar almacenamiento ([marco de pila](../07-codigo-objeto/01-marco-de-pila.md)).

Es además la representación sobre la que trabaja el **optimizador**: es
suficientemente simple para analizar y suficientemente abstracta para que una
transformación valga en cualquier máquina.

---

## 2. Qué es una cuádrupla

Una instrucción TAC es una **cuádrupla**: una tupla de cuatro componentes.

```c
typedef struct TACInstruction {
    TACOperation operation;     /* qué hacer */
    TACOperand   result;        /* dónde dejar el resultado */
    TACOperand   firstOperand;  /* primer operando */
    TACOperand   secondOperand; /* segundo operando */
    struct TACInstruction *next;
} TACInstruction;
```

El nombre "código de **tres direcciones**" viene de que cada instrucción menciona
a lo sumo **tres operandos**: un destino y dos fuentes. La "cuádrupla" es la
instrucción completa, o sea esos tres más el operador.

```
x = a + b        ->   (TAC_ADD, x, a, b)
x = -a           ->   (TAC_UMINUS, x, a, —)
goto L1          ->   (TAC_GOTO, —, L1, —)
if t false goto L2  -> (TAC_IF_FALSE, —, t, L2)
```

La propiedad clave es que **una instrucción no contiene expresiones**: sus
operandos son siempre atómicos. Una expresión anidada como `a + b * c` no puede
ser una sola cuádrupla; se descompone en dos, usando un temporal:

```
t0 = b * c
t1 = a + t0
```

Esa descomposición es lo que convierte el árbol en una lista, y es todo el
trabajo de la [generación de TAC](02-generacion-tac.md).

### Por qué cuádruplas y no texto

Esta representación es el resultado de una **migración**: la primera versión del
generador emitía el TAC como cadenas de texto directamente al archivo de salida.
Funcionaba para producir el `.ci`, pero no servía como representación interna, y
conviene poder explicar por qué.

| | Texto plano | Cuádruplas |
| --- | --- | --- |
| Leer un operando | Parsear la línea de nuevo | `instruction->firstOperand` |
| Saber si algo es constante | Adivinar por la forma de la cadena | `operand.kind == OPERAND_CONSTANT` |
| Reemplazar un operando | Reconstruir la línea entera | Asignar al campo |
| Eliminar una instrucción | Rearmar el texto sin esa línea | Desenlazar un nodo de la lista |
| Tipo de dato de un operando | No está | `operand.type` |

El punto decisivo es el **optimizador**: propagar una constante significa
reemplazar un operando por otro, y eliminar código muerto significa quitar una
instrucción. Las dos cosas son triviales sobre una estructura y requieren volver
a parsear sobre texto. El código objeto y el optimizador consumen esta
representación, así que cualquier cosa escrita sobre texto habría habido que
reescribirla.

El beneficio secundario fue de memoria: la versión textual usaba un `static char
buffer[32]` para los temporales —que se sobreescribía entre llamadas anidadas— y
`strdup` para las etiquetas, que no se liberaba nunca. Con los nombres viviendo
en campos de tamaño fijo dentro de la instrucción, los dos problemas desaparecen
de raíz.

---

## 3. El operando tipado: `TACOperand`

Es la pieza que da el salto de calidad sobre el texto.

```c
typedef struct TACOperand {
    OperandKind kind;              /* qué clase de cosa es */
    DataType type;                 /* su tipo de dato */
    char name[TAC_NAME_LENGTH];    /* su representación textual */
    Symbol *symbol;                /* solo en OPERAND_VARIABLE */
    union {
        int intValue;
    } value;                       /* solo en OPERAND_CONSTANT */
} TACOperand;
```

| Campo | Por qué existe |
| --- | --- |
| `kind` | **La razón de ser de la estructura.** Dice qué es el operando sin tener que deducirlo de la cadena |
| `type` | El tipo de dato del operando (`INT` o `BOOL`), que viaja hasta el generador de código |
| `name` | Buffer de tamaño fijo, 256 bytes. Es lo que se imprime en el `.ci` |
| `symbol` | Referencia al símbolo de la tabla, para resolver la ubicación de la variable |
| `value` | El valor inmediato de una constante, para que el optimizador pueda plegarla |

### Las seis categorías de `OperandKind`

| Categoría | Qué representa | `name` de ejemplo | Campos que usa |
| --- | --- | --- | --- |
| `OPERAND_NONE` | El operando no se usa en esta instrucción | `""` | ninguno |
| `OPERAND_TEMP` | Temporal del compilador | `t0` | `type` |
| `OPERAND_VARIABLE` | Variable o parámetro del programa | `x@7` | `type`, `symbol` |
| `OPERAND_CONSTANT` | Literal | `42`, `true`, `1.500000` | `type`, `value` |
| `OPERAND_LABEL` | Etiqueta de salto | `L3` | — |
| `OPERAND_METHOD` | Nombre de un método | `main` | `type` = tipo de retorno |

**`OPERAND_NONE` es lo que hace que la cuádrupla sea uniforme.** Todas las
instrucciones tienen tres operandos aunque usen uno; los que no se usan son
`NONE`. Así no hay instrucciones de tamaño variable, y `getOperandName` devuelve
`""` para ellos, lo que simplifica la impresión.

**Distinguir `OPERAND_TEMP` de `OPERAND_VARIABLE` es lo que permite optimizar con
confianza.** Un temporal es del compilador: nadie más lo lee, no sobrevive a la
instrucción que lo consume, y eliminarlo es seguro. Una variable es del
programador y puede ser global, caso en el que otra función podría observarla. Si
los dos fueran "nombres", el optimizador tendría que ser conservador con ambos.

### Los siete constructores

Ninguna parte del compilador arma un `TACOperand` a mano: todos pasan por un
constructor, y todos delegan en un `makeOperand` privado que hace `memset` a cero
antes de llenar los campos.

| Constructor | Produce |
| --- | --- |
| `tacNone()` | El operando ausente |
| `tacTemp(name, type)` | Un temporal |
| `tacVariable(symbol)` | Una variable, con el nombre calificado. Ver §4 |
| `tacConstantInt(value)` | Constante entera; el `name` es el número |
| `tacConstantBool(value)` | Constante booleana; el `name` es `"true"` o `"false"` |
| `tacMethod(name, returnType)` | Un método |
| `tacLabel(name)` | Una etiqueta |

El `memset` inicial importa: garantiza que los campos que una categoría no usa
queden en cero en lugar de contener basura, así que comparar `operand.value.intValue`
de un operando que no es constante da 0 y no un valor aleatorio.

### Dos preguntas compartidas: `holdsValue` e `isConditionalJump`

`tac.h` define dos funciones `static inline` para las preguntas sobre el TAC que
se hacen en varias fases, y así no se escriben a mano en cada una:

```c
static inline bool holdsValue(const TACOperand *operand) {
    return operand->kind == OPERAND_TEMP || operand->kind == OPERAND_VARIABLE;
}

static inline bool isConditionalJump(TACOperation operation) {
    return operation == TAC_IF_FALSE || operation == TAC_IF_TRUE;
}
```

`holdsValue` dice si el operando guarda un valor en tiempo de ejecución: es el
criterio de qué puede tener una constante conocida (propagación), qué puede ser
una asignación muerta (optimizador) y qué variables analiza la vivacidad.
`isConditionalJump` la usan el armado del CFG, la propagación de constantes, la
simplificación de saltos constantes y la fusión de comparación y salto del
generador de código objeto.

---

## 4. El nombre calificado `x@uid`

`tacVariable` no usa el nombre del programa tal cual: le pega el `uid` del
símbolo.

```c
snprintf(qualifiedName, sizeof(qualifiedName), "%s@%d", id, symbol->uid);
```

**Por qué hace falta.** El TAC es una lista plana: **no tiene ámbitos**. Un
programa con tres variables llamadas `x` en ámbitos distintos produciría tres
operandos con el mismo nombre, y ninguna fase posterior podría distinguirlas. El
`uid` que asignó la [tabla de símbolos](../04-semantico/01-tabla-de-simbolos.md) es único en todo el
programa, así que el nombre calificado los separa.

```c
int x;                      ->  global x@1, INT
int main(){
    int x; x = 10;          ->  local x@3, INT   /  x@3 = 10
    { int x; x = 20; }      ->  local x@4, INT   /  x@4 = 20
    return x;               ->  ret x@3
}
```

El `return x` quedó como `ret x@3`: el del ámbito del método, no el del bloque
interno. Toda la resolución de alcance quedó materializada en el nombre, y las
fases siguientes pueden ignorar el concepto de ámbito por completo.

**Por qué el separador es `@`.** Porque `@` no puede aparecer en un identificador
de C-TDS, así que al leer el TAC se ve de inmediato que el sufijo lo puso el
compilador. Con `_` no habría ambigüedad real —el `uid` se agrega **siempre**, así
que un `x` y un `x_3` del programa nunca producirían la misma cadena— pero `x_3`
se lee como si fuera un nombre que escribió el programador.

**El caso `uid == 0`** se imprime sin sufijo. Significa que el símbolo no quedó
registrado en ningún ámbito, lo que no debería pasar si el análisis semántico
terminó sin errores. Si aparece un nombre sin `@` en un `.ci`, es señal de un bug
en una fase anterior.

### Detección de nombres truncados

Tanto `makeOperand` como `tacVariable` comprueban el valor de retorno de
`snprintf`, que es la longitud que **habría** hecho falta:

```c
if (written < 0 || (size_t)written >= sizeof(qualifiedName)) {
    fprintf(stderr, "Error interno del TAC: el identificador calificado para '%s' excede el limite...");
}
```

Sin ese chequeo, un identificador de más de 256 caracteres se truncaría en
silencio y **perdería su sufijo `@uid`**, lo que haría que dos variables distintas
colapsaran en el mismo operando: un error de compilación silencioso que produce
un programa incorrecto. Con el chequeo, se reporta como error interno.

---

## 5. El catálogo de operaciones

23 opcodes en `TACOperation`, en seis familias. **Los 23 se emiten**: no hay
opcodes muertos, verificado contra el generador.

### Métodos y funciones

| Opcode | Forma textual | Operandos |
| --- | --- | --- |
| `TAC_BEGIN_FUNC` | `begin_func suma, 2` | `result` = método, `first` = cantidad de parámetros |
| `TAC_END_FUNC` | `end_func suma` | `result` = método |
| `TAC_PARAM_DECL` | `param_decl a@4` | `first` = el parámetro formal |
| `TAC_PARAM` | `param g@1` | `first` = el argumento a pasar |
| `TAC_CALL` | `t1 = call suma, 2` / `call nada, 0` | `result` opcional, `first` = método, `second` = cantidad de argumentos |
| `TAC_RETURN` | `ret t0` / `ret` | `first` opcional |

`TAC_BEGIN_FUNC` y `TAC_END_FUNC` delimitan cada método. No son estrictamente
necesarios para ejecutar, pero el generador de código los necesita para saber
dónde emitir el prólogo y el epílogo, y el optimizador para no propagar
constantes de una función a otra.

### Declaraciones

| Opcode | Forma textual |
| --- | --- |
| `TAC_GLOBAL_DECL` | `global g@1, INT` |
| `TAC_LOCAL_DECL` | `local x@8, INT` |

Que la distinción global/local esté **en el opcode** y no deducida después es lo
que permite al generador de código emitir las globales en `.data` y reservar las
locales en el marco de pila, sin volver a consultar la tabla de símbolos.

### Control de flujo

| Opcode | Forma textual |
| --- | --- |
| `TAC_LABEL` | `L3:` |
| `TAC_GOTO` | `goto L3` |
| `TAC_IF_FALSE` | `if t9 false goto L4` |
| `TAC_IF_TRUE` | `if t2 true goto L0` |

**Existen las dos variantes del salto condicional, y no es redundante.** Saltar si
falso es lo que necesita un `if` o un `while`; saltar si verdadero es lo que
necesita el corto circuito de `||`. Tener las dos evita emitir una negación
intermedia antes de cada salto.

### Asignación, aritmética, relacionales y unarios

| Opcode | Forma textual |
| --- | --- |
| `TAC_ASSIGN` | `x@8 = t1` |
| `TAC_ADD` `TAC_SUB` `TAC_MUL` `TAC_DIV` `TAC_MOD` | `t0 = a@4 + b@5` |
| `TAC_LT` `TAC_GT` `TAC_EQ` | `t4 = x@8 > 5` |
| `TAC_UMINUS` | `t0 = -x@2` |
| `TAC_NOT` | `t8 = !b@10` |

### Lo que no está: `TAC_AND` y `TAC_OR`

**No hay opcodes para `&&` ni `||`**, y es una decisión de diseño, no un olvido.
El lenguaje exige **evaluación de corto circuito**: si el primer operando ya
determina el resultado, el segundo no se evalúa. Un opcode `t = a && b` no puede
cumplir eso, porque sus dos operandos tienen que estar calculados antes de
ejecutarla.

Entonces el corto circuito se expresa con **saltos**:

```c
b = (x > 5) && (x < 100);
```

```
t4 = x@8 > 5
t5 = t4
if t5 false goto L0      <- si el primero es falso, el segundo no se evalua
t6 = x@8 < 100
t5 = t6
L0:
b@10 = t5
```

Y el `||` usa la variante simétrica:

```
t1 = x@2 > 0
t2 = t1
if t2 true goto L0       <- si el primero es verdadero, listo
t3 = x@2 < 100
t2 = t3
L0:
```

El detalle de cómo se generan estas secuencias es la
[generación de TAC](02-generacion-tac.md). Lo que importa acá es que **la ausencia de
esos dos opcodes es lo que garantiza la semántica del lenguaje**.

---

## 6. El formato textual, única fuente de verdad

`formatTACInstruction` renderiza una instrucción a una línea de texto, y es la
**única** función que conoce el formato:

```c
void formatTACInstruction(char *buffer, size_t size, const TACInstruction *instruction);
```

Encima hay dos envoltorios, uno por destino, y todos los consumidores pasan por
alguno de ellos:

| Función | Destino | Quién la usa |
| --- | --- | --- |
| `printTACInstruction` (vía `formatTACList`) | El acumulador de cadenas | El driver, para escribir el archivo `.ci` o `.opt` |
| `fprintTACInstruction(output, instruction)` | Un `FILE *` | El volcado de `-debug` del optimizador, que marca con `[ELIMINADA]` lo que sacó, y el del análisis de variables vivas, que imprime las instrucciones de cada bloque básico |

Tener una sola fuente garantiza que las tres salidas coincidan. Si el formato
estuviera duplicado, el TAC que muestra el optimizador podría diferir del que
escribe el archivo, y comparar los dos dejaría de servir para depurar.

El `switch` tiene un caso por familia, y el `default` cubre **todos los
operadores** aritméticos, relacionales y unarios, delegando el símbolo a
`getOperatorSymbol`. Unario o binario se decide por la forma de la instrucción:
si `secondOperand` es `OPERAND_NONE` imprime `result = sym first` (`t0 = -x@2`),
y si no `result = first sym second`. Agregar un operador nuevo no requiere tocar
el formateador: alcanza con su símbolo.

Hay dos formas que dependen de si un operando está presente:

```c
case TAC_CALL:
    if (instruction->result.kind != OPERAND_NONE)  /* t1 = call f, 2 */
    else                                           /* call f, 2      */

case TAC_RETURN:
    if (instruction->firstOperand.kind != OPERAND_NONE)  /* ret t0 */
    else                                                  /* ret    */
```

Es `OPERAND_NONE` ganándose el lugar: sin esa categoría habría que llevar una
bandera aparte para saber si la llamada tiene destino.

---

## 7. La lista: `TACList`

```c
typedef struct TACList {
    TACInstruction *head;
    TACInstruction *tail;
    int count;
} TACList;
```

Lista simplemente enlazada con puntero a la cola. El `tail` es lo que hace que
`appendInstruction` sea **O(1)**: sin él, agregar al final costaría recorrer toda
la lista y generar el TAC sería cuadrático.

| Función | Qué hace |
| --- | --- |
| `createTACList()` | Lista vacía |
| `createTACInstruction(op, result, first, second)` | Reserva una instrucción **sin enlazarla** |
| `appendTACInstruction(list, instruction)` | Enlaza al final una instrucción **ya creada**, en O(1) |
| `appendInstruction(list, op, result, first, second)` | Crea y enlaza: `createTACInstruction` + `appendTACInstruction` |
| `freeTACInstruction(instruction)` | Libera una instrucción suelta, sin seguir el `next` |
| `freeTACList(list)` | Recorre liberando cada instrucción y después el contenedor |
| `formatTACList(list)` | Vuelca la lista entera al `string_builder` |

**La creación y el enlazado están separados**, y `appendInstruction` es la
composición de los dos. El compilador usa siempre `appendInstruction`; las dos
mitades sueltas existen para que los tests armen y enlacen instrucciones a mano.

**`appendInstruction` devuelve la instrucción que creó, o `NULL` si no pudo
reservarla.** El `emit` del generador (ver la
[generación de TAC](02-generacion-tac.md)) usa ese `NULL` para detectar la falta de
memoria, y los tests usan el puntero para inspeccionar la instrucción recién
agregada. Las etiquetas de salto no necesitan retocar nada después de emitido:
son nombres, así que se pueden referenciar antes de definirlas.

### Los operandos se guardan por valor

`TACInstruction` tiene tres `TACOperand` **embebidos**, no punteros. Las
consecuencias:

- Una instrucción es **autocontenida**: copiarla copia sus operandos.
- `freeTACInstruction` es un solo `free`: no hay nada anidado que liberar.
- Cada instrucción ocupa unos 800 bytes, porque cada operando lleva su buffer de
  256 caracteres. Es derrochador en memoria y perfectamente adecuado acá: los
  programas tienen cientos de instrucciones, no millones. La alternativa —una
  tabla de cadenas compartida con los operandos guardando índices— ahorraría
  memoria a cambio de indirección y de un dueño más que manejar.

---

## 8. Conexión con las fases vecinas

| Viene de | Qué cruza la frontera |
| --- | --- |
| `symbol_table.c` | El `Symbol` con su `uid`, que `tacVariable` convierte en `x@uid` |
| `types.h` | `DataType`, que viaja en cada operando |

| Va hacia | Qué entrega |
| --- | --- |
| `intermediate_code.c` | Los constructores con los que arma la lista |
| `optimizer.c` | La lista, que recorre y modifica en el lugar |
| `live_variable_analysis.c` | La lista, que parte en bloques básicos |
| `object_code.c` | La lista, que traduce instrucción por instrucción |
| `driver.c` | `formatTACList` para escribir el `.ci` o el `.opt` |

**Este módulo es solo la estructura: no genera ni analiza nada.** Es la definición
del formato que comparten las cuatro fases de arriba, y esa es la razón de que
sea la primera que había que estabilizar antes de escribir el optimizador o el
generador de código.

---

## 9. Cómo se extiende

### Un operador binario nuevo

Tres lugares, todos chicos:

1. El valor en el enum `TACOperation`.
2. Su símbolo en `getOperatorSymbol`, y nada más en el formateador: el `default`
   ya cubre la forma `result = first sym second`.
3. Su traducción en el generador de código objeto.

### Un opcode con forma textual propia

Si la forma no es la de un binario (por ejemplo un `TAC_ARRAY_LOAD` que se
imprimiría `t0 = a[t1]`), hace falta además su `case` en `formatTACInstruction`.
Olvidarlo hace que caiga en el `default` y se imprima como un binario, lo cual se
nota al mirar el `.ci`.

### Un `OperandKind` nuevo

Por ejemplo `OPERAND_ADDRESS` para punteros. Requiere su constructor, y revisar
los lugares que hoy hacen `switch` o comparaciones sobre `kind`: el optimizador
decide qué puede plegar o propagar según la categoría, así que una categoría
nueva que no considere va a ser tratada conservadoramente, que es el lado seguro.

### Nombres más largos que 256 caracteres

`TAC_NAME_LENGTH` es el único número a cambiar, y el chequeo de truncamiento de
§4 es lo que avisa si hace falta. Subirlo multiplica el tamaño de cada
instrucción por tres, porque hay tres operandos.

---

## 10. Trampas y límites conocidos

- **No hay `TAC_AND` ni `TAC_OR`, y es a propósito** (§5). Agregarlos para
  "completar la simetría" rompería el corto circuito que exige el lenguaje.
- **`uid == 0` imprime el nombre sin sufijo.** No es un formato alternativo: es la
  señal de que un símbolo no quedó registrado, o sea un bug en una fase anterior.
- **Cada instrucción pesa unos 800 bytes** por los tres buffers de 256 (§7).
- **Los campos que una categoría no usa quedan en cero**, no indefinidos, gracias
  al `memset` de `makeOperand`. Pero cero es un valor válido: `value.intValue == 0`
  en un operando que no es constante no significa "sin valor", significa cero.
  Hay que mirar `kind` primero.
- **`getOperandName` devuelve un puntero al buffer interno** del operando. Es
  estable mientras la instrucción exista, pero no sobrevive a liberarla.
- **El `default` del formateador trata cualquier opcode desconocido como un
  operador**, y `getOperatorSymbol` devuelve `"?"`. Un opcode nuevo sin símbolo se
  imprime `t0 = a ? b` (o `t0 = ?a` si no tiene segundo operando) en lugar de
  fallar.
- **`TAC_INSTRUCTION_TEXT_LENGTH` es 1024** y los tres operandos suman 768 más el
  texto fijo. Alcanza, pero no hay margen si se agrandara `TAC_NAME_LENGTH` sin
  tocar este valor.

---

## 11. La teoría detrás

- **Representación intermedia** (*IR*): una forma del programa entre el fuente y
  el código máquina. La razón de existir es desacoplar: con *N* lenguajes y *M*
  arquitecturas, una IR común convierte *N×M* traductores en *N+M*.
- **Código de tres direcciones**: la IR lineal clásica, donde cada instrucción
  tiene a lo sumo un destino y dos fuentes. Su propiedad definitoria es que los
  operandos son **atómicos**: no hay expresiones anidadas. Eso la hace casi
  trivial de traducir a assembly y muy cómoda de analizar.
- **Cuádruplas contra triplas**: las dos formas habituales de implementar TAC. Una
  **tripla** omite el destino y se refiere al resultado de una instrucción por su
  **posición** en la lista. Ahorra espacio, pero cualquier reordenamiento o
  eliminación invalida todas las referencias, lo que la vuelve hostil para un
  optimizador. Las **cuádruplas** nombran el destino explícitamente, así que una
  instrucción se puede mover o borrar sin tocar las demás. Acá se eligieron
  cuádruplas justamente por el optimizador, y es el argumento que conviene tener
  listo si se pregunta por la elección.
- **Temporales**: nombres que genera el compilador para los resultados
  intermedios. En el TAC son **ilimitados**, y eso es deliberado: posponer la
  decisión de cuántos registros hay hasta la generación de código objeto. Acá esa
  decisión se simplifica dándole a cada temporal un lugar en la pila ([marco de pila](../07-codigo-objeto/01-marco-de-pila.md)),
  que es lo correcto pero no lo eficiente; la alternativa es la **asignación de
  registros**, que se modela como un coloreo del grafo de interferencia.
- **Renombrado único**: el sufijo `@uid` resuelve el mismo problema que las formas
  SSA en los compiladores modernos —darle un nombre irrepetible a cada entidad
  para poder descartar los ámbitos—, aunque acá se aplique a declaraciones y no a
  cada asignación.
- **Instrucciones de tres direcciones y máquinas de pila**: la alternativa a esta
  IR es una representación de pila, como el bytecode de la JVM, donde las
  operaciones toman sus operandos implícitamente del tope. Es más compacta y más
  fácil de interpretar; el TAC es más fácil de optimizar y de traducir a una
  máquina de registros, que es lo que x86-64 es.

---

## 12. Cómo se valida

- **Suite:** `tests/test_tac.c` verifica las cuádruplas que produce
  cada construcción del lenguaje, comparando la lista generada instrucción por
  instrucción. La entrada de cada caso es el AST anotado armado a mano, sin
  parser ni análisis semántico.
- **A mano:** el `.ci` es el volcado textual de la lista, así que leerlo es
  inspeccionar la estructura directamente:

```bash
./mi_compilador -target codinter examples/programa.ctds
```

- Un programa que recorre **los 23 opcodes** de una pasada, útil como referencia
  del formato y como prueba de humo del generador:

```c
int g;
int suma(int a, int b){ return a + b; }
void nada(){ return; }
int main(){
    int x; boolean b; boolean c;
    g = 10;                        // global, assign
    x = suma(g, 2);                // param, call con destino
    nada();                        // call sin destino
    x = x + 1;                     // add
    b = (x > 5) && (x < 100);      // gt, lt, if_false, label
    c = (x == 0) || (x > 1);       // eq, if_true
    if (b) { x = -x; } else { c = !b; }   // uminus, not, goto
    while (x < 3) { x = x + 1; }
    return x % 7;                  // mod, ret con valor
}
```

  Verificado: produce las seis familias completas, incluidas las dos variantes
  del salto condicional y las dos del `call`.

---

**Siguiente:** [Generación de Código Intermedio](02-generacion-tac.md)
