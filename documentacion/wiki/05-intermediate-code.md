# Generación de Código Intermedio (TAC)

La etapa de generación de código intermedio toma el AST validado y produce Código de Tres Direcciones (*Three-Address Code* o TAC). Transforma las construcciones complejas del lenguaje (expresiones compuestas, bifurcaciones condicionales, ciclos y llamadas anidadas) en secuencias lineales de instrucciones elementales, sirviendo de puente hacia la generación de código máquina.

---

## 1. Variables Temporales y Etiquetas de Salto

- **Temporales (`t0, t1, t2, ...`):** Cada sub-expresión intermedia almacena su valor en una variable temporal única generada automáticamente. Por ejemplo, `(a + b) * c` se descompone en:
  ```text
  t0 = a + b
  t1 = t0 * c
  ```
- **Etiquetas (`L0, L1, L2, ...`):** Las estructuras de control no lineales se convierten en saltos condicionales (`if <cond> false goto <label>`) y saltos incondicionales (`goto <label>`).

---

## 2. Instrucciones TAC Generadas

### Expresiones y Asignaciones
- **Asignación simple:** `<variable> = <valor>`
- **Operación binaria:** `tX = <op1> <operador> <op2>`
- **Operación unaria:** `tX = -<op>` o `tX = !<op>`

### Estructuras de Control

#### Condicional (`if-else`)
```text
L0:
if <condicion> false goto L1
<instrucciones_bloque_if>
goto L2
L1:
<instrucciones_bloque_else>
L2:
```

#### Ciclo (`while`)
```text
L0:
if <condicion> false goto L1
<instrucciones_cuerpo>
goto L0
L1:
```

### Funciones y Llamadas
- **Declaración de función:** Se emite una etiqueta con el nombre del método precediendo a su cuerpo.
- **Retorno (`return`):** `ret` (para funciones `void`) o `ret <expresion>` (para funciones con valor).
- **Llamadas a funciones:** Los argumentos se pasan secuencialmente con instrucciones `param` antes de invocar la función con `call`:
  ```text
  param arg1
  param arg2
  tX = call nombre_funcion, 2
  ```

---

## 3. Ejemplo de Salida TAC

Para el siguiente método en C-TDS:

```c
int sumar(int a, int b) {
    return a + b;
}

void main() {
    int total;
    total = sumar(10, 20);
}
```

La salida linealizada en TAC es:

```text
sumar:
t0 = a + b
ret t0

main:
param 10
param 20
total = call sumar, 2
```

---

**Siguiente:** [Índice de Onboarding](00-index.md)
