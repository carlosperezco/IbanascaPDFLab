/**
 * @file firmante_gnutls.cpp
 * @brief Firmante de escritorio: certificado .p12/.pfx leído con GnuTLS.
 *
 * Solo se compila en escritorio (opción IBZ_FIRMA_GNUTLS). GnuTLS ya es dependencia
 * de QPDF en MSYS2 y se enlaza dinámicamente (LGPL). Android no usa este archivo:
 * allí el firmante lo da la plataforma con las clases de Java del sistema.
 */
#include "ibanascapdfcore/firma.h"

#include "ibanascapdfcore/error.h"
#include "ibanascapdfcore/rutas.h"

#include <gnutls/abstract.h>
#include <gnutls/gnutls.h>
#include <gnutls/pkcs12.h>
#include <gnutls/x509.h>

#include <fstream>
#include <iterator>
#include <string>

namespace ibz {

namespace {

std::string errorGnutls(int r) {
    const char* m = gnutls_strerror(r);
    return m ? m : std::to_string(r);
}

class FirmanteGnutls final : public Firmante {
public:
    FirmanteGnutls(const std::string& ruta, const std::string& contrasena) {
        const std::filesystem::path p12 = desdeUtf8(ruta);
        std::ifstream f(p12, std::ios::binary);
        if (!f) throw Error("error.leer", ruta);
        std::string datos((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        if (datos.empty()) throw Error("error.certificado", ruta);

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
        gnutls_x509_crt_t* cadena = nullptr;
        unsigned nCadena = 0;
        const bool macOk = gnutls_pkcs12_verify_mac(pk, contrasena.c_str()) >= 0 ||
                           (contrasena.empty() && gnutls_pkcs12_verify_mac(pk, nullptr) >= 0);
        r = -1;
        for (const char* c : claves) {
            if (!c && !contrasena.empty()) break;
            r = gnutls_pkcs12_simple_parse(pk, c, &clavePriv, &cadena, &nCadena, nullptr, nullptr, nullptr,
                                           GNUTLS_PKCS12_SP_INCLUDE_SELF_SIGNED);
            if (r >= 0) break;
        }
        gnutls_pkcs12_deinit(pk);

        // Certificados en DER (el primero, el del firmante).
        for (unsigned i = 0; r >= 0 && i < nCadena; ++i) {
            std::size_t tam = 0;
            gnutls_x509_crt_export(cadena[i], GNUTLS_X509_FMT_DER, nullptr, &tam);
            std::vector<unsigned char> der(tam);
            if (gnutls_x509_crt_export(cadena[i], GNUTLS_X509_FMT_DER, der.data(), &tam) >= 0) {
                der.resize(tam);
                certs_.push_back(std::move(der));
            }
        }
        for (unsigned i = 0; i < nCadena; ++i) gnutls_x509_crt_deinit(cadena[i]);
        if (cadena) gnutls_free(cadena);

        if (r < 0 || !clavePriv || certs_.empty()) {
            if (clavePriv) gnutls_x509_privkey_deinit(clavePriv);
            throw Error(macOk ? "error.certificado" : "error.contrasena_certificado", errorGnutls(r));
        }
        if (gnutls_privkey_init(&clave_) < 0 ||
            gnutls_privkey_import_x509(clave_, clavePriv, GNUTLS_PRIVKEY_IMPORT_AUTO_RELEASE) < 0) {
            gnutls_x509_privkey_deinit(clavePriv);
            throw Error("error.certificado");
        }
    }

    ~FirmanteGnutls() override {
        if (clave_) gnutls_privkey_deinit(clave_);
    }
    FirmanteGnutls(const FirmanteGnutls&) = delete;
    FirmanteGnutls& operator=(const FirmanteGnutls&) = delete;

    const std::vector<std::vector<unsigned char>>& certificados() const override { return certs_; }

    std::vector<unsigned char> firmar(const unsigned char* datos, std::size_t n) const override {
        gnutls_datum_t entrada{const_cast<unsigned char*>(datos), static_cast<unsigned>(n)};
        gnutls_datum_t firma{nullptr, 0};
        const int r = gnutls_privkey_sign_data(clave_, GNUTLS_DIG_SHA256, 0, &entrada, &firma);
        if (r < 0) throw Error("error.firma", errorGnutls(r));
        std::vector<unsigned char> salida(firma.data, firma.data + firma.size);
        gnutls_free(firma.data);
        return salida;
    }

private:
    gnutls_privkey_t clave_ = nullptr;
    std::vector<std::vector<unsigned char>> certs_;
};

/// Se instala sola al cargar el programa (biblioteca de objetos, como los módulos).
[[maybe_unused]] const bool ibzFabricaGnutls = [] {
    establecerFabricaFirmante([](const std::string& referencia, const std::string& contrasena) {
        return std::make_unique<FirmanteGnutls>(referencia, contrasena);
    });
    return true;
}();

} // namespace

} // namespace ibz
