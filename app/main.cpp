/**
 * @file main.cpp
 * @brief Punto de entrada: recursos, configuración, idioma, línea de órdenes y arranque.
 */
#include "version.h"

#include <ibanascapdfcore/claves.h>
#include <ibanascapdfcore/documento.h>
#include <ibanascapdfcore/idioma.h>
#include <ibanascapdfcore/modulo.h>
#include <ibanascapdfcore/rutas.h>
#include <ibanascapdfgui/interfaz.h>

#include <glib.h>

#include <cstdio>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

/// En Windows el programa no tiene consola: se usa la de quien lo lanzó.
void prepararConsola() {
#ifdef _WIN32
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        FILE* f = nullptr;
        freopen_s(&f, "CONOUT$", "w", stdout);
        freopen_s(&f, "CONOUT$", "w", stderr);
        SetConsoleOutputCP(CP_UTF8);
    }
#endif
}

void imprimir(const std::string& texto) {
    std::fputs(texto.c_str(), stdout);
    std::fputc('\n', stdout);
    std::fflush(stdout);
}

int listarModulos() {
    const auto& modulos = ibz::Registro::instancia().modulos();
    if (modulos.empty()) {
        imprimir(ibz::tr("cli.sin_modulos"));
        return 0;
    }
    imprimir(ibz::tr("cli.modulos"));
    for (const auto& m : modulos) {
        const std::string clave = "modulo." + std::string(m->id()) + ".nombre";
        std::string nombre = ibz::tr(clave.c_str());
        std::string limpio;
        for (const char c : nombre)
            if (c != '_') limpio += c;
        imprimir("  " + std::string(m->id()) + "  (" + m->menu() + ")  " + limpio);
    }
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    const std::vector<std::string> args = ibz::argumentosUtf8(argc, argv);

    ibz::Claves config;
    config.cargar(ibz::carpetaDatos() / "config" / "app.conf");
    ibz::Claves ajustes;
    ajustes.cargar(ibz::rutaAjustes());

    std::string idioma = ajustes.obtener("idioma", config.obtener("idioma", "auto"));
    if (idioma.empty() || idioma == "auto") idioma = ibz::Idioma::idiomaSistema();
    ibz::Idioma::global().cargar(idioma);

    std::string archivo;
    for (std::size_t i = 1; i < args.size(); ++i) {
        const std::string& a = args[i];
        if (a == "--version" || a == "-v") {
            prepararConsola();
            imprimir(std::string(IBZ_NOMBRE) + " " + IBZ_VERSION);
            return 0;
        }
        if (a == "--help" || a == "-h") {
            prepararConsola();
            imprimir(ibz::tr("cli.uso"));
            return 0;
        }
        if (a == "--list") {
            prepararConsola();
            return listarModulos();
        }
        if (!a.empty() && a[0] == '-') {
            prepararConsola();
            imprimir(ibz::formato(ibz::tr("cli.opcion_desconocida"), {a}));
            imprimir(ibz::tr("cli.uso"));
            return 2;
        }
        archivo = a;
    }

    // Textos internos de GTK (selector de archivos, etc.) en el idioma elegido.
    g_setenv("LANGUAGE", ibz::Idioma::global().codigo().c_str(), TRUE);

    ibz::iniciarPdf();
    const int r = ibz::gui::ejecutar(config, ajustes, archivo);
    ibz::cerrarPdf();
    return r;
}
