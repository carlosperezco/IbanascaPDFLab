/**
 * @file firma.cpp
 * @brief Firma digital: preparación del PDF con QPDF (campo, aspecto y relleno).
 *
 * La firma CMS y el relleno final están en firma_cms.cpp.
 */
#include "ibanascapdfcore/firma.h"

#include "ibanascapdfcore/documento.h"
#include "ibanascapdfcore/error.h"
#include "ibanascapdfcore/idioma.h"
#include "ibanascapdfcore/texto.h"

#include "pdf_interno.h"

#include <qpdf/Buffer.hh>
#include <qpdf/QPDF.hh>
#include <qpdf/QPDFAcroFormDocumentHelper.hh>
#include <qpdf/QPDFFormFieldObjectHelper.hh>
#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFPageObjectHelper.hh>
#include <qpdf/QPDFWriter.hh>

#include <algorithm>
#include <cmath>
#include <ctime>
#include <exception>
#include <memory>
#include <string>

namespace ibz {

namespace {

/// Fecha PDF en UTC: "D:AAAAMMDDhhmmssZ".
std::string fechaPdf(std::time_t t) {
    std::tm g{};
#ifdef _WIN32
    gmtime_s(&g, &t);
#else
    gmtime_r(&t, &g);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "D:%Y%m%d%H%M%SZ", &g);
    return buf;
}

/// Fecha local legible para el aspecto: "dd/mm/aaaa hh:mm".
std::string fechaLocal(std::time_t t) {
    std::tm l{};
#ifdef _WIN32
    localtime_s(&l, &t);
#else
    localtime_r(&t, &l);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%d/%m/%Y %H:%M", &l);
    return buf;
}

} // namespace

void firmarDocumento(Documento& doc, const Firmante& firmante, const OpcionesFirma& o) {
    if (!doc.abierto()) throw Error("error.sin_documento");
    const bool visible = o.aspecto != AspectoFirma::Invisible;
    if (visible && (o.pagina < 0 || o.pagina >= doc.paginas())) throw Error("error.rangos");
    const std::time_t ahora = std::time(nullptr);

    Documento::Bytes salida;
    try {
        QPDF pdf;
        pdf.setSuppressWarnings(true);
        const auto& b = doc.bytes();
        pdf.processMemoryFile("documento", reinterpret_cast<const char*>(b.data()), b.size(),
                              doc.contrasena().empty() ? nullptr : doc.contrasena().c_str());
        if (pdf.isEncrypted()) throw Error("error.firma_cifrado");

        QPDFAcroFormDocumentHelper formulario(pdf);
        for (auto& campo : formulario.getFormFields())
            if (campo.getFieldType() == "/Sig" && campo.getValue().isDictionary()) throw Error("error.ya_firmado");

        std::vector<QPDFPageObjectHelper> paginas = QPDFPageDocumentHelper(pdf).getAllPages();
        QPDFPageObjectHelper pagina = paginas[static_cast<std::size_t>(visible ? o.pagina : 0)];

        // Diccionario de firma con relleno.
        QPDFObjectHandle firma = QPDFObjectHandle::newDictionary();
        firma.replaceKey("/Type", QPDFObjectHandle::newName("/Sig"));
        firma.replaceKey("/Filter", QPDFObjectHandle::newName("/Adobe.PPKLite"));
        firma.replaceKey("/SubFilter", QPDFObjectHandle::newName("/ETSI.CAdES.detached"));
        QPDFObjectHandle rango = QPDFObjectHandle::newArray();
        for (int i = 0; i < 4; ++i) rango.appendItem(QPDFObjectHandle::newInteger(9999999999LL));
        firma.replaceKey("/ByteRange", rango);
        firma.replaceKey("/Contents", QPDFObjectHandle::newString(std::string(kBytesFirma, '\0')));
        firma.replaceKey("/M", QPDFObjectHandle::newString(fechaPdf(ahora)));
        firma.replaceKey("/Name", QPDFObjectHandle::newUnicodeString(firmante.nombre()));
        firma = pdf.makeIndirectObject(firma);

        // Rectángulo y aspecto.
        double rect[4] = {0, 0, 0, 0};
        QPDFObjectHandle aspecto;
        if (visible) {
            const double w = std::max(10.0, o.anchoPt);
            double h = w * 0.32;
            if (o.aspecto == AspectoFirma::Imagen) {
                if (o.imagen.ancho <= 0 || o.imagen.alto <= 0) throw Error("error.png");
                h = w * o.imagen.alto / o.imagen.ancho;
            }
            const int rot = interno::rotacionPagina(pagina.getObjectHandle());
            const double rad = rot * 3.14159265358979323846 / 180.0;
            const double c = std::round(std::cos(rad));
            const double s = std::round(std::sin(rad));
            // Esquinas: P, P + w·derecha, P + h·abajo, P + w·derecha + h·abajo.
            const double xs[4] = {o.x, o.x + w * c, o.x + h * s, o.x + w * c + h * s};
            const double ys[4] = {o.y, o.y + w * s, o.y - h * c, o.y + w * s - h * c};
            rect[0] = *std::min_element(xs, xs + 4);
            rect[1] = *std::min_element(ys, ys + 4);
            rect[2] = *std::max_element(xs, xs + 4);
            rect[3] = *std::max_element(ys, ys + 4);

            std::string cs;
            QPDFObjectHandle recursos = QPDFObjectHandle::newDictionary();
            if (o.aspecto == AspectoFirma::Imagen) {
                QPDFObjectHandle xo = QPDFObjectHandle::newDictionary();
                xo.replaceKey("/Im", interno::xobjetoImagen(pdf, o.imagen));
                recursos.replaceKey("/XObject", xo);
                cs = "q " + numeroPdf(w) + " 0 0 " + numeroPdf(h) + " 0 0 cm /Im Do Q\n";
            } else {
                auto fuente = [&](const char* nombre) {
                    QPDFObjectHandle f = QPDFObjectHandle::newDictionary();
                    f.replaceKey("/Type", QPDFObjectHandle::newName("/Font"));
                    f.replaceKey("/Subtype", QPDFObjectHandle::newName("/Type1"));
                    f.replaceKey("/BaseFont", QPDFObjectHandle::newName(nombre));
                    f.replaceKey("/Encoding", QPDFObjectHandle::newName("/WinAnsiEncoding"));
                    return pdf.makeIndirectObject(f);
                };
                QPDFObjectHandle fuentes = QPDFObjectHandle::newDictionary();
                fuentes.replaceKey("/F1", fuente("/Helvetica"));
                fuentes.replaceKey("/F2", fuente("/Helvetica-Bold"));
                recursos.replaceKey("/Font", fuentes);

                const double margen = h * 0.10;
                const std::string titulo = aWinAnsi(tr("firma.aspecto.firmado_por"));
                const std::string nombre = aWinAnsi(firmante.nombre());
                const std::string fecha = aWinAnsi(formato(tr("firma.aspecto.fecha"), {fechaLocal(ahora)}));
                // Helvetica: ~0.55 em de ancho medio; el nombre se reduce si no cabe.
                const double tPequeno = h * 0.17;
                const double tNombre = std::min(h * 0.26, (w - 2 * margen) / (0.58 * std::max<std::size_t>(1, nombre.size())));
                cs += "q 0.6 0.6 0.6 RG 0.75 w 0.5 0.5 " + numeroPdf(w - 1) + " " + numeroPdf(h - 1) + " re S Q\n";
                cs += "BT 0.15 0.15 0.15 rg\n";
                cs += "/F1 " + numeroPdf(tPequeno) + " Tf " + numeroPdf(margen) + " " + numeroPdf(h - margen - tPequeno) +
                      " Td " + interno::literalPdf(titulo) + " Tj\n";
                cs += "/F2 " + numeroPdf(tNombre) + " Tf 0 " + numeroPdf(-(tNombre + h * 0.08)) + " Td " +
                      interno::literalPdf(nombre) + " Tj\n";
                cs += "/F1 " + numeroPdf(tPequeno) + " Tf 0 " + numeroPdf(-(tPequeno + h * 0.10)) + " Td " +
                      interno::literalPdf(fecha) + " Tj\nET\n";
            }
            aspecto = QPDFObjectHandle::newStream(&pdf, cs);
            QPDFObjectHandle da = aspecto.getDict();
            da.replaceKey("/Type", QPDFObjectHandle::newName("/XObject"));
            da.replaceKey("/Subtype", QPDFObjectHandle::newName("/Form"));
            QPDFObjectHandle bbox = QPDFObjectHandle::newArray();
            for (const double v : {0.0, 0.0, w, h}) bbox.appendItem(QPDFObjectHandle::newReal(v, 3));
            da.replaceKey("/BBox", bbox);
            // El aspecto se gira con la página para verse derecho.
            QPDFObjectHandle matriz = QPDFObjectHandle::newArray();
            for (const double v : {c, s, -s, c, 0.0, 0.0}) matriz.appendItem(QPDFObjectHandle::newReal(v, 0));
            da.replaceKey("/Matrix", matriz);
            da.replaceKey("/Resources", recursos);
        }

        // Campo de firma (widget) en la página y en el formulario.
        QPDFObjectHandle widget = QPDFObjectHandle::newDictionary();
        widget.replaceKey("/Type", QPDFObjectHandle::newName("/Annot"));
        widget.replaceKey("/Subtype", QPDFObjectHandle::newName("/Widget"));
        widget.replaceKey("/FT", QPDFObjectHandle::newName("/Sig"));
        widget.replaceKey("/T", QPDFObjectHandle::newUnicodeString("IbanascaFirma1"));
        widget.replaceKey("/V", firma);
        widget.replaceKey("/F", QPDFObjectHandle::newInteger(132)); // Print + Locked
        widget.replaceKey("/P", pagina.getObjectHandle());
        QPDFObjectHandle r = QPDFObjectHandle::newArray();
        for (const double v : rect) r.appendItem(QPDFObjectHandle::newReal(v, 3));
        widget.replaceKey("/Rect", r);
        if (visible) {
            QPDFObjectHandle ap = QPDFObjectHandle::newDictionary();
            ap.replaceKey("/N", aspecto);
            widget.replaceKey("/AP", ap);
        }
        widget = pdf.makeIndirectObject(widget);

        QPDFObjectHandle anotaciones = pagina.getObjectHandle().getKey("/Annots");
        if (!anotaciones.isArray()) {
            anotaciones = QPDFObjectHandle::newArray();
            pagina.getObjectHandle().replaceKey("/Annots", anotaciones);
        }
        anotaciones.appendItem(widget);

        QPDFObjectHandle raiz = pdf.getRoot();
        QPDFObjectHandle acro = raiz.getKey("/AcroForm");
        if (!acro.isDictionary()) {
            acro = pdf.makeIndirectObject(QPDFObjectHandle::newDictionary());
            raiz.replaceKey("/AcroForm", acro);
        }
        QPDFObjectHandle campos = acro.getKey("/Fields");
        if (!campos.isArray()) {
            campos = QPDFObjectHandle::newArray();
            acro.replaceKey("/Fields", campos);
        }
        campos.appendItem(widget);
        acro.replaceKey("/SigFlags", QPDFObjectHandle::newInteger(3));

        // Sin flujos de objetos: el diccionario /Sig queda como texto en el archivo.
        QPDFWriter wr(pdf);
        wr.setOutputMemory();
        wr.setObjectStreamMode(qpdf_o_disable);
        wr.write();
        const std::shared_ptr<Buffer> buf = wr.getBufferSharedPointer();
        salida.assign(buf->getBuffer(), buf->getBuffer() + buf->getSize());
    } catch (const Error&) {
        throw;
    } catch (const std::exception& e) {
        throw Error("error.qpdf", e.what());
    }

    completarFirma(salida, firmante);
    doc.reemplazar(std::move(salida));
}

} // namespace ibz
