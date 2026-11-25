/**
 * @file modulo.cpp
 * @brief Registro de módulos.
 */
#include "ibanascapdfcore/modulo.h"

#include "ibanascapdfcore/rangos.h"

#include "numeros.h"

#include <algorithm>
#include <charconv>
#include <cstring>

namespace ibz {

std::vector<int> paginasElegidas(const Valores& valores, const std::string& id, int total) {
    const auto it = valores.find(id);
    if (it != valores.end() && it->second.find_first_not_of(" \t") != std::string::npos)
        return analizarRangos(it->second, total);
    const auto actual = valores.find("pagina_actual");
    return analizarRangos(actual != valores.end() ? actual->second : std::string("1"), total);
}

std::string valor(const Valores& valores, const std::string& id, const std::string& defecto) {
    const auto it = valores.find(id);
    return (it == valores.end() || it->second.empty()) ? defecto : it->second;
}

double valorReal(const Valores& valores, const std::string& id, double defecto) {
    const auto it = valores.find(id);
    if (it == valores.end() || it->second.empty()) return defecto;
    double r = 0.0;
    return interno::leerReal(it->second, r) ? r : defecto;
}

void colorRgb(const std::string& texto, double& r, double& g, double& b) {
    r = g = b = 0.0;
    if (texto.size() != 7 || texto[0] != '#') return;
    unsigned v = 0;
    const auto [p, ec] = std::from_chars(texto.data() + 1, texto.data() + 7, v, 16);
    if (ec != std::errc() || p != texto.data() + 7) return;
    r = ((v >> 16) & 0xFF) / 255.0;
    g = ((v >> 8) & 0xFF) / 255.0;
    b = (v & 0xFF) / 255.0;
}

Registro& Registro::instancia() {
    static Registro r;
    return r;
}

bool Registro::agregar(std::unique_ptr<Modulo> modulo) {
    if (!modulo || buscar(modulo->id())) return false;
    const auto menor = [](const std::unique_ptr<Modulo>& a, const std::unique_ptr<Modulo>& b) {
        if (const int c = std::strcmp(a->menu(), b->menu())) return c < 0;
        if (a->orden() != b->orden()) return a->orden() < b->orden();
        return std::strcmp(a->id(), b->id()) < 0;
    };
    modulos_.insert(std::upper_bound(modulos_.begin(), modulos_.end(), modulo, menor), std::move(modulo));
    return true;
}

const Modulo* Registro::buscar(std::string_view id) const noexcept {
    for (const auto& m : modulos_)
        if (id == m->id()) return m.get();
    return nullptr;
}

} // namespace ibz
