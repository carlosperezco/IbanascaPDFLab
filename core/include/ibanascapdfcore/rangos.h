/**
 * @file rangos.h
 * @brief Interpretación de rangos de páginas escritos por el usuario.
 *
 * Sintaxis (páginas numeradas desde 1, espacios ignorados):
 *  - `5`      una página
 *  - `1-3`    de la 1 a la 3
 *  - `8-`     de la 8 al final
 *  - `-3`     del principio a la 3
 *  - `5-3`    orden inverso (5, 4, 3)
 *  - `1-3,5`  varios elementos separados por comas
 */
#pragma once

#include <string_view>
#include <vector>

namespace ibz {

/**
 * @brief Convierte el texto en índices de página base 0, en el orden escrito.
 * @param texto Rangos, p. ej. "1-3,5,8-".
 * @param total Número de páginas del documento.
 * @throws ibz::Error("error.rangos") si la sintaxis o los números no son válidos.
 */
std::vector<int> analizarRangos(std::string_view texto, int total);

/**
 * @brief Igual que analizarRangos(), pero cada elemento separado por comas es un grupo.
 *
 * Sirve para separar un PDF en varios archivos: "1-3,4-6" → {{0,1,2},{3,4,5}}.
 */
std::vector<std::vector<int>> analizarGrupos(std::string_view texto, int total);

} // namespace ibz
