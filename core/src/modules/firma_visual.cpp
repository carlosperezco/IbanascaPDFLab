/**
 * @file firma_visual.cpp
 * @brief Módulo: firma visual / manuscrita en el punto pulsado, dibujada a mano
 *        (ratón o dedo, como trazado vectorial) o desde una imagen PNG.
 */
#include "ibanascapdfcore/dibujo.h"
#include "ibanascapdfcore/documento.h"
#include "ibanascapdfcore/error.h"
#include "ibanascapdfcore/imagen.h"
#include "ibanascapdfcore/modulo.h"
#include "ibanascapdfcore/rutas.h"

#include <string>

namespace {

class FirmaVisual final : public ibz::Modulo {
public:
    const char* id() const override { return "firma_visual"; }
    const char* menu() const override { return "firma"; }
    int orden() const override { return 10; }
    bool requierePosicion() const override { return true; }
    std::vector<ibz::Parametro> parametros() const override {
        ibz::Parametro origen;
        origen.id = "origen";
        origen.clave = "param.origen_firma";
        origen.tipo = ibz::TipoParametro::Opcion;
        origen.opciones = {"dibujo", "imagen"};
        origen.defecto = "dibujo";

        ibz::Parametro dibujo;
        dibujo.id = "dibujo";
        dibujo.clave = "param.dibujo";
        dibujo.tipo = ibz::TipoParametro::Dibujo;
        dibujo.visibleSi = "origen=dibujo";

        ibz::Parametro color;
        color.id = "color";
        color.clave = "param.color";
        color.tipo = ibz::TipoParametro::Color;
        color.defecto = "#1f3a93";
        color.visibleSi = "origen=dibujo";

        ibz::Parametro imagen;
        imagen.id = "imagen";
        imagen.clave = "param.imagen";
        imagen.tipo = ibz::TipoParametro::ArchivoImagen;
        imagen.visibleSi = "origen=imagen";

        ibz::Parametro fondo;
        fondo.id = "fondo";
        fondo.clave = "param.fondo";
        fondo.tipo = ibz::TipoParametro::Opcion;
        fondo.opciones = {"quitar_blanco", "conservar"};
        fondo.defecto = "quitar_blanco";
        fondo.visibleSi = "origen=imagen";

        ibz::Parametro ancho;
        ancho.id = "ancho_mm";
        ancho.clave = "param.ancho_mm";
        ancho.tipo = ibz::TipoParametro::Entero;
        ancho.defecto = "50";
        ancho.minimo = 5;
        ancho.maximo = 400;
        return {origen, dibujo, color, imagen, fondo, ancho};
    }
    std::string ejecutar(ibz::Documento& doc, const ibz::Valores& v) const override {
        const int pagina = std::stoi(ibz::valor(v, "pagina", "0"));
        const double x = ibz::valorReal(v, "x", 0.0);
        const double y = ibz::valorReal(v, "y", 0.0);
        const double anchoPt = ibz::valorReal(v, "ancho_mm", 50.0) * 72.0 / 25.4;

        if (ibz::valor(v, "origen", "dibujo") == "dibujo") {
            const ibz::Dibujo dibujo = ibz::leerDibujo(ibz::valor(v, "dibujo"));
            if (dibujo.vacio()) throw ibz::Error("error.dibujo_vacio");
            double r = 0, g = 0, b = 0;
            ibz::colorRgb(ibz::valor(v, "color", "#1f3a93"), r, g, b);
            ibz::insertarDibujo(doc, pagina, x, y, anchoPt, dibujo, r, g, b);
            return {};
        }

        const std::string ruta = ibz::valor(v, "imagen");
        if (ruta.empty()) throw ibz::Error("error.sin_archivos");
        ibz::ImagenRaster img = ibz::leerPng(ibz::desdeUtf8(ruta));
        if (ibz::valor(v, "fondo", "quitar_blanco") == "quitar_blanco") ibz::quitarFondoBlanco(img);
        ibz::insertarImagen(doc, pagina, x, y, anchoPt, img);
        return {};
    }
};

} // namespace

IBZ_REGISTRAR_MODULO(FirmaVisual);
