/**
 * @file idioma.cpp
 * @brief Carga de traducciones e idioma del sistema.
 */
#include "ibanascapdfcore/idioma.h"

#include "ibanascapdfcore/claves.h"
#include "ibanascapdfcore/rutas.h"

#include <algorithm>
#include <system_error>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace ibz {

Idioma& Idioma::global() {
    static Idioma instancia;
    return instancia;
}

bool Idioma::cargar(const std::string& codigo) {
    const fs::path carpeta = carpetaDatos() / "lang";
    for (const std::string& c : {codigo, std::string("en")}) {
        Claves k;
        if (c.empty() || !k.cargar(carpeta / (c + ".lang"))) continue;
        textos_.clear();
        faltantes_.clear();
        for (const auto& [clave, valor] : k.todas()) textos_.emplace(clave, valor);
        codigo_ = c;
        return true;
    }
    return false;
}

const std::string& Idioma::tr(const char* clave) const {
    const std::string k(clave);
    const auto it = textos_.find(k);
    if (it != textos_.end()) return it->second;
    return faltantes_.emplace(k, k).first->second; // referencias estables (nodos)
}

std::vector<std::pair<std::string, std::string>> Idioma::disponibles() {
    std::vector<std::pair<std::string, std::string>> r;
    std::error_code ec;
    for (fs::directory_iterator it(carpetaDatos() / "lang", ec), fin; !ec && it != fin; it.increment(ec)) {
        if (it->path().extension() != ".lang") continue;
        Claves k;
        if (!k.cargar(it->path())) continue;
        const std::string codigo = utf8(it->path().stem());
        r.emplace_back(codigo, k.obtener("idioma.nombre", codigo));
    }
    std::sort(r.begin(), r.end());
    return r;
}

std::string Idioma::idiomaSistema() {
#ifdef _WIN32
    switch (PRIMARYLANGID(GetUserDefaultUILanguage())) {
    case LANG_SPANISH: return "es";
    case LANG_ITALIAN: return "it";
    default: return "en";
    }
#else
    for (const char* var : {"LC_ALL", "LC_MESSAGES", "LANG"}) {
        const std::string v = variableEntorno(var);
        if (v.size() < 2 || v == "C" || v == "POSIX") continue;
        const std::string c = v.substr(0, 2);
        if (c == "es" || c == "it" || c == "en") return c;
        return "en";
    }
    return "en";
#endif
}

std::string formato(const std::string& plantilla, std::initializer_list<std::string> args) {
    std::string r;
    r.reserve(plantilla.size() + 16);
    for (size_t i = 0; i < plantilla.size(); ++i) {
        if (plantilla[i] == '{' && i + 2 < plantilla.size() && plantilla[i + 2] == '}' &&
            plantilla[i + 1] >= '1' && plantilla[i + 1] <= '9') {
            const size_t n = static_cast<size_t>(plantilla[i + 1] - '1');
            if (n < args.size()) {
                r += *(args.begin() + n);
                i += 2;
                continue;
            }
        }
        r += plantilla[i];
    }
    return r;
}

} // namespace ibz
