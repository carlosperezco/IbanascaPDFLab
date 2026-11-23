/**
 * @file rotar_izquierda.cpp
 * @brief Módulo: girar la página actual 90° a la izquierda (sin diálogo).
 */
#include "ibanascapdfcore/documento.h"
#include "ibanascapdfcore/modulo.h"
#include "ibanascapdfcore/operaciones.h"

namespace {

class RotarIzquierda final : public ibz::Modulo {
public:
    const char* id() const override { return "rotar_izquierda"; }
    const char* menu() const override { return "paginas"; }
    int orden() const override { return 11; }
    const char* atajo() const override { return "<Control>l"; }
    std::vector<ibz::Parametro> parametros() const override { return {}; }
    std::string ejecutar(ibz::Documento& doc, const ibz::Valores& v) const override {
        ibz::rotarPaginas(doc, ibz::paginasElegidas(v, "", doc.paginas()), -90);
        return {};
    }
};

} // namespace

IBZ_REGISTRAR_MODULO(RotarIzquierda);
