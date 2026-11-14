# Generación de Código Objeto

**Archivos:** `include/object_code.h`, `src/object_code/object_code.c` (668 líneas)

La traducción de cada [cuádrupla TAC](../05-codigo-intermedio/01-representacion-tac.md) a assembly
x86-64 usando el [marco de pila](01-marco-de-pila.md). El assembly se emite como
texto con `buildStringf` del [acumulador de cadenas](../02-infraestructura/04-utilidades-y-tipos.md): no hay
una representación intermedia de instrucciones de máquina. **Es el archivo más
grande del proyecto.**

!!! note "Esta etapa cubre tipos enteros y lógicos"
    El enunciado acota el código objeto a **tipos enteros y lógicos**, así que
    acá no hay nada de `float`: un programa que use reales se rechaza antes, al
    generar el código intermedio, con el mensaje y la línea correspondientes.
    Ver [Alcance y Extensiones](../../decisiones/alcance-y-extensiones.md#1-alcance-de-float).

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
entero; las otras **treinta y dos** funciones del módulo son `static`.

**`enabled` llega acá porque `strength-reduce` es la única optimización que
depende de la máquina** (ver la [optimizador](../06-optimizador/02-optimizador.md)): decidir si una
multiplicación se convierte en un desplazamiento solo tiene sentido cuando ya se
sabe qué instrucción se va a emitir.

`getObjectCodeErrors` devuelve la cuenta de **errores internos**, que no son
errores del programa compilado sino bugs del compilador. Ver §9.

---

## 3. La estructura general

```c
emitGlobals(tacList);               /* .data:  las variables globales */
buildStringf(".text\n");            /* .text:  el codigo */

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

Dos secciones del objeto y **dos pasadas por función**, detalladas en la
[marco de pila](01-marco-de-pila.md).

**Las dos secciones se emiten en ese orden y no es indistinto:** las variables
globales tienen que estar declaradas antes de que el código las referencie, porque el ensamblador resuelve los símbolos en una pasada.

**`.note.GNU-stack` al final** le dice al enlazador que el programa no necesita
pila ejecutable. Sin esa nota, `ld` emite un aviso y marca la pila como
ejecutable por compatibilidad, lo que es un problema de seguridad y ensucia la
salida de la compilación.

---

## 4. El despachador

`generateInstruction` es un `switch` sobre los opcodes del TAC. Los que
comparten patrón comparten función, y la diferencia va por parámetro:
`TAC_ADD` y `TAC_SUB` llaman a `emitBinaryOp` con `"addl"` o `"subl"`,
`TAC_DIV` y `TAC_MOD` a `emitDivisionOp` con el registro de resultado,
`TAC_LT`/`TAC_GT`/`TAC_EQ` a `emitRelationalOp`, y `TAC_IF_FALSE`/`TAC_IF_TRUE`
a `emitConditionalJump` con `"je"` o `"jne"`. Dos casos valen notar:

| Caso | Qué hace |
| --- | --- |
| `TAC_LOCAL_DECL` | **Nada.** La ranura ya la reservó la pasada 1; no hay instrucción que emitir |
| `default` | Nada, en silencio |

Que `TAC_LOCAL_DECL` no emita nada es correcto: declarar una variable local en la
pila no requiere ninguna instrucción, porque el espacio se reservó de una sola
vez en el prólogo con `subq`.

### Los tres patrones de traducción

Casi todas las funciones de emisión siguen uno de tres esquemas:

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

System V AMD64 clasifica cada argumento y lo asigna a la secuencia de registros
de su clase. Como el lenguaje generable es de **enteros y booleanos**, acá se
usa una sola:

| Posición | Registro (32 bits) |
| :---: | :---: |
| 1 | `%edi` |
| 2 | `%esi` |
| 3 | `%edx` |
| 4 | `%ecx` |
| 5 | `%r8d` |
| 6 | `%r9d` |
| 7+ | por la pila |

La ABI define además una secuencia aparte, `%xmm0`–`%xmm7`, para los argumentos
de punto flotante, y **las dos avanzan independientemente**: en
`f(int a, float b, int c)`, `c` recibiría `%esi` —el segundo registro
*entero*— y no `%edx`. Vale saberlo porque es lo que haría falta si alguna vez
se extendiera la etapa a reales, y porque es un clásico de examen; en el
compilador actual no hace falta, porque un programa con reales no llega hasta
acá.

### Del lado del llamado: `emitParamDeclaration`

Cada `TAC_PARAM_DECL` copia su registro a la ranura del marco y avanza
`integerParamCount`. Si los seis registros ya se usaron, el argumento llegó por
la pila del llamador, se lee de `+16(%rbp)` en adelante y avanza
`stackParamCount`:

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

Es la función más compleja del módulo. Cuando llega el `call n`, los `n`
argumentos ya están copiados en ranuras del marco (los dejó ahí cada `param`,
ver la [marco de pila](01-marco-de-pila.md)); los últimos `n` de la pila de
pendientes son los de esta llamada:

```c
1. base = pendingParamCount - n;  /* primer argumento de esta llamada */
2. if (stackArguments impar) subq $8, %rsp       /* relleno de alineacion */
3. emitCallStackArguments(...)    /* del 7.o en adelante: pushq en orden INVERSO */
4. emitCallRegisterArguments(...) /* los primeros 6: ranura -> registro */
5. call <nombre>
6. if (hubo pila o relleno) addq $N, %rsp        /* limpieza */
7. pendingParamCount = base;
8. emitCallResult(...)            /* %eax -> la ranura del resultado */
```

**Primero se apila y después se cargan los registros.** Cada argumento de pila
pasa por `%eax` (`movl ranura, %eax; pushq %rax`), que no es registro de
argumento, así que no pisa ninguno; dejar las cargas a registro para el final
garantiza que nada las sobrescriba entre la carga y el `call`.

**Los argumentos de pila se apilan en orden inverso**
(`for (i = argumentCount - 1; i >= MAX_ARG_REGISTERS; i--)`), porque la pila
crece hacia abajo y la ABI exige que el argumento 7 quede en la dirección más
baja de los apilados, o sea que sea el último en apilarse.

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
case TAC_DIV:
    emitDivisionOp(instruction, frame, "%eax");    /* el cociente */
    break;

case TAC_MOD:
    emitDivisionOp(instruction, frame, "%edx");    /* el resto */
    break;
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
   el único lugar del generador que usa `%ecx`.

Verificado: `-17 / 5` da `-3` y `-17 % 5` da `-2`, que es la semántica de C
(truncamiento hacia cero).

### Comparaciones: `setCC` más `movzbl`

Las instrucciones `setl`, `setg`, `sete` solo pueden escribir en un registro de
**8 bits**. El mnemónico de cada opcode sale de `getRelationalMnemonics`, que
para `TAC_LT`, `TAC_GT` y `TAC_EQ` devuelve las tres variantes que se usan: el
`setCC` para producir el booleano, y el salto que se toma si la comparación da
verdadera y si da falsa (estos dos los usa la fusión con el salto siguiente, ver
el [ADR 003](../../decisiones/adr-003-fusion-comparacion-y-salto-condicional.md)):

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

Solo se aplica si `strength-reduce` está activa.

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
loadOperand(valor, "%eax");
buildStringf("    jmp .L_%s_epilogue\n", frame->functionName);
```

Todos los `return` **saltan al epílogo común** en lugar de duplicar el desarmado
del marco. El valor se deja en `%eax`, que es el registro de retorno que define
la ABI para la clase entera.

---

## 7. Las variables globales

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

**Saber si un operando es global no requiere ninguna búsqueda:**
`isDeclaredGlobal` lee `operand->symbol->isGlobal`, la marca que dejó la tabla
de símbolos al registrar la variable en el ámbito global (ver el
[ADR 002](../../decisiones/adr-002-resolucion-variables-globales-isglobal.md)).

Eso es deliberado y vale entender por qué: el `@uid` existe para desambiguar
nombres repetidos, y **una global no puede estar repetida**, porque hay un solo
ámbito global. Al no necesitar el sufijo, usar el nombre limpio hace que la tabla
de símbolos del objeto sea legible: un `nm` sobre el ejecutable muestra `g` y no
`g@1`. Los locales sí llevan el sufijo, pero nunca se vuelven símbolos: viven en
offsets del marco, que no tienen nombre en el objeto.

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

Las dos funciones de acceso son simétricas:

| Función | Qué hace |
| --- | --- |
| `loadOperand` | Constante → inmediato (`movl $42, %eax`); si no, pila o global → registro |
| `storeOperand` | Registro → pila o global |

Que toda constante quepa en un inmediato es lo que mantiene el acceso en dos
funciones: no hace falta ninguna tabla de constantes en memoria.

---

## 9. Los errores internos

El módulo lleva un contador `objectCodeErrors` que se incrementa en cuatro
situaciones, todas bugs del compilador o falta de memoria, y no errores del
programa compilado:

| Dónde | Cuándo |
| --- | --- |
| `generateAssembly` | No se pudo crear el marco de una función |
| `generateAssembly` | `buildFrameLayout` devolvió `false`: no se pudieron reservar las ranuras de argumentos |
| `emitParam` | No se pudo ampliar el arreglo de argumentos pendientes |
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

Treinta y siete funciones: **cinco públicas** y **treinta y dos `static`**. Es
el archivo con más funciones del proyecto, y está ordenado por la regla de
descenso: cada función aparece antes que las que invoca, el punto de entrada
arriba y el acceso a operandos abajo. El bloque de prototipos del principio
sigue el mismo orden y sirve de índice.

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

### La emisión por opcode (pasada 2)

| Función | Opcodes | Qué emite |
| --- | --- | --- |
| `emitParamDeclaration` | `TAC_PARAM_DECL` | Copia el registro de argumento a su ranura, o lo lee del marco del llamador si los registros se agotaron |
| `emitAssignment` | `TAC_ASSIGN` | Cargar a `%eax` y guardar |
| `emitBinaryOp` | `TAC_ADD`, `TAC_SUB` | `addl` / `subl` según el mnemónico que recibe |
| `emitMultiplication` | `TAC_MUL` | Primero prueba la reducción de fuerza; si no aplica, `emitBinaryOp` con `imull` |
| `emitDivisionOp` | `TAC_DIV`, `TAC_MOD` | `cltd` + `idivl %ecx`, guardando `%eax` (cociente) o `%edx` (resto) |
| `emitUnaryMinus` | `TAC_UMINUS` | `negl` sobre `%eax` |
| `emitNot` | `TAC_NOT` | `xorl $1` |
| `emitRelationalOp` | `TAC_LT`, `TAC_GT`, `TAC_EQ` | `cmpl` + `setCC` + `movzbl` |
| `emitLabel` | `TAC_LABEL` | `.L3:` — la etiqueta sale de `result` |
| `emitGoto` | `TAC_GOTO` | `jmp .L3` — sale de `firstOperand` |
| `emitConditionalJump` | `TAC_IF_FALSE`, `TAC_IF_TRUE` | `cmpl $0` más `je`/`jne` — la etiqueta sale de `secondOperand` |
| `emitParam` | `TAC_PARAM` | Copia el argumento a su ranura pendiente |
| `emitCall` | `TAC_CALL` | Los pasos de §5 |
| `emitReturn` | `TAC_RETURN` | Deja el valor en `%eax` y salta al epílogo común |

**Las tres posiciones distintas de la etiqueta** —`result` en `emitLabel`,
`firstOperand` en `emitGoto`, `secondOperand` en los saltos condicionales— son la
asimetría del catálogo de cuádruplas ([representación TAC](../05-codigo-intermedio/01-representacion-tac.md)).
Acá se paga: hay que saber en qué campo guardó la etiqueta cada opcode.

**`emitNot` merece un párrafo.** Negar un booleano con `xorl $1` da el resultado
correcto **solo si el valor es exactamente 0 o 1**: con cualquier otro bit
prendido devolvería un número distinto de 0 y de 1, y el `if` siguiente lo leería
como verdadero. El invariante lo garantizan las comparaciones, que terminan en
`movzbl` (§6), y las constantes booleanas del TAC, que son 0 o 1. A cambio es una
instrucción en lugar de las tres que costaría `cmpl $0` + `sete` + `movzbl`.

### Los auxiliares de emisión

| Función | Qué hace |
| --- | --- |
| `emitFunction` | Prólogo, el bucle de instrucciones hasta `end_func` (fusionando comparación y salto cuando se puede), epílogo. Devuelve dónde quedó el recorrido |
| `canFuseConditionAndJump` | Si `peephole` está activa y un relacional deja un temporal que lee el salto siguiente |
| `emitFusedComparisonAndJump` | `cmpl` y un único salto condicional, sin materializar el booleano |
| `getRelationalMnemonics` | El `setCC` y los dos saltos de cada relacional; devuelve `false` si el opcode no es relacional |
| `emitStrengthReducedMultiplication` | Los tres casos de §6; devuelve `false` si no aplica |
| `powerOfTwoExponent` | `(v & (v - 1)) == 0` y después el exponente |
| `emitCallStackArguments` | Los `pushq`, en orden inverso |
| `emitCallRegisterArguments` | Las cargas a los registros, después de los `pushq` |
| `emitCallResult` | `%eax` a la ranura del resultado, si la llamada tiene resultado |

Las funciones con un **mnemónico o un registro por parámetro** son el patrón que
mantiene corto al archivo: `emitBinaryOp` sirve para `+`, `-` y `*`,
`emitDivisionOp` para `/` y `%` cambiando el registro que se lee, y
`emitRelationalOp` para los tres relacionales con el `setCC` que le da
`getRelationalMnemonics`. Agregar `!=` es un `case` en esa tabla y en el
despachador.

`powerOfTwoExponent` tiene una sutileza de contrato: devuelve **`-1`** si el
valor no es potencia de dos, pero devuelve **`0`** para el valor `1`, que sí lo
es. El llamador exige `exponent > 0`, así que trata ese `0` como "no aplica" — y
es correcto, porque el caso `× 1` ya se resolvió antes, en su propia rama.

### El acceso a operandos

| Función | Qué hace |
| --- | --- |
| `loadOperand` | Constante → inmediato; si no, resuelve y carga de la pila o de la global |
| `storeOperand` | Resuelve y guarda a la pila o a la global |
| `resolveOperandLocation` | Pila, global, o el error interno de §8 |
| `isDeclaredGlobal` | Lee `operand->symbol->isGlobal` (§7) |
| `getGlobalName` | El `id` del símbolo si lo tiene; si no, el nombre calificado (§7) |

### Las globales

| Función | Qué hace |
| --- | --- |
| `emitGlobals` | Emite `.data` con una palabra (`.long 0`) por cada `global_decl` |

El generador no guarda estado propio sobre las globales: todo lo que necesita
saber de una variable lo lleva su `Symbol`, así que no hay nada que reiniciar
entre dos llamadas a `generateAssembly`.

---

## 11. Conexión con las fases vecinas

| Viene de | Qué cruza la frontera |
| --- | --- |
| `tac.h` | La lista de cuádruplas, ya optimizada si se pidió |
| `optimizer.h` | El `OptimizationSet`, para `strength-reduce` y la fusión de `peephole` |
| `function_frame.h` | `frameSize` y un offset por operando |
| `string_builder.h` | `buildStringf`, por donde sale todo el assembly |

| Va hacia | Qué entrega |
| --- | --- |
| `driver.c` | El texto del assembly, que se escribe al `.ass` o se pasa a `gcc` |

**Lo que esta fase asume y no verifica**, porque las anteriores lo garantizan:

- Todo operando tiene su ranura o es global (si no, §8 lo reporta).
- Todo método termina en un `ret` del TAC, escrito o implícito (lo garantiza
  `generateIntermediateCode`).
- Los tipos de los operandos son correctos (lo garantiza el chequeo de tipos).
- Toda etiqueta referenciada está definida (lo garantiza el generador de TAC).

---

## 12. Cómo se extiende

### Un opcode TAC nuevo

Un `case` en `generateInstruction` que llame a una función de emisión existente
o a una nueva. **Si se omite, cae en el `default` que no hace nada**: la
instrucción desaparece del ejecutable sin un solo aviso. Es el fallo silencioso
de este módulo, el mismo patrón que en [generación de TAC](../05-codigo-intermedio/02-generacion-tac.md).

### Un operador relacional nuevo (`!=`, `<=`, `>=`)

Un `case` en `getRelationalMnemonics` con sus tres mnemónicos (por ejemplo
`setne` / `jne` / `je` para `!=`) y su opcode en la lista de relacionales del
despachador, más su opcode en el TAC y su fila en el chequeo de tipos. La fusión
con el salto siguiente funciona sola, porque lee la misma tabla. Es la extensión
más barata de todas, y está desarrollada en la [optimizador](../06-optimizador/02-optimizador.md).

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
- **Los registros no conservan valores entre instrucciones TAC** (§4). Es
  deliberado, y es la razón de que el código sea largo.
- **Las globales pierden el sufijo `@uid` al volverse símbolos** (§7), al
  contrario de lo que pasa en el TAC. Buscar `g@1` en el `.ass` no encuentra nada;
  el símbolo es `g`.
- **Todo es de 32 bits.** Los mnemónicos llevan el sufijo `l` fijo.
- **Un programa con `float` no llega hasta acá**: se rechaza al generar el
  código intermedio. Si alguna vez se extendiera el alcance, el camino es el que
  describe §5 para la segunda secuencia de registros.

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
  INTEGER, SSE o MEMORY y lo asigna a la secuencia de registros de su clase.
  Como el código generable es solo de enteros y booleanos, acá se usa únicamente
  la secuencia INTEGER; §5 explica qué cambiaría con reales.
- **Registros preservados por el llamado contra por el llamador**
  (*callee-saved* / *caller-saved*): la ABI reparte los registros en dos grupos.
  Este generador usa solo `%eax`, `%edx`, `%ecx` y los de argumentos, que son
  todos *caller-saved*, así que **no necesita preservar nada**, y por eso el
  prólogo no tiene más que el `push` de `%rbp`. Usar `%rbx` o `%r12`–`%r15`
  obligaría a guardarlos y restaurarlos.
- **Código independiente de la posición** (*PIC*): el direccionamiento
  `símbolo(%rip)` calcula direcciones relativas al contador de programa, lo que
  permite cargar el ejecutable en cualquier dirección. Es el modo por defecto de
  los enlazadores actuales.
- **Reducción de fuerza** (*strength reduction*): reemplazar una operación por
  otra equivalente más barata. Depende de la máquina, y por eso es la única
  optimización que vive en esta fase.

---

## 15. Cómo se valida

- **Suite:** `tests/test_object_code.c`, una de las más grandes del proyecto. Verifica el assembly emitido por cada instrucción TAC, partiendo siempre de TAC (instrucciones sueltas o un listado `.ci` con `buildTAC`), sin ensamblar ni ejecutar.
- **Suite de integración:** `tests/test_integration.c` compila programas completos, los ensambla con `gcc`, los ejecuta y compara el código de salida.
- **A mano:** el `.ass` es la salida directa:

```bash
./mi_compilador -target assembly examples/factorial.ctds
```

- **La validación que importa es ejecutar.** Un error de esta fase casi nunca se
  manifiesta como un error de ensamblado: se manifiesta como un **resultado
  incorrecto**, porque el assembly es sintácticamente válido y hace algo distinto
  de lo pedido. Por eso cada construcción necesita, además del test unitario, un
  caso en la suite de integración que **compile, ejecute y compare**:

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
| Llamada anidada `f(g(1), h(2))` | Las ranuras de argumentos pendientes |
| Recursión (factorial) | El marco y el epílogo común |

- **Y el chequeo de que el optimizador no cambió nada:** compilar con y sin
  `-opt all`, ejecutar los dos y comparar.

---

**Fin del recorrido de las etapas del compilador.**
