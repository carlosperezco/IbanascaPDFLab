/**
 * @file separar.cpp
 * @brief Módulo: separar el documento en varios PDF o extraer páginas a otro PDF.
 *
 * Modos:
 *  - paginas: un archivo por cada página elegida   → <nombre>-p001.pdf …
 *  - rangos:  un archivo por cada rango («1-3,4-6») → <nombre>-1-3.pdf …
 *  - extraer: todas las páginas elegidas en un solo archivo → <nombre>-1-3_5.pdf
 *
 * El documento abierto no cambia y nunca se sobrescriben archivos existentes.
 */
#include "ibanascapdfcore/documento.h"
#include "ibanascapdfcore/idioma.h"
#include "ibanascapdfcore/modulo.h"
#include "ibanascapdfcore/operaciones.h"
#include "ibanascapdfcore/rangos.h"
#include "ibanascapdfcore/rutas.h"

#include <string>

namespace {

/// Describe un grupo de páginas (base 0) para el nombre del archivo: {0,1,2,4} → "1-3_5".
std::string describir(const std::vector<int>& g) {
    std::string r;
    for (std::size_t i = 0; i < g.size();) {
        std::size_t j = i;
        while (j + 1 < g.size() && g[j + 1] == g[j] + 1) ++j;
        if (!r.empty()) r += '_';
        r += std::to_string(g[i] + 1);
        if (j > i) r += '-' + std::to_string(g[j] + 1);
        i = j + 1;
    }
    return r;
}

/// Número con ceros a la izquierda según el total (p. ej. 7 de 120 → "007").
std::string conCeros(int n, int total) {
    const std::size_t ancho = std::to_string(total).size();
    std::string s = std::to_string(n);
    return std::string(ancho > s.size() ? ancho - s.size() : 0, '0') + s;
}

class Separar final : public ibz::Modulo {
public:
    const char* id() const override { return "separar"; }
    const char* menu() const override { return "documento"; }
    int orden() const override { return 20; }
    bool modificaDocumento() const override { return false; }
    std::vector<ibz::Parametro> parametros() const override {
        ibz::Parametro modo;
        modo.id = "modo";
        modo.clave = "param.modo";
        modo.tipo = ibz::TipoParametro::Opcion;
        modo.opciones = {"paginas", "rangos", "extraer"};
        modo.defecto = "paginas";

        ibz::Parametro paginas;
        paginas.id = "paginas";
        paginas.clave = "param.paginas";
        paginas.tipo = ibz::TipoParametro::Paginas;
        paginas.defecto = "1-";

        ibz::Parametro carpeta;
        carpeta.id = "carpeta";
        carpeta.clave = "param.carpeta";
        carpeta.tipo = ibz::TipoParametro::Carpeta;
        carpeta.defecto = "@carpeta";

        ibz::Parametro nombre;
        nombre.id = "nombre";
        nombre.clave = "param.nombre";
        nombre.tipo = ibz::TipoParametro::Texto;
        nombre.defecto = "@nombre";
        return {modo, paginas, carpeta, nombre};
    }
    std::string ejecutar(ibz::Documento& doc, const ibz::Valores& v) const override {
        const int total = doc.paginas();
        const std::string modo = ibz::valor(v, "modo", "paginas");
        const std::string rangos = ibz::valor(v, "paginas", "1-");
        const std::filesystem::path carpeta = v.count("carpeta") && !v.at("carpeta").empty()
                                                  ? ibz::desdeUtf8(v.at("carpeta"))
                                                  : doc.ruta().parent_path();
        const std::string base = ibz::valor(v, "nombre", ibz::utf8(doc.ruta().stem()));

        std::vector<std::vector<int>> grupos;
        std::vector<std::filesystem::path> destinos;
        if (modo == "rangos") {
            grupos = ibz::analizarGrupos(rangos, total);
        } else {
            std::vector<int> paginas = ibz::analizarRangos(rangos, total);
            if (modo == "extraer") {
                grupos.push_back(std::move(paginas));
            } else {
                grupos.reserve(paginas.size());
                for (const int p : paginas) grupos.push_back({p});
            }
        }
        destinos.reserve(grupos.size());
        for (const auto& g : grupos) {
            const std::string sufijo = (modo == "paginas") ? "-p" + conCeros(g.front() + 1, total) : "-" + describir(g);
            destinos.push_back(carpeta / ibz::desdeUtf8(base + sufijo + ".pdf"));
        }

        ibz::escribirGrupos(doc, grupos, destinos);
        if (destinos.size() == 1) return ibz::formato(ibz::tr("mensaje.extraido"), {ibz::utf8(destinos.front())});
        return ibz::formato(ibz::tr("mensaje.separado"), {std::to_string(destinos.size()), ibz::utf8(carpeta)});
    }
};

} // namespace

IBZ_REGISTRAR_MODULO(Separar);
