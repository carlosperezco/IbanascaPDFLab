/**
 * @file documento.cpp
 * @brief Documento PDF sobre PDFium.
 */
#include "ibanascapdfcore/documento.h"

#include "ibanascapdfcore/error.h"
#include "ibanascapdfcore/rutas.h"

#include <fpdfview.h>

#include <fstream>
#include <mutex>
#include <system_error>
#include <utility>

namespace fs = std::filesystem;

namespace ibz {

namespace {

inline FPDF_DOCUMENT D(void* p) noexcept { return static_cast<FPDF_DOCUMENT>(p); }

bool pdfIniciado = false;

/// Traduce el último error de PDFium a una clave.
[[noreturn]] void lanzarErrorPdfium(const std::string& detalle) {
    switch (FPDF_GetLastError()) {
    case FPDF_ERR_PASSWORD: throw Error("error.contrasena", detalle);
    case FPDF_ERR_SECURITY: throw Error("error.seguridad", detalle);
    case FPDF_ERR_FILE: throw Error("error.leer", detalle);
    default: throw Error("error.formato", detalle);
    }
}

} // namespace

void iniciarPdf() {
    static std::once_flag una;
    std::call_once(una, [] {
        FPDF_LIBRARY_CONFIG cfg{};
        cfg.version = 2;
        FPDF_InitLibraryWithConfig(&cfg);
        pdfIniciado = true;
    });
}

void cerrarPdf() {
    if (pdfIniciado) {
        FPDF_DestroyLibrary();
        pdfIniciado = false;
    }
}

Documento::~Documento() {
    cerrar();
}

void Documento::cerrar() noexcept {
    if (doc_) FPDF_CloseDocument(D(doc_));
    doc_ = nullptr;
    bytes_.clear();
    bytes_.shrink_to_fit();
    deshacer_.clear();
    rehacer_.clear();
    tamanos_.clear();
    ruta_.clear();
    contrasena_.clear();
    modificado_ = false;
    esNuevo_ = false;
}

void Documento::nuevo(Bytes&& datos, const std::string& nombre) {
    cargar(std::move(datos), {}, nombre);
    deshacer_.clear();
    rehacer_.clear();
    ruta_ = desdeUtf8(nombre);
    contrasena_.clear();
    modificado_ = true;
    esNuevo_ = true;
}

void Documento::abrir(const fs::path& ruta, const std::string& contrasena) {
    const std::string nombre = utf8(ruta);
    Bytes datos;
    {
        std::ifstream f(ruta, std::ios::binary | std::ios::ate);
        if (!f) throw Error("error.leer", nombre);
        const std::streamoff tam = f.tellg();
        if (tam <= 0) throw Error("error.formato", nombre);
        datos.resize(static_cast<std::size_t>(tam));
        f.seekg(0);
        if (!f.read(reinterpret_cast<char*>(datos.data()), tam)) throw Error("error.leer", nombre);
    }
    cargar(std::move(datos), contrasena, nombre);
    deshacer_.clear();
    rehacer_.clear();
    ruta_ = ruta;
    contrasena_ = contrasena;
    modificado_ = false;
    esNuevo_ = false;
}

void Documento::abrirMemoria(Bytes&& datos, const std::string& nombre, const std::string& contrasena) {
    if (datos.empty()) throw Error("error.formato", nombre);
    cargar(std::move(datos), contrasena, nombre);
    deshacer_.clear();
    rehacer_.clear();
    ruta_ = desdeUtf8(nombre);
    contrasena_ = contrasena;
    modificado_ = false;
    esNuevo_ = false;
}

void Documento::marcarGuardado(const std::string& nombre) {
    ruta_ = desdeUtf8(nombre);
    modificado_ = false;
    esNuevo_ = false;
}

void Documento::cargar(Bytes&& datos, const std::string& contrasena, const std::string& detalle) {
    // PDFium lee del búfer mientras el documento esté abierto: el vector se mueve
    // (no se copia), así que su memoria sigue siendo la misma.
    FPDF_DOCUMENT nuevo = FPDF_LoadMemDocument64(datos.data(), datos.size(),
                                                  contrasena.empty() ? nullptr : contrasena.c_str());
    if (!nuevo) lanzarErrorPdfium(detalle);

    const int n = FPDF_GetPageCount(nuevo);
    std::vector<TamanoPagina> tamanos(static_cast<std::size_t>(n > 0 ? n : 0));
    for (int i = 0; i < n; ++i) {
        FS_SIZEF t{};
        if (FPDF_GetPageSizeByIndexF(nuevo, i, &t)) tamanos[static_cast<std::size_t>(i)] = {t.width, t.height};
    }

    if (doc_) FPDF_CloseDocument(D(doc_));
    doc_ = nuevo;
    bytes_ = std::move(datos);
    tamanos_ = std::move(tamanos);
}

bool Documento::renderizar(int pagina, int inicioX, int inicioY, int anchoPagina, int altoPagina,
                           int anchoBufer, int altoBufer, unsigned char* destino, int paso) const noexcept {
    if (!doc_ || pagina < 0 || pagina >= paginas() || !destino || anchoBufer <= 0 || altoBufer <= 0) return false;
    FPDF_PAGE p = FPDF_LoadPage(D(doc_), pagina);
    if (!p) return false;
    FPDF_BITMAP b = FPDFBitmap_CreateEx(anchoBufer, altoBufer, FPDFBitmap_BGRx, destino, paso);
    if (!b) {
        FPDF_ClosePage(p);
        return false;
    }
    FPDFBitmap_FillRect(b, 0, 0, anchoBufer, altoBufer, 0xFFFFFFFF);
    FPDF_RenderPageBitmap(b, p, inicioX, inicioY, anchoPagina, altoPagina, 0, FPDF_ANNOT);
    FPDFBitmap_Destroy(b);
    FPDF_ClosePage(p);
    return true;
}

bool Documento::dispositivoAPagina(int pagina, int anchoPx, int altoPx, int x, int y, double& px,
                                   double& py) const noexcept {
    if (!doc_ || pagina < 0 || pagina >= paginas()) return false;
    FPDF_PAGE p = FPDF_LoadPage(D(doc_), pagina);
    if (!p) return false;
    const bool ok = FPDF_DeviceToPage(p, 0, 0, anchoPx, altoPx, 0, x, y, &px, &py);
    FPDF_ClosePage(p);
    return ok;
}

void Documento::reemplazar(Bytes&& nuevos) {
    if (!doc_) throw Error("error.sin_documento");
    Bytes anterior = std::move(bytes_);
    void* docAnterior = doc_;
    doc_ = nullptr; // cargar() no debe cerrar el anterior hasta validar el nuevo
    try {
        cargar(std::move(nuevos), contrasena_, utf8(ruta_));
    } catch (...) {
        doc_ = docAnterior;
        bytes_ = std::move(anterior);
        throw;
    }
    FPDF_CloseDocument(D(docAnterior));
    deshacer_.push_back(std::move(anterior));
    if (deshacer_.size() > kMaxDeshacer) deshacer_.pop_front();
    rehacer_.clear();
    modificado_ = true;
}

namespace {
/// Carga el último estado de @p desde y guarda el actual en @p hacia (deshacer/rehacer).
template <class Cargar>
void cambiarEstado(std::deque<Documento::Bytes>& desde, std::deque<Documento::Bytes>& hacia,
                   Documento::Bytes& actualBytes, Cargar&& cargar) {
    // El búfer actual sigue vivo (movido, no copiado) mientras PDFium cierra el documento.
    Documento::Bytes actual = std::move(actualBytes);
    try {
        cargar(std::move(desde.back()));
    } catch (...) {
        actualBytes = std::move(actual);
        throw;
    }
    desde.pop_back();
    hacia.push_back(std::move(actual));
    if (hacia.size() > Documento::kMaxDeshacer) hacia.pop_front();
}
} // namespace

void Documento::deshacer() {
    if (deshacer_.empty()) return;
    cambiarEstado(deshacer_, rehacer_, bytes_,
                  [this](Bytes&& b) { cargar(std::move(b), contrasena_, utf8(ruta_)); });
    modificado_ = true;
}

void Documento::rehacer() {
    if (rehacer_.empty()) return;
    cambiarEstado(rehacer_, deshacer_, bytes_,
                  [this](Bytes&& b) { cargar(std::move(b), contrasena_, utf8(ruta_)); });
    modificado_ = true;
}

void Documento::guardarComo(const fs::path& ruta) {
    if (!doc_) throw Error("error.sin_documento");
    const std::string nombre = utf8(ruta);
    std::error_code ec;
    if (!esNuevo_ && fs::exists(ruta, ec) && fs::equivalent(ruta, ruta_, ec))
        throw Error("error.sobrescribir_original", nombre);

    fs::path temporal = ruta;
    temporal += ".tmp";
    {
        std::ofstream f(temporal, std::ios::binary | std::ios::trunc);
        if (!f) throw Error("error.escribir", nombre);
        f.write(reinterpret_cast<const char*>(bytes_.data()), static_cast<std::streamsize>(bytes_.size()));
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
    if (esNuevo_) {
        // El documento nuevo pasa a ser este archivo (como si se hubiera abierto).
        ruta_ = ruta;
        esNuevo_ = false;
    }
    modificado_ = false;
}

} // namespace ibz
