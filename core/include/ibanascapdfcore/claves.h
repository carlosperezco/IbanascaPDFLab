/**
 * @file claves.h
 * @brief Archivos de texto `clave = valor` (configuración, idiomas, ajustes).
 *
 * Formato: UTF-8, una entrada por línea, `#` inicia un comentario de línea,
 * espacios alrededor de clave y valor ignorados. En el valor se admiten las
 * secuencias `\n`, `\t` y `\\`.
 */
#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>

namespace ibz {

/// Tabla de pares clave/valor leída de un archivo de texto.
class Claves {
public:
    /// Carga (y mezcla) el archivo. Devuelve false si no se pudo leer.
    bool cargar(const std::filesystem::path& ruta);

    /// Guarda todas las claves ordenadas alfabéticamente. Devuelve false si falla.
    bool guardar(const std::filesystem::path& ruta) const;

    /// Puntero al valor o nullptr si la clave no existe (sin copias).
    const std::string* buscar(const std::string& clave) const noexcept;

    /// Valor o @p defecto si la clave no existe.
    std::string obtener(const std::string& clave, const std::string& defecto = {}) const;

    /// Valor entero o @p defecto si no existe o no es un número.
    int obtenerEntero(const std::string& clave, int defecto) const;

    /// Crea o reemplaza una clave.
    void poner(const std::string& clave, std::string valor);

    /// Todas las entradas.
    const std::unordered_map<std::string, std::string>& todas() const noexcept { return mapa_; }

private:
    std::unordered_map<std::string, std::string> mapa_;
};

} // namespace ibz
