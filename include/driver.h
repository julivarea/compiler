#ifndef DRIVER_H
#define DRIVER_H

/**
 * @file driver.h
 * @brief Orquestación y ejecución de las etapas de compilación.
 */

#include "cli.h"

/**
 * @brief Ejecuta el proceso de compilación hasta la etapa indicada en las opciones.
 *
 * @param options Configuración resultante del procesamiento de argumentos de la CLI.
 * @return 0 si la compilación fue exitosa, 1 si ocurrió algún error en cualquier fase.
 */
int compile(const Options *options);

#endif // DRIVER_H
