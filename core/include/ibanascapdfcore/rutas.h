/**
 * @file rutas.h
 * @brief Rutas del programa (ejecutable, recursos, ajustes) y conversión UTF-8.
 *
 * Todas las rutas que cruzan la frontera núcleo/interfaz viajan como texto
 * UTF-8; dentro del núcleo se usan std::filesystem::path.
 */
#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace ibz {

/// Carpeta que contiene el ejecutable.
std::filesystem::path carpetaEjecutable();

/**
 * @brief Carpeta de recursos (config, lang, assets).
 *
 * Orden de búsqueda: variable IBANASCAPDF_DATOS, `<exe>/../share/ibanascapdf`
 * y, por último, la carpeta del ejecutable. Se calcula una sola vez.
 */
const std::filesystem::path& carpetaDatos();

/**
 * @brief Carpeta de ajustes del usuario; se crea si no existe.
 *
 * Windows: `%APPDATA%\DonatellaTech\IbanascaPDFLab\`.
 * Linux: `$XDG_CONFIG_HOME/donatellatech/ibanascapdf/` (o `~/.config/...`).
 */
std::filesystem::path carpetaUsuario();

/**
 * @brief Fija las carpetas de recursos y de ajustes (Android: las da la app).
 *
 * Debe llamarse antes de cualquier otra función de este archivo.
 */
void establecerCarpetas(const std::filesystem::path& datos, const std::filesystem::path& usuario);

/// Archivo de ajustes del usuario (`ajustes.conf` dentro de carpetaUsuario()).
std::filesystem::path rutaAjustes();

/// Ruta → texto UTF-8.
std::string utf8(const std::filesystem::path& ruta);

/// Texto UTF-8 → ruta.
std::filesystem::path desdeUtf8(const std::string& texto);

/// Variable de entorno en UTF-8 (cadena vacía si no existe).
std::string variableEntorno(const char* nombre);

/// Argumentos de la línea de órdenes en UTF-8 (en Windows se leen en UTF-16).
std::vector<std::string> argumentosUtf8(int argc, char** argv);

} // namespace ibz
