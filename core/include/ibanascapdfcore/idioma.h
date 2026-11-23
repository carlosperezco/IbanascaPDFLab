/**
 * @file idioma.h
 * @brief Traducciones de la interfaz (lang/es.lang, en.lang, it.lang).
 */
#pragma once

#include <initializer_list>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ibz {

/// Tabla de traducciones activa.
class Idioma {
public:
    /// Instancia única del programa.
    static Idioma& global();

    /**
     * @brief Carga `lang/<codigo>.lang` de la carpeta de datos.
     * Si no existe, intenta con inglés. Devuelve false si no se cargó ninguno.
     */
    bool cargar(const std::string& codigo);

    /// Código del idioma cargado ("es", "en", "it").
    const std::string& codigo() const noexcept { return codigo_; }

    /// Texto traducido; si falta la clave devuelve la propia clave.
    const std::string& tr(const char* clave) const;

    /// Idiomas disponibles: pares (código, nombre propio del idioma).
    static std::vector<std::pair<std::string, std::string>> disponibles();

    /// Idioma del sistema reducido a "es", "en" o "it" (inglés por defecto).
    static std::string idiomaSistema();

private:
    std::string codigo_;
    std::unordered_map<std::string, std::string> textos_;
    mutable std::unordered_map<std::string, std::string> faltantes_;
};

/// Atajo de Idioma::global().tr().
inline const std::string& tr(const char* clave) { return Idioma::global().tr(clave); }

/// Sustituye `{1}`, `{2}`… por los argumentos.
std::string formato(const std::string& plantilla, std::initializer_list<std::string> args);

} // namespace ibz
