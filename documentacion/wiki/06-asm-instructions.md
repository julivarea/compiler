# Instrucciones x86-64

**Ficha F16** · Archivos: `include/asm_instruction.h`, `src/asm_instruction.c` (391 líneas)

El modelo de datos de una instrucción de assembly y su emisión textual en
sintaxis AT&T.

---

## 1. Qué problema resuelve

Dar una representación con tipos a las instrucciones de máquina, para que el
[generador de código objeto](06-object-code.md) construya una instrucción
declarando *qué* quiere y no armando una cadena de texto a mano.

```c
/* con el modulo */                      /* sin el modulo */
emitAsm(&asmOpReg(ASM_ADD, ASM_RAX, ASM_RDX));   buildStringf("    addl %%edx, %%eax\n");
```

La diferencia no es cosmética: el modelo centraliza **la sintaxis AT&T en un solo
lugar** y hace imposible equivocarse en el orden de los operandos, que es el
error más frecuente al escribir assembly de GAS a mano.

---

## 2. Hasta dónde llega el módulo

Conviene decirlo de entrada, porque es la característica más importante de esta
ficha: **el módulo cubre solo el núcleo de aritmética entera de 32 bits.** Todo
lo demás se emite como texto directo desde `object_code.c`.

Medido sobre `src/object_code.c`:

| Vía | Llamadas |
| --- | --- |
| `emitAsm` (por el módulo) | **14** |
| `buildStringf` (texto directo) | **56** |

O sea que alrededor de un 20% del assembly pasa por el modelo tipado. Lo que
queda afuera:

| Qué | Mnemónicos emitidos como texto |
| --- | --- |
| Control de flujo | `jmp`, `je`, `jne`, `call`, `ret` |
| Marco de pila | `pushq`, `subq`, `addq`, `movq`, `leave` |
| Resultado de comparaciones | `movzbl`, `setnp`, `andb`, los `setCC` |
| Desplazamientos | `sall` (reducción de fuerza) |
| Directivas | `.data`, `.text`, `.globl`, `.type`, `.align`, `.section` |

**Es una limitación real, no una decisión elegante.** El módulo se diseñó para la
primera entrega, y cuando crecieron las
llamadas y el marco, esas partes se escribieron directo. La consecuencia práctica
es que **la sintaxis AT&T está centralizada solo para una parte del back-end**:
en el resto, el orden de los operandos depende de que cada `buildStringf` esté
bien escrito.

Si hubiera que elegir una sola mejora estructural del back-end, es extender este
módulo para cubrir el resto y que todo el assembly salga por `emitAsm`.

---

## 3. El modelo de datos

### `AsmInstruction`: opcode y dos operandos

```c
typedef struct AsmInstruction {
    AsmOpcode opcode;
    AsmOperand dest;   /* destino */
    AsmOperand src;    /* origen */
} AsmInstruction;
```

Tres campos, sin puntero `next`: a diferencia del TAC, **acá no hay lista**. El
generador construye una instrucción, la emite y la descarta. No existe una
representación intermedia de assembly sobre la que se pueda optimizar, y es la
razón por la que los tres patrones de mirilla sobre el código emitido siguen
pendientes: habría que conservar las instrucciones para poder mirar dos
contiguas.

### `AsmOpcode`: once operaciones

| Opcode | Mnemónico | Qué hace |
| --- | --- | --- |
| `ASM_MOV` | `movl` | Transferencia de datos |
| `ASM_ADD` | `addl` | Suma entera |
| `ASM_SUB` | `subl` | Resta entera |
| `ASM_IMUL` | `imull` | Multiplicación con signo |
| `ASM_IDIV` | `idivl` | División con signo |
| `ASM_NEG` | `negl` | Negación a complemento a dos |
| `ASM_AND` | `andl` | Conjunción de bits |
| `ASM_OR` | `orl` | Disyunción de bits |
| `ASM_XOR` | `xorl` | Disyunción exclusiva; se usa para negar un booleano |
| `ASM_CMP` | `cmpl` | Comparación: solo modifica flags |
| `ASM_CLTD` | `cltd` | Extiende el signo de `%eax` a `%edx:%eax` |

Son exactamente las que hacen falta para la ALU entera. El sufijo `l` de todos
los mnemónicos indica operandos de **32 bits** (*long*), que es el tamaño de un
`int` de C-TDS.

**`ASM_CLTD` es la única sin operandos**, y existe por una obligación de `idivl`:
la división con signo toma el dividendo de `%edx:%eax`, o sea de 64 bits
repartidos en dos registros, así que antes hay que propagar el bit de signo de
`%eax` a `%edx`. Sin el `cltd`, dividir un número negativo da basura. Es el
detalle de x86 que más se pregunta sobre división.

### `AsmRegister`: solo tres

```c
typedef enum AsmRegister {
    ASM_RAX,  /* %eax: acumulador, destino principal */
    ASM_RDX,  /* %edx: segundo operando, y parte alta en idivl */
    ASM_RCX   /* %ecx: divisor en idivl */
} AsmRegister;
```

Tres registros para un procesador que tiene dieciséis. No es un descuido: son los
únicos que el generador usa para evaluar una operación, porque **cada temporal
vive en la pila** y los registros solo sirven de paso (ver la ficha F17). Un
operando se carga a `%eax`, el otro a `%edx` o `%ecx`, se opera, y el resultado
vuelve a la pila.

Los registros que la convención de llamada exige para los argumentos —`%edi`,
`%esi`, `%r8d`, `%r9d`— no están en el enum, y para ellos existe la válvula de
escape de §4.

### `AsmOperandType`: cinco formas de nombrar un valor

| Tipo | Se emite como | Para qué |
| --- | --- | --- |
| `ASM_OP_NONE` | nada | Instrucciones unarias o sin operandos |
| `ASM_OP_REG` | `%eax` | Un registro |
| `ASM_OP_MEM` | `-4(%rbp)` | Una ranura del marco de pila |
| `ASM_OP_GLOBAL` | `g@1(%rip)` | Una variable global, por direccionamiento relativo al IP |
| `ASM_OP_IMM` | `$42` | Una constante inmediata |

`ASM_OP_MEM` y `ASM_OP_GLOBAL` son los dos modos de acceso a memoria del
compilador, y la distinción corresponde exactamente a la que el TAC ya hacía con
`TAC_LOCAL_DECL` y `TAC_GLOBAL_DECL`: lo local va al marco, lo global al segmento
de datos.

El direccionamiento `nombre(%rip)` para las globales no es la única forma posible
—`nombre` suelto también funciona— pero es la que produce **código independiente
de la posición**, que es lo que los enlazadores modernos esperan por defecto.

### `AsmOperand`: unión más un campo aparte

```c
typedef struct AsmOperand {
    AsmOperandType type;
    union {
        AsmRegister reg;
        int offset;
        int immediate;
        const char *globalName;
    };
    char customReg[16];
} AsmOperand;
```

La unión anónima es el valor según el `type`: cuatro miembros para cuatro de los
cinco tipos (`ASM_OP_NONE` no lleva valor).

**`customReg` está fuera de la unión, y eso es intencional.** Ver §4.

---

## 4. La válvula de escape: `customReg`

El enum tiene tres registros, pero el generador necesita emitir `%edi`, `%esi`,
`%r8d`… Para eso existe un segundo constructor de operandos de registro:

```c
opReg(ASM_RAX)      /* por enum   -> %eax */
opRegStr("%edi")    /* por nombre -> %edi */
```

`opRegStr` copia el nombre en `customReg`, y `emitOperand` prefiere ese campo si
no está vacío:

```c
case ASM_OP_REG:
    buildStringf("%s", op->customReg[0] != '\0' ? op->customReg : getRegisterName(op->reg));
```

Por eso `customReg` **no puede estar en la unión**: un operando de tipo
`ASM_OP_REG` necesita poder llevar o el enum o la cadena, y el desempate es
comprobar si la cadena está vacía.

Y es la válvula la que más se usa: en `object_code.c` hay **6 llamadas a
`opRegStr` contra 2 a `opReg`**. O sea que en la práctica el camino por nombre es
el principal, y el enum de tres registros queda para la aritmética.

La lectura honesta es que el enum quedó chico y la cadena lo compensa. Lo
correcto sería ampliar `AsmRegister` con los dieciséis registros y sus variantes
de tamaño, y eliminar `customReg`; mientras conviva, un registro se puede
nombrar de dos maneras y nada garantiza que `opRegStr("%eax")` y
`opReg(ASM_RAX)` se traten como el mismo.

---

## 5. Los constructores

Siete constructores de instrucción y seis de operando. Todos devuelven **por
valor**: no hay reservas de memoria en este módulo, así que no hay nada que
liberar.

### De operando

| Constructor | Produce |
| --- | --- |
| `opReg(r)` | Registro por enum |
| `opRegStr(name)` | Registro por nombre, copiando la cadena |
| `opMem(offset)` | `offset(%rbp)` |
| `opImm(val)` | `$val` |
| `opGlobal(name)` | `name(%rip)`, **guardando el puntero sin copiar** |
| `opNone()` | Operando ausente |

Los seis son `static inline` en el header y hacen `memset` a cero antes de llenar
el campo que corresponde, igual que los constructores de operandos del TAC.

**`opGlobal` guarda el puntero, no una copia.** Es la única asimetría con
`opRegStr`, que sí copia en `customReg`. La cadena tiene que seguir viva hasta que
la instrucción se emita; en la práctica siempre es el `name` de un `TACOperand`,
que vive mientras exista la lista TAC, así que es seguro. Pero es una dependencia
de duración no declarada.

### De instrucción

| Constructor | Genera |
| --- | --- |
| `asmLoad(dest, src)` | `movl src, dest` |
| `asmLoadImm(reg, val)` | `movl $val, reg` |
| `asmStore(dest, src)` | `movl src, dest` |
| `asmOpReg(op, dest, src)` | `<op> src, dest` entre registros |
| `asmOpImm(op, dest, imm)` | `<op> $imm, dest` |
| `asmOpUnary(op, dest)` | `<op> dest` |
| `asmCltd()` | `cltd` |

**`asmLoad` y `asmStore` son la misma función.** Las dos devuelven
`{ASM_MOV, dest, src}`, carácter por carácter:

```c
AsmInstruction asmLoad(AsmOperand dest, AsmOperand src)  { AsmInstruction i = { ASM_MOV, dest, src }; return i; }
AsmInstruction asmStore(AsmOperand dest, AsmOperand src) { AsmInstruction i = { ASM_MOV, dest, src }; return i; }
```

No es un error: x86 tiene **una sola instrucción** `mov` para leer y escribir
memoria, a diferencia de las arquitecturas RISC, que tienen `load` y `store`
separadas. Los dos nombres existen para que el generador exprese la intención, y
que el lector de `object_code.c` sepa si una línea está trayendo un valor o
guardándolo. Es documentación en forma de API.

---

## 6. La emisión: donde vive la sintaxis AT&T

`emitAsm` es la única función del módulo que produce texto, y tiene tres casos:

```c
if (inst->opcode == ASM_CLTD) { buildStringf("    cltd\n"); return; }       /* sin operandos */

if (inst->src.type == ASM_OP_NONE) {                                        /* unaria */
    buildStringf("    %s ", opStr);  emitOperand(&inst->dest);  buildStringf("\n");
    return;
}

buildStringf("    %s ", opStr);      /* binaria: AT&T invierte el orden */
emitOperand(&inst->src);
buildStringf(", ");
emitOperand(&inst->dest);
buildStringf("\n");
```

**El orden `src, dest` es la sintaxis AT&T**, al revés de la sintaxis Intel, que
escribe `dest, src`. O sea que `addl %edx, %eax` significa `%eax += %edx`: el
**segundo** operando es el que se modifica. Es la fuente de confusión número uno
al leer assembly de GAS, y en este módulo está decidido en un solo lugar.

Las tres cosas que `emitAsm` garantiza y que un `buildStringf` a mano no:

- La indentación de cuatro espacios, que es lo que separa una instrucción de una
  etiqueta.
- El `\n` final.
- El orden de los operandos.

### Las dos traducciones

`getRegisterName` y `getOpcodeMnemonic` son los dos `switch` que convierten enum
a texto. Los dos tienen un `default` **silencioso**:

| Función | `default` devuelve |
| --- | --- |
| `getRegisterName` | `"%eax"` |
| `getOpcodeMnemonic` | `"nop"` |

Son fallas silenciosas: un opcode nuevo sin su caso emite un `nop`, y el programa
compila y hace algo distinto de lo pedido sin un solo aviso. Comparado con el
`getTACOperation` de la ficha F13, que ante un operador desconocido imprime un
error interno, acá falta ese aviso. Es la mejora más chica y más valiosa del
módulo.

---

## 7. Conexión con las fases vecinas

| Viene de | Qué cruza la frontera |
| --- | --- |
| `function_frame.c` (F17) | Los offsets que se envuelven en `opMem` |
| `tac_operand.h` (F12) | Los nombres de las globales que se pasan a `opGlobal` |

| Va hacia | Qué entrega |
| --- | --- |
| `string_builder.c` (F04) | El texto, vía `buildStringf` |
| `object_code.c` (F18) | Los constructores y `emitAsm` |

El módulo **no conoce el TAC**: no incluye `tac_*.h` ni sabe qué es una
cuádrupla. Recibe registros, offsets y constantes. Esa independencia es lo que lo
haría reutilizable para otro front-end, y es la razón de que valga la pena
mantenerlo separado de `object_code.c` aunque hoy sea su único usuario.

---

## 8. Cómo se extiende

### Un opcode nuevo

Dos lugares: el valor en `AsmOpcode` y su mnemónico en `getOpcodeMnemonic`.
Olvidar el segundo emite `nop` en silencio (§6).

Si la instrucción tiene una forma textual distinta de `op src, dest` —por
ejemplo un desplazamiento `sall $3, %eax`, que sí encaja, contra un `movzbl
%al, %eax`, que también— hay que verificar que el patrón de `emitAsm` alcance. Hoy
solo soporta tres formas: sin operandos, unaria y binaria.

### Los registros que faltan

Agregar los valores a `AsmRegister` y sus nombres a `getRegisterName`, y después
reemplazar las llamadas a `opRegStr` por `opReg`. Es lo que permitiría eliminar
`customReg` y que un registro tenga una sola representación.

### Habilitar la mirilla sobre el assembly

Requiere el cambio estructural de §3: acumular las `AsmInstruction` en una lista
en vez de emitirlas de inmediato. Con la lista, los tres patrones pendientes de la
[ficha F15](05.9-optimizer.md) —`movl X, %eax` seguido de `movl %eax, X`, y
`movl $0, %eax` por `xorl %eax, %eax`— se vuelven un recorrido de dos
instrucciones contiguas, exactamente como `removeRedundantJumps` sobre el TAC.

---

## 9. Trampas y límites conocidos

- **El módulo cubre solo la aritmética entera** (§2). Saltos, llamadas,
  marco y directivas se emiten como texto desde `object_code.c`.
- **Los dos `default` silenciosos** de `getRegisterName` y `getOpcodeMnemonic`
  (§6).
- **`asmLoad` y `asmStore` son idénticas** (§5). Buscar una diferencia de
  comportamiento es perder el tiempo: la diferencia es de intención.
- **`opGlobal` no copia el nombre** (§5): guarda el puntero.
- **`customReg` tiene 16 bytes** y `strncpy` trunca sin avisar. Alcanza para
  cualquier registro de x86-64, pero no hay chequeo como el que sí tienen los
  operandos del TAC.
- **Un registro se puede nombrar de dos formas** (§4), y nada garantiza que se
  traten como equivalentes.
- **No hay lista de instrucciones** (§3), así que no se puede optimizar el
  assembly emitido.
- **El sufijo `l` está fijo en los mnemónicos.** Todas las operaciones son de 32
  bits. Soportar otro tamaño pediría o un campo de tamaño en la instrucción o
  opcodes separados.

---

## 10. La teoría detrás

- **Generación de código objeto**: la fase final, que traduce la representación
  intermedia a instrucciones de una máquina concreta. Es la primera que depende de
  la arquitectura: todo lo anterior valdría igual para ARM.
- **Sintaxis AT&T contra Intel**: las dos notaciones del assembly x86. AT&T
  —la de GNU Assembler— escribe `op src, dest`, prefija los registros con `%` y
  los inmediatos con `$`, y codifica el tamaño del operando en el sufijo del
  mnemónico (`movl`, `movq`). Intel escribe `op dest, src` sin prefijos. Elegir
  una y documentarla es necesario porque el mismo texto significa cosas opuestas
  en cada una.
- **Máquina de registros contra máquina de pila**: x86-64 es de registros, y por
  eso la traducción del TAC exige decidir dónde vive cada valor. Acá la decisión
  es trivial —todo en la pila, los registros solo de paso— que es lo correcto y no
  lo eficiente. La alternativa es la **asignación de registros**, que se modela
  como un coloreo del grafo de interferencia construido a partir del
  [análisis de variables vivas](05.7-live-variables.md): dos variables que están
  vivas al mismo tiempo interfieren y no pueden compartir registro.
- **Selección de instrucciones** (*instruction selection*): elegir qué
  instrucciones de máquina implementan cada operación de la IR. Acá es uno a uno
  y directo; en un compilador de producción es un problema de búsqueda sobre
  patrones de árbol, porque una sola instrucción de x86 puede implementar varias
  operaciones de la IR a la vez.
- **Modelo tipado contra emisión textual**: la decisión de diseño de esta ficha.
  Un modelo con tipos detecta errores en compilación y centraliza la sintaxis;
  emitir texto es más rápido de escribir y más fácil de leer para un caso
  puntual. El proyecto terminó con los dos, y la medición de §2 muestra en qué
  proporción.
- **Código independiente de la posición** (*PIC*): el direccionamiento
  `nombre(%rip)` calcula la dirección relativa al contador de programa en lugar de
  usar una absoluta, lo que permite cargar el ejecutable en cualquier dirección.
  Es el modo por defecto que esperan los enlazadores actuales.

---

## 11. Cómo se valida

- **Suite:** `tests/test_object_code.c` (1401 líneas) verifica el assembly emitido
  por cada instrucción TAC, lo que ejercita este módulo indirectamente.
- **A mano:** el `.ass` es el texto que produce `emitAsm` junto con el resto:

```bash
./mi_compilador -target assembly examples/factorial.ctds
```

- **La validación de fondo** es que el assembly **ensamble y corra**. Un error en
  el orden de los operandos o en un mnemónico casi siempre lo detecta el propio
  `gcc` al ensamblar, y lo que no, aparece como un resultado incorrecto:

```bash
./mi_compilador -o /tmp/f examples/dead_code.ctds && /tmp/f; echo $?
```

---

**Siguiente:** [Marco de Pila](06.5-stack-frame.md)
