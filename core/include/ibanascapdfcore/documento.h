/**
 * @file documento.h
 * @brief Documento PDF abierto: bytes en memoria, vista PDFium y pila de deshacer.
 *
 * Modelo de trabajo: el PDF completo vive en memoria (`bytes()`). Cada
 * operación produce un PDF nuevo que se entrega con reemplazar(); PDFium lo
 * vuelve a cargar desde memoria, sin archivos temporales. El archivo original
 * nunca se modifica: solo se guarda con guardarComo() en otra ruta.
 */
#pragma once

#include <cstddef>
#include <deque>
#include <filesystem>
#include <string>
#include <vector>

namespace ibz {

/// Inicializa PDFium (una vez, al arrancar).
void iniciarPdf();
/// Libera PDFium (al salir, con todos los documentos cerrados).
void cerrarPdf();

/// Tamaño de una página en puntos PDF (1/72"), tal como se ve (rotación aplicada).
struct TamanoPagina {
    float ancho = 0.f;
    float alto = 0.f;
};

/// Documento PDF abierto.
class Documento {
public:
    using Bytes = std::vector<unsigned char>;

    /// Máximo de pasos que se pueden deshacer.
    static constexpr std::size_t kMaxDeshacer = 20;

    Documento() = default;
    ~Documento();
    Documento(const Documento&) = delete;
    Documento& operator=(const Documento&) = delete;

    /**
     * @brief Abre un archivo. Si falla, el documento anterior sigue abierto.
     * @throws ibz::Error ("error.leer", "error.formato", "error.contrasena", "error.seguridad").
     */
    void abrir(const std::filesystem::path& ruta, const std::string& contrasena = {});

    /// Cierra el documento y vacía la pila de deshacer.
    void cerrar() noexcept;

    bool abierto() const noexcept { return doc_ != nullptr; }
    int paginas() const noexcept { return static_cast<int>(tamanos_.size()); }
    const TamanoPagina& tamano(int pagina) const { return tamanos_.at(static_cast<std::size_t>(pagina)); }

    /**
     * @brief Dibuja (parte de) una página en un búfer BGRx de 32 bits
     *        (mismo formato que CAIRO_FORMAT_RGB24 en little-endian).
     *
     * La página completa mide @p anchoPagina × @p altoPagina píxeles y su esquina
     * superior izquierda cae en (@p inicioX, @p inicioY) del búfer; así se puede
     * dibujar solo la parte visible con desplazamientos negativos.
     * @return false si la página no se pudo cargar.
     */
    bool renderizar(int pagina, int inicioX, int inicioY, int anchoPagina, int altoPagina,
                    int anchoBufer, int altoBufer, unsigned char* destino, int paso) const noexcept;

    /**
     * @brief Convierte un punto de la página dibujada (píxeles) a puntos PDF del espacio de usuario.
     * @param anchoPx, altoPx Tamaño con que se dibujó la página completa.
     * @param x, y           Punto pulsado, relativo a la esquina superior izquierda de la página.
     */
    bool dispositivoAPagina(int pagina, int anchoPx, int altoPx, int x, int y, double& px, double& py) const noexcept;

    /// PDF actual (para las operaciones de los módulos).
    const Bytes& bytes() const noexcept { return bytes_; }

    /**
     * @brief Sustituye el PDF por el resultado de una operación (se puede deshacer).
     * @throws ibz::Error("error.formato") si PDFium no puede abrir el resultado;
     *         en ese caso el documento no cambia.
     */
    void reemplazar(Bytes&& nuevos);

    bool puedeDeshacer() const noexcept { return !deshacer_.empty(); }
    bool puedeRehacer() const noexcept { return !rehacer_.empty(); }
    /// Vuelve al estado anterior a la última operación.
    void deshacer();
    /// Vuelve a aplicar la última operación deshecha.
    void rehacer();

    /// Hay cambios sin guardar.
    bool modificado() const noexcept { return modificado_; }

    /**
     * @brief Guarda el PDF actual en otra ruta.
     * @throws ibz::Error("error.sobrescribir_original") si es el archivo abierto;
     *         ibz::Error("error.escribir") si no se puede escribir.
     */
    void guardarComo(const std::filesystem::path& ruta);

    /// Archivo original.
    const std::filesystem::path& ruta() const noexcept { return ruta_; }
    /// Contraseña con la que se abrió (vacía si no tenía).
    const std::string& contrasena() const noexcept { return contrasena_; }

private:
    /// Abre @p datos con PDFium; si va bien, sustituye el documento actual.
    void cargar(Bytes&& datos, const std::string& contrasena, const std::string& detalle);

    void* doc_ = nullptr;  ///< FPDF_DOCUMENT (opaco para no exponer PDFium)
    Bytes bytes_;          ///< PDF actual; PDFium lee directamente de aquí
    std::deque<Bytes> deshacer_;
    std::deque<Bytes> rehacer_;
    std::vector<TamanoPagina> tamanos_;
    std::filesystem::path ruta_;
    std::string contrasena_;
    bool modificado_ = false;
};

} // namespace ibz
