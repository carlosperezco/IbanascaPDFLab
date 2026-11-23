/**
 * @file fusionar.cpp
 * @brief Módulo: añadir al documento las páginas de otros PDF.
 */
#include "ibanascapdfcore/documento.h"
#include "ibanascapdfcore/modulo.h"
#include "ibanascapdfcore/operaciones.h"
#include "ibanascapdfcore/rutas.h"

#include <string>
#include <string_view>

namespace {

class Fusionar final : public ibz::Modulo {
public:
    const char* id() const override { return "fusionar"; }
    const char* menu() const override { return "documento"; }
    int orden() const override { return 10; }
    std::vector<ibz::Parametro> parametros() const override {
        ibz::Parametro archivos;
        archivos.id = "archivos";
        archivos.clave = "param.archivos";
        archivos.tipo = ibz::TipoParametro::ArchivosPdf;

        ibz::Parametro donde;
        donde.id = "donde";
        donde.clave = "param.donde";
        donde.tipo = ibz::TipoParametro::Opcion;
        donde.opciones = {"final", "inicio", "actual"};
        donde.defecto = "final";
        return {archivos, donde};
    }
    std::string ejecutar(ibz::Documento& doc, const ibz::Valores& v) const override {
        // Rutas UTF-8 separadas por saltos de línea, en el orden elegido.
        std::vector<std::filesystem::path> archivos;
        std::string_view resto = ibz::valor(v, "archivos");
        while (!resto.empty()) {
            const std::size_t fin = resto.find('\n');
            const std::string_view r = resto.substr(0, fin);
            if (!r.empty()) archivos.push_back(ibz::desdeUtf8(std::string(r)));
            resto = (fin == std::string_view::npos) ? std::string_view() : resto.substr(fin + 1);
        }

        const std::string donde = ibz::valor(v, "donde", "final");
        int posicion = doc.paginas();
        if (donde == "inicio") posicion = 0;
        if (donde == "actual") posicion = std::stoi(ibz::valor(v, "pagina_actual", "1")); // tras la visible
        ibz::fusionarArchivos(doc, archivos, posicion);
        return {};
    }
};

} // namespace

IBZ_REGISTRAR_MODULO(Fusionar);
