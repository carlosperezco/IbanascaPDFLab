/**
 * @file texto.cpp
 * @brief Texto con fuentes estándar del PDF, añadido con QPDF.
 */
#include "ibanascapdfcore/texto.h"

#include "ibanascapdfcore/documento.h"
#include "ibanascapdfcore/error.h"

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
#include <memory>
#include <vector>

namespace ibz {

namespace {

/// Caracteres de cp1252 entre 0x80 y 0x9F (0 = no asignado).
constexpr char32_t kCp1252Alto[32] = {
    0x20AC, 0,      0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160,
    0x2039, 0x0152, 0,      0x017D, 0,      0,      0x2018, 0x2019, 0x201C, 0x201D, 0x2022,
    0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0,      0x017E, 0x0178};

char aCp1252(char32_t c) {
    if (c < 0x80 || (c >= 0xA0 && c <= 0xFF)) return static_cast<char>(c);
    for (int i = 0; i < 32; ++i)
        if (kCp1252Alto[i] == c && c != 0) return static_cast<char>(0x80 + i);
    return '?';
}

/// Cadena literal de PDF: escapa ( ) \ y escribe en octal los bytes no ASCII.
std::string literalPdf(const std::string& winansi) {
    std::string r;
    r.reserve(winansi.size() + 2);
    r += '(';
    for (const char ch : winansi) {
        const auto c = static_cast<unsigned char>(ch);
        if (c == '(' || c == ')' || c == '\\') {
            r += '\\';
            r += static_cast<char>(c);
        } else if (c < 0x20 || c >= 0x7F) {
            r += '\\';
            r += static_cast<char>('0' + ((c >> 6) & 7));
            r += static_cast<char>('0' + ((c >> 3) & 7));
            r += static_cast<char>('0' + (c & 7));
        } else {
            r += static_cast<char>(c);
        }
    }
    r += ')';
    return r;
}

/// Nombre PostScript de la fuente estándar y su ascendente (fracción del tamaño).
void fuenteEstandar(const std::string& id, const char*& nombre, double& ascendente) {
    if (id == "helvetica_negrita") { nombre = "/Helvetica-Bold"; ascendente = 0.718; }
    else if (id == "times") { nombre = "/Times-Roman"; ascendente = 0.683; }
    else if (id == "times_negrita") { nombre = "/Times-Bold"; ascendente = 0.676; }
    else if (id == "courier") { nombre = "/Courier"; ascendente = 0.629; }
    else { nombre = "/Helvetica"; ascendente = 0.718; }
}

} // namespace

std::string numeroPdf(double v) {
    if (!std::isfinite(v)) v = 0.0;
    char buf[64];
    const auto [fin, ec] = std::to_chars(buf, buf + sizeof(buf), v, std::chars_format::fixed, 3);
    std::string s(buf, ec == std::errc() ? fin : buf);
    if (s.find('.') != std::string::npos) {
        while (!s.empty() && s.back() == '0') s.pop_back();
        if (!s.empty() && s.back() == '.') s.pop_back();
    }
    if (s.empty() || s == "-0") s = "0";
    return s;
}

std::string aWinAnsi(std::string_view u) {
    std::string r;
    r.reserve(u.size());
    for (std::size_t i = 0; i < u.size();) {
        const auto c0 = static_cast<unsigned char>(u[i]);
        char32_t cp = 0;
        std::size_t n = 0;
        if (c0 < 0x80) { cp = c0; n = 1; }
        else if ((c0 & 0xE0) == 0xC0) { cp = c0 & 0x1F; n = 2; }
        else if ((c0 & 0xF0) == 0xE0) { cp = c0 & 0x0F; n = 3; }
        else if ((c0 & 0xF8) == 0xF0) { cp = c0 & 0x07; n = 4; }
        else { r += '?'; ++i; continue; }
        if (i + n > u.size()) { r += '?'; break; }
        bool valido = true;
        for (std::size_t k = 1; k < n; ++k) {
            const auto ck = static_cast<unsigned char>(u[i + k]);
            if ((ck & 0xC0) != 0x80) { valido = false; break; }
            cp = (cp << 6) | (ck & 0x3F);
        }
        if (!valido) { r += '?'; ++i; continue; }
        r += aCp1252(cp);
        i += n;
    }
    return r;
}

void escribirTexto(Documento& doc, int pagina, double x, double y, const OpcionesTexto& o) {
    if (o.texto.find_first_not_of(" \t\r\n") == std::string::npos) throw Error("error.texto_vacio");
    if (!doc.abierto()) throw Error("error.sin_documento");
    if (pagina < 0 || pagina >= doc.paginas()) throw Error("error.rangos");
    const double tamano = std::clamp(o.tamano, 1.0, 500.0);

    Documento::Bytes nuevo;
    try {
        QPDF pdf;
        pdf.setSuppressWarnings(true);
        const auto& b = doc.bytes();
        pdf.processMemoryFile("documento", reinterpret_cast<const char*>(b.data()), b.size(),
                              doc.contrasena().empty() ? nullptr : doc.contrasena().c_str());
        std::vector<QPDFPageObjectHelper> paginas = QPDFPageDocumentHelper(pdf).getAllPages();
        QPDFPageObjectHelper& p = paginas[static_cast<std::size_t>(pagina)];

        // Rotación de la página: el texto se gira lo mismo para verse horizontal.
        int rotacion = 0;
        QPDFObjectHandle rot = p.getAttribute("/Rotate", false);
        if (rot.isInteger()) rotacion = ((static_cast<int>(rot.getIntValue()) % 360) + 360) % 360;
        const double rad = rotacion * 3.14159265358979323846 / 180.0;
        const double c = std::cos(rad);
        const double s = std::sin(rad);

        // Fuente en los recursos de la página (con un nombre que no esté en uso).
        QPDFObjectHandle recursos = p.getAttribute("/Resources", true);
        if (!recursos.isDictionary()) {
            recursos = QPDFObjectHandle::newDictionary();
            p.getObjectHandle().replaceKey("/Resources", recursos);
        }
        QPDFObjectHandle fuentes = recursos.getKey("/Font");
        if (!fuentes.isDictionary()) {
            fuentes = QPDFObjectHandle::newDictionary();
            recursos.replaceKey("/Font", fuentes);
        }
        std::string nombreRecurso;
        for (int i = 1;; ++i) {
            nombreRecurso = "/IbzF" + std::to_string(i);
            if (!fuentes.hasKey(nombreRecurso)) break;
        }
        const char* nombrePs = nullptr;
        double ascendente = 0.7;
        fuenteEstandar(o.fuente, nombrePs, ascendente);
        QPDFObjectHandle fuente = QPDFObjectHandle::newDictionary();
        fuente.replaceKey("/Type", QPDFObjectHandle::newName("/Font"));
        fuente.replaceKey("/Subtype", QPDFObjectHandle::newName("/Type1"));
        fuente.replaceKey("/BaseFont", QPDFObjectHandle::newName(nombrePs));
        fuente.replaceKey("/Encoding", QPDFObjectHandle::newName("/WinAnsiEncoding"));
        fuentes.replaceKey(nombreRecurso, pdf.makeIndirectObject(fuente));

        // Contenido: el origen del texto es la esquina superior izquierda (x, y).
        std::string cs;
        cs.reserve(o.texto.size() + 200);
        cs += "Q\nq\nBT\n";
        cs += nombreRecurso + " " + numeroPdf(tamano) + " Tf\n";
        cs += numeroPdf(o.r) + " " + numeroPdf(o.g) + " " + numeroPdf(o.b) + " rg\n";
        cs += numeroPdf(c) + " " + numeroPdf(s) + " " + numeroPdf(-s) + " " + numeroPdf(c) + " " + numeroPdf(x) + " " +
              numeroPdf(y) + " Tm\n";
        cs += "0 " + numeroPdf(-ascendente * tamano) + " Td\n";
        cs += numeroPdf(tamano * 1.2) + " TL\n";
        std::string_view resto = o.texto;
        bool primera = true;
        while (true) {
            const std::size_t fin = resto.find('\n');
            std::string_view linea = resto.substr(0, fin);
            if (!linea.empty() && linea.back() == '\r') linea.remove_suffix(1);
            if (!primera) cs += "T*\n";
            cs += literalPdf(aWinAnsi(linea)) + " Tj\n";
            primera = false;
            if (fin == std::string_view::npos) break;
            resto.remove_prefix(fin + 1);
        }
        cs += "ET\nQ\n";

        // El contenido existente queda entre q … Q para no heredar su estado gráfico.
        p.addPageContents(QPDFObjectHandle::newStream(&pdf, "q\n"), true);
        p.addPageContents(QPDFObjectHandle::newStream(&pdf, cs), false);

        QPDFWriter w(pdf);
        w.setOutputMemory();
        w.write();
        const std::shared_ptr<Buffer> buf = w.getBufferSharedPointer();
        nuevo.assign(buf->getBuffer(), buf->getBuffer() + buf->getSize());
    } catch (const Error&) {
        throw;
    } catch (const std::exception& e) {
        throw Error("error.qpdf", e.what());
    }
    doc.reemplazar(std::move(nuevo));
}

} // namespace ibz
