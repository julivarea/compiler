# Visión General y Arquitectura

Compilador de múltiples pasadas para el lenguaje **C-TDS (TDS26)**. La arquitectura desacopla el análisis sintáctico de la validación semántica y de la posterior emisión de código, utilizando el Árbol de Sintaxis Abstracta (AST) como estructura central de comunicación entre etapas.

---

## Pipeline de Compilación

La transformación del código fuente se estructura en fases secuenciales:

1. **Análisis Léxico:** Reconoce y clasifica los caracteres del código fuente en tokens (palabras clave, identificadores, literales, operadores). Descarta espacios en blanco y comentarios, y rastrea números de línea para reportes de error.
2. **Análisis Sintáctico:** Verifica que la secuencia de tokens respete la gramática libre de contexto de C-TDS mediante análisis LALR(1). Ante el primer error formal, interrumpe el proceso.
3. **Construcción del AST:** Durante las reducciones sintácticas se construye un árbol jerárquico representativo del programa, filtrando detalles puramente sintácticos que no aportan semántica (puntos y comas, paréntesis, llaves).
4. **Análisis Semántico y Resolución de Ámbitos:**
   - Construye la jerarquía de ámbitos (*scopes*) para variables globales, locales y parámetros.
   - Aplica reglas de alcance (*scope chaining* y *shadowing*).
   - Valida el tipado estático, coherencia de retornos en funciones y condiciones booleanas en estructuras de control.
   - A diferencia del analizador sintáctico, acumula y reporta todos los errores encontrados antes de abortar.
5. **Generación de Código Intermedio (TAC):** Recorre el AST validado y emite Código de Tres Direcciones linealizado con variables temporales y etiquetas para control de saltos.

---

## Principios de Diseño

- **Desacoplamiento Front-End / Back-End:** La gramática únicamente produce el AST. Ninguna regla de tipo ni validación de identificadores se ejecuta durante el parseo, evitando complejizar las acciones sintácticas.
- **Representación Uniforme en Memoria:** Todos los nodos del programa comparten una estructura base en el AST, permitiendo recorridos recursivos homogéneos para análisis semántico e impresión.
- **Ámbitos Jerárquicos en Árbol:** Cada bloque delimitado por llaves o firma de función posee su propio entorno léxico con puntero a su ámbito contenedor, permitiendo resolución de identificadores en tiempo ascendente hacia la raíz.
- **Linealización Atómica:** El generador intermedio convierte bifurcaciones y expresiones anidadas en instrucciones elementales de a lo sumo tres direcciones, preparando la base para la futura generación a assembly x86-64.

---

**Siguiente:** [Análisis Léxico y Sintáctico](02-lexical-syntax-analysis.md)
