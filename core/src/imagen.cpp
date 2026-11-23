/**
 * @file imagen.cpp
 * @brief Lector PNG propio (zlib) e inserción de imágenes con QPDF.
 */
#include "ibanascapdfcore/imagen.h"

#include "ibanascapdfcore/documento.h"
#include "ibanascapdfcore/error.h"
#include "ibanascapdfcore/rutas.h"
#include "ibanascapdfcore/texto.h"

#include "pdf_interno.h"

#include <qpdf/Buffer.hh>
#include <qpdf/QPDF.hh>
#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFPageObjectHelper.hh>
#include <qpdf/QPDFWriter.hh>

#include <zlib.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>

namespace fs = std::filesystem;

namespace ibz {

namespace {

constexpr std::uint64_t kMaxPixeles = 40ull * 1000 * 1000; ///< límite de seguridad

std::uint32_t be32(const unsigned char* p) {
    return (std::uint32_t(p[0]) << 24) | (std::uint32_t(p[1]) << 16) | (std::uint32_t(p[2]) << 8) | p[3];
}

/// Descomprime el flujo zlib de los IDAT; el tamaño esperado se conoce por la cabecera.
std::vector<unsigned char> inflar(const std::string& comprimido, std::size_t esperado) {
    std::vector<unsigned char> salida(esperado);
    z_stream z{};
    if (inflateInit(&z) != Z_OK) throw Error("error.png");
    z.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(comprimido.data()));
    z.avail_in = static_cast<uInt>(comprimido.size());
    z.next_out = salida.data();
    z.avail_out = static_cast<uInt>(salida.size());
    const int r = inflate(&z, Z_FINISH);
    const std::size_t producido = salida.size() - z.avail_out;
    inflateEnd(&z);
    if ((r != Z_STREAM_END && r != Z_BUF_ERROR) || producido != esperado) throw Error("error.png");
    return salida;
}

/// Deshace los filtros PNG (None, Sub, Up, Average, Paeth) en el sitio.
void desfiltrar(std::vector<unsigned char>& d, std::size_t filas, std::size_t bytesFila, std::size_t bpp) {
    const std::size_t paso = bytesFila + 1;
    for (std::size_t y = 0; y < filas; ++y) {
        unsigned char* fila = d.data() + y * paso;
        const unsigned tipo = fila[0];
        unsigned char* cur = fila + 1;
        const unsigned char* prev = y ? d.data() + (y - 1) * paso + 1 : nullptr;
        for (std::size_t i = 0; i < bytesFila; ++i) {
            const int a = i >= bpp ? cur[i - bpp] : 0;
            const int b = prev ? prev[i] : 0;
            const int c = (prev && i >= bpp) ? prev[i - bpp] : 0;
            int pred = 0;
            switch (tipo) {
            case 0: pred = 0; break;
            case 1: pred = a; break;
            case 2: pred = b; break;
            case 3: pred = (a + b) / 2; break;
            case 4: {
                const int p = a + b - c;
                const int pa = std::abs(p - a);
                const int pb = std::abs(p - b);
                const int pc = std::abs(p - c);
                pred = (pa <= pb && pa <= pc) ? a : (pb <= pc ? b : c);
                break;
            }
            default: throw Error("error.png");
            }
            cur[i] = static_cast<unsigned char>(cur[i] + pred);
        }
    }
}

/// Muestra n-ésima de una fila con @p bits por muestra (1, 2, 4, 8 o 16).
inline unsigned muestra(const unsigned char* fila, std::size_t n, int bits) {
    switch (bits) {
    case 8: return fila[n];
    case 16: return (unsigned(fila[2 * n]) << 8) | fila[2 * n + 1];
    default: {
        const std::size_t bit = n * static_cast<std::size_t>(bits);
        const unsigned byte = fila[bit / 8];
        const unsigned desplaza = 8 - static_cast<unsigned>(bits) - static_cast<unsigned>(bit % 8);
        return (byte >> desplaza) & ((1u << bits) - 1u);
    }
    }
}

/// Lleva una muestra de @p bits a 8 bits.
inline std::uint8_t a8(unsigned v, int bits) {
    switch (bits) {
    case 1: return v ? 255 : 0;
    case 2: return static_cast<std::uint8_t>(v * 85);
    case 4: return static_cast<std::uint8_t>(v * 17);
    case 8: return static_cast<std::uint8_t>(v);
    default: return static_cast<std::uint8_t>(v >> 8);
    }
}

} // namespace

ImagenRaster leerPng(std::string_view datos) {
    static const unsigned char firma[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    if (datos.size() < 8 || std::memcmp(datos.data(), firma, 8) != 0) throw Error("error.png");

    std::uint32_t ancho = 0;
    std::uint32_t alto = 0;
    int bits = 0;
    int tipo = -1;
    std::string idat;
    std::vector<std::array<std::uint8_t, 3>> paleta;
    std::vector<std::uint8_t> trnsPaleta;
    int trnsGris = -1;
    std::array<int, 3> trnsRgb{-1, -1, -1};

    const auto* p = reinterpret_cast<const unsigned char*>(datos.data());
    std::size_t pos = 8;
    bool fin = false;
    while (!fin && pos + 12 <= datos.size()) {
        const std::uint32_t largo = be32(p + pos);
        if (largo > datos.size() - pos - 12) throw Error("error.png");
        const unsigned char* tipoTrozo = p + pos + 4;
        const unsigned char* d = p + pos + 8;
        if (std::memcmp(tipoTrozo, "IHDR", 4) == 0) {
            if (largo < 13) throw Error("error.png");
            ancho = be32(d);
            alto = be32(d + 4);
            bits = d[8];
            tipo = d[9];
            if (d[10] != 0 || d[11] != 0) throw Error("error.png");
            if (d[12] != 0) throw Error("error.png_entrelazado");
        } else if (std::memcmp(tipoTrozo, "PLTE", 4) == 0) {
            for (std::uint32_t i = 0; i + 2 < largo; i += 3) paleta.push_back({d[i], d[i + 1], d[i + 2]});
        } else if (std::memcmp(tipoTrozo, "tRNS", 4) == 0) {
            if (tipo == 3) trnsPaleta.assign(d, d + largo);
            else if (tipo == 0 && largo >= 2) trnsGris = (d[0] << 8) | d[1];
            else if (tipo == 2 && largo >= 6) trnsRgb = {(d[0] << 8) | d[1], (d[2] << 8) | d[3], (d[4] << 8) | d[5]};
        } else if (std::memcmp(tipoTrozo, "IDAT", 4) == 0) {
            idat.append(reinterpret_cast<const char*>(d), largo);
        } else if (std::memcmp(tipoTrozo, "IEND", 4) == 0) {
            fin = true;
        }
        pos += 12 + largo;
    }

    int canalesPng = 0;
    switch (tipo) {
    case 0: canalesPng = 1; break;
    case 2: canalesPng = 3; break;
    case 3: canalesPng = 1; break;
    case 4: canalesPng = 2; break;
    case 6: canalesPng = 4; break;
    default: throw Error("error.png");
    }
    const bool bitsValidos = (tipo == 0 && (bits == 1 || bits == 2 || bits == 4 || bits == 8 || bits == 16)) ||
                             (tipo == 3 && (bits == 1 || bits == 2 || bits == 4 || bits == 8)) ||
                             ((tipo == 2 || tipo == 4 || tipo == 6) && (bits == 8 || bits == 16));
    if (!bitsValidos || ancho == 0 || alto == 0 || idat.empty() || (tipo == 3 && paleta.empty()))
        throw Error("error.png");
    if (std::uint64_t(ancho) * alto > kMaxPixeles) throw Error("error.png");

    const std::size_t w = ancho;
    const std::size_t h = alto;
    const std::size_t bytesFila = (w * static_cast<std::size_t>(canalesPng) * static_cast<std::size_t>(bits) + 7) / 8;
    const std::size_t bpp = std::max<std::size_t>(1, static_cast<std::size_t>(canalesPng * bits) / 8);
    std::vector<unsigned char> crudo = inflar(idat, h * (bytesFila + 1));
    idat.clear();
    idat.shrink_to_fit();
    desfiltrar(crudo, h, bytesFila, bpp);

    ImagenRaster img;
    img.ancho = static_cast<int>(w);
    img.alto = static_cast<int>(h);
    img.canales = (tipo == 0 || tipo == 4) ? 1 : 3;
    img.color.resize(w * h * static_cast<std::size_t>(img.canales));
    const bool conAlfa = tipo == 4 || tipo == 6 || !trnsPaleta.empty() || trnsGris >= 0 || trnsRgb[0] >= 0;
    if (conAlfa) img.alfa.assign(w * h, 255);

    for (std::size_t y = 0; y < h; ++y) {
        const unsigned char* fila = crudo.data() + y * (bytesFila + 1) + 1;
        for (std::size_t x = 0; x < w; ++x) {
            const std::size_t i = y * w + x;
            const std::size_t m = x * static_cast<std::size_t>(canalesPng);
            switch (tipo) {
            case 0: {
                const unsigned v = muestra(fila, m, bits);
                img.color[i] = a8(v, bits);
                if (trnsGris >= 0 && v == static_cast<unsigned>(trnsGris)) img.alfa[i] = 0;
                break;
            }
            case 2: {
                const unsigned r = muestra(fila, m, bits);
                const unsigned g = muestra(fila, m + 1, bits);
                const unsigned b = muestra(fila, m + 2, bits);
                img.color[3 * i] = a8(r, bits);
                img.color[3 * i + 1] = a8(g, bits);
                img.color[3 * i + 2] = a8(b, bits);
                if (trnsRgb[0] >= 0 && r == unsigned(trnsRgb[0]) && g == unsigned(trnsRgb[1]) && b == unsigned(trnsRgb[2]))
                    img.alfa[i] = 0;
                break;
            }
            case 3: {
                const unsigned idx = muestra(fila, m, bits);
                const auto& c = idx < paleta.size() ? paleta[idx] : paleta.front();
                img.color[3 * i] = c[0];
                img.color[3 * i + 1] = c[1];
                img.color[3 * i + 2] = c[2];
                if (idx < trnsPaleta.size()) img.alfa[i] = trnsPaleta[idx];
                break;
            }
            case 4:
                img.color[i] = a8(muestra(fila, m, bits), bits);
                img.alfa[i] = a8(muestra(fila, m + 1, bits), bits);
                break;
            case 6:
                for (int k = 0; k < 3; ++k) img.color[3 * i + static_cast<std::size_t>(k)] = a8(muestra(fila, m + static_cast<std::size_t>(k), bits), bits);
                img.alfa[i] = a8(muestra(fila, m + 3, bits), bits);
                break;
            default: break;
            }
        }
    }
    // Si resultó totalmente opaca, no hace falta máscara.
    if (conAlfa && std::all_of(img.alfa.begin(), img.alfa.end(), [](std::uint8_t a) { return a == 255; }))
        img.alfa.clear();
    return img;
}

ImagenRaster leerPng(const fs::path& ruta) {
    std::ifstream f(ruta, std::ios::binary);
    if (!f) throw Error("error.leer", utf8(ruta));
    const std::string datos((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    try {
        return leerPng(std::string_view(datos));
    } catch (const Error& e) {
        throw Error(e.clave(), utf8(ruta));
    }
}

void quitarFondoBlanco(ImagenRaster& img) {
    constexpr int kOpaco = 200;  ///< más oscuro: trazo opaco
    constexpr int kFondo = 235;  ///< más claro: fondo transparente
    const std::size_t n = static_cast<std::size_t>(img.ancho) * static_cast<std::size_t>(img.alto);
    if (img.alfa.empty()) img.alfa.assign(n, 255);
    for (std::size_t i = 0; i < n; ++i) {
        int m = img.color[i * static_cast<std::size_t>(img.canales)];
        for (int k = 1; k < img.canales; ++k) m = std::min<int>(m, img.color[i * static_cast<std::size_t>(img.canales) + static_cast<std::size_t>(k)]);
        int a = 255;
        if (m >= kFondo) a = 0;
        else if (m > kOpaco) a = (kFondo - m) * 255 / (kFondo - kOpaco);
        img.alfa[i] = static_cast<std::uint8_t>(std::min<int>(img.alfa[i], a));
    }
}

QPDFObjectHandle interno::xobjetoImagen(QPDF& pdf, const ImagenRaster& img) {
    // Imagen sin comprimir: QPDFWriter la comprime con Flate al escribir.
    const auto aCadena = [](const std::vector<std::uint8_t>& v) {
        return std::string(reinterpret_cast<const char*>(v.data()), v.size());
    };
    QPDFObjectHandle imagen = QPDFObjectHandle::newStream(&pdf, aCadena(img.color));
    QPDFObjectHandle dic = imagen.getDict();
    dic.replaceKey("/Type", QPDFObjectHandle::newName("/XObject"));
    dic.replaceKey("/Subtype", QPDFObjectHandle::newName("/Image"));
    dic.replaceKey("/Width", QPDFObjectHandle::newInteger(img.ancho));
    dic.replaceKey("/Height", QPDFObjectHandle::newInteger(img.alto));
    dic.replaceKey("/ColorSpace", QPDFObjectHandle::newName(img.canales == 1 ? "/DeviceGray" : "/DeviceRGB"));
    dic.replaceKey("/BitsPerComponent", QPDFObjectHandle::newInteger(8));
    if (!img.alfa.empty()) {
        QPDFObjectHandle mascara = QPDFObjectHandle::newStream(&pdf, aCadena(img.alfa));
        QPDFObjectHandle dm = mascara.getDict();
        dm.replaceKey("/Type", QPDFObjectHandle::newName("/XObject"));
        dm.replaceKey("/Subtype", QPDFObjectHandle::newName("/Image"));
        dm.replaceKey("/Width", QPDFObjectHandle::newInteger(img.ancho));
        dm.replaceKey("/Height", QPDFObjectHandle::newInteger(img.alto));
        dm.replaceKey("/ColorSpace", QPDFObjectHandle::newName("/DeviceGray"));
        dm.replaceKey("/BitsPerComponent", QPDFObjectHandle::newInteger(8));
        dic.replaceKey("/SMask", mascara);
    }
    return imagen;
}

void insertarImagen(Documento& doc, int pagina, double x, double y, double anchoPt, const ImagenRaster& img) {
    if (!doc.abierto()) throw Error("error.sin_documento");
    if (pagina < 0 || pagina >= doc.paginas()) throw Error("error.rangos");
    if (img.ancho <= 0 || img.alto <= 0) throw Error("error.png");
    const double w = std::clamp(anchoPt, 1.0, 5000.0);
    const double h = w * img.alto / img.ancho;

    Documento::Bytes nuevo;
    try {
        QPDF pdf;
        pdf.setSuppressWarnings(true);
        const auto& b = doc.bytes();
        pdf.processMemoryFile("documento", reinterpret_cast<const char*>(b.data()), b.size(),
                              doc.contrasena().empty() ? nullptr : doc.contrasena().c_str());
        std::vector<QPDFPageObjectHelper> paginas = QPDFPageDocumentHelper(pdf).getAllPages();
        QPDFPageObjectHelper& p = paginas[static_cast<std::size_t>(pagina)];

        const int rotacion = interno::rotacionPagina(p.getObjectHandle());
        const double rad = rotacion * 3.14159265358979323846 / 180.0;
        const double c = std::cos(rad);
        const double s = std::sin(rad);

        QPDFObjectHandle imagen = interno::xobjetoImagen(pdf, img);

        QPDFObjectHandle recursos = p.getAttribute("/Resources", true);
        if (!recursos.isDictionary()) {
            recursos = QPDFObjectHandle::newDictionary();
            p.getObjectHandle().replaceKey("/Resources", recursos);
        }
        QPDFObjectHandle xobjetos = recursos.getKey("/XObject");
        if (!xobjetos.isDictionary()) {
            xobjetos = QPDFObjectHandle::newDictionary();
            recursos.replaceKey("/XObject", xobjetos);
        }
        std::string nombre;
        for (int i = 1;; ++i) {
            nombre = "/IbzIm" + std::to_string(i);
            if (!xobjetos.hasKey(nombre)) break;
        }
        xobjetos.replaceKey(nombre, imagen);

        // Esquina inferior izquierda de la imagen derecha: (x, y) + h · «abajo» girado.
        const double ox = x + h * s;
        const double oy = y - h * c;
        std::string cs = "Q\nq\n";
        cs += numeroPdf(w * c) + " " + numeroPdf(w * s) + " " + numeroPdf(-h * s) + " " + numeroPdf(h * c) + " " +
              numeroPdf(ox) + " " + numeroPdf(oy) + " cm\n";
        cs += nombre + " Do\nQ\n";
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
