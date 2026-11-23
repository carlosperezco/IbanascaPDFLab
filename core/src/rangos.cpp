/**
 * @file rangos.cpp
 * @brief Interpretación de rangos de páginas.
 */
#include "ibanascapdfcore/rangos.h"

#include "ibanascapdfcore/error.h"

#include <charconv>
#include <string>

namespace ibz {

namespace {

std::string_view recortar(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.remove_suffix(1);
    return s;
}

/// Número de página (1..total) o error.
int numero(std::string_view s, int total, std::string_view original) {
    s = recortar(s);
    int n = 0;
    const auto [p, ec] = std::from_chars(s.data(), s.data() + s.size(), n);
    if (s.empty() || ec != std::errc() || p != s.data() + s.size() || n < 1 || n > total)
        throw Error("error.rangos", std::string(original));
    return n;
}

/// Añade a @p salida las páginas (base 0) de un elemento "a", "a-b", "a-", "-b".
void elemento(std::string_view e, int total, std::string_view original, std::vector<int>& salida) {
    e = recortar(e);
    if (e.empty()) throw Error("error.rangos", std::string(original));
    const size_t guion = e.find('-');
    int a = 0;
    int b = 0;
    if (guion == std::string_view::npos) {
        a = b = numero(e, total, original);
    } else {
        const std::string_view izq = recortar(e.substr(0, guion));
        const std::string_view der = recortar(e.substr(guion + 1));
        if (izq.empty() && der.empty()) throw Error("error.rangos", std::string(original));
        a = izq.empty() ? 1 : numero(izq, total, original);
        b = der.empty() ? total : numero(der, total, original);
    }
    if (a <= b) {
        for (int i = a; i <= b; ++i) salida.push_back(i - 1);
    } else {
        for (int i = a; i >= b; --i) salida.push_back(i - 1);
    }
}

} // namespace

std::vector<std::vector<int>> analizarGrupos(std::string_view texto, int total) {
    std::vector<std::vector<int>> grupos;
    if (total < 1 || recortar(texto).empty()) throw Error("error.rangos", std::string(texto));
    std::string_view resto = texto;
    for (;;) {
        const size_t coma = resto.find(',');
        grupos.emplace_back();
        elemento(resto.substr(0, coma), total, texto, grupos.back());
        if (coma == std::string_view::npos) break;
        resto.remove_prefix(coma + 1);
    }
    return grupos;
}

std::vector<int> analizarRangos(std::string_view texto, int total) {
    std::vector<int> r;
    for (auto& g : analizarGrupos(texto, total)) r.insert(r.end(), g.begin(), g.end());
    return r;
}

} // namespace ibz
