/**
 * @file mover.cpp
 * @brief Módulo: mover páginas (rangos) a otra posición.
 *
 * También se puede reordenar arrastrando las miniaturas; esa acción usa la
 * misma operación del núcleo (ibz::moverPaginas).
 */
#include "ibanascapdfcore/documento.h"
#include "ibanascapdfcore/error.h"
#include "ibanascapdfcore/modulo.h"
#include "ibanascapdfcore/operaciones.h"

#include <string>

namespace {

class Mover final : public ibz::Modulo {
public:
    const char* id() const override { return "mover"; }
    const char* menu() const override { return "paginas"; }
    int orden() const override { return 20; }
    std::vector<ibz::Parametro> parametros() const override {
        ibz::Parametro paginas;
        paginas.id = "paginas";
        paginas.clave = "param.paginas";
        paginas.tipo = ibz::TipoParametro::Paginas;
        paginas.defecto = "@actual";

        ibz::Parametro posicion;
        posicion.id = "posicion";
        posicion.clave = "param.posicion";
        posicion.tipo = ibz::TipoParametro::Entero;
        posicion.defecto = "1";
        posicion.minimo = 1;
        posicion.maximo = -1; // número de páginas
        return {paginas, posicion};
    }
    std::string ejecutar(ibz::Documento& doc, const ibz::Valores& v) const override {
        const auto it = v.find("posicion");
        if (it == v.end() || it->second.empty()) throw ibz::Error("error.rangos");
        ibz::moverPaginas(doc, ibz::paginasElegidas(v, "paginas", doc.paginas()), std::stoi(it->second) - 1);
        return {};
    }
};

} // namespace

IBZ_REGISTRAR_MODULO(Mover);
