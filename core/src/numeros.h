/**
 * @file numeros.h
 * @brief Conversión entre texto y números reales, independiente del idioma del sistema.
 *
 * Siempre usa el punto como separador decimal (como el formato PDF). Se escribe aquí
 * porque std::from_chars/std::to_chars para double no existen en todas las bibliotecas
 * de C++ (por ejemplo, la del NDK de Android), y printf/strtod dependen del idioma.
 */
#pragma once

#include <string>
#include <string_view>

namespace ibz::interno {

/// Lee un real en formato [+-]dígitos[.dígitos][e[+-]dígitos], sin espacios.
/// Devuelve false si el texto no es exactamente un número finito.
bool leerReal(std::string_view s, double& r);

/// Escribe @p v con como mucho @p decimales decimales (0..9), sin ceros finales
/// innecesarios ("2.50" → "2.5", "3.000" → "3"). Los valores no finitos dan "0".
std::string escribirReal(double v, int decimales);

} // namespace ibz::interno
