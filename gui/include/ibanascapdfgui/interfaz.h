/**
 * @file interfaz.h
 * @brief Punto de entrada de la interfaz GTK 3.
 */
#pragma once

#include <string>

namespace ibz {
class Claves;
}

namespace ibz::gui {

/**
 * @brief Crea la ventana principal y ejecuta el bucle de GTK.
 * @param config  Configuración del programa (config/app.conf).
 * @param ajustes Ajustes del usuario; se guardan al cambiar el idioma y al salir.
 * @param archivo PDF que se abre al arrancar (UTF-8, puede estar vacío).
 * @return Código de salida del proceso.
 */
int ejecutar(const Claves& config, Claves& ajustes, const std::string& archivo);

} // namespace ibz::gui
