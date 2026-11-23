/**
 * @file escribir_texto.cpp
 * @brief Módulo: añadir texto en el punto de la página que el usuario pulsa.
 */
#include "ibanascapdfcore/documento.h"
#include "ibanascapdfcore/modulo.h"
#include "ibanascapdfcore/texto.h"

#include <string>

namespace {

class Texto final : public ibz::Modulo {
public:
    const char* id() const override { return "texto"; }
    const char* menu() const override { return "paginas"; }
    int orden() const override { return 40; }
    const char* atajo() const override { return "<Control>t"; }
    bool requierePosicion() const override { return true; }
    std::vector<ibz::Parametro> parametros() const override {
        ibz::Parametro texto;
        texto.id = "texto";
        texto.clave = "param.texto";
        texto.tipo = ibz::TipoParametro::TextoLargo;

        ibz::Parametro fuente;
        fuente.id = "fuente";
        fuente.clave = "param.fuente";
        fuente.tipo = ibz::TipoParametro::Opcion;
        fuente.opciones = {"helvetica", "helvetica_negrita", "times", "times_negrita", "courier"};
        fuente.defecto = "helvetica";

        ibz::Parametro tamano;
        tamano.id = "tamano";
        tamano.clave = "param.tamano";
        tamano.tipo = ibz::TipoParametro::Entero;
        tamano.defecto = "12";
        tamano.minimo = 4;
        tamano.maximo = 200;

        ibz::Parametro color;
        color.id = "color";
        color.clave = "param.color";
        color.tipo = ibz::TipoParametro::Color;
        color.defecto = "#000000";
        return {texto, fuente, tamano, color};
    }
    std::string ejecutar(ibz::Documento& doc, const ibz::Valores& v) const override {
        ibz::OpcionesTexto o;
        o.texto = ibz::valor(v, "texto");
        o.fuente = ibz::valor(v, "fuente", "helvetica");
        o.tamano = ibz::valorReal(v, "tamano", 12.0);
        ibz::colorRgb(ibz::valor(v, "color", "#000000"), o.r, o.g, o.b);
        ibz::escribirTexto(doc, std::stoi(ibz::valor(v, "pagina", "0")), ibz::valorReal(v, "x", 0.0),
                           ibz::valorReal(v, "y", 0.0), o);
        return {};
    }
};

} // namespace

IBZ_REGISTRAR_MODULO(Texto);
