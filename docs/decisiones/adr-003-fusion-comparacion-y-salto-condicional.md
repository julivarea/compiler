# ADR 003: Fusión de Comparaciones Relacionales y Saltos Condicionales

- **Estado:** Aceptado (`canFuseConditionAndJump` y `emitFusedComparisonAndJump` en `src/object_code/object_code.c`, activo con `-opt peephole`)
- **Contexto:** Emisión de código x86-64 para saltos condicionales generados por `if`, `if-else` y `while`.

---

## 1. Problema

El generador de código intermedio traduce las condiciones relacionales (`a < b`, `a > b`, `a == b`) en dos cuádruplas TAC consecutivas:

```text
t0 = a < b
ifFalse t0 goto .L1
```

Cuando el generador de código objeto traduce cada cuádrupla de forma aislada:

1. `emitRelationalOp` compara los operandos (`cmpl`), materializa el booleano `0`/`1` en `%eax` (`setl %al` + `movzbl %al, %eax`) y lo guarda en el slot de pila de `t0`.
2. `emitConditionalJump` vuelve a cargar `t0` desde la pila a `%eax`, ejecuta `cmpl $0, %eax` y salta con `je .L1`.

Esto consume 9 instrucciones y 2 accesos a pila para reconstruir en `EFLAGS` una condición que el primer `cmpl` ya había calculado.

---

## 2. Decisión

Con `-opt peephole` (o `-opt all`), el bucle de `emitFunction` en `src/object_code/object_code.c` inspecciona una ventana de dos instrucciones mediante `canFuseConditionAndJump(inst, next)`:

- Si `inst` es una comparación relacional (`TAC_LT`, `TAC_GT`, `TAC_EQ`) cuyo destino es un temporal (`OPERAND_TEMP`), y
- `next` es un salto condicional (`TAC_IF_FALSE` o `TAC_IF_TRUE`) que evalúa ese mismo temporal, y
- el temporal no vuelve a leerse en ninguna instrucción posterior de la función (`!isTemporaryUsedLater`),

entonces `emitFusedComparisonAndJump` emite directamente la comparación seguida del salto condicional correspondiente y avanza dos instrucciones en la lista:

```assembly
movl -4(%rbp), %eax
movl -8(%rbp), %edx
cmpl %edx, %eax
jge .L1
```

### Tabla de mnemónicos (`getRelationalMnemonics`)

| Operación TAC | Comparación (`%eax` vs `%edx`) | Salto en `ifFalse` (condición invertida) | Salto en `ifTrue` (condición directa) |
| --- | --- | --- | --- |
| `TAC_LT` (`<`) | `cmpl %edx, %eax` | `jge` | `jl` |
| `TAC_GT` (`>`) | `cmpl %edx, %eax` | `jle` | `jg` |
| `TAC_EQ` (`==`) | `cmpl %edx, %eax` | `jne` | `je` |

---

## 3. Casos borde contemplados

- **Variables de usuario o temporales reutilizados:** Si el resultado de la comparación se guarda en una variable o si el temporal se lee más adelante, `canFuseConditionAndJump` devuelve `false` y se mantiene la traducción estándar con `setcc` + `movzbl`.
- **Compatibilidad con `generateInstruction` individual:** La traducción por instrucción individual se conserva intacta cuando no se activa `-opt peephole` o cuando se invoca `generateInstruction` de forma aislada en pruebas unitarias.
