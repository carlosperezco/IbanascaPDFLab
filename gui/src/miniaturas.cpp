/**
 * @file miniaturas.cpp
 * @brief Panel de miniaturas.
 */
#include "miniaturas.h"

#include <ibanascapdfcore/documento.h>

#include <algorithm>
#include <cmath>
#include <string>

namespace ibz::gui {

namespace {
constexpr int kAncho = 110;      ///< ancho de la miniatura (px lógicos)
constexpr int kAltoMax = 180;    ///< alto máximo (páginas muy alargadas)
constexpr int kPorTanda = 4;     ///< miniaturas dibujadas en cada llamada en tiempo libre
constexpr const char* kClavePagina = "ibz-pagina";

/// Arrastre interno de miniaturas (solo dentro del programa).
GtkTargetEntry kObjetivo[] = {{const_cast<gchar*>("IBZ_PAGINA"), GTK_TARGET_SAME_APP, 0}};
} // namespace

Miniaturas::Miniaturas(const Documento& doc) : doc_(doc) {
    lista_ = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(lista_), GTK_SELECTION_SINGLE);
    g_signal_connect(lista_, "row-selected", G_CALLBACK(alSeleccionarFila), this);

    raiz_ = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(raiz_), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(raiz_, kAncho + 40, -1);
    gtk_container_add(GTK_CONTAINER(raiz_), lista_);
}

Miniaturas::~Miniaturas() {
    detener();
    if (fuenteMover_) g_source_remove(fuenteMover_);
}

void Miniaturas::detener() {
    if (fuente_) g_source_remove(fuente_);
    fuente_ = 0;
}

void Miniaturas::recargar() {
    detener();
    GList* hijos = gtk_container_get_children(GTK_CONTAINER(lista_));
    for (GList* h = hijos; h; h = h->next) gtk_widget_destroy(GTK_WIDGET(h->data));
    g_list_free(hijos);
    imagenes_.clear();
    anchoPx_.clear();
    altoPx_.clear();
    siguiente_ = 0;
    if (!doc_.abierto()) return;

    const int n = doc_.paginas();
    imagenes_.reserve(static_cast<std::size_t>(n));
    anchoPx_.reserve(static_cast<std::size_t>(n));
    altoPx_.reserve(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        const TamanoPagina& t = doc_.tamano(i);
        const double prop = t.ancho > 0.f ? t.alto / t.ancho : 1.4142;
        int w = kAncho;
        int h = static_cast<int>(std::lround(kAncho * prop));
        if (h > kAltoMax) {
            h = kAltoMax;
            w = std::max(1, static_cast<int>(std::lround(kAltoMax / prop)));
        }
        h = std::max(1, h);

        GtkWidget* img = gtk_image_new();
        gtk_widget_set_size_request(img, w, h);
        GtkWidget* num = gtk_label_new(std::to_string(i + 1).c_str());
        GtkWidget* caja = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
        gtk_widget_set_margin_top(caja, 6);
        gtk_widget_set_margin_bottom(caja, 6);
        gtk_box_pack_start(GTK_BOX(caja), img, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(caja), num, FALSE, FALSE, 0);

        // Caja de eventos: origen y destino del arrastre para reordenar.
        GtkWidget* evento = gtk_event_box_new();
        gtk_event_box_set_visible_window(GTK_EVENT_BOX(evento), FALSE);
        gtk_container_add(GTK_CONTAINER(evento), caja);
        g_object_set_data(G_OBJECT(evento), kClavePagina, GINT_TO_POINTER(i));
        gtk_drag_source_set(evento, GDK_BUTTON1_MASK, kObjetivo, 1, GDK_ACTION_MOVE);
        gtk_drag_source_set_icon_name(evento, "x-office-document");
        gtk_drag_dest_set(evento, GTK_DEST_DEFAULT_ALL, kObjetivo, 1, GDK_ACTION_MOVE);
        g_signal_connect(evento, "drag-data-get", G_CALLBACK(alDarDatos), this);
        g_signal_connect(evento, "drag-data-received", G_CALLBACK(alRecibir), this);
        gtk_list_box_insert(GTK_LIST_BOX(lista_), evento, -1);

        imagenes_.push_back(img);
        anchoPx_.push_back(w);
        altoPx_.push_back(h);
    }
    gtk_widget_show_all(lista_);
    fuente_ = g_idle_add(alInactivo, this);
}

gboolean Miniaturas::alInactivo(gpointer d) {
    auto* m = static_cast<Miniaturas*>(d);
    const int sf = gtk_widget_get_scale_factor(m->lista_);
    for (int k = 0; k < kPorTanda && m->siguiente_ < m->imagenes_.size(); ++k, ++m->siguiente_) {
        const std::size_t i = m->siguiente_;
        const int w = m->anchoPx_[i] * sf;
        const int h = m->altoPx_[i] * sf;
        cairo_surface_t* s = cairo_image_surface_create(CAIRO_FORMAT_RGB24, w, h);
        if (cairo_surface_status(s) == CAIRO_STATUS_SUCCESS) {
            cairo_surface_flush(s);
            if (m->doc_.renderizar(static_cast<int>(i), 0, 0, w, h, w, h, cairo_image_surface_get_data(s),
                                   cairo_image_surface_get_stride(s))) {
                cairo_surface_mark_dirty(s);
                cairo_surface_set_device_scale(s, sf, sf);
                gtk_image_set_from_surface(GTK_IMAGE(m->imagenes_[i]), s);
            }
        }
        cairo_surface_destroy(s); // GtkImage guarda su propia referencia
    }
    if (m->siguiente_ < m->imagenes_.size()) return G_SOURCE_CONTINUE;
    m->fuente_ = 0;
    return G_SOURCE_REMOVE;
}

void Miniaturas::seleccionar(int pagina) {
    GtkListBoxRow* fila = gtk_list_box_get_row_at_index(GTK_LIST_BOX(lista_), pagina);
    if (!fila || fila == gtk_list_box_get_selected_row(GTK_LIST_BOX(lista_))) return;
    silencio_ = true;
    gtk_list_box_select_row(GTK_LIST_BOX(lista_), fila);
    silencio_ = false;
    asegurarVisible(GTK_WIDGET(fila));
}

void Miniaturas::asegurarVisible(GtkWidget* fila) {
    GtkAllocation a;
    gtk_widget_get_allocation(fila, &a);
    if (a.height <= 1) return; // aún sin tamaño
    GtkAdjustment* adj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(raiz_));
    const double v = gtk_adjustment_get_value(adj);
    const double p = gtk_adjustment_get_page_size(adj);
    if (a.y < v) {
        gtk_adjustment_set_value(adj, a.y);
    } else if (a.y + a.height > v + p) {
        gtk_adjustment_set_value(adj, a.y + a.height - p);
    }
}

void Miniaturas::alDarDatos(GtkWidget* w, GdkDragContext*, GtkSelectionData* datos, guint, guint, gpointer) {
    const gint32 pagina = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(w), kClavePagina));
    gtk_selection_data_set(datos, gtk_selection_data_get_target(datos), 32,
                           reinterpret_cast<const guchar*>(&pagina), sizeof(pagina));
}

void Miniaturas::alRecibir(GtkWidget* w, GdkDragContext*, gint, gint y, GtkSelectionData* datos, guint, guint,
                           gpointer d) {
    auto* m = static_cast<Miniaturas*>(d);
    if (gtk_selection_data_get_length(datos) != static_cast<gint>(sizeof(gint32)) || !m->alMover) return;
    gint32 origen = 0;
    std::copy_n(gtk_selection_data_get_data(datos), sizeof(origen), reinterpret_cast<guchar*>(&origen));
    const int sobre = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(w), kClavePagina));

    // Mitad superior: antes de esa página; mitad inferior: después.
    const int insertar = (y > gtk_widget_get_allocated_height(w) / 2) ? sobre + 1 : sobre;
    const int destino = insertar > origen ? insertar - 1 : insertar;
    if (destino == origen) return;

    // La lista se reconstruye al mover: se hace fuera de la señal del arrastre.
    m->moverOrigen_ = origen;
    m->moverDestino_ = destino;
    if (!m->fuenteMover_) {
        m->fuenteMover_ = g_idle_add(
            [](gpointer p) -> gboolean {
                auto* mm = static_cast<Miniaturas*>(p);
                mm->fuenteMover_ = 0;
                if (mm->alMover) mm->alMover(mm->moverOrigen_, mm->moverDestino_);
                return G_SOURCE_REMOVE;
            },
            m);
    }
}

void Miniaturas::alSeleccionarFila(GtkListBox*, GtkListBoxRow* fila, gpointer d) {
    auto* m = static_cast<Miniaturas*>(d);
    if (m->silencio_ || !fila || !m->alElegir) return;
    m->alElegir(gtk_list_box_row_get_index(fila));
}

} // namespace ibz::gui
