# Generación de Código Objeto

**Archivos:** `include/object_code.h`, `src/object_code/object_code.c` (668 líneas)

El generador de código objeto traduce la lista de [cuádruplas TAC](../05-codigo-intermedio/01-representacion-tac.md) a assembly x86-64 (sintaxis AT&T) apoyándose en el [marco de pila](01-marco-de-pila.md). El código se emite directamente como texto mediante `buildStringf` del [acumulador de cadenas](../02-infraestructura/04-utilidades-y-tipos.md) para ser ensamblado y enlazado por `gcc`.

!!! note "Esta etapa cubre tipos enteros y lógicos"
    El enunciado acota la generación de código objeto a tipos enteros y lógicos.
    Todo programa que utilice `float` es rechazado previamente al generar el
    código intermedio con el mensaje y línea correspondientes. Ver
    [Alcance y Extensiones](../../decisiones/alcance-y-extensiones.md#1-alcance-de-float).

---

## 1. Arquitectura general e interfaz pública

### Interfaz pública (`include/object_code.h`)

```c
void generateAssembly(const TACList *tacList, OptimizationSet enabled);
int  getObjectCodeErrors(void);

void emitPrologue(const FunctionFrame *frame);
void emitEpilogue(const FunctionFrame *frame);
void generateInstruction(const TACInstruction *instruction, FunctionFrame *frame);
```

- `generateAssembly` es el punto de entrada del módulo. Recibe el conjunto de optimizaciones `enabled` para aplicar aquellas transformaciones que dependen de la arquitectura (`strength-reduce` y la fusión de comparación y salto de `peephole`; ver [optimizador](../06-optimizador/02-optimizador.md)).
- `getObjectCodeErrors` devuelve la cantidad de errores internos registrados durante la generación (§5).
- `emitPrologue`, `emitEpilogue` y `generateInstruction` tienen visibilidad pública para permitir que las pruebas unitarias verifiquen la traducción de instrucciones individuales sobre un marco de pila preconstruido. En `emitEpilogue`, si la función es `main` y su tipo de retorno es `TYPE_VOID`, se emite `xorl %eax, %eax` antes de `leave` para garantizar que el proceso devuelva código de salida `0` al sistema operativo.

### Estructura del archivo assembly emitido

`generateAssembly` organiza la salida en tres partes:

```c
emitGlobals(tacList);               /* 1. Sección .data: variables globales */
buildStringf(".text\n");            /* 2. Sección .text: funciones */

for (cada instrucción) {
    if (es TAC_BEGIN_FUNC) {
        frame = createFrame(nombre);
        frame->returnType = ...;
        buildFrameLayout(frame, instruccion);            /* pasada 1: dimensionar */
        instruccion = emitFunction(frame, instruccion);  /* pasada 2: emitir */
        freeFrame(frame);
    }
}
buildStringf("\n.section .note.GNU-stack,\"\",@progbits\n"); /* 3. Pila no ejecutable */
```

Las variables globales se declaran en `.data` antes de `.text`. Al final del archivo se emite la directiva `.section .note.GNU-stack,"",@progbits` para indicar al enlazador (`ld`) que el binario no requiere pila ejecutable.

---

## 2. Esquema de traducción y despacho por opcode

### Estrategia cargar-operar-guardar y uso de registros

Como todos los parámetros, variables locales y temporales tienen una ranura asignada en memoria, cada instrucción TAC se traduce de forma independiente: se cargan los operandos en registros de trabajo, se ejecuta la instrucción aritmética o lógica y se almacena el resultado en su destino:

```c
/* Binaria */                          /* Unaria */              /* Relacional */
loadOperand(first,  "%eax");           loadOperand(first,"%eax") loadOperand(first,  "%eax")
loadOperand(second, "%edx");           <op> %eax                 loadOperand(second, "%edx")
<op> %edx, %eax                        store(result, "%eax")     cmpl %edx, %eax
store(result, "%eax")                                            set<cc> %al
                                                                 movzbl %al, %eax
                                                                 store(result, "%eax")
```

El generador utiliza únicamente registros volátiles (*caller-saved* en la ABI System V AMD64: `%eax`, `%edx`, `%ecx` y los registros de paso de argumentos). Ningún valor tiene que sobrevivir en un registro entre dos instrucciones TAC consecutivas, y el prólogo de las funciones no necesita preservar registros *callee-saved* (`%rbx`, `%r12`–`%r15`), limitándose a guardar `%rbp`.

### Despacho en `generateInstruction`

| Opcode TAC | Función de emisión | Acción realizada |
| :--- | :--- | :--- |
| `TAC_PARAM_DECL` | `emitParamDeclaration` | Copia el parámetro desde su registro de argumento o desde el marco del llamador a su ranura local. |
| `TAC_LOCAL_DECL` | *(ninguna)* | No emite código; el espacio fue reservado en el prólogo mediante `subq $frameSize, %rsp`. |
| `TAC_ASSIGN` | `emitAssignment` | Carga `firstOperand` en `%eax` y lo guarda en `result`. |
| `TAC_ADD`, `TAC_SUB` | `emitBinaryOp` | Emite `addl` o `subl` entre `%edx` y `%eax`. |
| `TAC_MUL` | `emitMultiplication` | Intenta `emitStrengthReducedMultiplication` (§4.3); si no aplica, invoca `emitBinaryOp` con `imull`. |
| `TAC_DIV`, `TAC_MOD` | `emitDivisionOp` | Ejecuta `cltd` + `idivl %ecx` y guarda `%eax` (cociente) o `%edx` (resto) (§4.1). |
| `TAC_UMINUS` | `emitUnaryMinus` | Carga en `%eax`, aplica `negl %eax` y guarda. |
| `TAC_NOT` | `emitNot` | Carga en `%eax`, aplica `xorl $1, %eax` y guarda (§4.2). |
| `TAC_LT`, `TAC_GT`, `TAC_EQ` | `emitRelationalOp` | Emite `cmpl`, `set<cc> %al` y `movzbl %al, %eax` (§4.2). |
| `TAC_LABEL` | `emitLabel` | Emite `.L<nombre>:` tomando la etiqueta desde `result`. |
| `TAC_GOTO` | `emitGoto` | Emite `jmp .L<nombre>` tomando la etiqueta desde `firstOperand`. |
| `TAC_IF_FALSE`, `TAC_IF_TRUE` | `emitConditionalJump` | Compara `firstOperand` con `$0` y emite `je` o `jne` hacia `secondOperand` (§4.2). |
| `TAC_PARAM` | `emitParam` | Copia el argumento a su ranura anónima de argumento pendiente. |
| `TAC_CALL` | `emitCall` | Coloca argumentos en pila y registros, invoca `call` y almacena `%eax` (§3). |
| `TAC_RETURN` | `emitReturn` | Carga el valor de retorno (si existe) en `%eax` y emite `jmp .L_<fn>_epilogue`. |

La posición del operando etiqueta varía según el formato de cada cuádrupla definido en la [representación TAC](../05-codigo-intermedio/01-representacion-tac.md): `result` en `TAC_LABEL`, `firstOperand` en `TAC_GOTO` y `secondOperand` en `TAC_IF_FALSE` / `TAC_IF_TRUE`.

---

## 3. Convención de llamada (System V AMD64 ABI)

### Paso de argumentos enteros

La ABI System V AMD64 asigna los primeros seis argumentos de clase entera o booleana a registros de 32 bits y transfiere del séptimo en adelante a través de la pila:

| Posición del argumento | Destino |
| :---: | :--- |
| 1 | `%edi` |
| 2 | `%esi` |
| 3 | `%edx` |
| 4 | `%ecx` |
| 5 | `%r8d` |
| 6 | `%r9d` |
| $k \ge 7$ | Pila: `+16 + 8 * (k - 7)(%rbp)` en el marco del llamado |

*(La ABI define además la secuencia independiente `%xmm0`–`%xmm7` para argumentos de punto flotante, que no se utiliza dado que esta etapa opera únicamente sobre tipos enteros y lógicos).*

### Recepción en la función llamada (`emitParamDeclaration`)

Al inicio de la función, cada `TAC_PARAM_DECL` vuelca el argumento entrante a su propia ranura de 4 bytes en el marco local:

- Si `frame->integerParamCount < 6`, copia directamente el registro correspondiente a la ranura (`movl %edi, -4(%rbp)`) e incrementa `integerParamCount`.
- Si `frame->integerParamCount >= 6`, el argumento fue apilado por el llamador antes de `call`. Como cada `pushq` en x86-64 ocupa 8 bytes, se lee desde `16 + frame->stackParamCount * 8(%rbp)` pasando por `%eax` y se incrementa `stackParamCount`.

Copiar los parámetros formales a la pila al ingresar al método permite tratarlos de manera uniforme junto con las variables locales y evita que una llamada interna sobrescriba sus valores.

### Invocación desde el llamador (`emitCall`)

Cuando se procesa `TAC_CALL` con `n` argumentos, las `n` instrucciones `TAC_PARAM` previas ya guardaron los valores de los argumentos en las ranuras anónimas `pendingParamSlots[base .. pendingParamCount - 1]` del marco actual (ver [marco de pila](01-marco-de-pila.md)). `emitCall` ejecuta la llamada en ocho pasos:

```c
1. base = frame->pendingParamCount - argumentCount;
2. if (stackArguments % 2 != 0) subq $8, %rsp;   /* relleno de alineación a 16 bytes */
3. emitCallStackArguments(...)                   /* argumentos >= 7: pushq en orden inverso */
4. emitCallRegisterArguments(...)                /* argumentos 1..6: ranura -> registro */
5. buildStringf("    call %s\n", functionName);
6. if (stackBytes > 0) addq $stackBytes, %rsp;   /* limpieza de argumentos y relleno */
7. frame->pendingParamCount = base;              /* restaura tope de argumentos pendientes */
8. emitCallResult(...)                           /* movl %eax a la ranura de result */
```

- **Orden entre pila y registros:** primero se apilan los argumentos 7 en adelante usando `%eax` como registro intermedio (`movl offset(%rbp), %eax` seguido de `pushq %rax`), y recién después se cargan los primeros 6 argumentos en `%edi`..`%r9d` para que ninguna operación previa los altere.
- **Orden inverso en la pila:** los argumentos en pila se recorren desde `argumentCount - 1` hasta `6`, de modo que el séptimo argumento quede en el tope de la pila (`+16(%rbp)` para la función llamada).
- **Alineación en llamadas con más de 6 argumentos:** cada `pushq` desplaza `%rsp` en 8 bytes. Si la cantidad de argumentos apilados es impar, se restan 8 bytes adicionales antes de apilar para mantener `%rsp` alineado a 16 bytes en el punto del `call`, y se suman al regresar junto con el espacio de los argumentos.
- **Restauración de `pendingParamCount`:** al asignar `frame->pendingParamCount = base` en lugar de `0`, se preservan intactos los argumentos pendientes de llamadas externas cuando se evalúan llamadas anidadas como `f(g(1), h(2))`.

---

## 4. Traducción de operaciones específicas

### 4.1 División y módulo con signo (`emitDivisionOp`)

La instrucción `idivl` de x86 divide el dividendo con signo de 64 bits almacenado en el par `%edx:%eax` por el operando divisor, dejando el cociente en `%eax` y el resto en `%edx`. Por ello, `TAC_DIV` y `TAC_MOD` comparten `emitDivisionOp`, diferenciándose únicamente en el registro que almacenan al finalizar:

```asm
movl  <dividendo>, %eax
movl  <divisor>,   %ecx
cltd                      # extiende el bit de signo de %eax a todo %edx:%eax
idivl %ecx                # cociente en %eax, resto en %edx
```

- **Extensión de signo (`cltd`):** inicializa `%edx` con copias del bit de signo de `%eax`. Omitirla dejaría valores residuales en `%edx`, produciendo resultados erróneos o excepciones de desborde.
- **Divisor en `%ecx`:** dado que `cltd` sobrescribe `%edx`, el divisor no puede cargarse en `%edx` como en el resto de las operaciones binarias; se carga en `%ecx`.

### 4.2 Operaciones relacionales, negación lógica y saltos

#### Comparaciones (`emitRelationalOp`)

En sintaxis AT&T, `cmpl %edx, %eax` calcula `%eax - %edx`. Las instrucciones `setl`, `setg` y `sete` escriben `1` o `0` únicamente en el byte bajo (`%al`), sin modificar los 24 bits superiores de `%eax`. Por eso se encadena `movzbl %al, %eax`:

```asm
cmpl   %edx, %eax      # compara primer operando (%eax) con segundo (%edx)
setl   %al             # %al = (%eax < %edx) ? 1 : 0
movzbl %al, %eax       # extiende con ceros a 32 bits (0 o 1 en %eax)
```

#### Negación booleana (`emitNot`)

`emitNot` invierte el valor lógico mediante `xorl $1, %eax`. Esta traducción en una sola instrucción es válida porque tanto las constantes booleanas como los resultados de `emitRelationalOp` (gracias a `movzbl`) garantizan el invariante de que todo booleano en memoria vale exactamente `0` o `1`.

#### Saltos condicionales y fusión con comparación

En el caso general, `emitConditionalJump` carga la condición en `%eax`, la compara contra cero (`cmpl $0, %eax`) y emite `je .L<id>` para `TAC_IF_FALSE` o `jne .L<id>` para `TAC_IF_TRUE`.

Cuando la optimización `-opt peephole` está activa y una instrucción relacional es seguida inmediatamente por un salto condicional que evalúa ese temporal, `canFuseConditionAndJump` y `emitFusedComparisonAndJump` unifican ambas cuádruplas en un `cmpl` seguido del salto condicional directo (`jge`, `jl`, `jne`, `je`), evitando materializar el booleano en `%al` y guardarlo en el marco (ver [ADR 003](../../decisiones/adr-003-fusion-comparacion-y-salto-condicional.md)).

### 4.3 Reducción de fuerza en multiplicaciones (`emitStrengthReducedMultiplication`)

Cuando `-opt strength-reduce` está activa y alguno de los operandos de `TAC_MUL` es una constante entera, se evita emitir `imull`:

| Multiplicador constante | Código emitido |
| :--- | :--- |
| `0` | `xorl %eax, %eax` |
| `1` | Carga directa del otro operando en `%eax`. |
| Potencia de dos $2^n$ ($n > 0$) | `sall $n, %eax` (calculado por `powerOfTwoExponent` mediante `(v & (v - 1)) == 0`). |
| Otro valor | Se delega en `emitBinaryOp` con `imull`. |

---

## 5. Variables globales, resolución de operandos y errores internos

### Variables globales en `.data` y direccionamiento `%rip`

Cada instrucción `TAC_GLOBAL_DECL` genera una entrada de 4 bytes en la sección `.data`, inicializada con su valor constante o en `0`:

```asm
.data
.globl g
.align 4
g:
    .long 0
```

A diferencia de las variables locales —que se convierten en desplazamientos numéricos respecto de `%rbp`—, las variables globales se emiten como símbolos en el archivo objeto y se acceden mediante direccionamiento relativo al contador de programa (`g(%rip)`), compatible con código independiente de la posición (*PIC*).

Para determinar si una variable es global sin realizar búsquedas en listas, `isDeclaredGlobal` consulta en $O(1)$ el campo `operand->symbol->isGlobal` (ver [ADR 002](../../decisiones/adr-002-resolucion-variables-globales-isglobal.md)). Asimismo, `getGlobalName` utiliza el identificador original `operand->symbol->id` sin el sufijo `@uid`, dado que el ámbito global es único y no puede existir ambigüedad de nombres en la tabla de símbolos del objeto.

### Resolución de operandos (`resolveOperandLocation`)

Todas las lecturas (`loadOperand`) y escrituras (`storeOperand`) delegan en `resolveOperandLocation`:

1. Si `getSlotOffset(frame, operand->name, offset)` lo encuentra en el marco actual, retorna `LOCATION_STACK` (`offset(%rbp)`).
2. Si es un `OPERAND_VARIABLE` con `isDeclaredGlobal(operand)`, retorna `LOCATION_GLOBAL` (`nombre(%rip)`).
3. En cualquier otro caso, retorna `LOCATION_UNRESOLVED`, incrementa `objectCodeErrors` e informa el error por `stderr`.

### Manejo de errores internos (`getObjectCodeErrors`)

El contador `objectCodeErrors` registra fallos internos del compilador o agotamiento de memoria en cuatro puntos:

- Fallo al reservar el `FunctionFrame` en `generateAssembly`.
- Fallo de reserva de ranuras durante `buildFrameLayout`.
- Fallo al redimensionar `pendingParamSlots` en `emitParam`.
- Operando sin ubicación en el marco ni marca de variable global en `resolveOperandLocation`.

Ante los dos primeros fallos se interrumpe la generación de la función actual. El driver consulta `getObjectCodeErrors()` al finalizar `generateAssembly` y aborta el pipeline si es mayor a cero, impidiendo invocar a `gcc` sobre assembly incompleto o con desplazamientos inválidos.

---

## 6. Cómo se extiende

- **Incorporar un nuevo opcode TAC:** agregar el `case` correspondiente en el `switch` de `generateInstruction` y, si define un temporal, asegurarse de que `allocateLocalSlots` lo contemple. Debe tenerse en cuenta que el bloque `default` de `generateInstruction` no emite código ni advertencias, por lo que omitir un opcode nuevo haría que la instrucción se ignore en silencio (comportamiento análogo al de [generación de TAC](../05-codigo-intermedio/02-generacion-tac.md)).
- **Incorporar operadores relacionales (`!=`, `<=`, `>=`):** agregar el opcode en la lista de casos relacionales de `generateInstruction` y su fila de tres mnemónicos (`set<cc>`, salto por verdadero y salto por falso) en `getRelationalMnemonics`. Tanto la emisión estándar como la fusión con el salto condicional de `peephole` consultan esa misma función.
- **Retención de temporales en registros:** para evitar guardar y recargar inmediatamente un temporal entre dos instrucciones contiguas (`t0 = a + b; t1 = t0 * c`), podría agregarse en el generador un descriptor del último temporal cargado en `%eax`, omitiendo el `movl` a la pila cuando el temporal muere en la instrucción siguiente.

---

## 7. Validación

- **Suite unitaria:** `tests/test_object_code.c` verifica el texto assembly generado para cada instrucción TAC de manera aislada y para funciones completas construidas con `buildTAC`.
- **Suite de integración:** `tests/test_integration.c` compila programas C-TDS completos hasta código ejecutable mediante `gcc`, los corre y compara su código de retorno contra el resultado esperado.
- **Casos de prueba críticos cubiertos:**

| Escenario | Qué valida en el generador |
| :--- | :--- |
| División y módulo con operandos negativos (`-17 / 5`, `-17 % 5`) | Extensión de signo con `cltd`, divisor en `%ecx` y truncamiento hacia cero. |
| Llamadas con 7 y 8 argumentos | Paso de argumentos por pila en orden inverso y relleno de alineación de 8 bytes con cantidad impar. |
| Llamadas anidadas (`f(g(1), h(2))`) | Preservación de argumentos en ranuras anónimas pendientes (`pendingParamSlots`). |
| Funciones recursivas (`factorial`, `fib`) | Independencia de cada registro de activación y convergencia en el epílogo común. |
| Compilación doble con y sin `-opt all` | Equivalencia semántica de `strength-reduce` y de la fusión de comparación y salto. |

- **Inspección y ejecución manual:**

```bash
# Generar e inspeccionar el archivo .ass
./mi_compilador -target assembly examples/factorial.ctds

# Compilar a binario, ejecutar y consultar el código de salida
./mi_compilador -o /tmp/prog examples/dead_code.ctds && /tmp/prog; echo $?
```

---

**Fin del recorrido de las etapas del compilador.**
