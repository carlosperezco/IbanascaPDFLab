/**
 * @file util.cpp
 * @brief Utilidades de la interfaz.
 */
#include "util.h"

#include <ibanascapdfcore/rutas.h>

namespace ibz::gui {

std::filesystem::path rutaDesdeGlib(const char* nombre) {
    if (!nombre) return {};
#ifdef _WIN32
    return desdeUtf8(nombre);
#else
    return std::filesystem::path(nombre);
#endif
}

std::string aGlib(const std::filesystem::path& ruta) {
#ifdef _WIN32
    return utf8(ruta);
#else
    return ruta.string();
#endif
}

std::string sinMnemonico(const std::string& texto) {
    std::string r;
    r.reserve(texto.size());
    for (size_t i = 0; i < texto.size(); ++i) {
        if (texto[i] == '_') {
            if (i + 1 < texto.size() && texto[i + 1] == '_') {
                r += '_';
                ++i;
            }
            continue;
        }
        r += texto[i];
    }
    static const std::string puntos = "\xE2\x80\xA6"; // «…»
    if (r.size() >= puntos.size() && r.compare(r.size() - puntos.size(), puntos.size(), puntos) == 0)
        r.erase(r.size() - puntos.size());
    return r;
}

} // namespace ibz::gui
