# Generación de Código Objeto

**Ficha F18** · Archivos: `include/object_code.h`, `src/object_code.c` (1112 líneas)

La traducción de cada [cuádrupla TAC](05-tac-representation.md) a assembly
x86-64, usando el [modelo de instrucciones](06-asm-instructions.md) y el
[marco de pila](06.5-stack-frame.md). **Es el archivo más grande del proyecto.**

!!! note "El soporte de `float` es una extensión nuestra"
    El enunciado acotaba el alcance de esta etapa a **tipos enteros y lógicos**:
    generar código para `float` **no era necesario**. Se implementó igual, para
    que el compilador no quedara incoherente con la especificación de su propio
    lenguaje. El argumento completo está en la sección *Extensiones sobre el
    enunciado* de [la página inicial](../index.md).

---

## 1. Qué problema resuelve

Convertir una lista de instrucciones abstractas, con temporales ilimitados y
operandos simbólicos, en instrucciones de una máquina real con dieciséis
registros, memoria y una convención de llamada que hay que respetar para que el
resultado se pueda enlazar con libc.

Es la última fase. Lo que emite se ensambla con `gcc` y se ejecuta.

**La prioridad declarada es la correctitud, no la eficiencia.** La estrategia es
la más simple que funciona: cada valor vive en la pila, cada instrucción TAC se
traduce cargando sus operandos a registros, operando y guardando el resultado de
vuelta. Es código lento y es código que se puede verificar leyéndolo.

---

## 2. Interfaz pública

```c
void generateAssembly(const TACList *tacList, OptimizationSet enabled);
int  getObjectCodeErrors(void);

void emitPrologue(const FunctionFrame *frame);
void emitEpilogue(const FunctionFrame *frame);
void generateInstruction(const TACInstruction *instruction, FunctionFrame *frame);
```

`generateAssembly` es el punto de entrada. Las otras tres están expuestas para
que los tests puedan emitir una instrucción aislada sin recorrer un programa
entero; las otras **cincuenta** funciones del módulo son `static`.

**`enabled` llega acá porque `strength-reduce` es la única optimización que
depende de la máquina** (ver la [ficha F15](05.9-optimizer.md)): decidir si una
multiplicación se convierte en un desplazamiento solo tiene sentido cuando ya se
sabe qué instrucción se va a emitir.

`getObjectCodeErrors` devuelve la cuenta de **errores internos**, que no son
errores del programa compilado sino bugs del compilador. Ver §9.

---

## 3. La estructura general

```c
resetGlobalVariables(); resetFloatConstants();
emitFloatConstantPool(tacList);     /* .rodata: las constantes reales */
emitGlobals(tacList);               /* .data:   las variables globales */
buildStringf(".text\n");            /* .text:   el codigo */

for (cada instruccion) {
    if (es TAC_BEGIN_FUNC) {
        frame = createFrame(nombre);
        frame->returnType = ...;
        buildFrameLayout(frame, instruccion);   /* pasada 1: dimensionar */
        instruccion = emitFunction(frame, instruccion);  /* pasada 2: emitir */
        freeFrame(frame);
    }
}
buildStringf("\n.section .note.GNU-stack,\"\",@progbits\n");
```

Tres secciones del objeto y **dos pasadas por función**, detalladas en la
[ficha F17](06.5-stack-frame.md).

**Las tres secciones se emiten en ese orden y no es indistinto:** las constantes
reales y las globales tienen que estar declaradas antes de que el código las
referencie, porque el ensamblador resuelve los símbolos en una pasada.

**`.note.GNU-stack` al final** le dice al enlazador que el programa no necesita
pila ejecutable. Sin esa nota, `ld` emite un aviso y marca la pila como
ejecutable por compatibilidad, lo que es un problema de seguridad y ensucia la
salida de la compilación.

---

## 4. El despachador

`generateInstruction` es un `switch` sobre los 24 opcodes del TAC, con una
función `emitX` por cada uno. Dos casos valen notar:

| Caso | Qué hace |
| --- | --- |
| `TAC_LOCAL_DECL` | **Nada.** La ranura ya la reservó la pasada 1; no hay instrucción que emitir |
| `default` | Nada, en silencio |

Que `TAC_LOCAL_DECL` no emita nada es correcto: declarar una variable local en la
pila no requiere ninguna instrucción, porque el espacio se reservó de una sola
vez en el prólogo con `subq`.

### Los tres patrones de traducción

Casi todas las funciones `emitX` siguen uno de tres esquemas:

```c
/* binaria */                          /* unaria */              /* comparacion */
loadOperand(first,  "%eax");           loadOperand(first,"%eax") loadOperand(first,  "%eax")
loadOperand(second, "%edx");           <op> %eax                 loadOperand(second, "%edx")
<op> %edx, %eax                        store(result, "%eax")     cmpl %edx, %eax
store(result, "%eax")                                            set<cc> %al
                                                                 movzbl %al, %eax
                                                                 store(result, "%eax")
```

**Cargar, operar, guardar.** Los registros nunca conservan un valor entre dos
instrucciones TAC, así que no hace falta llevar la cuenta de qué hay en cada
uno: al principio de cada traducción, el estado de los registros es irrelevante.
Eso es lo que hace al generador fácil de verificar y lo que lo hace lento.

---

## 5. La convención de llamada

### El reparto en registros

System V AMD64 asigna los argumentos a **dos secuencias independientes** según
su clase:

| Posición | Enteros y booleanos (32 bits) | Reales |
| :---: | :---: | :---: |
| 1 | `%edi` | `%xmm0` |
| 2 | `%esi` | `%xmm1` |
| 3 | `%edx` | `%xmm2` |
| 4 | `%ecx` | `%xmm3` |
| 5 | `%r8d` | `%xmm4` |
| 6 | `%r9d` | `%xmm5` |
| 7, 8 | por la pila | `%xmm6`, `%xmm7` |
| 9+ | por la pila | por la pila |

**Son dos contadores separados, no uno.** Verificado con
`f(int a, float b, int c, float d)`:

```asm
f:
    pushq %rbp
    movq %rsp, %rbp
    subq $32, %rsp
    movl %edi, -4(%rbp)        # a: primer entero
    movss %xmm0, -8(%rbp)      # b: primer real
    movl %esi, -12(%rbp)       # c: SEGUNDO entero, no tercer argumento
    movss %xmm1, -16(%rbp)     # d: segundo real
```

`c` recibe `%esi` y no `%edx`: lo que cuenta es su posición **entre los
enteros**, no en la firma. Equivocarse acá produce un programa que compila y
devuelve basura.

### Del lado del llamado: `emitParamDeclaration`

Cada `TAC_PARAM_DECL` copia su registro a la ranura del marco, y **avanza el
contador de su clase**. Si los registros de su clase se agotaron, el argumento
llegó por la pila del llamador y se lee de `+16(%rbp)` en adelante:

```c
int callerOffset = 16 + frame->stackParamCount * 8;
buildStringf("    movl %d(%%rbp), %%eax\n", callerOffset);
buildStringf("    movl %%eax, %d(%%rbp)\n", offset);
```

El 16 es `8` de la dirección de retorno más `8` del `%rbp` guardado, y cada
argumento de pila ocupa 8 bytes aunque el valor sea de 4: la ABI alinea los
argumentos de pila a 8.

**Copiar los argumentos a la pila al entrar** es lo que permite tratarlos igual
que a las locales en el resto de la función, y es lo que los protege de que una
llamada interna sobrescriba los registros.

### Del lado del llamador: `emitCall`

Es la función más compleja del módulo, en cinco pasos:

```c
1. classifyCallArguments(...)     /* quien va a registro y quien a la pila */
2. if (stackArguments impar) subq $8, %rsp       /* relleno de alineacion */
3. emitCallStackArguments(...)    /* pushq en orden INVERSO */
4. emitCallRegisterArguments(...) /* cargar los registros */
5. call <nombre>
6. if (hubo pila o relleno) addq $N, %rsp        /* limpieza */
7. emitCallResult(...)            /* %eax o %xmm0 -> la ranura del resultado */
```

**El orden de los pasos 3 y 4 importa:** los `pushq` se hacen **antes** de cargar
los registros, porque `pushq` usa `%rax` como intermediario y clobbearía un
registro de argumento ya cargado.

**Los argumentos de pila se apilan en orden inverso** (`for (i = n-1; i >= 0; i--)`),
porque la pila crece hacia abajo y la ABI exige que el argumento 7 quede en la
dirección más baja de los apilados, o sea que sea el último en apilarse.

**El relleno de alineación.** Cada `pushq` mueve `%rsp` 8 bytes. Si se apila una
cantidad **impar** de argumentos, `%rsp` queda desalineado respecto de los 16
bytes que la ABI exige en el `call`, así que se resta un 8 extra antes. Verificado
con una llamada de 7 argumentos (uno por pila, impar) y otra de 8 (dos por pila,
par): las dos dan el resultado correcto.

**`frame->pendingParamCount = base;`** restaura el contador al valor que tenía
antes de esta llamada, **no a cero**. Esa diferencia es lo que permite las
llamadas anidadas: los argumentos de la llamada externa que ya estaban
pendientes siguen ahí.

---

## 6. Las traducciones que tienen algo que contar

### División y módulo: la misma instrucción

```c
static void emitModulo(const TACInstruction *instruction, const FunctionFrame *frame) {
    emitDivisionOp(instruction, frame, "%edx");    /* el resto */
}
static void emitDivision(...) {
    emitDivisionOp(instruction, frame, "%eax");    /* el cociente */
}
```

`idivl` deja el **cociente en `%eax` y el resto en `%edx`**, así que división y
módulo son exactamente el mismo código leyendo distinto registro. Esa es la
razón de que las dos compartan `emitDivisionOp`.

```asm
movl  <dividendo>, %eax
movl  <divisor>,   %ecx
cltd                      # extiende el signo de %eax a %edx:%eax
idivl %ecx
```

**Dos detalles obligatorios de x86:**

1. **`cltd` antes de `idivl`.** La división con signo toma un dividendo de 64
   bits repartido en `%edx:%eax`, así que hay que propagar el bit de signo. Sin
   el `cltd`, `%edx` tiene basura y dividir un negativo da un resultado
   arbitrario.
2. **El divisor va en `%ecx`, nunca en `%edx`.** Porque `cltd` **sobrescribe
   `%edx`**: si el divisor estuviera ahí, se destruiría justo antes de usarlo. Es
   la razón de que `%ecx` esté en el enum de registros de la ficha F16.

Verificado: `-17 / 5` da `-3` y `-17 % 5` da `-2`, que es la semántica de C
(truncamiento hacia cero).

### Comparaciones: `setCC` más `movzbl`

Las instrucciones `setl`, `setg`, `sete` solo pueden escribir en un registro de
**8 bits**:

```asm
cmpl   %edx, %eax      # calcula %eax - %edx y setea flags
setl   %al             # %al = 1 o 0; los otros 24 bits de %eax quedan sucios
movzbl %al, %eax       # extiende con ceros a los 32 bits
```

El `movzbl` es obligatorio: sin él, el booleano arrastraría los bits altos de lo
que hubiera antes en `%eax`, y un `if` sobre ese valor compararía contra cero un
número que no es 0 ni 1.

**El orden de `cmpl` en sintaxis AT&T es el inverso del que se lee.**
`cmpl %edx, %eax` calcula `%eax - %edx`, así que `setl` significa
`%eax < %edx`, o sea *primer operando menor que el segundo*. Es correcto, y es
fácil de invertir por accidente.

### Comparaciones de reales: `comiss` y el caso NaN

```asm
movss  <a>, %xmm0
movss  <b>, %xmm1
comiss %xmm1, %xmm0
setb   %al             # ¡setb, no setl!
```

**`comiss` setea los flags como una comparación sin signo**, así que las
condiciones son `setb`/`seta`/`sete` (*below*/*above*) y no `setl`/`setg`
(*less*/*greater*). Usar las de con signo da resultados incorrectos.

Y hay un segundo problema, que es el detalle más fino del back-end:

```c
// En IEEE-754 las comparaciones con NaN son falsas; setnp filtra el flag de paridad.
bool requiresUnorderedCheck = (strcmp(conditionMnemonic, "sete") == 0 ||
                               strcmp(conditionMnemonic, "setb") == 0);
if (requiresUnorderedCheck) {
    buildStringf("    setnp %%dl\n");
    buildStringf("    andb %%dl, %%al\n");
}
```

Cuando alguno de los operandos es **NaN**, `comiss` deja la comparación
*no ordenada* y levanta el **flag de paridad**, además de dejar `ZF` y `CF` en
un estado que haría que `sete` y `setb` dieran verdadero. Pero IEEE-754 exige
que **toda comparación con NaN sea falsa**, incluida `NaN == NaN`.

La corrección es leer el flag de paridad con `setnp` y hacer un `and` con el
resultado: si la comparación fue no ordenada, el resultado se fuerza a 0.

Verificado produciendo un NaN con `0.0 / 0.0`:

```c
int main(){ float z; float n; z = 0.0; n = z / z; if (n == n) { return 1; } return 42; }
```

Devuelve **42**: `NaN == NaN` dio falso, como corresponde.

Solo `sete` y `setb` lo necesitan; `seta` ya da falso en el caso no ordenado.

### Reducción de fuerza en multiplicaciones

La única optimización que vive acá. Si uno de los operandos es una constante
entera, `emitStrengthReducedMultiplication` prueba tres casos:

| Multiplicador | Qué emite |
| --- | --- |
| `0` | `xorl %eax, %eax` — más corto que cargar `$0` |
| `1` | Solo la carga y el guardado: la multiplicación desaparece |
| Potencia de dos | `sall $n, %eax` — un desplazamiento |
| Cualquier otro | Devuelve `false`: se emite el `imull` normal |

La detección de potencia de dos es el truco clásico: `(v & (v - 1)) == 0`.

Solo se aplica si `strength-reduce` está activa, y solo a constantes **enteras**:
en punto flotante el desplazamiento no equivale a multiplicar.

### Saltos condicionales

```asm
movl <condicion>, %eax
cmpl $0, %eax
je   .L3              # TAC_IF_FALSE: salta si es cero
```

`TAC_IF_TRUE` es idéntico con `jne`. Las dos variantes del opcode del TAC se
traducen a las dos variantes del salto de máquina, sin ninguna negación
intermedia: ahí se cobra la decisión de tener los dos opcodes en el TAC.

**Las etiquetas llevan un punto adelante**: la etiqueta `L3` del TAC se emite como
`.L3`, lo que la marca como **local al archivo** y evita que entre en la tabla de
símbolos del objeto.

### Retorno

```c
loadOperand(valor, "%eax")  /* o loadFloatOperand(valor, "%xmm0") */
buildStringf("    jmp .L_%s_epilogue\n", frame->functionName);
```

Todos los `return` **saltan al epílogo común** en lugar de duplicar el desarmado
del marco. El valor se deja en `%eax` o en `%xmm0` según la clase, que es lo que
la ABI define como registro de retorno.

---

## 7. Globales y constantes reales

### Las globales van a `.data`

```asm
.data
.globl g
.align 4
g:
    .long 0
```

Y se acceden con `g(%rip)` en lugar de un offset del marco. La distinción la
hereda del opcode del TAC: `TAC_GLOBAL_DECL` contra `TAC_LOCAL_DECL`.

**El símbolo usa el nombre del programa, sin el sufijo `@uid`.** `getGlobalName`
devuelve `operand->symbol->id` —el identificador original— y solo cae al
`operand->name` calificado si el operando no tiene símbolo asociado.

Eso es deliberado y vale entender por qué: el `@uid` existe para desambiguar
nombres repetidos, y **una global no puede estar repetida**, porque hay un solo
ámbito global. Al no necesitar el sufijo, usar el nombre limpio hace que la tabla
de símbolos del objeto sea legible: un `nm` sobre el ejecutable muestra `g` y no
`g@1`. Los locales sí llevan el sufijo, pero nunca se vuelven símbolos: viven en
offsets del marco, que no tienen nombre en el objeto.

### Las constantes reales necesitan un pool

`movss` **no acepta operandos inmediatos**: no existe `movss $1.5, %xmm0`. Las
constantes reales tienen que estar en memoria, así que se emiten a `.rodata` con
una etiqueta cada una, y `internFloatConstant` devuelve la etiqueta:

```asm
.section .rodata
.align 4
.LC0:
    .long 1069547520        # 1.5 en IEEE-754 de 32 bits
```

```asm
movss .LC0(%rip), %xmm0
```

El valor se emite como el **patrón de bits** del `float`, obtenido con
`getFloatBits`, y no como un literal decimal: así no hay pérdida por la
conversión a texto y vuelta.

`internFloatConstant` **reutiliza la etiqueta** si la constante ya apareció, de
ahí el nombre: es un *interning*, igual que el de cadenas de otros lenguajes.

---

## 8. La resolución de operandos

Todas las cargas y guardados pasan por `resolveOperandLocation`, que contesta
dónde está un operando:

```c
if (getSlotOffset(frame, operand->name, offset))  return LOCATION_STACK;
if (operand->kind == OPERAND_VARIABLE && isDeclaredGlobal(operand)) return LOCATION_GLOBAL;

fprintf(stderr, "Error interno del generador de codigo objeto: el operando '%s' "
                "no tiene lugar asignado en el frame de '%s' ni esta declarado como global.\n", ...);
objectCodeErrors++;
return LOCATION_UNRESOLVED;
```

**El tercer caso es el valioso.** Si un operando no está ni en el marco ni entre
las globales, significa que la pasada de dimensionamiento se lo salteó, o sea un
bug del compilador. Antes de este chequeo el generador emitía `0(%rbp)` —el
`%rbp` guardado— y producía un ejecutable que corrompía su propio marco: un fallo
silencioso de los peores. Ahora se reporta con el nombre del operando y de la
función.

Las cuatro funciones de acceso son simétricas:

| Función | Qué hace |
| --- | --- |
| `loadOperand` | Constante → inmediato; si no, pila o global → registro general |
| `storeOperand` | Registro general → pila o global |
| `loadFloatOperand` | Constante → etiqueta de `.rodata`; si no, pila o global → `%xmm` |
| `storeFloatOperand` | `%xmm` → pila o global |

Que una constante entera se cargue como **inmediato** (`movl $42, %eax`) y una
real tenga que pasar por `.rodata` es la asimetría de §7.

---

## 9. Los errores internos

El módulo lleva un contador `objectCodeErrors` que se incrementa en cinco
situaciones, todas bugs del compilador y no del programa compilado:

| Dónde | Cuándo |
| --- | --- |
| `generateAssembly` | No se pudo crear el marco de una función |
| `generateAssembly` | `buildFrameLayout` devolvió `false`: no se pudieron reservar las ranuras de argumentos |
| `emitParam` | No se pudo ampliar el arreglo de argumentos pendientes |
| `emitCall` | No se pudo reservar la clasificación de argumentos |
| `resolveOperandLocation` | Un operando sin lugar asignado (§8) |

**Los dos primeros se reportan en `generateAssembly` y no en la función que
falló**, que es la forma del arreglo de propagación de fallas de reserva:
`buildFrameLayout` y `allocatePendingParamSlots` devuelven `bool` y el mensaje
—que es el que tiene el nombre de la función a mano— lo emite el llamador. En los
dos casos se hace `break` del bucle: si no se pudo dimensionar un marco, seguir
emitiendo produciría assembly con offsets inventados.

`getObjectCodeErrors` permite que el driver los consulte y aborte en lugar de
entregar un ejecutable que compiló pero está roto. La diferencia con un error
semántico es importante: un error semántico significa "tu programa está mal", un
error interno significa "el compilador está mal".

---

## 10. Inventario de funciones

Cincuenta y cinco funciones: **cinco públicas** y **cincuenta `static`**. Es el
archivo con más funciones del proyecto, y está ordenado por la regla de descenso:
el punto de entrada arriba, el acceso a operandos abajo.

### Públicas

| Función | Qué hace |
| --- | --- |
| `generateAssembly(tacList, enabled)` | Las tres secciones y el bucle por función de §3 |
| `getObjectCodeErrors()` | La cuenta de errores internos de §9 |
| `emitPrologue(frame)` | `.globl`, la etiqueta, `pushq %rbp`, `movq %rsp, %rbp` y el `subq` del marco |
| `emitEpilogue(frame)` | La etiqueta `.L_f_epilogue`, el `leave` y el `ret` |
| `generateInstruction(instruction, frame)` | El despachador de §4 |

`emitEpilogue` tiene un caso especial que conviene conocer porque se pregunta
fácil: si la función es `main` **y** su tipo de retorno es `void`, pone `%eax` en
cero antes de salir. El sistema operativo toma `%eax` como código de salida del
proceso, así que un `void main` sin esa línea devolvería lo que hubiera quedado
en el registro.

### El armado del marco (pasada 1)

| Función | Qué hace |
| --- | --- |
| `buildFrameLayout` | Llama a las dos de abajo y **alinea `frameSize` a 16** |
| `allocateLocalSlots` | Una ranura por `local_decl`, por `param_decl` y por cada temporal que aparezca como resultado |
| `allocatePendingParamSlots` | Calcula el **pico** de argumentos pendientes y reserva esa cantidad de ranuras anónimas |

`allocatePendingParamSlots` es la más sutil de las tres. Recorre la función
llevando un contador: cada `param` lo sube, cada `call n` lo baja en `n`, y se
queda con el máximo que alcanzó. Ese pico es cuántas ranuras hacen falta **a la
vez**, y es lo que hace que `f(g(1), h(2))` funcione: cuando se evalúa `g(1)` ya
hay un argumento de `f` pendiente, así que los dos necesitan ranura propia. Un
contador que se reseteara en cada `call` daría un pico demasiado chico y los
argumentos se pisarían.

### Una función por opcode (pasada 2)

| Función | Qué emite |
| --- | --- |
| `emitParamDeclaration` | Copia el registro de argumento a su ranura, o lo lee del marco del llamador si los registros se agotaron. Avanza el contador de **su clase** |
| `emitAssignment` | Cargar y guardar, por `%eax` o por `%xmm0` según el tipo |
| `emitAddition` / `emitSubtraction` | `addss`/`subss` si es real; si no, `ASM_ADD`/`ASM_SUB` vía `emitBinaryOp` |
| `emitMultiplication` | Primero prueba la reducción de fuerza; si no aplica, el `imull` normal |
| `emitDivision` | `divss` si es real; si no, `emitDivisionOp` leyendo `%eax` |
| `emitModulo` | `emitDivisionOp` leyendo `%edx`. **No pregunta si es real:** `%` solo existe para enteros |
| `emitUnaryMinus` | `negl` para enteros; para reales, `0 - x` con `pxor` y `subss` |
| `emitNot` | `xorl $1` |
| `emitLessThan` / `emitGreaterThan` / `emitEquality` | Eligen entre la comparación entera (`setl`/`setg`/`sete`) y la de reales (`setb`/`seta`/`sete`) |
| `emitIntToFloat` | `cvtsi2ss` de `%eax` a `%xmm0`: la coerción de la regla 14 |
| `emitLabel` | `.L3:` — la etiqueta sale de `result` |
| `emitGoto` | `jmp .L3` — sale de `firstOperand` |
| `emitIfFalse` / `emitIfTrue` | `cmpl $0` más `je`/`jne` — la etiqueta sale de `secondOperand` |
| `emitParam` | Deja el argumento en su ranura pendiente y anota su tipo |
| `emitCall` | Los siete pasos de §5 |
| `emitReturn` | Deja el valor en `%eax`/`%xmm0` y salta al epílogo común |

**Las tres posiciones distintas de la etiqueta** —`result` en `emitLabel`,
`firstOperand` en `emitGoto`, `secondOperand` en los saltos condicionales— son la
asimetría del catálogo de cuádruplas, y son exactamente la razón de que el
generador de TAC tenga una capa de emisores con nombre
([ficha F13](05.5-tac-generation.md)). Acá se paga del otro lado: hay que saber en
qué campo guardó la etiqueta cada opcode.

**`emitNot` merece un párrafo.** Negar un booleano con `xorl $1` da el resultado
correcto **solo si el valor es exactamente 0 o 1**: con cualquier otro bit
prendido devolvería un número distinto de 0 y de 1, y el `if` siguiente lo leería
como verdadero. El invariante lo garantizan las comparaciones, que terminan en
`movzbl` (§6), y las constantes booleanas del TAC, que son 0 o 1. A cambio es una
instrucción en lugar de las tres que costaría `cmpl $0` + `sete` + `movzbl`.

### Los auxiliares de emisión

| Función | Qué hace |
| --- | --- |
| `emitFunction` | Prólogo, el bucle de instrucciones hasta `end_func`, epílogo. Devuelve dónde quedó el recorrido |
| `emitBinaryOp` | El patrón entero: `%eax`, `%edx`, operar, guardar |
| `emitFloatBinaryOp` | El mismo con `%xmm0`/`%xmm1` y el mnemónico por parámetro |
| `emitDivisionOp` | `cltd` más `idivl %ecx`, con el registro de resultado por parámetro |
| `emitRelationalOp` | `cmpl` + `setCC` + `movzbl` |
| `emitFloatRelationalOp` | `comiss` + `setCC` + el filtro de NaN si corresponde + `movzbl` |
| `emitStrengthReducedMultiplication` | Los tres casos de §6; devuelve `false` si no aplica |
| `powerOfTwoExponent` | `(v & (v - 1)) == 0` y después el exponente |
| `classifyCallArguments` | Decide, argumento por argumento, registro de su clase o pila |
| `emitCallStackArguments` | Los `pushq`, en orden inverso |
| `emitCallRegisterArguments` | Las cargas a los registros, después de los `pushq` |
| `emitCallResult` | `%eax`/`%xmm0` a la ranura del resultado, si la llamada tiene resultado |

Las cuatro funciones con un **mnemónico o un registro por parámetro** son el
patrón que mantiene corto al archivo: `emitDivisionOp` sirve para `/` y para `%`
cambiando el registro que se lee, y `emitRelationalOp` sirve para los tres
relacionales cambiando el `setCC`. Agregar `!=` es una línea precisamente por eso.

`powerOfTwoExponent` tiene una sutileza de contrato: devuelve **`-1`** si el
valor no es potencia de dos, pero devuelve **`0`** para el valor `1`, que sí lo
es. El llamador exige `exponent > 0`, así que trata ese `0` como "no aplica" — y
es correcto, porque el caso `× 1` ya se resolvió antes, en su propia rama.

### El acceso a operandos

| Función | Qué hace |
| --- | --- |
| `loadOperand` | Constante → inmediato; si no, resuelve y carga de la pila o de la global |
| `storeOperand` | Resuelve y guarda a la pila o a la global |
| `loadFloatOperand` | Constante → `movss` de la etiqueta de `.rodata`; si no, como el anterior |
| `storeFloatOperand` | `movss` del `%xmm` a la pila o a la global |
| `resolveOperandLocation` | Pila, global, o el error interno de §8 |
| `isFloatOperand` | `operand->type == TYPE_FLOAT` |
| `isDeclaredGlobal` | Búsqueda lineal en el arreglo de nombres globales |
| `registerGlobalVariable` | Agrega un nombre al arreglo, duplicando la capacidad desde 64 |
| `getGlobalName` | El `id` del símbolo si lo tiene; si no, el nombre calificado (§7) |

**`isFloatOperand` es una línea y es la bisagra del módulo entero**: cada `emitX`
con dos rutas la consulta. Y **cuál operando se le pasa no es indistinto**:

| Quién pregunta | Qué operando mira | Por qué |
| --- | --- | --- |
| Las aritméticas | `result` | El tipo del resultado es el tipo de la operación |
| Las comparaciones | `firstOperand` | El resultado de una comparación es **siempre `boolean`**: preguntarle a él nunca diría que la comparación es de reales |

Equivocarse ahí es un error que compila y da resultados incorrectos solo en los
programas con reales, que es la peor clase de bug para encontrar.

Y hay una asimetría más, entre las dos funciones de globales:
`isDeclaredGlobal` busca por `operand->name`, el nombre **calificado con `@uid`**,
mientras `getGlobalName` emite el símbolo **sin** el sufijo. Las dos conviven
porque la lista interna habla el idioma del TAC y el assembly habla el del
programa (§7).

### Las constantes reales y las globales

| Función | Qué hace |
| --- | --- |
| `internFloatConstant` | Devuelve la etiqueta de un valor, reutilizándola si ya apareció |
| `getFloatBits` | El `union` que lee un `float` como `unsigned int` |
| `emitFloatConstantPool` | Una pasada por todo el TAC: interna cada constante real y emite `.rodata` |
| `emitGlobals` | Emite `.data` con una palabra por `global_decl`, y las registra |
| `resetGlobalVariables` / `resetFloatConstants` | Liberan los dos arreglos estáticos |

**`internFloatConstant` compara patrones de bits, no valores**, y eso resuelve
dos problemas de IEEE-754 de una vez: `-0.0 == 0.0` es verdadero, así que con `==`
las dos constantes compartirían etiqueta y una quedaría con el signo equivocado;
y `NaN == NaN` es falso, así que un NaN repetido nunca se reutilizaría y llenaría
el pool de copias. Comparar los 32 bits es exacto en los dos casos.

**Los dos `reset` se llaman al entrar y al salir** de `generateAssembly`. El de
salida es el que permite que los tests llamen a `generateAssembly` muchas veces
en el mismo proceso sin arrastrar las globales ni las etiquetas `.LC` de la
corrida anterior.

---

## 11. Conexión con las fases vecinas

| Viene de | Qué cruza la frontera |
| --- | --- |
| `tac_list.h` (F12, F13) | La lista de cuádruplas, ya optimizada si se pidió |
| `optimizer.h` (F15) | El `OptimizationSet`, solo para `strength-reduce` |
| `function_frame.h` (F17) | `frameSize` y un offset por operando |
| `asm_instruction.h` (F16) | Los constructores y `emitAsm`, para el 20% del assembly |
| `string_builder.h` (F04) | `buildStringf`, para el otro 80% |

| Va hacia | Qué entrega |
| --- | --- |
| `driver.c` (F02) | El texto del assembly, que se escribe al `.ass` o se pasa a `gcc` |

**Lo que esta fase asume y no verifica**, porque las anteriores lo garantizan:

- Todo operando tiene su ranura o es global (si no, §8 lo reporta).
- Todo método termina en un `ret` del TAC, escrito o implícito (lo garantiza
  `generateIntermediateCode`).
- Los tipos de los operandos son correctos (lo garantiza el chequeo de tipos).
- Toda etiqueta referenciada está definida (lo garantiza el generador de TAC).

---

## 12. Cómo se extiende

### Un opcode TAC nuevo

Un `case` en `generateInstruction` y su función `emitX`. **Si se omite, cae en el
`default` que no hace nada**: la instrucción desaparece del ejecutable sin un solo
aviso. Es el fallo silencioso de esta ficha, el mismo patrón que en F13.

### Un operador relacional nuevo (`!=`, `<=`, `>=`)

Cada uno es una línea: `emitRelationalOp(instruction, "setne", frame)` y su
variante de reales con el mnemónico sin signo correspondiente. **Y hay que
revisar si necesita el filtro de NaN** de §6: la regla es que lo necesita toda
condición que daría verdadero en el caso no ordenado.

### Soportar `double`

Toca las tres fichas del back-end: mnemónicos `sd` en vez de `ss`
(`movsd`, `addsd`, `comisd`), ranuras de 8 bytes con su alineación en F17, y la
clase de registro sigue siendo `%xmm` pero con el doble de ancho.

### Reducir el código generado

La mejora más grande sería **mantener valores en registros** entre instrucciones
TAC consecutivas, en lugar de guardar y recargar. Hoy `t0 = a + b; t1 = t0 * c`
guarda `t0` a la pila y lo vuelve a leer en la instrucción siguiente. Detectar
que un temporal se usa inmediatamente después y dejarlo en `%eax` eliminaría dos
instrucciones por operación. Requiere llevar el estado de los registros, que es
exactamente lo que el diseño actual evita a propósito.

---

## 13. Trampas y límites conocidos

- **El `default` del despachador no avisa** (§12).
- **El divisor nunca puede ir en `%edx`** porque `cltd` lo sobrescribe (§6).
- **Las comparaciones de reales usan condiciones sin signo** (`setb`/`seta`), no
  las de con signo (§6).
- **Solo `sete` y `setb` llevan el filtro de NaN** (§6). Agregar un relacional
  nuevo exige decidir si lo necesita.
- **`movss` no acepta inmediatos**, de ahí el pool de `.rodata` (§7).
- **Los registros no conservan valores entre instrucciones TAC** (§4). Es
  deliberado, y es la razón de que el código sea largo.
- **`emitCall` libera `registerIndex` y `goesToStack` en el camino normal**, pero
  en el camino de error de la reserva devuelve antes sin haber reservado, así que
  no hay fuga; conviene no reordenar esos `return`.
- **Las globales pierden el sufijo `@uid` al volverse símbolos** (§7), al
  contrario de lo que pasa en el TAC. Buscar `g@1` en el `.ass` no encuentra nada;
  el símbolo es `g`.
- **Todo es de 32 bits.** Los mnemónicos tienen el sufijo `l` fijo y los reales
  son de precisión simple.
- **`-0.0` no se puede producir.** `emitUnaryMinus` niega un real como `0 - x`, y
  en IEEE-754 `0.0 - 0.0` es `+0.0`, no `-0.0`. Negar el cero da cero positivo.
  La alternativa correcta sería invertir el bit de signo con un `xorps` contra una
  máscara. No afecta a ninguna operación aritmética del lenguaje, pero es una
  diferencia real con la semántica de C.
- **`internFloatConstant` degrada en silencio si falla la reserva**: devuelve la
  etiqueta de la primera constante del pool, o la cadena vacía si el pool está
  vacío. Lo primero haría que el programa calcule con **otra** constante; es
  inalcanzable en la práctica, pero es el único camino del módulo que produce
  código incorrecto en lugar de reportar el error.

---

## 14. La teoría detrás

- **Generación de código** (*code generation*): la fase que traduce la IR a
  instrucciones de máquina. Sus tres subproblemas clásicos son la **selección de
  instrucciones**, la **asignación de registros** y el **ordenamiento de
  instrucciones**. Acá el primero es uno a uno, el segundo es trivial (todo a la
  pila) y el tercero no se hace: se respeta el orden del TAC.
- **Convención de llamada** (*calling convention* / ABI): el contrato entre
  funciones compiladas separadamente. Define quién pasa los argumentos dónde,
  quién preserva qué registros, por dónde vuelve el resultado y cómo queda
  alineada la pila. Respetarla es lo que permite enlazar con libc y que `gcc`
  acepte el objeto.
- **Clasificación de argumentos**: la ABI de System V clasifica cada argumento en
  INTEGER, SSE o MEMORY y lo asigna a la secuencia de registros de su clase. Las
  dos secuencias independientes de §5 son la implementación de esa regla para los
  dos casos que el lenguaje necesita.
- **Registros preservados por el llamado contra por el llamador**
  (*callee-saved* / *caller-saved*): la ABI reparte los registros en dos grupos.
  Este generador usa solo `%eax`, `%edx`, `%ecx` y los de argumentos, que son
  todos *caller-saved*, así que **no necesita preservar nada**, y por eso el
  prólogo no tiene más que el `push` de `%rbp`. Usar `%rbx` o `%r12`–`%r15`
  obligaría a guardarlos y restaurarlos.
- **IEEE-754 y las comparaciones no ordenadas**: el estándar define que cualquier
  comparación en la que participe un NaN es falsa, incluida la igualdad de un NaN
  consigo mismo. Es la razón del filtro de paridad de §6, y la razón de que en
  todos los lenguajes con reales `x != x` sea una forma de preguntar si `x` es
  NaN.
- **Código independiente de la posición** (*PIC*): el direccionamiento
  `símbolo(%rip)` calcula direcciones relativas al contador de programa, lo que
  permite cargar el ejecutable en cualquier dirección. Es el modo por defecto de
  los enlazadores actuales.
- **Interning de constantes**: reutilizar una sola copia de cada valor repetido.
  Acá se aplica a las constantes reales porque no pueden ser inmediatos y hay que
  darles una dirección.
- **Reducción de fuerza** (*strength reduction*): reemplazar una operación por
  otra equivalente más barata. Depende de la máquina, y por eso es la única
  optimización que vive en esta fase.

---

## 15. Cómo se valida

- **Suite:** `tests/test_object_code.c` (1401 líneas), la más grande del
  proyecto. Verifica el assembly emitido por cada instrucción TAC.
- **A mano:** el `.ass` es la salida directa:

```bash
./mi_compilador -target assembly examples/factorial.ctds
```

- **La validación que importa es ejecutar.** Un error de esta fase casi nunca se
  manifiesta como un error de ensamblado: se manifiesta como un **resultado
  incorrecto**, porque el assembly es sintácticamente válido y hace algo distinto
  de lo pedido. Por eso cada construcción necesita un test que **compile, ejecute
  y compare**:

```bash
./mi_compilador -o /tmp/f examples/dead_code.ctds && /tmp/f; echo $?   # espera 10
```

- **Los casos que conviene tener en la batería**, porque son los que rompieron
  alguna vez:

| Caso | Qué verifica |
| --- | --- |
| División y módulo de negativos | El `cltd` y el divisor en `%ecx` |
| Llamada con 7 argumentos | El relleno de alineación con cantidad impar en pila |
| Llamada con 8 argumentos | Los argumentos por pila en orden inverso |
| Argumentos `int` y `float` intercalados | Las dos secuencias de registros |
| Llamada anidada `f(g(1), h(2))` | Las ranuras de argumentos pendientes |
| `NaN == NaN` vía `0.0 / 0.0` | El filtro de paridad |
| Recursión (factorial) | El marco y el epílogo común |
| Comparaciones de reales | Las condiciones sin signo |

- **Y el chequeo de que el optimizador no cambió nada:** compilar con y sin
  `-opt all`, ejecutar los dos y comparar.

---

**Siguiente:** [Intérprete](07-interpreter.md) · extensión nuestra, fuera de la entrega
