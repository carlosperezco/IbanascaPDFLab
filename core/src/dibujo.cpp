/**
 * @file dibujo.cpp
 * @brief Firma dibujada: formato de texto, suavizado y escritura vectorial en el PDF.
 */
#include "ibanascapdfcore/dibujo.h"

#include "ibanascapdfcore/documento.h"
#include "ibanascapdfcore/error.h"
#include "ibanascapdfcore/texto.h"

#include "pdf_interno.h"

#include <qpdf/Buffer.hh>
#include <qpdf/QPDF.hh>
#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFPageObjectHelper.hh>
#include <qpdf/QPDFWriter.hh>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <exception>
#include <limits>
#include <memory>

namespace ibz {

namespace {

using Punto = std::pair<float, float>;

/// Límites razonables: un dibujo normal tiene unos cientos o miles de puntos.
constexpr std::size_t kMaxPuntos = 200000;

double leerNumero(std::string_view s) {
    double v = 0;
    const auto [fin, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
    if (ec != std::errc() || fin != s.data() + s.size() || !std::isfinite(v)) throw Error("error.dibujo");
    return v;
}

/// Divide @p s por @p sep (sin copiar).
std::vector<std::string_view> dividir(std::string_view s, char sep) {
    std::vector<std::string_view> r;
    std::size_t ini = 0;
    while (true) {
        const std::size_t p = s.find(sep, ini);
        r.push_back(s.substr(ini, p == std::string_view::npos ? std::string_view::npos : p - ini));
        if (p == std::string_view::npos) break;
        ini = p + 1;
    }
    return r;
}

std::string numero(double v) {
    char buf[32];
    const auto [fin, ec] = std::to_chars(buf, buf + sizeof(buf), v, std::chars_format::fixed, 1);
    std::string s(buf, ec == std::errc() ? fin : buf);
    if (s.size() > 2 && s.compare(s.size() - 2, 2, ".0") == 0) s.resize(s.size() - 2);
    return s.empty() ? "0" : s;
}

struct Caja {
    double x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    double ancho() const { return x1 - x0; }
    double alto() const { return y1 - y0; }
};

/// Contorno de los trazos con medio grosor de margen.
Caja caja(const Dibujo& d) {
    Caja c{std::numeric_limits<double>::max(), std::numeric_limits<double>::max(),
           std::numeric_limits<double>::lowest(), std::numeric_limits<double>::lowest()};
    for (const auto& t : d.trazos)
        for (const Punto& p : t) {
            c.x0 = std::min(c.x0, double(p.first));
            c.y0 = std::min(c.y0, double(p.second));
            c.x1 = std::max(c.x1, double(p.first));
            c.y1 = std::max(c.y1, double(p.second));
        }
    const double m = std::max(0.5, d.grosor / 2);
    c.x0 -= m;
    c.y0 -= m;
    c.x1 += m;
    c.y1 += m;
    return c;
}

/// Quita puntos casi repetidos (menos de @p minimo de distancia al anterior conservado).
std::vector<Punto> aclarar(const std::vector<Punto>& t, double minimo) {
    std::vector<Punto> r;
    r.reserve(t.size());
    for (const Punto& p : t) {
        if (!r.empty()) {
            const double dx = p.first - r.back().first;
            const double dy = p.second - r.back().second;
            if (dx * dx + dy * dy < minimo * minimo) continue;
        }
        r.push_back(p);
    }
    // El último punto se conserva siempre para no acortar el trazo.
    if (!t.empty() && r.back() != t.back()) r.push_back(t.back());
    return r;
}

} // namespace

bool Dibujo::vacio() const noexcept {
    return std::none_of(trazos.begin(), trazos.end(), [](const auto& t) { return !t.empty(); });
}

Dibujo leerDibujo(std::string_view texto) {
    Dibujo d;
    if (texto.empty()) return d;
    const std::vector<std::string_view> partes = dividir(texto, ';');
    if (partes.size() < 2 || partes[0] != "ibzd1") throw Error("error.dibujo");
    const std::vector<std::string_view> cab = dividir(partes[1], ',');
    if (cab.size() != 3) throw Error("error.dibujo");
    d.ancho = leerNumero(cab[0]);
    d.alto = leerNumero(cab[1]);
    d.grosor = std::clamp(leerNumero(cab[2]), 0.1, 1000.0);
    std::size_t total = 0;
    for (std::size_t i = 2; i < partes.size(); ++i) {
        if (partes[i].empty()) continue;
        const std::vector<std::string_view> nums = dividir(partes[i], ',');
        if (nums.size() % 2 != 0) throw Error("error.dibujo");
        std::vector<Punto> t;
        t.reserve(nums.size() / 2);
        for (std::size_t k = 0; k < nums.size(); k += 2)
            t.emplace_back(static_cast<float>(leerNumero(nums[k])), static_cast<float>(leerNumero(nums[k + 1])));
        total += t.size();
        if (total > kMaxPuntos) throw Error("error.dibujo");
        d.trazos.push_back(std::move(t));
    }
    return d;
}

std::string escribirDibujo(const Dibujo& d) {
    std::string s = "ibzd1;" + numero(d.ancho) + "," + numero(d.alto) + "," + numero(d.grosor);
    for (const auto& t : d.trazos) {
        if (t.empty()) continue;
        s += ';';
        for (std::size_t i = 0; i < t.size(); ++i) {
            if (i) s += ',';
            s += numero(t[i].first) + "," + numero(t[i].second);
        }
    }
    return s;
}

double proporcionDibujo(const Dibujo& d) {
    if (d.vacio()) return 0;
    const Caja c = caja(d);
    return c.alto() / std::max(1e-6, c.ancho());
}

std::string contenidoDibujo(const Dibujo& d, double ancho, double r, double g, double b) {
    if (d.vacio()) return {};
    const Caja c = caja(d);
    const double k = ancho / std::max(1e-6, c.ancho()); // lienzo → puntos PDF
    // Del lienzo (y hacia abajo) al recuadro (y hacia arriba).
    auto px = [&](double x) { return numeroPdf((x - c.x0) * k); };
    auto py = [&](double y) { return numeroPdf((c.y1 - y) * k); };

    std::string cs = "q 1 J 1 j " + numeroPdf(r) + " " + numeroPdf(g) + " " + numeroPdf(b) + " RG " +
                     numeroPdf(std::max(0.2, d.grosor * k)) + " w\n";
    const double minimo = std::max(0.5, d.grosor * 0.3);
    for (const auto& original : d.trazos) {
        if (original.empty()) continue;
        const std::vector<Punto> t = aclarar(original, minimo);
        cs += px(t[0].first) + " " + py(t[0].second) + " m\n";
        if (t.size() == 1) {
            cs += px(t[0].first) + " " + py(t[0].second) + " l\n"; // punto: con extremo redondo
        } else if (t.size() == 2) {
            cs += px(t[1].first) + " " + py(t[1].second) + " l\n";
        } else {
            // Curva por los puntos medios usando cada punto como control (cuadrática → cúbica).
            double ax = t[0].first;
            double ay = t[0].second;
            for (std::size_t i = 1; i + 1 < t.size(); ++i) {
                const double qx = t[i].first;
                const double qy = t[i].second;
                const double bx = (t[i].first + t[i + 1].first) / 2.0;
                const double by = (t[i].second + t[i + 1].second) / 2.0;
                cs += px(ax + 2.0 / 3.0 * (qx - ax)) + " " + py(ay + 2.0 / 3.0 * (qy - ay)) + " " +
                      px(bx + 2.0 / 3.0 * (qx - bx)) + " " + py(by + 2.0 / 3.0 * (qy - by)) + " " + px(bx) + " " +
                      py(by) + " c\n";
                ax = bx;
                ay = by;
            }
            cs += px(t.back().first) + " " + py(t.back().second) + " l\n";
        }
        cs += "S\n";
    }
    cs += "Q\n";
    return cs;
}

void insertarDibujo(Documento& doc, int pagina, double x, double y, double anchoPt, const Dibujo& dibujo, double r,
                    double g, double b) {
    if (!doc.abierto()) throw Error("error.sin_documento");
    if (dibujo.vacio()) throw Error("error.dibujo_vacio");
    if (pagina < 0 || pagina >= doc.paginas()) throw Error("error.rangos");
    const double w = std::clamp(anchoPt, 1.0, 5000.0);
    const double h = w * proporcionDibujo(dibujo);

    Documento::Bytes nuevo;
    try {
        QPDF pdf;
        pdf.setSuppressWarnings(true);
        const auto& bytes = doc.bytes();
        pdf.processMemoryFile("documento", reinterpret_cast<const char*>(bytes.data()), bytes.size(),
                              doc.contrasena().empty() ? nullptr : doc.contrasena().c_str());
        std::vector<QPDFPageObjectHelper> paginas = QPDFPageDocumentHelper(pdf).getAllPages();
        QPDFPageObjectHelper& p = paginas[static_cast<std::size_t>(pagina)];

        const int rotacion = interno::rotacionPagina(p.getObjectHandle());
        const double rad = rotacion * 3.14159265358979323846 / 180.0;
        const double c = std::round(std::cos(rad));
        const double s = std::round(std::sin(rad));

        // Esquina inferior izquierda del dibujo derecho: (x, y) + h · «abajo» girado.
        const double ox = x + h * s;
        const double oy = y - h * c;
        std::string cs = "Q\nq\n";
        cs += numeroPdf(c) + " " + numeroPdf(s) + " " + numeroPdf(-s) + " " + numeroPdf(c) + " " + numeroPdf(ox) + " " +
              numeroPdf(oy) + " cm\n";
        cs += contenidoDibujo(dibujo, w, r, g, b);
        cs += "Q\n";
        p.addPageContents(QPDFObjectHandle::newStream(&pdf, "q\n"), true);
        p.addPageContents(QPDFObjectHandle::newStream(&pdf, cs), false);

        QPDFWriter wr(pdf);
        wr.setOutputMemory();
        wr.write();
        const std::shared_ptr<Buffer> buf = wr.getBufferSharedPointer();
        nuevo.assign(buf->getBuffer(), buf->getBuffer() + buf->getSize());
    } catch (const Error&) {
        throw;
    } catch (const std::exception& e) {
        throw Error("error.qpdf", e.what());
    }
    doc.reemplazar(std::move(nuevo));
}

} // namespace ibz
