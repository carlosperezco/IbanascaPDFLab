/**
 * @file firma.h
 * @brief Firma digital PAdES-B-B (QPDF + CMS propio, sin PoDoFo).
 *
 * Proceso:
 *  1. firmarDocumento(): QPDF añade el campo de firma, su aspecto (invisible, texto,
 *     imagen o dibujo) y un diccionario /Sig con /ByteRange y /Contents de relleno,
 *     y escribe el PDF completo sin flujos de objetos (el diccionario queda legible).
 *  2. completarFirma(): localiza el relleno, calcula /ByteRange, arma el CMS separado
 *     (SHA-256, con el atributo signing-certificate-v2 y sin signing-time, como pide
 *     PAdES) y lo escribe en /Contents.
 *
 * El núcleo arma el CMS él mismo; la plataforma solo aporta un @ref Firmante que
 * entrega los certificados y firma unos bytes con la clave privada:
 *  - escritorio: archivo .p12/.pfx leído con GnuTLS (firmante_gnutls.cpp);
 *  - Android: archivo .p12/.pfx o certificado instalado, con las clases de Java del
 *    sistema (sin bibliotecas añadidas).
 *
 * Limitaciones: una sola firma por documento (QPDF reescribe el archivo entero: una
 * segunda firma invalidaría la primera), sin sello de tiempo, y no se firman
 * documentos cifrados.
 */
#pragma once

#include "ibanascapdfcore/dibujo.h"
#include "ibanascapdfcore/imagen.h"

#include <cstddef>
#include <ctime>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace ibz {

class Documento;

/// Tipo de clave del certificado.
enum class AlgoritmoClave { Rsa, Ecdsa };

/// Datos del certificado que usa el núcleo (leídos del DER, sin bibliotecas).
struct InfoCertificado {
    std::string nombre;          ///< CN del titular en UTF-8 (vacío si no tiene)
    std::time_t desde = 0;       ///< inicio de la vigencia (UTC)
    std::time_t hasta = 0;       ///< fin de la vigencia (UTC)
    AlgoritmoClave algoritmo = AlgoritmoClave::Rsa;
};

/**
 * @brief Lee titular, vigencia y tipo de clave de un certificado X.509 (DER).
 * @throws ibz::Error("error.certificado") si no es válido, ("error.certificado_algoritmo")
 *         si la clave no es RSA ni ECDSA.
 */
InfoCertificado leerCertificado(const std::vector<unsigned char>& der);

/**
 * @brief Comprueba que el certificado esté vigente ahora.
 * @throws ibz::Error("error.certificado_caducado") o ("error.certificado_no_vigente").
 */
void comprobarVigencia(const InfoCertificado& info);

/// Clave privada y certificados del firmante; lo implementa cada plataforma.
class Firmante {
public:
    virtual ~Firmante() = default;
    /// Certificados en DER: el primero es el del firmante; después, los intermedios.
    virtual const std::vector<std::vector<unsigned char>>& certificados() const = 0;
    /**
     * @brief Firma @p datos con SHA-256 y la clave privada.
     * @return RSA: firma PKCS#1 v1.5. ECDSA: Ecdsa-Sig-Value en DER.
     */
    virtual std::vector<unsigned char> firmar(const unsigned char* datos, std::size_t n) const = 0;
};

/**
 * @brief Crea un firmante a partir de una referencia y una contraseña.
 *
 * Escritorio: la referencia es la ruta UTF-8 de un .p12/.pfx. Android: ruta de un
 * .p12/.pfx o "keychain:<alias>" para un certificado instalado en el sistema.
 */
using FabricaFirmante =
    std::function<std::unique_ptr<Firmante>(const std::string& referencia, const std::string& contrasena)>;

/// Instala la fábrica de la plataforma (el escritorio la instala sola si se compila con GnuTLS).
void establecerFabricaFirmante(FabricaFirmante fabrica);

/**
 * @brief Firmante para @p referencia con la fábrica instalada.
 * @throws ibz::Error("error.firma_no_disponible") si no hay fábrica; los errores
 *         de la fábrica ("error.certificado", "error.contrasena_certificado", …).
 */
std::unique_ptr<Firmante> crearFirmante(const std::string& referencia, const std::string& contrasena);

/**
 * @brief CMS/PKCS#7 SignedData separado (DER) de @p datos, con SHA-256 y los
 *        atributos firmados de PAdES-B-B (contentType, messageDigest, signing-certificate-v2).
 */
std::vector<unsigned char> cmsFirmado(const Firmante& firmante, const unsigned char* datos, std::size_t n);

/// Aspecto de la firma en la página.
enum class AspectoFirma { Invisible, Texto, Imagen, Dibujo };

/// Opciones de la firma.
struct OpcionesFirma {
    AspectoFirma aspecto = AspectoFirma::Texto;
    int pagina = 0;        ///< base 0 (solo si es visible)
    double x = 0, y = 0;   ///< esquina superior izquierda tal como se ve (puntos PDF)
    double anchoPt = 170;  ///< ancho del recuadro visible
    ImagenRaster imagen;   ///< para AspectoFirma::Imagen
    Dibujo dibujo;         ///< para AspectoFirma::Dibujo
    double r = 0.12, g = 0.23, b = 0.58; ///< color del dibujo (0–1)
};

/// Bytes reservados para la firma CMS (el hexadecimal ocupa el doble).
constexpr std::size_t kBytesFirma = 16384;

/**
 * @brief Firma el documento (paso 1 + paso 2). Se puede deshacer.
 * @return Nombre del firmante (CN del certificado).
 * @throws ibz::Error("error.ya_firmado"), ("error.firma_cifrado"), ("error.qpdf"), …
 */
std::string firmarDocumento(Documento& doc, const Firmante& firmante, const OpcionesFirma& opciones);

/**
 * @brief Paso 2: completa la firma de un PDF preparado (con /ByteRange y /Contents de relleno).
 * @throws ibz::Error("error.firma") si no encuentra el relleno o la firma no cabe.
 */
void completarFirma(std::vector<unsigned char>& pdf, const Firmante& firmante);

} // namespace ibz
