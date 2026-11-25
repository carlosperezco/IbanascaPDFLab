/**
 * @file der.cpp
 * @brief Codificación y lectura DER mínimas.
 */
#include "der.h"

#include <algorithm>
#include <cstdint>

namespace ibz::interno {

Der tlv(unsigned char etiqueta, const Der& contenido) {
    Der r;
    r.reserve(contenido.size() + 6);
    r.push_back(etiqueta);
    const std::size_t n = contenido.size();
    if (n < 0x80) {
        r.push_back(static_cast<unsigned char>(n));
    } else {
        unsigned char bytes[sizeof(std::size_t)];
        int k = 0;
        for (std::size_t v = n; v; v >>= 8) bytes[k++] = static_cast<unsigned char>(v & 0xFF);
        r.push_back(static_cast<unsigned char>(0x80 | k));
        while (k) r.push_back(bytes[--k]);
    }
    r.insert(r.end(), contenido.begin(), contenido.end());
    return r;
}

Der concat(std::initializer_list<Der> partes) {
    Der r;
    for (const Der& p : partes) r.insert(r.end(), p.begin(), p.end());
    return r;
}

Der oid(std::string_view puntos) {
    std::vector<std::uint64_t> arcos;
    std::uint64_t v = 0;
    for (const char c : puntos) {
        if (c == '.') {
            arcos.push_back(v);
            v = 0;
        } else {
            v = v * 10 + static_cast<std::uint64_t>(c - '0');
        }
    }
    arcos.push_back(v);
    Der cuerpo;
    auto base128 = [&cuerpo](std::uint64_t x) {
        unsigned char tmp[10];
        int k = 0;
        do {
            tmp[k++] = static_cast<unsigned char>(x & 0x7F);
            x >>= 7;
        } while (x);
        while (k > 1) cuerpo.push_back(static_cast<unsigned char>(0x80 | tmp[--k]));
        cuerpo.push_back(tmp[0]);
    };
    base128(arcos[0] * 40 + (arcos.size() > 1 ? arcos[1] : 0));
    for (std::size_t i = 2; i < arcos.size(); ++i) base128(arcos[i]);
    return tlv(0x06, cuerpo);
}

Der entero(unsigned valor) {
    Der cuerpo;
    do {
        cuerpo.insert(cuerpo.begin(), static_cast<unsigned char>(valor & 0xFF));
        valor >>= 8;
    } while (valor);
    if (cuerpo.front() & 0x80) cuerpo.insert(cuerpo.begin(), 0);
    return tlv(0x02, cuerpo);
}

Der conjunto(std::vector<Der> elementos, unsigned char etiqueta) {
    std::sort(elementos.begin(), elementos.end());
    Der cuerpo;
    for (const Der& e : elementos) cuerpo.insert(cuerpo.end(), e.begin(), e.end());
    return tlv(etiqueta, cuerpo);
}

bool leerTlv(const Der& d, std::size_t pos, std::size_t limite, Tlv& t) {
    limite = std::min(limite, d.size());
    if (pos + 2 > limite) return false;
    t.inicio = pos;
    t.etiqueta = d[pos];
    std::size_t n = d[pos + 1];
    std::size_t p = pos + 2;
    if (n & 0x80) {
        const std::size_t k = n & 0x7F;
        if (k == 0 || k > sizeof(std::size_t) || p + k > limite) return false;
        n = 0;
        for (std::size_t i = 0; i < k; ++i) n = (n << 8) | d[p + i];
        p += k;
    }
    if (n > limite - p) return false;
    t.contenido = p;
    t.largo = n;
    return true;
}

} // namespace ibz::interno
