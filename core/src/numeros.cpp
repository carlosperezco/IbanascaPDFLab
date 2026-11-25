/**
 * @file numeros.cpp
 * @brief Conversión entre texto y números reales, independiente del idioma del sistema.
 */
#include "numeros.h"

#include <cmath>
#include <cstdint>
#include <cstdio>

namespace ibz::interno {

namespace {

bool esDigito(char c) { return c >= '0' && c <= '9'; }

} // namespace

bool leerReal(std::string_view s, double& r) {
    std::size_t i = 0;
    const std::size_t n = s.size();
    bool negativo = false;
    if (i < n && (s[i] == '+' || s[i] == '-')) negativo = s[i++] == '-';

    std::uint64_t mantisa = 0;
    int significativos = 0;  // dígitos guardados en la mantisa
    long exponente = 0;      // potencia de 10 que falta aplicar
    int digitos = 0;         // dígitos leídos en total

    for (; i < n && esDigito(s[i]); ++i, ++digitos) {
        if (significativos < 19) {
            mantisa = mantisa * 10 + static_cast<unsigned>(s[i] - '0');
            if (mantisa) ++significativos;
        } else {
            ++exponente;  // dígito que no cabe: solo cuenta su posición
        }
    }
    if (i < n && s[i] == '.') {
        for (++i; i < n && esDigito(s[i]); ++i, ++digitos) {
            if (significativos < 19) {
                mantisa = mantisa * 10 + static_cast<unsigned>(s[i] - '0');
                if (mantisa) ++significativos;
                --exponente;
            }
        }
    }
    if (digitos == 0) return false;

    if (i < n && (s[i] == 'e' || s[i] == 'E')) {
        ++i;
        bool expNegativo = false;
        if (i < n && (s[i] == '+' || s[i] == '-')) expNegativo = s[i++] == '-';
        if (i >= n || !esDigito(s[i])) return false;
        long e = 0;
        for (; i < n && esDigito(s[i]); ++i)
            if (e < 100000) e = e * 10 + (s[i] - '0');
        exponente += expNegativo ? -e : e;
    }
    if (i != n) return false;

    double v = static_cast<double>(mantisa);
    if (mantisa != 0) {
        if (exponente < -400) v = 0.0;
        else if (exponente > 400) return false;
        else if (exponente < 0) v /= std::pow(10.0, static_cast<double>(-exponente));
        else if (exponente > 0) v *= std::pow(10.0, static_cast<double>(exponente));
    }
    if (!std::isfinite(v)) return false;
    r = negativo ? -v : v;
    return true;
}

std::string escribirReal(double v, int decimales) {
    if (!std::isfinite(v)) return "0";
    if (decimales < 0) decimales = 0;
    if (decimales > 9) decimales = 9;

    std::uint64_t escala = 1;
    for (int k = 0; k < decimales; ++k) escala *= 10;

    const double redondeado = std::round(std::fabs(v) * static_cast<double>(escala));
    if (redondeado >= 9.0e18) {
        // Fuera de rango para enteros de 64 bits: sin decimales ("%.0f" no usa separador).
        char buf[400];
        std::snprintf(buf, sizeof(buf), "%.0f", v);
        return buf;
    }
    const auto total = static_cast<std::uint64_t>(redondeado);
    std::string s = std::to_string(total / escala);
    if (decimales > 0) {
        std::string frac = std::to_string(total % escala);
        frac.insert(0, static_cast<std::size_t>(decimales) - frac.size(), '0');
        while (!frac.empty() && frac.back() == '0') frac.pop_back();
        if (!frac.empty()) s += '.' + frac;
    }
    if (v < 0 && total != 0) s.insert(0, 1, '-');
    return s;
}

} // namespace ibz::interno
