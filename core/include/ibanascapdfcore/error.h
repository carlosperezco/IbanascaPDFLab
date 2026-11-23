/**
 * @file error.h
 * @brief Error del núcleo con clave de traducción.
 *
 * El núcleo nunca muestra textos al usuario: lanza ibz::Error con una clave
 * (por ejemplo "error.leer") que la interfaz traduce con ibz::tr().
 */
#pragma once

#include <stdexcept>
#include <string>

namespace ibz {

/// Error con clave de traducción y detalle opcional (ruta, mensaje técnico…).
class Error : public std::runtime_error {
public:
    /**
     * @param clave   Clave del archivo de idioma (archivos .lang).
     * @param detalle Texto adicional sin traducir (ruta del archivo, etc.).
     */
    explicit Error(std::string clave, std::string detalle = {})
        : std::runtime_error(clave), clave_(std::move(clave)), detalle_(std::move(detalle)) {}

    /// Clave de traducción del mensaje principal.
    const std::string& clave() const noexcept { return clave_; }
    /// Detalle sin traducir (puede estar vacío).
    const std::string& detalle() const noexcept { return detalle_; }

private:
    std::string clave_;
    std::string detalle_;
};

} // namespace ibz
