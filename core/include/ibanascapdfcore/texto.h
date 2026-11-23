/**
 * @file texto.h
 * @brief Añadir texto a una página con las fuentes estándar del PDF.
 *
 * Se usan las fuentes estándar (Helvetica, Times, Courier) con codificación
 * WinAnsi: no hace falta incrustar archivos de fuente y cubren español,
 * italiano, inglés y el resto de idiomas de Europa occidental. Los caracteres
 * que WinAnsi no tiene se sustituyen por «?».
 */
#pragma once

#include <string>
#include <string_view>

namespace ibz {

class Documento;

/// Formato del texto.
struct OpcionesTexto {
    std::string texto;              ///< UTF-8; '\n' separa líneas
    std::string fuente = "helvetica"; ///< helvetica, helvetica_negrita, times, times_negrita, courier
    double tamano = 12.0;           ///< puntos
    double r = 0.0, g = 0.0, b = 0.0; ///< color, de 0 a 1
};

/**
 * @brief Escribe texto en una página (se puede deshacer).
 *
 * @p x, @p y son coordenadas de la página en puntos PDF (espacio de usuario) y
 * marcan la esquina superior izquierda del texto tal como se ve; el texto
 * queda horizontal aunque la página tenga rotación (/Rotate).
 * @throws ibz::Error("error.texto_vacio"), ("error.rangos"), ("error.qpdf").
 */
void escribirTexto(Documento& doc, int pagina, double x, double y, const OpcionesTexto& opciones);

/// Convierte UTF-8 a WinAnsi (cp1252); lo que no existe en WinAnsi pasa a '?'.
std::string aWinAnsi(std::string_view utf8);

/// Número para el contenido de un PDF: punto decimal (sin locale), hasta 3 decimales.
std::string numeroPdf(double v);

} // namespace ibz
