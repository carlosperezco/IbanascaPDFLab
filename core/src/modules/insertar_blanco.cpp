/**
 * @file insertar_blanco.cpp
 * @brief Módulo: insertar una página en blanco antes o después de la actual, o al final.
 */
#include "ibanascapdfcore/documento.h"
#include "ibanascapdfcore/modulo.h"
#include "ibanascapdfcore/operaciones.h"

#include <string>

namespace {

class InsertarBlanco final : public ibz::Modulo {
public:
    const char* id() const override { return "insertar_blanco"; }
    const char* menu() const override { return "paginas"; }
    int orden() const override { return 50; }
    std::vector<ibz::Parametro> parametros() const override {
        ibz::Parametro donde;
        donde.id = "donde";
        donde.clave = "param.donde_blanco";
        donde.tipo = ibz::TipoParametro::Opcion;
        donde.opciones = {"despues", "antes", "final"};
        donde.defecto = "despues";

        ibz::Parametro tamano;
        tamano.id = "tamano";
        tamano.clave = "param.tamano_pagina";
        tamano.tipo = ibz::TipoParametro::Opcion;
        tamano.opciones = {"igual", "a4", "carta"};
        tamano.defecto = "igual";
        return {donde, tamano};
    }
    std::string ejecutar(ibz::Documento& doc, const ibz::Valores& v) const override {
        const int actual = std::stoi(ibz::valor(v, "pagina_actual", "1")) - 1; // base 0
        const std::string donde = ibz::valor(v, "donde", "despues");
        const int posicion = donde == "antes" ? actual : donde == "final" ? doc.paginas() : actual + 1;

        // Tamaño: el de la página visible (tal como se ve), A4 o Carta.
        const std::string tamano = ibz::valor(v, "tamano", "igual");
        double ancho = 595.276;
        double alto = 841.89;
        if (tamano == "carta") {
            ancho = 612.0;
            alto = 792.0;
        } else if (tamano == "igual" && actual >= 0 && actual < doc.paginas()) {
            ancho = doc.tamano(actual).ancho;
            alto = doc.tamano(actual).alto;
        }
        ibz::insertarPaginaBlanco(doc, posicion, ancho, alto);
        return {};
    }
};

} // namespace

IBZ_REGISTRAR_MODULO(InsertarBlanco);
