#ifndef STRING_BUILDER_H
#define STRING_BUILDER_H

/**
 * @file string_builder.h
 * @brief Acumulador dinámico de cadenas y formateador de texto en memoria.
 */

/**
 * @brief Agrega una cadena de caracteres terminada en nulo al búfer acumulador.
 *
 * Reserva o expande dinámicamente el búfer interno según sea necesario y
 * concatena el contenido provisto, garantizando la terminación en nulo.
 *
 * @param string Cadena constante a concatenar (si es NULL no realiza acción).
 */
void buildString(const char *string);

/**
 * @brief Agrega texto con formato estilo printf al búfer acumulador.
 *
 * Calcula la longitud requerida, redimensiona la capacidad del búfer dinámico
 * y escribe los argumentos formateados según el especificador. Comparte el
 * mismo acumulador interno que @p buildString.
 *
 * @param format Cadena de control de formato con especificadores estilo printf (si es NULL no realiza acción).
 * @param ... Argumentos variables correspondientes a los especificadores de formato.
 */
void buildStringf(const char *format, ...);

/**
 * @brief Obtiene la cadena acumulada completa de sólo lectura.
 *
 * Proporciona acceso directo al búfer dinámico generado hasta el momento.
 * La cadena devuelta pertenece al acumulador y no debe ser liberada por el llamador.
 *
 * @return Puntero constante a la cadena acumulada, o cadena vacía "" si no se ha emitido texto.
 */
const char *getBuiltString(void);

/**
 * @brief Restablece el acumulador liberando la memoria dinámica reservada.
 *
 * Libera el búfer interno y reinicia la longitud y la capacidad a cero,
 * dejando el acumulador listo para un nuevo ciclo de emisión.
 */
void resetBuildString(void);

#endif // STRING_BUILDER_H
