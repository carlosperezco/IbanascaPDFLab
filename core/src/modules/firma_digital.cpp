/**
 * @file firma_digital.cpp
 * @brief Módulo: firma digital PAdES-B-B con certificado .p12/.pfx.
 *
 * Primero el diálogo (certificado, contraseña, aspecto); si la firma es visible,
 * después se pide el clic en la página.
 */
#include "ibanascapdfcore/documento.h"
#include "ibanascapdfcore/error.h"
#include "ibanascapdfcore/firma.h"
#include "ibanascapdfcore/idioma.h"
#include "ibanascapdfcore/imagen.h"
#include "ibanascapdfcore/modulo.h"
#include "ibanascapdfcore/rutas.h"

#include <string>

namespace {

class FirmaDigital final : public ibz::Modulo {
public:
    const char* id() const override { return "firma_digital"; }
    const char* menu() const override { return "firma"; }
    int orden() const override { return 20; }
    bool posicionTrasDialogo(const ibz::Valores& v) const override {
        return ibz::valor(v, "aspecto", "texto") != "invisible";
    }
    std::vector<ibz::Parametro> parametros() const override {
        ibz::Parametro certificado;
        certificado.id = "certificado";
        certificado.clave = "param.certificado";
        certificado.tipo = ibz::TipoParametro::ArchivoCertificado;

        ibz::Parametro contrasena;
        contrasena.id = "contrasena";
        contrasena.clave = "param.contrasena";
        contrasena.tipo = ibz::TipoParametro::Contrasena;

        ibz::Parametro aspecto;
        aspecto.id = "aspecto";
        aspecto.clave = "param.aspecto";
        aspecto.tipo = ibz::TipoParametro::Opcion;
        aspecto.opciones = {"texto", "imagen", "invisible"};
        aspecto.defecto = "texto";

        ibz::Parametro imagen;
        imagen.id = "imagen";
        imagen.clave = "param.imagen_firma";
        imagen.tipo = ibz::TipoParametro::ArchivoImagen;

        ibz::Parametro ancho;
        ancho.id = "ancho_mm";
        ancho.clave = "param.ancho_mm";
        ancho.tipo = ibz::TipoParametro::Entero;
        ancho.defecto = "60";
        ancho.minimo = 20;
        ancho.maximo = 200;
        return {certificado, contrasena, aspecto, imagen, ancho};
    }
    std::string ejecutar(ibz::Documento& doc, const ibz::Valores& v) const override {
        const std::string ruta = ibz::valor(v, "certificado");
        if (ruta.empty()) throw ibz::Error("error.sin_certificado");
        const ibz::Firmante firmante(ibz::desdeUtf8(ruta), ibz::valor(v, "contrasena"));

        ibz::OpcionesFirma o;
        const std::string aspecto = ibz::valor(v, "aspecto", "texto");
        o.aspecto = aspecto == "invisible" ? ibz::AspectoFirma::Invisible
                    : aspecto == "imagen"  ? ibz::AspectoFirma::Imagen
                                           : ibz::AspectoFirma::Texto;
        if (o.aspecto == ibz::AspectoFirma::Imagen) {
            const std::string img = ibz::valor(v, "imagen");
            if (img.empty()) throw ibz::Error("error.sin_archivos");
            o.imagen = ibz::leerPng(ibz::desdeUtf8(img));
            ibz::quitarFondoBlanco(o.imagen); // firma escaneada: trazo sobre fondo transparente
        }
        o.pagina = std::stoi(ibz::valor(v, "pagina", "0"));
        o.x = ibz::valorReal(v, "x", 0.0);
        o.y = ibz::valorReal(v, "y", 0.0);
        o.anchoPt = ibz::valorReal(v, "ancho_mm", 60.0) * 72.0 / 25.4;

        ibz::firmarDocumento(doc, firmante, o);
        return ibz::formato(ibz::tr("mensaje.firmado"), {firmante.nombre()});
    }
};

} // namespace

IBZ_REGISTRAR_MODULO(FirmaDigital);
