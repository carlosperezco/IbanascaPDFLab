/**
 * @file rotar.cpp
 * @brief Módulo: girar varias páginas (rangos) el ángulo elegido.
 */
#include "ibanascapdfcore/documento.h"
#include "ibanascapdfcore/modulo.h"
#include "ibanascapdfcore/operaciones.h"

#include <string>

namespace {

class Rotar final : public ibz::Modulo {
public:
    const char* id() const override { return "rotar"; }
    const char* menu() const override { return "paginas"; }
    int orden() const override { return 12; }
    std::vector<ibz::Parametro> parametros() const override {
        ibz::Parametro paginas;
        paginas.id = "paginas";
        paginas.clave = "param.paginas";
        paginas.tipo = ibz::TipoParametro::Paginas;
        paginas.defecto = "@actual";

        ibz::Parametro angulo;
        angulo.id = "angulo";
        angulo.clave = "param.angulo";
        angulo.tipo = ibz::TipoParametro::Opcion;
        angulo.opciones = {"90", "180", "270"};
        angulo.defecto = "90";
        return {paginas, angulo};
    }
    std::string ejecutar(ibz::Documento& doc, const ibz::Valores& v) const override {
        const auto it = v.find("angulo");
        const int angulo = (it != v.end() && !it->second.empty()) ? std::stoi(it->second) : 90;
        ibz::rotarPaginas(doc, ibz::paginasElegidas(v, "paginas", doc.paginas()), angulo);
        return {};
    }
};

} // namespace

IBZ_REGISTRAR_MODULO(Rotar);
