/**
 * @file prueba_core.cpp
 * @brief Pruebas mínimas del núcleo (sin marco de pruebas externo).
 */
#include <ibanascapdfcore/claves.h>
#include <ibanascapdfcore/error.h>
#include <ibanascapdfcore/idioma.h>
#include <ibanascapdfcore/modulo.h>
#include <ibanascapdfcore/rangos.h>
#include <ibanascapdfcore/texto.h>

#include <cstdio>
#include <filesystem>
#include <vector>

namespace {

int fallos = 0;

void comprobar(bool ok, const char* que) {
    if (!ok) {
        std::printf("FALLO: %s\n", que);
        ++fallos;
    }
}

bool lanzaRangos(const char* t, int total) {
    try {
        ibz::analizarRangos(t, total);
    } catch (const ibz::Error& e) {
        return e.clave() == "error.rangos";
    }
    return false;
}

} // namespace

int main() {
    using V = std::vector<int>;
    comprobar(ibz::analizarRangos("1-3,5", 10) == V{0, 1, 2, 4}, "rangos simples");
    comprobar(ibz::analizarRangos(" 8- ", 10) == V{7, 8, 9}, "rango abierto al final");
    comprobar(ibz::analizarRangos("-2", 10) == V{0, 1}, "rango abierto al principio");
    comprobar(ibz::analizarRangos("3-1", 10) == V{2, 1, 0}, "rango inverso");
    comprobar(ibz::analizarGrupos("1-2,3-4", 4).size() == 2, "grupos");
    comprobar(lanzaRangos("0", 5), "página 0");
    comprobar(lanzaRangos("6", 5), "página fuera de rango");
    comprobar(lanzaRangos("1,,2", 5), "elemento vacío");
    comprobar(lanzaRangos("a-b", 5), "texto no numérico");
    comprobar(lanzaRangos("-", 5), "solo guion");

    comprobar(ibz::formato("Página {1} de {2}", {"3", "9"}) == "Página 3 de 9", "formato");
    comprobar(ibz::formato("{3}", {"a"}) == "{3}", "formato sin argumento");

    const auto ruta = std::filesystem::temp_directory_path() / "ibz_prueba_claves.conf";
    ibz::Claves a;
    a.poner("uno", "línea 1\nlínea 2");
    a.poner("dos", "7");
    comprobar(a.guardar(ruta), "guardar claves");
    ibz::Claves b;
    comprobar(b.cargar(ruta), "cargar claves");
    comprobar(b.obtener("uno") == "línea 1\nlínea 2", "escapes");
    comprobar(b.obtenerEntero("dos", 0) == 7, "entero");
    comprobar(b.obtenerEntero("uno", -1) == -1, "entero no válido");
    std::error_code ec;
    std::filesystem::remove(ruta, ec);

    // Texto: UTF-8 → WinAnsi (cp1252) y números con punto decimal.
    comprobar(ibz::aWinAnsi("Año €5 – “ok”") == std::string("A\xF1o \x80" "5 \x96 \x93ok\x94"), "WinAnsi");
    comprobar(ibz::aWinAnsi("日本") == "??", "fuera de WinAnsi");
    comprobar(ibz::aWinAnsi(std::string("\xC3", 1)) == "?", "UTF-8 cortado");
    comprobar(ibz::numeroPdf(12.0) == "12", "numero entero");
    comprobar(ibz::numeroPdf(-0.5) == "-0.5", "numero negativo");
    comprobar(ibz::numeroPdf(1.23456) == "1.235", "numero redondeado");
    comprobar(ibz::numeroPdf(-0.0001) == "0", "menos cero");
    ibz::Valores v{{"x", "12.5"}, {"mal", "1,5"}};
    comprobar(ibz::valorReal(v, "x", 0) == 12.5, "valorReal");
    comprobar(ibz::valorReal(v, "mal", -1) == -1, "valorReal con coma");
    double rojo = 0, verde = 0, azul = 0;
    ibz::colorRgb("#ff8000", rojo, verde, azul);
    comprobar(rojo == 1.0 && verde > 0.5 && verde < 0.51 && azul == 0.0, "colorRgb");

    if (fallos == 0) std::printf("Todas las pruebas correctas\n");
    return fallos == 0 ? 0 : 1;
}
