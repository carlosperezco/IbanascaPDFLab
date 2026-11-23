/**
 * @file rotar_derecha.cpp
 * @brief Módulo: girar la página actual 90° a la derecha (sin diálogo).
 */
#include "ibanascapdfcore/documento.h"
#include "ibanascapdfcore/modulo.h"
#include "ibanascapdfcore/operaciones.h"

namespace {

class RotarDerecha final : public ibz::Modulo {
public:
    const char* id() const override { return "rotar_derecha"; }
    const char* menu() const override { return "paginas"; }
    int orden() const override { return 10; }
    const char* atajo() const override { return "<Control>r"; }
    std::vector<ibz::Parametro> parametros() const override { return {}; }
    std::string ejecutar(ibz::Documento& doc, const ibz::Valores& v) const override {
        ibz::rotarPaginas(doc, ibz::paginasElegidas(v, "", doc.paginas()), 90);
        return {};
    }
};

} // namespace

IBZ_REGISTRAR_MODULO(RotarDerecha);
