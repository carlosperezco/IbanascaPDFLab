/**
 * @file firma_cms.cpp
 * @brief Firma CMS (PAdES-B-B) con GnuTLS: certificado .p12/.pfx, atributos y relleno del PDF.
 *
 * No depende de QPDF: recibe un PDF ya preparado (ver firma.cpp).
 */
#include "ibanascapdfcore/firma.h"

#include "ibanascapdfcore/error.h"
#include "ibanascapdfcore/rutas.h"

#include <gnutls/abstract.h>
#include <gnutls/crypto.h>
#include <gnutls/gnutls.h>
#include <gnutls/pkcs12.h>
#include <gnutls/pkcs7.h>
#include <gnutls/x509.h>

#include <algorithm>
#include <ctime>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>

namespace fs = std::filesystem;

namespace ibz {

namespace {

// ------------------------------------------------------------ DER mínimo

using Der = std::vector<unsigned char>;

/// TLV con la longitud codificada en DER.
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

/// Lee la cabecera de un TLV en @p pos: devuelve la posición del contenido y su longitud.
bool leerTlv(const Der& d, std::size_t pos, unsigned char& etiqueta, std::size_t& inicio, std::size_t& largo) {
    if (pos + 2 > d.size()) return false;
    etiqueta = d[pos];
    std::size_t n = d[pos + 1];
    std::size_t p = pos + 2;
    if (n & 0x80) {
        const std::size_t k = n & 0x7F;
        if (k == 0 || k > sizeof(std::size_t) || p + k > d.size()) return false;
        n = 0;
        for (std::size_t i = 0; i < k; ++i) n = (n << 8) | d[p + i];
        p += k;
    }
    if (p + n > d.size()) return false;
    inicio = p;
    largo = n;
    return true;
}

/**
 * @brief Extrae del certificado (DER) el número de serie y el emisor, como TLV completos.
 *
 * Certificate ::= SEQUENCE { tbsCertificate SEQUENCE { [0] version OPCIONAL,
 *                 serialNumber INTEGER, signature AlgorithmIdentifier, issuer Name, … } … }
 */
void serieYEmisor(const Der& cert, Der& serie, Der& emisor) {
    unsigned char t = 0;
    std::size_t ini = 0;
    std::size_t n = 0;
    if (!leerTlv(cert, 0, t, ini, n) || t != 0x30) throw Error("error.certificado");
    std::size_t tbsIni = 0;
    std::size_t tbsN = 0;
    if (!leerTlv(cert, ini, t, tbsIni, tbsN) || t != 0x30) throw Error("error.certificado");
    std::size_t pos = tbsIni;
    auto siguiente = [&](unsigned char& et, std::size_t& a, std::size_t& b) {
        std::size_t ci = 0;
        std::size_t cn = 0;
        if (!leerTlv(cert, pos, et, ci, cn)) throw Error("error.certificado");
        a = pos;
        b = ci + cn;
        pos = b;
    };
    std::size_t a = 0;
    std::size_t b = 0;
    siguiente(t, a, b);
    if (t == 0xA0) siguiente(t, a, b); // versión
    if (t != 0x02) throw Error("error.certificado");
    serie.assign(cert.begin() + static_cast<long>(a), cert.begin() + static_cast<long>(b));
    siguiente(t, a, b); // algoritmo de firma
    siguiente(t, a, b); // emisor
    if (t != 0x30) throw Error("error.certificado");
    emisor.assign(cert.begin() + static_cast<long>(a), cert.begin() + static_cast<long>(b));
}

/**
 * @brief Atributo ESS signing-certificate-v2 (RFC 5035), obligatorio en PAdES-B-B.
 *
 * SigningCertificateV2 ::= SEQUENCE { certs SEQUENCE OF ESSCertIDv2 }
 * ESSCertIDv2 ::= SEQUENCE { certHash OCTET STRING  -- SHA-256 (algoritmo por defecto)
 *                            issuerSerial IssuerSerial }
 * IssuerSerial ::= SEQUENCE { issuer GeneralNames, serialNumber INTEGER }
 */
Der signingCertificateV2(const Der& cert) {
    Der hash(32);
    if (gnutls_hash_fast(GNUTLS_DIG_SHA256, cert.data(), cert.size(), hash.data()) < 0) throw Error("error.firma");
    Der serie;
    Der emisor;
    serieYEmisor(cert, serie, emisor);
    const Der generalNames = tlv(0x30, tlv(0xA4, emisor)); // directoryName [4] EXPLICIT Name
    const Der issuerSerial = tlv(0x30, concat({generalNames, serie}));
    const Der essCertIdV2 = tlv(0x30, concat({tlv(0x04, hash), issuerSerial}));
    return tlv(0x30, tlv(0x30, essCertIdV2));
}

std::string errorGnutls(int r) {
    const char* m = gnutls_strerror(r);
    return m ? m : std::to_string(r);
}

// ------------------------------------------------------------ fechas

std::string fechaCorta(std::time_t t) {
    std::tm l{};
#ifdef _WIN32
    localtime_s(&l, &t);
#else
    localtime_r(&t, &l);
#endif
    char buf[16];
    std::strftime(buf, sizeof(buf), "%d/%m/%Y", &l);
    return buf;
}

} // namespace

// ------------------------------------------------------------ Firmante

struct Firmante::Impl {
    gnutls_x509_crt_t* cadena = nullptr;
    unsigned nCadena = 0;
    gnutls_privkey_t clave = nullptr;
    std::string nombre;
    Der certDer;

    ~Impl() {
        if (clave) gnutls_privkey_deinit(clave);
        for (unsigned i = 0; i < nCadena; ++i) gnutls_x509_crt_deinit(cadena[i]);
        if (cadena) gnutls_free(cadena);
    }
};

Firmante::Firmante(const fs::path& p12, const std::string& contrasena) : d_(std::make_unique<Impl>()) {
    const std::string nombreArchivo = utf8(p12);
    std::ifstream f(p12, std::ios::binary);
    if (!f) throw Error("error.leer", nombreArchivo);
    std::string datos((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (datos.empty()) throw Error("error.certificado", nombreArchivo);

    gnutls_global_init();
    gnutls_pkcs12_t pk = nullptr;
    if (gnutls_pkcs12_init(&pk) < 0) throw Error("error.certificado");
    gnutls_datum_t d{reinterpret_cast<unsigned char*>(datos.data()), static_cast<unsigned>(datos.size())};
    int r = gnutls_pkcs12_import(pk, &d, GNUTLS_X509_FMT_DER, 0);
    if (r < 0) {
        gnutls_pkcs12_deinit(pk);
        throw Error("error.certificado", errorGnutls(r));
    }

    // Contraseña vacía: se prueba sin contraseña y con la cadena vacía.
    const char* claves[2] = {contrasena.c_str(), nullptr};
    gnutls_x509_privkey_t clavePriv = nullptr;
    const bool macOk = gnutls_pkcs12_verify_mac(pk, contrasena.c_str()) >= 0 ||
                       (contrasena.empty() && gnutls_pkcs12_verify_mac(pk, nullptr) >= 0);
    r = -1;
    for (const char* c : claves) {
        if (!c && !contrasena.empty()) break;
        r = gnutls_pkcs12_simple_parse(pk, c, &clavePriv, &d_->cadena, &d_->nCadena, nullptr, nullptr, nullptr,
                                       GNUTLS_PKCS12_SP_INCLUDE_SELF_SIGNED);
        if (r >= 0) break;
    }
    gnutls_pkcs12_deinit(pk);
    if (r < 0 || !clavePriv || d_->nCadena == 0) {
        if (clavePriv) gnutls_x509_privkey_deinit(clavePriv);
        throw Error(macOk ? "error.certificado" : "error.contrasena_certificado", errorGnutls(r));
    }

    if (gnutls_privkey_init(&d_->clave) < 0 ||
        gnutls_privkey_import_x509(d_->clave, clavePriv, GNUTLS_PRIVKEY_IMPORT_AUTO_RELEASE) < 0) {
        gnutls_x509_privkey_deinit(clavePriv);
        throw Error("error.certificado");
    }

    gnutls_x509_crt_t cert = d_->cadena[0];
    char cn[512] = {};
    std::size_t tam = sizeof(cn);
    if (gnutls_x509_crt_get_dn_by_oid(cert, GNUTLS_OID_X520_COMMON_NAME, 0, 0, cn, &tam) >= 0) d_->nombre = cn;
    if (d_->nombre.empty()) d_->nombre = utf8(p12.stem());

    const std::time_t ahora = std::time(nullptr);
    const std::time_t caduca = gnutls_x509_crt_get_expiration_time(cert);
    const std::time_t desde = gnutls_x509_crt_get_activation_time(cert);
    if (caduca != static_cast<std::time_t>(-1) && ahora > caduca)
        throw Error("error.certificado_caducado", fechaCorta(caduca));
    if (desde != static_cast<std::time_t>(-1) && ahora < desde)
        throw Error("error.certificado_no_vigente", fechaCorta(desde));

    tam = 0;
    gnutls_x509_crt_export(cert, GNUTLS_X509_FMT_DER, nullptr, &tam);
    d_->certDer.resize(tam);
    if (gnutls_x509_crt_export(cert, GNUTLS_X509_FMT_DER, d_->certDer.data(), &tam) < 0) throw Error("error.certificado");
    d_->certDer.resize(tam);
}

Firmante::~Firmante() = default;

const std::string& Firmante::nombre() const noexcept {
    return d_->nombre;
}

std::vector<unsigned char> Firmante::firmarCms(const unsigned char* datos, std::size_t n) const {
    Der atributo = signingCertificateV2(d_->certDer);
    gnutls_pkcs7_attrs_t firmados = nullptr;
    gnutls_datum_t valor{atributo.data(), static_cast<unsigned>(atributo.size())};
    int r = gnutls_pkcs7_add_attr(&firmados, "1.2.840.113549.1.9.16.2.47", &valor, 0);
    if (r < 0) throw Error("error.firma", errorGnutls(r));

    gnutls_pkcs7_t p7 = nullptr;
    if (gnutls_pkcs7_init(&p7) < 0) {
        gnutls_pkcs7_attrs_deinit(firmados);
        throw Error("error.firma");
    }
    gnutls_datum_t contenido{const_cast<unsigned char*>(datos), static_cast<unsigned>(n)};
    // Separada (sin EMBED_DATA), con el certificado del firmante y sin signing-time (PAdES).
    r = gnutls_pkcs7_sign(p7, d_->cadena[0], d_->clave, &contenido, firmados, nullptr, GNUTLS_DIG_SHA256,
                          GNUTLS_PKCS7_INCLUDE_CERT);
    gnutls_pkcs7_attrs_deinit(firmados);
    for (unsigned i = 1; r >= 0 && i < d_->nCadena; ++i) r = gnutls_pkcs7_set_crt(p7, d_->cadena[i]);

    std::vector<unsigned char> salida;
    if (r >= 0) {
        std::size_t tam = 0;
        gnutls_pkcs7_export(p7, GNUTLS_X509_FMT_DER, nullptr, &tam);
        salida.resize(tam);
        r = gnutls_pkcs7_export(p7, GNUTLS_X509_FMT_DER, salida.data(), &tam);
        salida.resize(tam);
    }
    gnutls_pkcs7_deinit(p7);
    if (r < 0) throw Error("error.firma", errorGnutls(r));
    return salida;
}

// ------------------------------------------------------------ PDF

void completarFirma(std::vector<unsigned char>& pdf, const Firmante& firmante) {
    // Relleno de /Contents: "<" + 2·kBytesFirma ceros + ">".
    const std::string relleno = "<" + std::string(2 * kBytesFirma, '0') + ">";
    const std::string_view texto(reinterpret_cast<const char*>(pdf.data()), pdf.size());
    const std::size_t a = texto.find(relleno);
    if (a == std::string_view::npos || texto.find(relleno, a + 1) != std::string_view::npos)
        throw Error("error.firma");
    const std::size_t b = a + relleno.size();

    // /ByteRange del mismo diccionario: el último antes del relleno (QPDF ordena las claves).
    const std::size_t br = texto.rfind("/ByteRange", a);
    if (br == std::string_view::npos || a - br > 4096) throw Error("error.firma");
    const std::size_t ini = texto.find('[', br);
    const std::size_t fin = texto.find(']', ini);
    if (ini == std::string_view::npos || fin == std::string_view::npos || fin > a) throw Error("error.firma");

    std::string rango = std::to_string(0) + " " + std::to_string(a) + " " + std::to_string(b) + " " +
                        std::to_string(pdf.size() - b);
    const std::size_t hueco = fin - ini - 1;
    if (rango.size() + 2 > hueco) throw Error("error.firma");
    rango = " " + rango;
    rango.resize(hueco, ' ');
    std::copy(rango.begin(), rango.end(), pdf.begin() + static_cast<long>(ini + 1));

    // Bytes firmados: todo el archivo salvo el hueco de /Contents.
    std::vector<unsigned char> firmado;
    firmado.reserve(pdf.size() - (b - a));
    firmado.insert(firmado.end(), pdf.begin(), pdf.begin() + static_cast<long>(a));
    firmado.insert(firmado.end(), pdf.begin() + static_cast<long>(b), pdf.end());
    const std::vector<unsigned char> cms = firmante.firmarCms(firmado.data(), firmado.size());
    if (cms.size() > kBytesFirma) throw Error("error.firma", "CMS " + std::to_string(cms.size()) + " bytes");

    static const char kHex[] = "0123456789ABCDEF";
    std::size_t p = a + 1;
    for (const unsigned char c : cms) {
        pdf[p++] = static_cast<unsigned char>(kHex[c >> 4]);
        pdf[p++] = static_cast<unsigned char>(kHex[c & 0x0F]);
    }
}

} // namespace ibz
