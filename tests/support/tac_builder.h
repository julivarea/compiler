#ifndef TAC_BUILDER_H
#define TAC_BUILDER_H

/**
 * @file tac_builder.h
 * @brief Construcción de listas TAC desde texto para el Arrange de las etapas que reciben TAC.
 *
 * Permite escribir la entrada del optimizador, del análisis de variables vivas y del
 * generador de código objeto en el mismo formato que produce `-target codinter`, sin
 * pasar por el front-end. Así un test de esas etapas solo puede fallar por un defecto
 * propio.
 *
 * Formato admitido (una instrucción por línea; las líneas vacías se ignoran):
 *
 *     global g@1, INT          local x@2, BOOL          param_decl n@3, INT
 *     begin_func f, 2          end_func f               L0:
 *     goto L0                  if t0 false goto L1      if t0 true goto L1
 *     param x@2                t1 = call f, 2           call g, 0
 *     ret t0                   ret
 *     x@2 = t0                 t0 = -x@2                t0 = !b@3
 *     t0 = a@1 + 5             (operadores: + - * / % < > ==)
 *
 * Operandos: `tN` es un temporal, `nombre@uid` una variable (global si aparece en una
 * línea `global`), un entero o `true`/`false` una constante. `x = -N` se interpreta como
 * la negación unaria de la constante N, que es lo que emite el generador.
 *
 * El tipo de retorno de cada método se deduce de sus `ret`: VOID si ninguno lleva valor.
 */

#include "tac.h"

/**
 * @brief Construye la lista TAC descrita por @p text.
 *
 * Ante una línea que no reconoce imprime el problema por stderr y devuelve NULL.
 */
TACList *buildTAC(const char *text);

/** @brief Libera los símbolos creados por buildTAC. Se llama en el tearDown de la suite. */
void releaseTACSymbols(void);

/** @brief Vuelca @p list en su formato textual (el del `.ci`) y devuelve la cadena. */
const char *formatTAC(const TACList *list);

#endif // TAC_BUILDER_H
