/**
 * @file duplicar.cpp
 * @brief Módulo: duplicar páginas (cada copia queda después de su original).
 */
#include "ibanascapdfcore/documento.h"
#include "ibanascapdfcore/modulo.h"
#include "ibanascapdfcore/operaciones.h"

#include <string>

namespace {

class Duplicar final : public ibz::Modulo {
public:
    const char* id() const override { return "duplicar"; }
    const char* menu() const override { return "paginas"; }
    int orden() const override { return 55; }
    const char* atajo() const override { return "<Control>d"; }
    std::vector<ibz::Parametro> parametros() const override {
        ibz::Parametro paginas;
        paginas.id = "paginas";
        paginas.clave = "param.paginas";
        paginas.tipo = ibz::TipoParametro::Paginas;
        paginas.defecto = "@actual";

        ibz::Parametro copias;
        copias.id = "copias";
        copias.clave = "param.copias";
        copias.tipo = ibz::TipoParametro::Entero;
        copias.defecto = "1";
        copias.minimo = 1;
        copias.maximo = 100;
        return {paginas, copias};
    }
    std::string ejecutar(ibz::Documento& doc, const ibz::Valores& v) const override {
        ibz::duplicarPaginas(doc, ibz::paginasElegidas(v, "paginas", doc.paginas()),
                             std::stoi(ibz::valor(v, "copias", "1")));
        return {};
    }
};

} // namespace

IBZ_REGISTRAR_MODULO(Duplicar);
