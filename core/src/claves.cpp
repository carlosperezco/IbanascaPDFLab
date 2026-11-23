/**
 * @file claves.cpp
 * @brief Lectura y escritura de archivos `clave = valor`.
 */
#include "ibanascapdfcore/claves.h"

#include <algorithm>
#include <charconv>
#include <fstream>
#include <iterator>
#include <string_view>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

namespace ibz {

namespace {

std::string_view recortar(std::string_view s) {
    const auto es = [](char c) { return c == ' ' || c == '\t' || c == '\r'; };
    while (!s.empty() && es(s.front())) s.remove_prefix(1);
    while (!s.empty() && es(s.back())) s.remove_suffix(1);
    return s;
}

std::string desescapar(std::string_view s) {
    std::string r;
    r.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            switch (s[i + 1]) {
            case 'n': r += '\n'; ++i; continue;
            case 't': r += '\t'; ++i; continue;
            case '\\': r += '\\'; ++i; continue;
            default: break;
            }
        }
        r += s[i];
    }
    return r;
}

std::string escapar(const std::string& s) {
    std::string r;
    r.reserve(s.size());
    for (const char c : s) {
        switch (c) {
        case '\n': r += "\\n"; break;
        case '\t': r += "\\t"; break;
        case '\\': r += "\\\\"; break;
        default: r += c; break;
        }
    }
    return r;
}

} // namespace

bool Claves::cargar(const fs::path& ruta) {
    std::ifstream f(ruta, std::ios::binary);
    if (!f) return false;
    const std::string texto((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    std::string_view resto(texto);
    if (resto.substr(0, 3) == "\xEF\xBB\xBF") resto.remove_prefix(3); // BOM UTF-8

    while (!resto.empty()) {
        const size_t fin = resto.find('\n');
        std::string_view linea = recortar(resto.substr(0, fin));
        resto = (fin == std::string_view::npos) ? std::string_view() : resto.substr(fin + 1);
        if (linea.empty() || linea.front() == '#') continue;
        const size_t igual = linea.find('=');
        if (igual == std::string_view::npos) continue;
        const std::string_view clave = recortar(linea.substr(0, igual));
        if (clave.empty()) continue;
        mapa_[std::string(clave)] = desescapar(recortar(linea.substr(igual + 1)));
    }
    return true;
}

bool Claves::guardar(const fs::path& ruta) const {
    std::vector<const std::pair<const std::string, std::string>*> orden;
    orden.reserve(mapa_.size());
    for (const auto& par : mapa_) orden.push_back(&par);
    std::sort(orden.begin(), orden.end(), [](const auto* a, const auto* b) { return a->first < b->first; });

    fs::path temporal = ruta;
    temporal += ".tmp";
    {
        std::ofstream f(temporal, std::ios::binary | std::ios::trunc);
        if (!f) return false;
        for (const auto* par : orden) f << par->first << " = " << escapar(par->second) << '\n';
        if (!f) return false;
    }
    std::error_code ec;
    fs::rename(temporal, ruta, ec);
    if (ec) {
        fs::remove(temporal, ec);
        return false;
    }
    return true;
}

const std::string* Claves::buscar(const std::string& clave) const noexcept {
    const auto it = mapa_.find(clave);
    return it == mapa_.end() ? nullptr : &it->second;
}

std::string Claves::obtener(const std::string& clave, const std::string& defecto) const {
    const std::string* v = buscar(clave);
    return v ? *v : defecto;
}

int Claves::obtenerEntero(const std::string& clave, int defecto) const {
    const std::string* v = buscar(clave);
    if (!v || v->empty()) return defecto;
    int r = 0;
    const auto [p, ec] = std::from_chars(v->data(), v->data() + v->size(), r);
    return (ec == std::errc() && p == v->data() + v->size()) ? r : defecto;
}

void Claves::poner(const std::string& clave, std::string valor) {
    mapa_[clave] = std::move(valor);
}

} // namespace ibz
