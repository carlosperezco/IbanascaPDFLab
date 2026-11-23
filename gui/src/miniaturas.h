/**
 * @file miniaturas.h
 * @brief Panel lateral de miniaturas de página.
 *
 * Las miniaturas se dibujan poco a poco en tiempo libre (g_idle_add), para
 * que abrir un documento grande no bloquee la ventana.
 */
#pragma once

#include <gtk/gtk.h>

#include <functional>
#include <vector>

namespace ibz {
class Documento;
}

namespace ibz::gui {

/// Lista de miniaturas; al pulsar una se avisa con alElegir.
class Miniaturas {
public:
    explicit Miniaturas(const Documento& doc);
    ~Miniaturas();
    Miniaturas(const Miniaturas&) = delete;
    Miniaturas& operator=(const Miniaturas&) = delete;

    GtkWidget* widget() const noexcept { return raiz_; }

    /// Vuelve a crear las miniaturas a partir del documento.
    void recargar();

    /// Marca la página @p pagina sin avisar (la vista cambió de página).
    void seleccionar(int pagina);

    /// Se llama cuando el usuario elige una página (base 0).
    std::function<void(int)> alElegir;

    /**
     * @brief Se llama al arrastrar una miniatura a otro sitio.
     * @param origen  Página arrastrada (base 0).
     * @param destino Posición que debe ocupar en el documento resultante (base 0).
     */
    std::function<void(int, int)> alMover;

private:
    static gboolean alInactivo(gpointer d);
    static void alSeleccionarFila(GtkListBox* lista, GtkListBoxRow* fila, gpointer d);
    static void alDarDatos(GtkWidget* w, GdkDragContext* ctx, GtkSelectionData* datos, guint info, guint tiempo,
                           gpointer d);
    static void alRecibir(GtkWidget* w, GdkDragContext* ctx, gint x, gint y, GtkSelectionData* datos, guint info,
                          guint tiempo, gpointer d);
    void asegurarVisible(GtkWidget* fila);
    void detener();

    const Documento& doc_;
    GtkWidget* raiz_ = nullptr;
    GtkWidget* lista_ = nullptr;
    std::vector<GtkWidget*> imagenes_;
    std::vector<int> anchoPx_;
    std::vector<int> altoPx_;
    std::size_t siguiente_ = 0;
    guint fuente_ = 0;
    guint fuenteMover_ = 0;
    int moverOrigen_ = 0;
    int moverDestino_ = 0;
    bool silencio_ = false;
};

} // namespace ibz::gui
