#ifndef DEBUG_H
#define DEBUG_H

/**
 * @file debug.h
 * @brief Utilidad centralizada de salida de diagnóstico controlada por `-debug`.
 */

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>

/*
 * Convencion de salida de debug del compilador (unica forma en todo el codigo):
 *  - La salida de debug va a stdout, y solo si el usuario paso -debug.
 *    Los errores van a stderr aparte (error_handler.h), nunca por aqui.
 *  - Toda impresion de debug pasa por debugPrintf con el bool de la opcion.
 *  - Cada mensaje lleva prefijo de etapa: [SCAN], [TAC], [OPT], [ASM]...
 *  - Texto plano, sin colores ni separadores ANSI.
 */
static void debugPrintf(bool debugEnabled, const char *format, ...) {
    if (!debugEnabled) {
        return;
    }

    va_list arguments;
    va_start(arguments, format);
    vprintf(format, arguments);
    va_end(arguments);
}

#endif // DEBUG_H
