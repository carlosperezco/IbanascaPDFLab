/**
 * @file firma.h
 * @brief Firma digital PAdES-B-B con certificado .p12/.pfx (GnuTLS + QPDF, sin PoDoFo).
 *
 * Proceso:
 *  1. prepararFirma(): QPDF añade el campo de firma, su aspecto (invisible, texto o
 *     imagen) y un diccionario /Sig con /ByteRange y /Contents de relleno, y escribe
 *     el PDF completo sin flujos de objetos (el diccionario queda legible).
 *  2. completarFirma(): localiza el relleno, calcula /ByteRange, firma esos bytes
 *     (CMS separado, SHA-256, con el atributo signing-certificate-v2 y sin
 *     signing-time, como pide PAdES) y escribe la firma en /Contents.
 *
 * Limitaciones de la 1.0: una sola firma por documento (QPDF reescribe el archivo
 * entero: una segunda firma invalidaría la primera), sin sello de tiempo, y no se
 * firman documentos cifrados.
 */
#pragma once

#include "ibanascapdfcore/imagen.h"

#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace ibz {

class Documento;

/// Certificado y clave privada leídos de un archivo PKCS#12 (.p12/.pfx).
class Firmante {
public:
    /**
     * @throws ibz::Error("error.leer"), ("error.certificado"), ("error.contrasena_certificado"),
     *         ("error.certificado_caducado"), ("error.certificado_no_vigente").
     */
    Firmante(const std::filesystem::path& p12, const std::string& contrasena);
    ~Firmante();
    Firmante(const Firmante&) = delete;
    Firmante& operator=(const Firmante&) = delete;

    /// Nombre del titular (CN del certificado), UTF-8.
    const std::string& nombre() const noexcept;

    /// CMS/PKCS#7 separado (DER) de los bytes indicados, con SHA-256 y atributos PAdES.
    std::vector<unsigned char> firmarCms(const unsigned char* datos, std::size_t n) const;

private:
    struct Impl;
    std::unique_ptr<Impl> d_;
};

/// Aspecto de la firma en la página.
enum class AspectoFirma { Invisible, Texto, Imagen };

/// Opciones de la firma.
struct OpcionesFirma {
    AspectoFirma aspecto = AspectoFirma::Texto;
    int pagina = 0;        ///< base 0 (solo si es visible)
    double x = 0, y = 0;   ///< esquina superior izquierda tal como se ve (puntos PDF)
    double anchoPt = 170;  ///< ancho del recuadro visible
    ImagenRaster imagen;   ///< para AspectoFirma::Imagen
};

/// Bytes reservados para la firma CMS (el hexadecimal ocupa el doble).
constexpr std::size_t kBytesFirma = 16384;

/**
 * @brief Firma el documento (paso 1 + paso 2). Se puede deshacer.
 * @throws ibz::Error("error.ya_firmado"), ("error.firma_cifrado"), ("error.qpdf"), …
 */
void firmarDocumento(Documento& doc, const Firmante& firmante, const OpcionesFirma& opciones);

/**
 * @brief Paso 2: completa la firma de un PDF preparado (con /ByteRange y /Contents de relleno).
 * @throws ibz::Error("error.firma") si no encuentra el relleno o la firma no cabe.
 */
void completarFirma(std::vector<unsigned char>& pdf, const Firmante& firmante);

} // namespace ibz
