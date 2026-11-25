/**
 * @file prueba_core.cpp
 * @brief Pruebas mínimas del núcleo (sin marco de pruebas externo).
 */
#include <ibanascapdfcore/claves.h>
#include <ibanascapdfcore/dibujo.h>
#include <ibanascapdfcore/error.h>
#include <ibanascapdfcore/firma.h>
#include <ibanascapdfcore/imagen.h>
#include <ibanascapdfcore/idioma.h>
#include <ibanascapdfcore/modulo.h>
#include <ibanascapdfcore/rangos.h>
#include <ibanascapdfcore/texto.h>

#include "../core/src/numeros.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <memory>
#include <string>
#include <zlib.h>
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
    {
        double r = 0;
        comprobar(ibz::interno::leerReal("2.5", r) && r == 2.5, "leer real");
        comprobar(ibz::interno::leerReal("-0.125", r) && r == -0.125, "leer negativo");
        comprobar(ibz::interno::leerReal("1e3", r) && r == 1000.0, "leer exponente");
        comprobar(ibz::interno::leerReal(".5", r) && r == 0.5, "leer sin entero");
        comprobar(ibz::interno::leerReal("595.276", r) && std::fabs(r - 595.276) < 1e-12, "leer decimales");
        comprobar(!ibz::interno::leerReal("", r) && !ibz::interno::leerReal("2,5", r) &&
                  !ibz::interno::leerReal(" 2", r) && !ibz::interno::leerReal("abc", r) &&
                  !ibz::interno::leerReal("1e", r) && !ibz::interno::leerReal("1e999", r),
                  "rechazar no numeros");
        comprobar(ibz::interno::escribirReal(2.25, 1) == "2.3" || ibz::interno::escribirReal(2.25, 1) == "2.2",
                  "escribir redondeo");
        comprobar(ibz::interno::escribirReal(-3.0, 1) == "-3", "escribir entero");
        comprobar(ibz::interno::escribirReal(0.05, 3) == "0.05", "escribir ceros");
    }
    ibz::Valores v{{"x", "12.5"}, {"mal", "1,5"}};
    comprobar(ibz::valorReal(v, "x", 0) == 12.5, "valorReal");
    comprobar(ibz::valorReal(v, "mal", -1) == -1, "valorReal con coma");
    double rojo = 0, verde = 0, azul = 0;
    ibz::colorRgb("#ff8000", rojo, verde, azul);
    comprobar(rojo == 1.0 && verde > 0.5 && verde < 0.51 && azul == 0.0, "colorRgb");

    // PNG mínimo: 2×1 RGBA (rojo opaco, azul medio transparente) con filtro Sub.
    {
        const auto be = [](std::uint32_t v) {
            return std::string{char(v >> 24), char(v >> 16), char(v >> 8), char(v)};
        };
        const auto trozo = [&](const std::string& tipo, const std::string& d) {
            const std::string td = tipo + d;
            return be(static_cast<std::uint32_t>(d.size())) + td +
                   be(static_cast<std::uint32_t>(crc32(0, reinterpret_cast<const Bytef*>(td.data()), static_cast<uInt>(td.size()))));
        };
        // Fila con filtro 1 (Sub): el segundo píxel se guarda como diferencia con el primero.
        const unsigned char fila[] = {1, 255, 0, 0, 255, static_cast<unsigned char>(0 - 255), 0, 255, static_cast<unsigned char>(128 - 255)};
        unsigned char comp[64];
        uLongf largo = sizeof(comp);
        compress(comp, &largo, fila, sizeof(fila));
        const std::string ihdr = be(2) + be(1) + std::string{8, 6, 0, 0, 0};
        const std::string png = std::string("\x89PNG\r\n\x1a\n", 8) + trozo("IHDR", ihdr) +
                                trozo("IDAT", std::string(reinterpret_cast<char*>(comp), largo)) + trozo("IEND", "");
        const ibz::ImagenRaster img = ibz::leerPng(std::string_view(png));
        comprobar(img.ancho == 2 && img.alto == 1 && img.canales == 3, "PNG cabecera");
        comprobar(img.color == std::vector<std::uint8_t>{255, 0, 0, 0, 0, 255}, "PNG color (filtro Sub)");
        comprobar(img.alfa == std::vector<std::uint8_t>{255, 128}, "PNG alfa");
        bool error = false;
        try { ibz::leerPng(std::string_view("no es png")); } catch (const ibz::Error&) { error = true; }
        comprobar(error, "PNG no válido");
    }

    // Firma CMS sobre un PDF mínimo con el relleno de /ByteRange y /Contents.
    {
        const std::string p12 = (std::filesystem::path(IBZ_DATOS_PRUEBA) / "firma_prueba.p12").string();
        const std::unique_ptr<ibz::Firmante> firmante = ibz::crearFirmante(p12, "prueba");
        const ibz::Firmante& f = *firmante;
        const ibz::InfoCertificado info = ibz::leerCertificado(f.certificados().front());
        comprobar(info.nombre == "Firmante de Prueba Ñandú", "nombre del firmante (UTF-8)");
        comprobar(info.algoritmo == ibz::AlgoritmoClave::Rsa, "clave RSA");
        comprobar(info.desde < std::time(nullptr) && info.hasta > std::time(nullptr), "vigencia del certificado");
        const std::string cab = "%PDF-1.7\n1 0 obj << /ByteRange [ 9999999999 9999999999 9999999999 9999999999 ] /Contents <";
        const std::string pie = "> /Type /Sig >> endobj\ntrailer << >>\n%%EOF\n";
        std::string texto = cab + std::string(2 * ibz::kBytesFirma, '0') + pie;
        std::vector<unsigned char> pdf(texto.begin(), texto.end());
        ibz::completarFirma(pdf, f);
        const std::string r(pdf.begin(), pdf.end());
        const std::size_t a = cab.size() - 1;
        const std::size_t b = a + 2 + 2 * ibz::kBytesFirma;
        const std::string rango = "[ 0 " + std::to_string(a) + " " + std::to_string(b) + " " + std::to_string(r.size() - b);
        comprobar(r.find(rango) != std::string::npos, "ByteRange calculado");
        comprobar(r.compare(a + 1, 4, "3082") == 0, "Contents con CMS (SEQUENCE DER)");
        comprobar(r.size() == texto.size(), "tamaño del PDF sin cambios");
        bool error = false;
        try { ibz::crearFirmante(p12, "otra"); }
        catch (const ibz::Error& e) { error = e.clave() == "error.contrasena_certificado"; }
        comprobar(error, "contraseña incorrecta");
    }

    // Firma dibujada: formato de texto y trazado vectorial.
    {
        const ibz::Dibujo d = ibz::leerDibujo("ibzd1;400,160,3;10,20,30,40,50,20;100.5,80");
        comprobar(d.ancho == 400 && d.alto == 160 && d.grosor == 3 && d.trazos.size() == 2, "dibujo leído");
        comprobar(d.trazos[0].size() == 3 && d.trazos[1].size() == 1, "puntos de cada trazo");
        comprobar(ibz::leerDibujo(ibz::escribirDibujo(d)).trazos == d.trazos, "dibujo escrito y releído");
        comprobar(ibz::leerDibujo("").vacio(), "dibujo vacío");
        // Contorno: x 10..100.5, y 20..80, más medio grosor (1.5) de margen.
        const double prop = ibz::proporcionDibujo(d);
        comprobar(std::abs(prop - 63.0 / 93.5) < 1e-6, "proporción del dibujo");
        const std::string cs = ibz::contenidoDibujo(d, 93.5, 0, 0, 1);
        comprobar(cs.find(" c\n") != std::string::npos && cs.find("1 J 1 j") != std::string::npos, "curvas y extremos redondos");
        comprobar(cs.find("3 w") != std::string::npos, "grosor a escala 1:1");
        bool error = false;
        try { ibz::leerDibujo("ibzd1;400,160,3;10,20,30"); } catch (const ibz::Error& e) { error = e.clave() == "error.dibujo"; }
        comprobar(error, "dibujo mal formado");
    }

    // Visibilidad condicionada de los parámetros.
    {
        ibz::Parametro p;
        p.visibleSi = "aspecto=texto|dibujo";
        auto con = [](std::string v) { return [v](const std::string&) { return v; }; };
        comprobar(ibz::parametroVisible(p, con("dibujo")) && !ibz::parametroVisible(p, con("invisible")), "visibleSi");
    }

    if (fallos == 0) std::printf("Todas las pruebas correctas\n");
    return fallos == 0 ? 0 : 1;
}
