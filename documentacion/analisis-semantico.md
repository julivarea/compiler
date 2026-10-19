# Análisis Semántico

El alcance del análisis semántico para este compilador se limita exclusivamente a las siguientes responsabilidades:

1. **Tipos definidos:** Verificar que todos los tipos utilizados en el programa estén correctamente definidos y existan.
2. **Compatibilidad de tipos en asignaciones (y definiciones):** Asegurar que el tipo de la expresión a la derecha de una asignación o inicialización de variable coincida o sea compatible con el tipo de la variable a la izquierda.
3. **Validez de variables:** Validar que cualquier variable a la que se haga referencia o se llame haya sido previamente declarada en el ámbito correspondiente y sea válida.

Cualquier otra validación queda fuera del alcance definido para esta etapa. Ese es el único trabajo del analizador semántico.
