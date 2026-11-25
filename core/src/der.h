/**
 * @file der.h
 * @brief Codificación y lectura DER mínimas para certificados y CMS (uso interno).
 */
#pragma once

#include <cstddef>
#include <initializer_list>
#include <string_view>
#include <vector>

namespace ibz::interno {

using Der = std::vector<unsigned char>;

/// TLV con la longitud codificada en DER.
Der tlv(unsigned char etiqueta, const Der& contenido);

/// Concatenación de varios TLV.
Der concat(std::initializer_list<Der> partes);

/// OBJECT IDENTIFIER a partir de la notación con puntos ("1.2.840.113549.1.7.2").
Der oid(std::string_view puntos);

/// INTEGER pequeño no negativo.
Der entero(unsigned valor);

/// SET OF en DER: los elementos se ordenan por su codificación (X.690 11.6).
Der conjunto(std::vector<Der> elementos, unsigned char etiqueta = 0x31);

/// Elemento leído: etiqueta, inicio del TLV completo, inicio y largo del contenido.
struct Tlv {
    unsigned char etiqueta = 0;
    std::size_t inicio = 0;    ///< posición de la etiqueta
    std::size_t contenido = 0; ///< posición del contenido
    std::size_t largo = 0;     ///< largo del contenido
    std::size_t fin() const noexcept { return contenido + largo; }
};

/// Lee el TLV que empieza en @p pos (sin pasar de @p limite). false si está mal formado.
bool leerTlv(const Der& d, std::size_t pos, std::size_t limite, Tlv& t);

} // namespace ibz::interno
