/**
 * @file visor.h
 * @brief Vista continua de las páginas (PDFium → cairo).
 *
 * No usa un widget del tamaño del documento (los sistemas de ventanas limitan
 * su tamaño): dibuja solo las páginas visibles en un GtkDrawingArea y maneja
 * las barras de desplazamiento con dos GtkAdjustment propios.
 */
#pragma once

#include <gtk/gtk.h>

#include <functional>
#include <vector>

namespace ibz {
class Documento;
}

namespace ibz::gui {

/// Visor de páginas con desplazamiento continuo y zoom.
class Visor {
public:
    explicit Visor(const Documento& doc);
    ~Visor();
    Visor(const Visor&) = delete;
    Visor& operator=(const Visor&) = delete;

    /// Widget raíz para insertar en la ventana.
    GtkWidget* widget() const noexcept { return raiz_; }

    /**
     * @brief El documento cambió (abierto, cerrado o modificado).
     * @param conservarPosicion Mantener la página visible (tras una operación).
     */
    void recargar(bool conservarPosicion = false);

    /// Desplaza la vista al principio de la página @p pagina (base 0).
    void irAPagina(int pagina);
    /// Página visible (base 0).
    int paginaActual() const noexcept { return actual_; }

    /// Pone el foco del teclado en la vista (flechas, Inicio, Fin).
    void enfocar() { gtk_widget_grab_focus(area_); }

    /// Modo «colocar»: el siguiente clic sobre una página llama a alColocar (Esc cancela).
    void empezarColocar();
    void cancelarColocar();
    bool colocando() const noexcept { return colocando_; }

    /**
     * @brief Clic en modo colocar.
     * @param pagina Página pulsada (base 0).
     * @param x, y   Punto relativo a la esquina superior izquierda de la página (px lógicos).
     * @param ancho, alto Tamaño de la página dibujada (px lógicos).
     */
    std::function<void(int pagina, int x, int y, int ancho, int alto)> alColocar;
    /// El modo colocar terminó sin clic (Esc o cancelado).
    std::function<void()> alCancelarColocar;

    /// Zoom: 1.0 = tamaño real (96 ppp).
    void ponerZoom(double zoom);
    void multiplicarZoom(double factor) { ponerZoom(zoom_ * factor); }
    /// Ajusta el zoom al ancho de la ventana y lo mantiene al redimensionar.
    void ajustarAncho();
    /// Ajusta el zoom para ver la página actual completa y lo mantiene al redimensionar.
    void ajustarPagina();
    double zoom() const noexcept { return zoom_; }

    /// Se llama cuando cambian la página actual o el zoom.
    std::function<void(int)> alCambiar;

private:
    struct Superficie {
        int pagina = -1;
        cairo_surface_t* s = nullptr;
        unsigned uso = 0;
    };
    enum class Ajuste { Ninguno, Ancho, Pagina };
    struct Ancla {
        int pagina = 0;
        double fraccion = 0.0;
    };

    static gboolean alDibujar(GtkWidget* w, cairo_t* cr, gpointer d);
    static void alAsignar(GtkWidget* w, GdkRectangle* r, gpointer d);
    static gboolean alRueda(GtkWidget* w, GdkEventScroll* ev, gpointer d);
    static gboolean alTecla(GtkWidget* w, GdkEventKey* ev, gpointer d);
    static gboolean alPulsar(GtkWidget* w, GdkEventButton* ev, gpointer d);
    static void alMover(GtkAdjustment* a, gpointer d);

    void aplicarAjuste(Ajuste a);
    void calcularDisposicion();
    void actualizarAjustadores();
    Ancla tomarAncla() const;
    void restaurarAncla(const Ancla& a);
    int paginaEn(double y) const;
    void detectarPagina();
    void ponerCursor(const char* nombre);
    double inicioX(std::size_t pagina) const;
    void avisar();
    void vaciarCache();
    cairo_surface_t* superficie(int pagina, int sf);
    void dibujarPagina(cairo_t* cr, int pagina, double x, double y, int vw, int vh, int sf);

    const Documento& doc_;
    GtkWidget* raiz_ = nullptr;
    GtkWidget* area_ = nullptr;
    GtkWidget* barraH_ = nullptr;
    GtkAdjustment* hadj_ = nullptr;
    GtkAdjustment* vadj_ = nullptr;

    std::vector<double> y_;   ///< borde superior de cada página (px lógicos)
    std::vector<int> ancho_;  ///< ancho de cada página (px lógicos)
    std::vector<int> alto_;   ///< alto de cada página (px lógicos)
    double totalAncho_ = 0.0;
    double totalAlto_ = 0.0;
    double zoom_ = 1.0;
    double zoomCache_ = 0.0;
    int sfCache_ = 0;
    Ajuste ajuste_ = Ajuste::Ancho;
    int actual_ = 0;
    bool colocando_ = false;
    unsigned reloj_ = 0;
    guint idleBarra_ = 0;
    std::vector<Superficie> cache_;
};

} // namespace ibz::gui
