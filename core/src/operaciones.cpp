/**
 * @file operaciones.cpp
 * @brief Operaciones de páginas con QPDF.
 */
#include "ibanascapdfcore/operaciones.h"

#include "ibanascapdfcore/documento.h"
#include "ibanascapdfcore/error.h"
#include "ibanascapdfcore/rutas.h"

#include <qpdf/Buffer.hh>
#include <qpdf/Constants.h>
#include <qpdf/QPDF.hh>
#include <qpdf/QPDFExc.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFPageObjectHelper.hh>
#include <qpdf/QPDFWriter.hh>

#include <algorithm>
#include <exception>
#include <fstream>
#include <set>
#include <system_error>
#include <memory>

namespace fs = std::filesystem;

namespace ibz {

namespace {

/// Lee un archivo completo en memoria.
Documento::Bytes leerArchivo(const fs::path& ruta) {
    const std::string nombre = utf8(ruta);
    std::ifstream f(ruta, std::ios::binary | std::ios::ate);
    if (!f) throw Error("error.leer", nombre);
    const std::streamoff tam = f.tellg();
    if (tam <= 0) throw Error("error.formato", nombre);
    Documento::Bytes datos(static_cast<std::size_t>(tam));
    f.seekg(0);
    if (!f.read(reinterpret_cast<char*>(datos.data()), tam)) throw Error("error.leer", nombre);
    return datos;
}

/// Escribe un archivo que no debe existir (se escribe en .tmp y se renombra).
void escribirArchivoNuevo(const fs::path& ruta, const Documento::Bytes& datos) {
    const std::string nombre = utf8(ruta);
    std::error_code ec;
    if (fs::exists(ruta, ec)) throw Error("error.existe", nombre);
    fs::path temporal = ruta;
    temporal += ".tmp";
    {
        std::ofstream f(temporal, std::ios::binary | std::ios::trunc);
        if (!f) throw Error("error.escribir", nombre);
        f.write(reinterpret_cast<const char*>(datos.data()), static_cast<std::streamsize>(datos.size()));
        if (!f) {
            f.close();
            fs::remove(temporal, ec);
            throw Error("error.escribir", nombre);
        }
    }
    fs::rename(temporal, ruta, ec);
    if (ec) {
        fs::remove(temporal, ec);
        throw Error("error.escribir", nombre);
    }
}

/// Abre el PDF actual del documento con QPDF (lee directamente de su memoria).
std::unique_ptr<QPDF> abrirQpdf(const Documento& doc) {
    if (!doc.abierto()) throw Error("error.sin_documento");
    auto pdf = std::make_unique<QPDF>();
    pdf->setSuppressWarnings(true);
    const auto& b = doc.bytes();
    pdf->processMemoryFile("documento", reinterpret_cast<const char*>(b.data()), b.size(),
                           doc.contrasena().empty() ? nullptr : doc.contrasena().c_str());
    return pdf;
}

/// Escribe el PDF en memoria (conserva el cifrado del original, si lo tenía).
Documento::Bytes escribirQpdf(QPDF& pdf) {
    QPDFWriter w(pdf);
    w.setOutputMemory();
    w.write();
    const std::shared_ptr<Buffer> buf = w.getBufferSharedPointer();
    const unsigned char* p = buf->getBuffer();
    return Documento::Bytes(p, p + buf->getSize());
}

/// Ejecuta @p op sobre las páginas con QPDF y sustituye el documento.
template <class Operacion>
void conQpdf(Documento& doc, Operacion&& op) {
    Documento::Bytes nuevo;
    try {
        std::unique_ptr<QPDF> pdf = abrirQpdf(doc);
        QPDFPageDocumentHelper paginas(*pdf);
        op(paginas);
        nuevo = escribirQpdf(*pdf);
    } catch (const Error&) {
        throw;
    } catch (const std::exception& e) {
        throw Error("error.qpdf", e.what());
    }
    doc.reemplazar(std::move(nuevo));
}

/// Comprueba que todos los índices existen.
void validar(const std::vector<int>& paginas, int total) {
    if (paginas.empty()) throw Error("error.rangos");
    for (const int p : paginas)
        if (p < 0 || p >= total) throw Error("error.rangos");
}

/// Sustituye el árbol de páginas por @p orden (índices sobre @p todas).
void aplicarOrden(QPDFPageDocumentHelper& h, const std::vector<QPDFPageObjectHelper>& todas,
                  const std::vector<int>& orden) {
    for (const auto& p : todas) h.removePage(p);
    for (const int i : orden) h.addPage(todas[static_cast<std::size_t>(i)], false);
}

} // namespace

void rotarPaginas(Documento& doc, const std::vector<int>& paginas, int angulo) {
    validar(paginas, doc.paginas());
    if (angulo % 90 != 0) throw Error("error.rangos");
    conQpdf(doc, [&](QPDFPageDocumentHelper& h) {
        std::vector<QPDFPageObjectHelper> todas = h.getAllPages();
        std::vector<bool> hecha(todas.size(), false);
        for (const int p : paginas) {
            const std::size_t i = static_cast<std::size_t>(p);
            if (hecha[i]) continue;
            hecha[i] = true;
            todas[i].rotatePage(angulo, true);
        }
    });
}

void eliminarPaginas(Documento& doc, const std::vector<int>& paginas) {
    const int total = doc.paginas();
    validar(paginas, total);
    std::vector<bool> quitar(static_cast<std::size_t>(total), false);
    for (const int p : paginas) quitar[static_cast<std::size_t>(p)] = true;
    if (std::all_of(quitar.begin(), quitar.end(), [](bool q) { return q; })) throw Error("error.eliminar_todas");

    conQpdf(doc, [&](QPDFPageDocumentHelper& h) {
        const std::vector<QPDFPageObjectHelper> todas = h.getAllPages();
        for (std::size_t i = 0; i < todas.size(); ++i)
            if (quitar[i]) h.removePage(todas[i]);
    });
}

void moverPaginas(Documento& doc, const std::vector<int>& paginas, int posicion) {
    const int total = doc.paginas();
    validar(paginas, total);
    std::vector<bool> elegida(static_cast<std::size_t>(total), false);
    std::vector<int> bloque;
    bloque.reserve(paginas.size());
    for (const int p : paginas) {
        if (elegida[static_cast<std::size_t>(p)]) continue;
        elegida[static_cast<std::size_t>(p)] = true;
        bloque.push_back(p);
    }
    std::vector<int> orden;
    orden.reserve(static_cast<std::size_t>(total));
    for (int i = 0; i < total; ++i)
        if (!elegida[static_cast<std::size_t>(i)]) orden.push_back(i);
    const int pos = std::clamp(posicion, 0, static_cast<int>(orden.size()));
    orden.insert(orden.begin() + pos, bloque.begin(), bloque.end());
    reordenarPaginas(doc, orden);
}

void reordenarPaginas(Documento& doc, const std::vector<int>& orden) {
    const int total = doc.paginas();
    if (static_cast<int>(orden.size()) != total) throw Error("error.orden");
    std::vector<bool> visto(static_cast<std::size_t>(total), false);
    bool igual = true;
    for (std::size_t i = 0; i < orden.size(); ++i) {
        const int p = orden[i];
        if (p < 0 || p >= total || visto[static_cast<std::size_t>(p)]) throw Error("error.orden");
        visto[static_cast<std::size_t>(p)] = true;
        igual = igual && p == static_cast<int>(i);
    }
    if (igual) return; // nada que hacer

    conQpdf(doc, [&](QPDFPageDocumentHelper& h) { aplicarOrden(h, h.getAllPages(), orden); });
}

void fusionarArchivos(Documento& doc, const std::vector<fs::path>& archivos, int posicion) {
    if (archivos.empty()) throw Error("error.sin_archivos");
    const int total = doc.paginas();
    const int pos = std::clamp(posicion, 0, total);

    // Los PDF de origen y su memoria deben seguir vivos hasta escribir el resultado:
    // QPDF copia sus objetos al escribir.
    std::vector<Documento::Bytes> datos;
    std::vector<std::unique_ptr<QPDF>> fuentes;
    datos.reserve(archivos.size());
    fuentes.reserve(archivos.size());
    Documento::Bytes nuevo;
    try {
        std::unique_ptr<QPDF> pdf = abrirQpdf(doc);
        QPDFPageDocumentHelper h(*pdf);
        const std::vector<QPDFPageObjectHelper> propias = h.getAllPages();
        std::vector<QPDFPageObjectHelper> ajenas;
        for (const fs::path& ruta : archivos) {
            const std::string nombre = utf8(ruta);
            datos.push_back(leerArchivo(ruta));
            auto f = std::make_unique<QPDF>();
            f->setSuppressWarnings(true);
            try {
                f->processMemoryFile(nombre.c_str(), reinterpret_cast<const char*>(datos.back().data()),
                                     datos.back().size());
            } catch (const QPDFExc& e) {
                if (e.getErrorCode() == qpdf_e_password) throw Error("error.contrasena_archivo", nombre);
                throw Error("error.formato", nombre);
            }
            for (const auto& p : QPDFPageDocumentHelper(*f).getAllPages()) ajenas.push_back(p);
            fuentes.push_back(std::move(f));
        }
        for (const auto& p : propias) h.removePage(p);
        for (int i = 0; i < pos; ++i) h.addPage(propias[static_cast<std::size_t>(i)], false);
        for (const auto& p : ajenas) h.addPage(p, false);
        for (int i = pos; i < total; ++i) h.addPage(propias[static_cast<std::size_t>(i)], false);
        nuevo = escribirQpdf(*pdf);
    } catch (const Error&) {
        throw;
    } catch (const std::exception& e) {
        throw Error("error.qpdf", e.what());
    }
    doc.reemplazar(std::move(nuevo));
}

void escribirGrupos(const Documento& doc, const std::vector<std::vector<int>>& grupos,
                    const std::vector<fs::path>& destinos) {
    if (grupos.empty() || grupos.size() != destinos.size()) throw Error("error.rangos");
    const int total = doc.paginas();
    std::set<fs::path> vistos;
    for (std::size_t i = 0; i < grupos.size(); ++i) {
        validar(grupos[i], total);
        std::error_code ec;
        if (fs::exists(destinos[i], ec)) throw Error("error.existe", utf8(destinos[i]));
        if (!vistos.insert(destinos[i]).second) throw Error("error.rangos", utf8(destinos[i]));
    }

    try {
        // El documento se analiza una sola vez para todos los archivos.
        std::unique_ptr<QPDF> pdf = abrirQpdf(doc);
        const std::vector<QPDFPageObjectHelper> todas = QPDFPageDocumentHelper(*pdf).getAllPages();
        for (std::size_t g = 0; g < grupos.size(); ++g) {
            QPDF salida;
            salida.setSuppressWarnings(true);
            salida.emptyPDF();
            QPDFPageDocumentHelper h(salida);
            for (const int p : grupos[g]) h.addPage(todas[static_cast<std::size_t>(p)], false);
            escribirArchivoNuevo(destinos[g], escribirQpdf(salida));
        }
    } catch (const Error&) {
        throw;
    } catch (const std::exception& e) {
        throw Error("error.qpdf", e.what());
    }
}

} // namespace ibz
