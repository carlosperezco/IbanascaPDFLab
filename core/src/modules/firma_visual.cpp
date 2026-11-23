/**
 * @file firma_visual.cpp
 * @brief Módulo: firma visual / manuscrita con una imagen PNG en el punto pulsado.
 */
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
        ibz::Parametro imagen;
        imagen.id = "imagen";
        imagen.clave = "param.imagen";
        imagen.tipo = ibz::TipoParametro::ArchivoImagen;

        ibz::Parametro ancho;
        ancho.id = "ancho_mm";
        ancho.clave = "param.ancho_mm";
        ancho.tipo = ibz::TipoParametro::Entero;
        ancho.defecto = "50";
        ancho.minimo = 5;
        ancho.maximo = 400;

        ibz::Parametro fondo;
        fondo.id = "fondo";
        fondo.clave = "param.fondo";
        fondo.tipo = ibz::TipoParametro::Opcion;
        fondo.opciones = {"quitar_blanco", "conservar"};
        fondo.defecto = "quitar_blanco";
        return {imagen, ancho, fondo};
    }
    std::string ejecutar(ibz::Documento& doc, const ibz::Valores& v) const override {
        const std::string ruta = ibz::valor(v, "imagen");
        if (ruta.empty()) throw ibz::Error("error.sin_archivos");
        ibz::ImagenRaster img = ibz::leerPng(ibz::desdeUtf8(ruta));
        if (ibz::valor(v, "fondo", "quitar_blanco") == "quitar_blanco") ibz::quitarFondoBlanco(img);
        const double anchoPt = ibz::valorReal(v, "ancho_mm", 50.0) * 72.0 / 25.4;
        ibz::insertarImagen(doc, std::stoi(ibz::valor(v, "pagina", "0")), ibz::valorReal(v, "x", 0.0),
                            ibz::valorReal(v, "y", 0.0), anchoPt, img);
        return {};
    }
};

} // namespace

IBZ_REGISTRAR_MODULO(FirmaVisual);
