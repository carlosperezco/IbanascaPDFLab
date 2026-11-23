/**
 * @file eliminar.cpp
 * @brief Módulo: eliminar páginas (rangos).
 */
#include "ibanascapdfcore/documento.h"
#include "ibanascapdfcore/modulo.h"
#include "ibanascapdfcore/operaciones.h"

namespace {

class Eliminar final : public ibz::Modulo {
public:
    const char* id() const override { return "eliminar"; }
    const char* menu() const override { return "paginas"; }
    int orden() const override { return 30; }
    const char* atajo() const override { return "Delete"; }
    std::vector<ibz::Parametro> parametros() const override {
        ibz::Parametro paginas;
        paginas.id = "paginas";
        paginas.clave = "param.paginas";
        paginas.tipo = ibz::TipoParametro::Paginas;
        paginas.defecto = "@actual";
        return {paginas};
    }
    std::string ejecutar(ibz::Documento& doc, const ibz::Valores& v) const override {
        ibz::eliminarPaginas(doc, ibz::paginasElegidas(v, "paginas", doc.paginas()));
        return {};
    }
};

} // namespace

IBZ_REGISTRAR_MODULO(Eliminar);
