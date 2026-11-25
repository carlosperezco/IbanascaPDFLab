/**
 * @file firma_cms.cpp
 * @brief Firma CMS (PAdES-B-B) propia: lectura del certificado, atributos firmados,
 *        SignedData y relleno del PDF.
 *
 * No depende de QPDF ni de ninguna biblioteca criptográfica: la clave privada la
 * maneja el @ref ibz::Firmante de cada plataforma.
 */
#include "ibanascapdfcore/firma.h"

#include "ibanascapdfcore/error.h"

#include "der.h"
#include "sha256.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>

namespace ibz {

using interno::concat;
using interno::conjunto;
using interno::Der;
using interno::entero;
using interno::oid;
using interno::Tlv;
using interno::tlv;

namespace {

// OID usados.
constexpr std::string_view kOidData = "1.2.840.113549.1.7.1";
constexpr std::string_view kOidSignedData = "1.2.840.113549.1.7.2";
constexpr std::string_view kOidContentType = "1.2.840.113549.1.9.3";
constexpr std::string_view kOidMessageDigest = "1.2.840.113549.1.9.4";
constexpr std::string_view kOidSigningCertV2 = "1.2.840.113549.1.9.16.2.47";
constexpr std::string_view kOidSha256 = "2.16.840.1.101.3.4.2.1";
constexpr std::string_view kOidRsa = "1.2.840.113549.1.1.1";
constexpr std::string_view kOidEcPublica = "1.2.840.10045.2.1";
constexpr std::string_view kOidEcdsaSha256 = "1.2.840.10045.4.3.2";

/// Partes del certificado que hacen falta para la firma.
struct PartesCertificado {
    Der serie;   ///< INTEGER completo
    Der emisor;  ///< Name completo
    Tlv validez{};
    Tlv sujeto{};
    Tlv clavePublica{};
};

/// Recorre tbsCertificate: [0] versión, serie, algoritmo, emisor, validez, sujeto, clave pública.
PartesCertificado partes(const Der& c) {
    Tlv cert;
    Tlv tbs;
    if (!interno::leerTlv(c, 0, c.size(), cert) || cert.etiqueta != 0x30 ||
        !interno::leerTlv(c, cert.contenido, cert.fin(), tbs) || tbs.etiqueta != 0x30)
        throw Error("error.certificado");
    std::size_t pos = tbs.contenido;
    auto siguiente = [&](unsigned char esperada) {
        Tlv t;
        if (!interno::leerTlv(c, pos, tbs.fin(), t)) throw Error("error.certificado");
        if (esperada && t.etiqueta != esperada) throw Error("error.certificado");
        pos = t.fin();
        return t;
    };
    PartesCertificado p;
    Tlv t = siguiente(0);
    if (t.etiqueta == 0xA0) t = siguiente(0); // versión
    if (t.etiqueta != 0x02) throw Error("error.certificado");
    p.serie.assign(c.begin() + static_cast<long>(t.inicio), c.begin() + static_cast<long>(t.fin()));
    siguiente(0x30); // algoritmo de la firma del emisor
    t = siguiente(0x30);
    p.emisor.assign(c.begin() + static_cast<long>(t.inicio), c.begin() + static_cast<long>(t.fin()));
    p.validez = siguiente(0x30);
    p.sujeto = siguiente(0x30);
    p.clavePublica = siguiente(0x30);
    return p;
}

/// Días desde 1970-01-01 (algoritmo de H. Hinnant, calendario gregoriano proléptico).
long long diasCiviles(long long a, unsigned m, unsigned d) {
    a -= m <= 2;
    const long long era = (a >= 0 ? a : a - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(a - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + static_cast<long long>(doe) - 719468;
}

/// UTCTime (AAMMDDhhmm[ss]Z) o GeneralizedTime (AAAAMMDDhhmm[ss][.f]Z) → time_t UTC.
std::time_t leerFecha(const Der& c, const Tlv& t) {
    const std::string s(c.begin() + static_cast<long>(t.contenido), c.begin() + static_cast<long>(t.fin()));
    auto num = [&](std::size_t i, std::size_t n) {
        int v = 0;
        for (std::size_t k = 0; k < n; ++k) {
            if (i + k >= s.size() || s[i + k] < '0' || s[i + k] > '9') throw Error("error.certificado");
            v = v * 10 + (s[i + k] - '0');
        }
        return v;
    };
    std::size_t i = 0;
    long long anio = 0;
    if (t.etiqueta == 0x17) {
        const int aa = num(0, 2);
        anio = aa >= 50 ? 1900 + aa : 2000 + aa;
        i = 2;
    } else if (t.etiqueta == 0x18) {
        anio = num(0, 4);
        i = 4;
    } else {
        throw Error("error.certificado");
    }
    const int mes = num(i, 2);
    const int dia = num(i + 2, 2);
    const int hora = num(i + 4, 2);
    const int minuto = num(i + 6, 2);
    const int segundo = (i + 10 <= s.size() && s[i + 8] >= '0' && s[i + 8] <= '9') ? num(i + 8, 2) : 0;
    if (mes < 1 || mes > 12 || dia < 1 || dia > 31) throw Error("error.certificado");
    const long long dias = diasCiviles(anio, static_cast<unsigned>(mes), static_cast<unsigned>(dia));
    return static_cast<std::time_t>(dias * 86400 + hora * 3600 + minuto * 60 + segundo);
}

/// Añade a @p r el carácter Unicode @p cp en UTF-8.
void ponerUtf8(std::string& r, std::uint32_t cp) {
    if (cp < 0x80) {
        r += static_cast<char>(cp);
    } else if (cp < 0x800) {
        r += static_cast<char>(0xC0 | (cp >> 6));
        r += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        r += static_cast<char>(0xE0 | (cp >> 12));
        r += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        r += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        r += static_cast<char>(0xF0 | (cp >> 18));
        r += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        r += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        r += static_cast<char>(0x80 | (cp & 0x3F));
    }
}

/// DirectoryString (UTF8, Printable, IA5, Teletex, BMP, Universal) → UTF-8.
std::string cadenaDirectorio(const Der& c, const Tlv& t) {
    const unsigned char* p = c.data() + t.contenido;
    const std::size_t n = t.largo;
    std::string r;
    switch (t.etiqueta) {
    case 0x0C: // UTF8String
    case 0x13: // PrintableString
    case 0x16: // IA5String
        r.assign(reinterpret_cast<const char*>(p), n);
        break;
    case 0x14: // TeletexString: se trata como Latin-1
        for (std::size_t i = 0; i < n; ++i) ponerUtf8(r, p[i]);
        break;
    case 0x1E: // BMPString: UTF-16BE
        for (std::size_t i = 0; i + 1 < n; i += 2) {
            std::uint32_t u = (std::uint32_t(p[i]) << 8) | p[i + 1];
            if (u >= 0xD800 && u < 0xDC00 && i + 3 < n) {
                const std::uint32_t v = (std::uint32_t(p[i + 2]) << 8) | p[i + 3];
                u = 0x10000 + ((u - 0xD800) << 10) + (v - 0xDC00);
                i += 2;
            }
            ponerUtf8(r, u);
        }
        break;
    case 0x1C: // UniversalString: UTF-32BE
        for (std::size_t i = 0; i + 3 < n; i += 4)
            ponerUtf8(r, (std::uint32_t(p[i]) << 24) | (std::uint32_t(p[i + 1]) << 16) |
                             (std::uint32_t(p[i + 2]) << 8) | p[i + 3]);
        break;
    default:
        break;
    }
    return r;
}

/// Contenido de un OBJECT IDENTIFIER leído, comparable con oid().
bool esOid(const Der& c, const Tlv& t, std::string_view puntos) {
    const Der esperado = oid(puntos);
    return t.etiqueta == 0x06 && t.fin() - t.inicio == esperado.size() &&
           std::equal(esperado.begin(), esperado.end(), c.begin() + static_cast<long>(t.inicio));
}

/**
 * @brief Atributo ESS signing-certificate-v2 (RFC 5035), obligatorio en PAdES-B-B.
 *
 * SigningCertificateV2 ::= SEQUENCE { certs SEQUENCE OF ESSCertIDv2 }
 * ESSCertIDv2 ::= SEQUENCE { certHash OCTET STRING  -- SHA-256 (algoritmo por defecto)
 *                            issuerSerial IssuerSerial }
 * IssuerSerial ::= SEQUENCE { issuer GeneralNames, serialNumber INTEGER }
 */
Der signingCertificateV2(const Der& cert, const PartesCertificado& p) {
    const auto hash = interno::Sha256::de(cert.data(), cert.size());
    const Der generalNames = tlv(0x30, tlv(0xA4, p.emisor)); // directoryName [4] EXPLICIT Name
    const Der issuerSerial = tlv(0x30, concat({generalNames, p.serie}));
    const Der essCertIdV2 = tlv(0x30, concat({tlv(0x04, Der(hash.begin(), hash.end())), issuerSerial}));
    return tlv(0x30, tlv(0x30, essCertIdV2));
}

/// Atributo CMS: SEQUENCE { tipo OID, valores SET OF }.
Der atributo(std::string_view tipo, const Der& valor) {
    return tlv(0x30, concat({oid(tipo), conjunto({valor})}));
}

FabricaFirmante& fabricaGlobal() {
    static FabricaFirmante f;
    return f;
}

} // namespace

// ------------------------------------------------------------ certificado

InfoCertificado leerCertificado(const std::vector<unsigned char>& der) {
    const PartesCertificado p = partes(der);
    InfoCertificado info;

    // Validez: SEQUENCE { notBefore Time, notAfter Time }.
    Tlv a;
    Tlv b;
    if (!interno::leerTlv(der, p.validez.contenido, p.validez.fin(), a) ||
        !interno::leerTlv(der, a.fin(), p.validez.fin(), b))
        throw Error("error.certificado");
    info.desde = leerFecha(der, a);
    info.hasta = leerFecha(der, b);

    // Sujeto: SEQUENCE OF SET OF SEQUENCE { tipo OID, valor }; se busca el CN (2.5.4.3).
    for (std::size_t pos = p.sujeto.contenido; pos < p.sujeto.fin();) {
        Tlv rdn;
        if (!interno::leerTlv(der, pos, p.sujeto.fin(), rdn)) throw Error("error.certificado");
        for (std::size_t q = rdn.contenido; q < rdn.fin();) {
            Tlv atv;
            Tlv tipo;
            Tlv valor;
            if (!interno::leerTlv(der, q, rdn.fin(), atv) || !interno::leerTlv(der, atv.contenido, atv.fin(), tipo) ||
                !interno::leerTlv(der, tipo.fin(), atv.fin(), valor))
                throw Error("error.certificado");
            if (info.nombre.empty() && esOid(der, tipo, "2.5.4.3")) info.nombre = cadenaDirectorio(der, valor);
            q = atv.fin();
        }
        pos = rdn.fin();
    }

    // Clave pública: SEQUENCE { algoritmo SEQUENCE { OID, … }, BIT STRING }.
    Tlv alg;
    Tlv oidClave;
    if (!interno::leerTlv(der, p.clavePublica.contenido, p.clavePublica.fin(), alg) ||
        !interno::leerTlv(der, alg.contenido, alg.fin(), oidClave))
        throw Error("error.certificado");
    if (esOid(der, oidClave, kOidRsa)) info.algoritmo = AlgoritmoClave::Rsa;
    else if (esOid(der, oidClave, kOidEcPublica)) info.algoritmo = AlgoritmoClave::Ecdsa;
    else throw Error("error.certificado_algoritmo");
    return info;
}

void comprobarVigencia(const InfoCertificado& info) {
    const std::time_t ahora = std::time(nullptr);
    auto fecha = [](std::time_t t) {
        std::tm l{};
#ifdef _WIN32
        localtime_s(&l, &t);
#else
        localtime_r(&t, &l);
#endif
        char buf[16];
        std::strftime(buf, sizeof(buf), "%d/%m/%Y", &l);
        return std::string(buf);
    };
    if (ahora > info.hasta) throw Error("error.certificado_caducado", fecha(info.hasta));
    if (ahora < info.desde) throw Error("error.certificado_no_vigente", fecha(info.desde));
}

// ------------------------------------------------------------ fábrica

void establecerFabricaFirmante(FabricaFirmante fabrica) {
    fabricaGlobal() = std::move(fabrica);
}

std::unique_ptr<Firmante> crearFirmante(const std::string& referencia, const std::string& contrasena) {
    if (!fabricaGlobal()) throw Error("error.firma_no_disponible");
    std::unique_ptr<Firmante> f = fabricaGlobal()(referencia, contrasena);
    if (!f || f->certificados().empty()) throw Error("error.certificado");
    return f;
}

// ------------------------------------------------------------ CMS

std::vector<unsigned char> cmsFirmado(const Firmante& firmante, const unsigned char* datos, std::size_t n) {
    const auto& certs = firmante.certificados();
    if (certs.empty()) throw Error("error.certificado");
    const Der& cert = certs.front();
    const PartesCertificado p = partes(cert);
    const InfoCertificado info = leerCertificado(cert);

    const Der algSha256 = tlv(0x30, oid(kOidSha256));
    const auto resumen = interno::Sha256::de(datos, n);

    // Atributos firmados (SET OF en DER, ordenado). Sin signing-time: PAdES lo prohíbe.
    const std::vector<Der> atributos = {
        atributo(kOidContentType, oid(kOidData)),
        atributo(kOidMessageDigest, tlv(0x04, Der(resumen.begin(), resumen.end()))),
        atributo(kOidSigningCertV2, signingCertificateV2(cert, p)),
    };
    const Der firmados = conjunto(atributos);             // se firma con la etiqueta SET (0x31)
    Der firmadosImplicitos = firmados;                     // y se guarda como [0] IMPLICIT
    firmadosImplicitos[0] = 0xA0;

    const std::vector<unsigned char> firma = firmante.firmar(firmados.data(), firmados.size());
    if (firma.empty()) throw Error("error.firma");

    const Der algFirma = info.algoritmo == AlgoritmoClave::Rsa ? tlv(0x30, concat({oid(kOidRsa), Der{0x05, 0x00}}))
                                                                : tlv(0x30, oid(kOidEcdsaSha256));
    const Der signerInfo = tlv(0x30, concat({
                                         entero(1),
                                         tlv(0x30, concat({p.emisor, p.serie})), // IssuerAndSerialNumber
                                         algSha256,
                                         firmadosImplicitos,
                                         algFirma,
                                         tlv(0x04, firma),
                                     }));

    const Der signedData = tlv(0x30, concat({
                                         entero(1),
                                         conjunto({algSha256}),
                                         tlv(0x30, oid(kOidData)), // contenido separado: sin eContent
                                         conjunto(certs, 0xA0),    // certificates [0] IMPLICIT
                                         conjunto({signerInfo}),
                                     }));
    return tlv(0x30, concat({oid(kOidSignedData), tlv(0xA0, signedData)}));
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
    const std::vector<unsigned char> cms = cmsFirmado(firmante, firmado.data(), firmado.size());
    if (cms.size() > kBytesFirma) throw Error("error.firma", "CMS " + std::to_string(cms.size()) + " bytes");

    static const char kHex[] = "0123456789ABCDEF";
    std::size_t p = a + 1;
    for (const unsigned char c : cms) {
        pdf[p++] = static_cast<unsigned char>(kHex[c >> 4]);
        pdf[p++] = static_cast<unsigned char>(kHex[c & 0x0F]);
    }
}

} // namespace ibz
