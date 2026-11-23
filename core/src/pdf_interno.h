/**
 * @file pdf_interno.h
 * @brief Utilidades internas del núcleo compartidas por texto, imagen y firma (no es API pública).
 */
#pragma once

#include "ibanascapdfcore/imagen.h"

#include <qpdf/QPDF.hh>
#include <qpdf/QPDFObjectHandle.hh>

#include <string>

namespace ibz::interno {

/// Cadena literal de PDF a partir de texto WinAnsi: escapa ( ) \ y pasa a octal lo no ASCII.
std::string literalPdf(const std::string& winansi);

/// Crea el XObject de imagen (con /SMask si hay transparencia) dentro de @p pdf.
QPDFObjectHandle xobjetoImagen(QPDF& pdf, const ImagenRaster& img);

/// Rotación /Rotate de la página normalizada a 0, 90, 180 o 270.
int rotacionPagina(QPDFObjectHandle pagina);

} // namespace ibz::interno
