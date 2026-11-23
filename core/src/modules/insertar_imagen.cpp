/**
 * @file insertar_imagen.cpp
 * @brief Módulo: insertar una imagen PNG completa (sin quitar el fondo) en el punto pulsado.
 *
 * Usa el mismo código que la firma visual; la diferencia es que la imagen se
 * inserta tal cual, conservando su propia transparencia si la tiene.
 */
#include "ibanascapdfcore/documento.h"
#include "ibanascapdfcore/error.h"
#include "ibanascapdfcore/imagen.h"
#include "ibanascapdfcore/modulo.h"
#include "ibanascapdfcore/rutas.h"

#include <string>

namespace {

class InsertarImagen final : public ibz::Modulo {
public:
    const char* id() const override { return "insertar_imagen"; }
    const char* menu() const override { return "paginas"; }
    int orden() const override { return 45; }
    const char* atajo() const override { return "<Control>i"; }
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
        ancho.defecto = "80";
        ancho.minimo = 5;
        ancho.maximo = 600;
        return {imagen, ancho};
    }
    std::string ejecutar(ibz::Documento& doc, const ibz::Valores& v) const override {
        const std::string ruta = ibz::valor(v, "imagen");
        if (ruta.empty()) throw ibz::Error("error.sin_archivos");
        const ibz::ImagenRaster img = ibz::leerPng(ibz::desdeUtf8(ruta));
        const double anchoPt = ibz::valorReal(v, "ancho_mm", 80.0) * 72.0 / 25.4;
        ibz::insertarImagen(doc, std::stoi(ibz::valor(v, "pagina", "0")), ibz::valorReal(v, "x", 0.0),
                            ibz::valorReal(v, "y", 0.0), anchoPt, img);
        return {};
    }
};

} // namespace

IBZ_REGISTRAR_MODULO(InsertarImagen);
