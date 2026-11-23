/**
 * @file util.h
 * @brief Utilidades de la interfaz: rutas GLib ↔ std::filesystem y textos de menú.
 */
#pragma once

#include <filesystem>
#include <string>

namespace ibz::gui {

/// Nombre de archivo de GLib (UTF-8 en Windows, bytes del sistema en Linux) → ruta.
std::filesystem::path rutaDesdeGlib(const char* nombre);

/// Ruta → nombre de archivo de GLib.
std::string aGlib(const std::filesystem::path& ruta);

/// Quita los subrayados de mnemónico y los puntos suspensivos finales ("_Abrir…" → "Abrir").
std::string sinMnemonico(const std::string& texto);

} // namespace ibz::gui
