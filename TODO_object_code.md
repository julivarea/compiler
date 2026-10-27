# Checklist y Plan de Trabajo: Generador de Código Objeto x86-64 (`object_code`)

Este documento detalla el estado actual del generador de código objeto (`src/object_code.h` y `src/object_code.c`), audita el código generado preliminarmente por IA frente a las estructuras definidas manualmente (`FunctionFrame`, `StackSlot`, `callParams`), y define el plan de trabajo pendiente (TODO) en cumplimiento con la especificación oficial del proyecto (`documentacion/spec/00-TDS-proyecto.pdf`) y del lenguaje (`documentacion/spec/01-TDS-spec-lenguaje.pdf`).

---

## 0. Resumen Ejecutivo y Estado Actual

- **Objetivo de la etapa (Spec TDS 2026):** Generar código assembly x86-64 (GNU Assembler / GAS) a partir del Código de Tres Direcciones (TAC).
  - *Entrega Enteros (20% de la nota final):* Soporte completo de tipos enteros y booleanos, control de flujo (`if`/`while`), variables locales y globales, y llamadas a funciones con pasaje de parámetros.
  - *Criterio de evaluación:* 30% diseño/documentación y 70% casos de prueba ejecutables (50% públicos, 50% privados). Se prioriza la **corrección** sobre la optimización.
- **Estado de partida y avances recientes:**
  - ✅ **Modelo `AsmInstruction` operativo (`src/asm_instruction.h` y `src/asm_instruction.c`):** Abstracción compacta de instrucciones Assembly con registros (`%eax`, `%edx`, `%ecx`), inmediatos y stack offsets.
  - ✅ **Abstracción de operaciones binarias y relacionales (`emitBinaryOp`, `emitRelationalOp`):** Se unificó el ciclo *Cargar -> Operar -> Guardar*, resolviendo `TAC_ADD`, `TAC_SUB`, `TAC_MUL`, `TAC_AND`, `TAC_OR`, `TAC_LT`, `TAC_GT`, `TAC_EQ`.
  - ✅ **Negación booleana y aritmética:** `TAC_NOT` implementado vía `xorl $1, %eax` (los bools son estrictamente `{0, 1}`) y `TAC_NEG` vía `negl %eax`.
  - ⚠️ **Próximo foco crítico:** Pasaje de parámetros en llamadas a métodos (`TAC_PARAM`/`TAC_CALL` con buffer/pila para llamadas anidadas), control de flujo (`if`/`while`/`goto`) y conexión con la CLI (`-target assembly`).

---

## 1. Auditoría Crítica: Errores, Riesgos y Discrepancias Detectadas

### 1.1 Instrucciones Aritméticas, Lógicas y Asignación no Implementadas (Stubs vacíos)
- **Ubicación:** `src/object_code.c` (`generateInstruction`).
- **Problema:** Los casos `TAC_ASSIGN`, `TAC_ADD`, `TAC_SUB`, `TAC_MUL`, `TAC_DIV`, `TAC_MOD`, `TAC_NEG`, `TAC_NOT`, `TAC_LT`, `TAC_GT`, `TAC_EQ`, `TAC_AND` contienen únicamente un `break;`. Solo `TAC_OR` tenía código provisional.
- **Acción requerida:** Implementar la secuencia de instrucciones assembly GAS para cada operador.

### 1.2 Bug Crítico en Pasaje de Argumentos (`callArgIndex` global y llamadas anidadas)
- **Ubicación:** `src/object_code.c` (`TAC_PARAM` y `TAC_CALL`).
- **Problema:** Se utiliza una variable global `static int callArgIndex = 0`. Cuando un argumento es una expresión que contiene otra llamada a función (por ejemplo `foo(bar(1), 2)` o `factorial(a - 1)` dentro de una expresión compuesta), la llamada interna emite sus propios `TAC_PARAM` y `TAC_CALL`, reseteando `callArgIndex = 0` y pisando los registros `%edi`, `%esi`, etc. de la función externa.
- **Acción requerida:** Implementar una estructura de pila de parámetros (`CallParamBuffer` o `ParamStack`) que acumule los operandos de los `param` y los cargue a los registros correspondientes inmediatamente antes del `call`, utilizando la cantidad de argumentos indicada en `instruction->arg2`.

### 1.3 Límite estricto de 6 parámetros y descarte silencioso de argumentos 7+
- **Ubicación:** `src/object_code.c` (`TAC_PARAM` y `TAC_PARAM_DECL`).
- **Problema:** `if (callArgIndex < MAX_ARG_REGISTERS)` y `if (frame->paramCount < MAX_ARG_REGISTERS)` descartan silenciosamente cualquier parámetro más allá del 6to.
- **Acción requerida:**
  - En la llamada: Empujar a la pila (`pushq` o almacenamiento en el área de argumentos) en orden inverso los parámetros 7 en adelante, asegurando la alineación a 16 bytes antes del `call`.
  - En la definición: Los parámetros 7+ no se leen de registros, sino del frame del caller en desplazamientos positivos (`16(%rbp)`, `24(%rbp)`, etc.).

### 1.4 División (`TAC_DIV`) y Módulo (`TAC_MOD`) con `idivl`
- **Ubicación:** `src/object_code.c`.
- **Problema:** En x86-64, `idivl` divide el par `%edx:%eax` por el divisor. Requiere extender el signo de `%eax` a `%edx` mediante `cltd` (o `cdq`). Si el divisor se coloca en `%edx`, `cltd` lo sobreescribe antes de la división.
- **Acción requerida:**
  - Cargar dividendo en `%eax`.
  - Cargar divisor en un registro auxiliar como `%ecx` (nunca `%edx`).
  - Ejecutar `cltd`.
  - Ejecutar `idivl %ecx`.
  - Para `TAC_DIV`: el cociente queda en `%eax`.
  - Para `TAC_MOD`: el resto queda en `%edx`.

### 1.5 Comparaciones Relacionales y Orden en Sintaxis AT&T (GAS)
- **Ubicación:** `src/object_code.c` (`TAC_LT`, `TAC_GT`, `TAC_EQ`).
- **Problema:** En sintaxis AT&T, la instrucción `cmpl %src, %dest` evalúa `%dest - %src`.
  - Si cargamos `arg1` en `%eax` y `arg2` en `%edx`, `cmpl %edx, %eax` evalúa `%eax - %edx` (`arg1 - arg2`).
  - Para `<`, la condición sobre `%eax` respecto a `%edx` es `setl %al`.
  - Para `>`, es `setg %al`.
  - Para `==`, es `sete %al`.
  - Luego debe extenderse a 32 bits con `movzbl %al, %eax` antes de almacenar el resultado booleano (0 o 1).

### 1.6 Detección de Variables Globales vs Locales en `loadOperand` y `storeOperand`
- **Ubicación:** `src/object_code.c` (`loadOperand`, `storeOperand`).
- **Problema:** Actualmente se asume que si `getSlotOffset(frame, op->name) == 0`, el símbolo es automáticamente global y emite `movl nombre(%rip), %reg`. Si una variable local o temporal no se registró en los slots por un bug, no emite advertencia y genera assembly inválido que falla recién en el linker (`undefined reference to 't0'`).
- **Acción requerida:** Utilizar la información del símbolo (`op->kind == OPERAND_VARIABLE` y el alcance en la tabla de símbolos) para validar si efectivamente es global. Si es `OPERAND_TEMP` o variable local y no tiene slot, registrar error interno del compilador.

### 1.7 Código de Salida y Retorno de `main`
- **Ubicación:** `src/object_code.c` (`emitEpilogue`, `TAC_RETURN`).
- **Problema:** C-TDS permite métodos `void main()`. Si `main` finaliza sin valor de retorno explícito, el registro `%eax` contendrá el residuo de la última operación ejecutada. Al enlazar con `gcc`, el sistema operativo tomará `%eax` como código de salida del proceso, haciendo que pruebas válidas reporten un exit code distinto de 0.
- **Acción requerida:** Asegurar que si la función es `main`, se emita `movl $0, %eax` antes del epílogo si no hubo un retorno con valor.

### 1.8 Conversión `int` a `float` Incompleta (`TAC_INT_TO_FLOAT`)
- **Ubicación:** `src/object_code.c` (caso `TAC_INT_TO_FLOAT`).
- **Problema:** Emite `cvtsi2ss %eax, %xmm0`, pero no almacena el contenido de `%xmm0` en el slot de destino ni gestiona registros SSE2 en `storeOperand`.
- **Acción requerida:** Diseñar el soporte de flotantes de forma aislada en la Fase 3, completando primero el 100% de enteros y booleanos exigido para la entrega.

---

## 2. Checklist de Implementación (TODO)

### Fase 1: Núcleo de Código Objeto (Enteros y Booleanos)
- [ ] **1.1 Asignación y Operaciones Aritméticas Básicas (`generateInstruction`)**
  - [X] Implementar `TAC_ASSIGN`: cargar `arg1` en `%eax` y almacenar en `result` (vía `loadRegisterBasedOnOperand` y `storeRegisterToOperand`).
  - [X] Implementar `TAC_ADD`: `emitBinaryOp(instruction, ASM_ADD, frame)`.
  - [X] Implementar `TAC_SUB`: `emitBinaryOp(instruction, ASM_SUB, frame)`.
  - [X] Implementar `TAC_MUL`: `emitBinaryOp(instruction, ASM_IMUL, frame)`.
  - [ ] Implementar `TAC_DIV`: dividendo en `%eax`, divisor en `%ecx`, `cltd`, `idivl %ecx`, guardar `%eax`.
  - [ ] Implementar `TAC_MOD`: dividendo en `%eax`, divisor en `%ecx`, `cltd`, `idivl %ecx`, guardar `%edx`.
  - [ ] Implementar `TAC_NEG`: `movl arg1, %eax; negl %eax; store result`.

- [X] **1.2 Operaciones Booleanas y Relacionales**
  - [X] Implementar `TAC_NOT`: simplificado a `xorl $1, %eax` (los bools son estrictamente `{0, 1}`, sin chequeo de `TYPE_INT`).
  - [X] Implementar `TAC_LT`: `emitRelationalOp(instruction, "setl", frame)`.
  - [X] Implementar `TAC_GT`: `emitRelationalOp(instruction, "setg", frame)`.
  - [X] Implementar `TAC_EQ`: `emitRelationalOp(instruction, "sete", frame)`.
  - [X] Implementar `TAC_AND`: `emitBinaryOp(instruction, ASM_AND, frame)`.
  - [X] Implementar `TAC_OR`: `emitBinaryOp(instruction, ASM_OR, frame)`.

- [ ] **1.3 Salto y Control de Flujo**
  - [ ] Auditar `TAC_LABEL`, `TAC_GOTO`, `TAC_IF_FALSE`, `TAC_IF_TRUE` para asegurar coherencia en el prefijo de etiquetas (`.L` estándar en GAS).

- [ ] **1.4 Pasaje de Parámetros y Llamadas a Métodos (Auditoría de `callParams`)**
  - [ ] Reemplazar `callArgIndex` global por un buffer o pila de argumentos para la llamada activa.
  - [ ] Asociar los argumentos al `TAC_CALL` respectivo mediante la aridad provista en TAC (`instruction->arg2.value.int_val`).
  - [ ] Cargar registros `%edi, %esi, %edx, %ecx, %r8d, %r9d` justo antes de emitir `call`.
  - [ ] Manejar retorno en `%eax` y almacenar en `result` si la llamada no es `void`.
  - [ ] Soporte para funciones con > 6 argumentos (pasaje por pila según System V ABI).

- [ ] **1.5 Gestión del Stack Frame y Prólogo/Epílogo (Auditoría de `FunctionFrame`)**
  - [ ] Garantizar alineación del stack a 16 bytes (`frameSize = ALIGN_UP(nextOffset, 16)`) antes de cualquier instrucción `call`.
  - [ ] Registrar en el prólogo la copia de los registros de entrada (`%edi..%r9d`) a sus respectivos slots locales en el stack.
  - [ ] Manejar lectura de parámetros 7+ desde offsets positivos (`+16(%rbp)`, `+24(%rbp)`).
  - [ ] Emitir `movl $0, %eax` en `main` antes del epílogo si el tipo de retorno es `void`.

- [ ] **1.6 Segmento de Datos (`emitGlobals`)**
  - [ ] Declaración de variables globales con `.globl`, `.align 4` y directiva `.long 0`.
  - [ ] Acceso mediante direccionamiento relativo a IP: `nombre(%rip)`.

---

### Fase 2: Integración en el Pipeline y CLI del Compilador
- [ ] **2.1 Actualización de CLI y Driver**
  - [ ] Actualizar `CURRENT_TARGET` en `src/cli.h` a `TARGET_ASSEMBLY`.
  - [ ] En `src/cli.c`: discriminar cuando el usuario no pasa `-target` (debe generar el ejecutable `.out`) de cuando pasa explícitamente `-target assembly` (genera `.ass`).
  - [ ] Respetar opción `-o <salida>` para renombrar el archivo resultante.
  - [ ] Silenciar toda salida en stdout si no se especificó `-debug`.

- [ ] **2.2 Conexión en `src/bison.y`**
  - [ ] Implementar `writeAssemblyOutput(const Options *options)`.
  - [ ] Si el objetivo final es el ejecutable (`.out`), generar el archivo assembly temporal o intermedio e invocar `gcc -no-pie <archivo.ass> -o <archivo.out>` (o ensamblador del sistema).
  - [ ] Manejar errores de ensamblado/enlace y propagar el código de salida adecuado.

---

### Fase 3: Soporte de Tipos Flotantes (Extensión opcional / Entrega Final)
- [ ] Representación en memoria de variables `float` (4 bytes, precisión simple IEEE-754).
- [ ] Emisión de constantes flotantes en sección `.rodata` o `.data`.
- [ ] Registro y uso de registros SSE2 (`%xmm0` a `%xmm7`) para pasaje de parámetros y evaluación.
- [ ] Implementación de `TAC_INT_TO_FLOAT` con almacenamiento efectivo en slot destino.
- [ ] Operaciones flotantes: `movss`, `addss`, `subss`, `mulss`, `divss`, `ucomiss`.

---

### Fase 4: Pruebas Unitarias e Integración (Testing)
- [ ] **4.1 Pruebas Unitarias de `object_code` (`tests/test_object_code.c`)**
  - [ ] Verificar creación de frames, asignación correcta de offsets y alineación a 16 bytes.
  - [ ] Probar traducción aislada de cada cuádrupla TAC a assembly.
- [ ] **4.2 Pruebas de Integración de Punta a Punta**
  - [ ] Compilar y ejecutar `examples/factorial.ctds`: verificar recursión y valor retornado.
  - [ ] Compilar y ejecutar `examples/programa_sin_error.ctds`: verificar operaciones aritméticas combinadas y precedencia.
  - [ ] Crear tests con llamadas anidadas: `sumar(doble(2), triple(3))`.
  - [ ] Crear tests con más de 6 parámetros para verificar el pasaje por pila.
  - [ ] Crear tests con variables globales y variables locales con el mismo nombre (shadowing).
  - [ ] Integrar la ejecución de los binarios generados en `test_suite.sh`.
