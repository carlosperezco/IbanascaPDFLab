/**
 * @file visor.cpp
 * @brief Vista continua de páginas.
 */
#include "visor.h"

#include <ibanascapdfcore/documento.h>

#include <algorithm>
#include <cmath>

namespace ibz::gui {

namespace {
constexpr int kMargen = 16;                         ///< separación entre páginas (px)
constexpr double kPxPorPunto = 96.0 / 72.0;         ///< zoom 1.0 = tamaño real a 96 ppp
constexpr std::size_t kMaxCache = 8;                ///< páginas dibujadas que se guardan
constexpr long long kMaxPixelesCache = 16LL << 20;  ///< más grande: se dibuja solo lo visible
constexpr double kZoomMin = 0.10;
constexpr double kZoomMax = 4.0;
constexpr double kPasoRueda = 60.0;
constexpr double kPasoTecla = 48.0;
} // namespace

Visor::Visor(const Documento& doc) : doc_(doc) {
    hadj_ = gtk_adjustment_new(0, 0, 1, kPasoTecla, 100, 1);
    vadj_ = gtk_adjustment_new(0, 0, 1, kPasoTecla, 100, 1);

    area_ = gtk_drawing_area_new();
    gtk_widget_set_hexpand(area_, TRUE);
    gtk_widget_set_vexpand(area_, TRUE);
    gtk_widget_set_can_focus(area_, TRUE);
    gtk_widget_add_events(area_, GDK_SCROLL_MASK | GDK_SMOOTH_SCROLL_MASK | GDK_BUTTON_PRESS_MASK | GDK_KEY_PRESS_MASK);

    GtkWidget* barraV = gtk_scrollbar_new(GTK_ORIENTATION_VERTICAL, vadj_);
    barraH_ = gtk_scrollbar_new(GTK_ORIENTATION_HORIZONTAL, hadj_);
    gtk_widget_set_no_show_all(barraH_, TRUE);

    raiz_ = gtk_grid_new();
    gtk_grid_attach(GTK_GRID(raiz_), area_, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(raiz_), barraV, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(raiz_), barraH_, 0, 1, 1, 1);

    g_signal_connect(area_, "draw", G_CALLBACK(alDibujar), this);
    g_signal_connect(area_, "size-allocate", G_CALLBACK(alAsignar), this);
    g_signal_connect(area_, "scroll-event", G_CALLBACK(alRueda), this);
    g_signal_connect(area_, "key-press-event", G_CALLBACK(alTecla), this);
    g_signal_connect(area_, "button-press-event", G_CALLBACK(alPulsar), this);
    g_signal_connect(vadj_, "value-changed", G_CALLBACK(alMover), this);
    g_signal_connect(hadj_, "value-changed", G_CALLBACK(alMover), this);
}

Visor::~Visor() {
    if (idleBarra_) g_source_remove(idleBarra_);
    vaciarCache();
}

void Visor::recargar(bool conservarPosicion) {
    if (colocando_ && !doc_.abierto()) cancelarColocar();
    const Ancla a = tomarAncla();
    vaciarCache();
    zoomCache_ = 0.0;
    calcularDisposicion();
    if (conservarPosicion) {
        restaurarAncla(a);
    } else {
        actual_ = 0;
        gtk_adjustment_set_value(vadj_, 0.0);
        gtk_adjustment_set_value(hadj_, 0.0);
    }
    detectarPagina();
    gtk_widget_queue_draw(area_);
    avisar();
}

void Visor::irAPagina(int pagina) {
    if (y_.empty()) return;
    pagina = std::clamp(pagina, 0, static_cast<int>(y_.size()) - 1);
    gtk_adjustment_set_value(vadj_, y_[static_cast<std::size_t>(pagina)] - kMargen / 2.0);
    actual_ = pagina;
    avisar();
}

void Visor::ponerZoom(double zoom) {
    ajuste_ = Ajuste::Ninguno;
    const Ancla a = tomarAncla();
    zoom_ = std::clamp(zoom, kZoomMin, kZoomMax);
    calcularDisposicion();
    restaurarAncla(a);
    gtk_widget_queue_draw(area_);
    avisar();
}

void Visor::ajustarAncho() {
    aplicarAjuste(Ajuste::Ancho);
}

void Visor::ajustarPagina() {
    aplicarAjuste(Ajuste::Pagina);
}

void Visor::aplicarAjuste(Ajuste a) {
    ajuste_ = a;
    const Ancla ancla = tomarAncla();
    const int pagina = actual_;
    calcularDisposicion();
    if (a == Ajuste::Pagina && !y_.empty()) {
        // La página actual queda entera y centrada en la vista.
        const int i = std::min(pagina, static_cast<int>(y_.size()) - 1);
        gtk_adjustment_set_value(vadj_, y_[static_cast<std::size_t>(i)] - kMargen);
        actual_ = i;
    } else {
        restaurarAncla(ancla);
    }
    gtk_widget_queue_draw(area_);
    avisar();
}

void Visor::calcularDisposicion() {
    const int n = doc_.abierto() ? doc_.paginas() : 0;
    const std::size_t un = static_cast<std::size_t>(n);
    y_.assign(un, 0.0);
    ancho_.assign(un, 0);
    alto_.assign(un, 0);

    const int vw = gtk_widget_get_allocated_width(area_);
    const int vh = gtk_widget_get_allocated_height(area_);
    if (n > 0 && ajuste_ == Ajuste::Ancho && vw > 2 * kMargen) {
        float maxPt = 0.f;
        for (int i = 0; i < n; ++i) maxPt = std::max(maxPt, doc_.tamano(i).ancho);
        if (maxPt > 0.f)
            zoom_ = std::clamp((vw - 2.0 * kMargen - 1.0) / (maxPt * kPxPorPunto), kZoomMin, kZoomMax);
    } else if (n > 0 && ajuste_ == Ajuste::Pagina && vw > 2 * kMargen && vh > 2 * kMargen) {
        const TamanoPagina& t = doc_.tamano(std::clamp(actual_, 0, n - 1));
        if (t.ancho > 0.f && t.alto > 0.f) {
            const double zw = (vw - 2.0 * kMargen - 1.0) / (t.ancho * kPxPorPunto);
            const double zh = (vh - 2.0 * kMargen - 1.0) / (t.alto * kPxPorPunto);
            zoom_ = std::clamp(std::min(zw, zh), kZoomMin, kZoomMax);
        }
    }
    if (zoom_ != zoomCache_) {
        vaciarCache();
        zoomCache_ = zoom_;
    }

    const double esc = zoom_ * kPxPorPunto;
    double y = kMargen;
    int maxW = 0;
    for (std::size_t i = 0; i < un; ++i) {
        const TamanoPagina& t = doc_.tamano(static_cast<int>(i));
        ancho_[i] = std::max(1, static_cast<int>(std::lround(t.ancho * esc)));
        alto_[i] = std::max(1, static_cast<int>(std::lround(t.alto * esc)));
        y_[i] = y;
        y += alto_[i] + kMargen;
        maxW = std::max(maxW, ancho_[i]);
    }
    totalAlto_ = n ? y : 0.0;
    totalAncho_ = n ? maxW + 2.0 * kMargen : 0.0;
    actualizarAjustadores();
}

void Visor::actualizarAjustadores() {
    const double vw = gtk_widget_get_allocated_width(area_);
    const double vh = gtk_widget_get_allocated_height(area_);
    const double alto = std::max(totalAlto_, vh);
    const double ancho = std::max(totalAncho_, vw);
    gtk_adjustment_configure(vadj_, std::clamp(gtk_adjustment_get_value(vadj_), 0.0, alto - vh), 0.0, alto,
                             kPasoTecla, vh * 0.9, vh);
    gtk_adjustment_configure(hadj_, std::clamp(gtk_adjustment_get_value(hadj_), 0.0, ancho - vw), 0.0, ancho,
                             kPasoTecla, vw * 0.9, vw);

    // Mostrar u ocultar la barra horizontal fuera de la asignación de tamaño.
    const bool necesaria = totalAncho_ > vw + 1.0;
    if (necesaria != static_cast<bool>(gtk_widget_get_visible(barraH_)) && !idleBarra_) {
        idleBarra_ = g_idle_add(
            [](gpointer d) -> gboolean {
                auto* v = static_cast<Visor*>(d);
                v->idleBarra_ = 0;
                const double w = gtk_widget_get_allocated_width(v->area_);
                gtk_widget_set_visible(v->barraH_, v->totalAncho_ > w + 1.0);
                return G_SOURCE_REMOVE;
            },
            this);
    }
}

Visor::Ancla Visor::tomarAncla() const {
    if (y_.empty()) return {};
    const double v = gtk_adjustment_get_value(vadj_);
    const int i = paginaEn(v);
    const std::size_t ui = static_cast<std::size_t>(i);
    return {i, (v - y_[ui]) / (alto_[ui] + kMargen)};
}

void Visor::restaurarAncla(const Ancla& a) {
    if (y_.empty()) return;
    const std::size_t i = static_cast<std::size_t>(std::min(a.pagina, static_cast<int>(y_.size()) - 1));
    gtk_adjustment_set_value(vadj_, y_[i] + a.fraccion * (alto_[i] + kMargen));
}

int Visor::paginaEn(double y) const {
    if (y_.empty()) return 0;
    const auto it = std::upper_bound(y_.begin(), y_.end(), y);
    return std::clamp(static_cast<int>(it - y_.begin()) - 1, 0, static_cast<int>(y_.size()) - 1);
}

void Visor::detectarPagina() {
    if (y_.empty()) return;
    const int i = paginaEn(gtk_adjustment_get_value(vadj_) + gtk_adjustment_get_page_size(vadj_) / 3.0);
    if (i != actual_) {
        actual_ = i;
        avisar();
    }
}

void Visor::avisar() {
    if (alCambiar) alCambiar(actual_);
}

void Visor::vaciarCache() {
    for (Superficie& c : cache_) cairo_surface_destroy(c.s);
    cache_.clear();
}

cairo_surface_t* Visor::superficie(int pagina, int sf) {
    if (sf != sfCache_) {
        vaciarCache();
        sfCache_ = sf;
    }
    for (Superficie& c : cache_) {
        if (c.pagina == pagina) {
            c.uso = ++reloj_;
            return c.s;
        }
    }
    const std::size_t i = static_cast<std::size_t>(pagina);
    const int w = ancho_[i] * sf;
    const int h = alto_[i] * sf;
    cairo_surface_t* s = cairo_image_surface_create(CAIRO_FORMAT_RGB24, w, h);
    if (cairo_surface_status(s) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(s);
        return nullptr;
    }
    cairo_surface_flush(s);
    if (!doc_.renderizar(pagina, 0, 0, w, h, w, h, cairo_image_surface_get_data(s),
                         cairo_image_surface_get_stride(s))) {
        cairo_surface_destroy(s);
        return nullptr;
    }
    cairo_surface_mark_dirty(s);
    cairo_surface_set_device_scale(s, sf, sf);

    if (cache_.size() < kMaxCache) {
        cache_.push_back({pagina, s, ++reloj_});
    } else {
        auto viejo = std::min_element(cache_.begin(), cache_.end(),
                                      [](const Superficie& a, const Superficie& b) { return a.uso < b.uso; });
        cairo_surface_destroy(viejo->s);
        *viejo = {pagina, s, ++reloj_};
    }
    return s;
}

void Visor::dibujarPagina(cairo_t* cr, int pagina, double x, double y, int vw, int vh, int sf) {
    const std::size_t i = static_cast<std::size_t>(pagina);
    const int w = ancho_[i];
    const int h = alto_[i];

    if (static_cast<long long>(w) * sf * h * sf <= kMaxPixelesCache) {
        if (cairo_surface_t* s = superficie(pagina, sf)) {
            cairo_set_source_surface(cr, s, x, y);
        } else {
            cairo_set_source_rgb(cr, 1, 1, 1);
        }
        cairo_rectangle(cr, x, y, w, h);
        cairo_fill(cr);
        return;
    }

    // Página muy grande (zoom alto): se dibuja solo la parte visible, sin guardar.
    const double x0 = std::max(x, 0.0);
    const double y0 = std::max(y, 0.0);
    const double x1 = std::min(x + w, static_cast<double>(vw));
    const double y1 = std::min(y + h, static_cast<double>(vh));
    if (x1 <= x0 || y1 <= y0) return;
    const int tw = static_cast<int>(std::ceil((x1 - x0) * sf));
    const int th = static_cast<int>(std::ceil((y1 - y0) * sf));
    cairo_surface_t* s = cairo_image_surface_create(CAIRO_FORMAT_RGB24, tw, th);
    if (cairo_surface_status(s) == CAIRO_STATUS_SUCCESS) {
        cairo_surface_flush(s);
        const bool ok = doc_.renderizar(pagina, static_cast<int>(std::lround((x - x0) * sf)),
                                        static_cast<int>(std::lround((y - y0) * sf)), w * sf, h * sf, tw, th,
                                        cairo_image_surface_get_data(s), cairo_image_surface_get_stride(s));
        cairo_surface_mark_dirty(s);
        cairo_surface_set_device_scale(s, sf, sf);
        if (ok) {
            cairo_set_source_surface(cr, s, x0, y0);
        } else {
            cairo_set_source_rgb(cr, 1, 1, 1);
        }
        cairo_rectangle(cr, x0, y0, x1 - x0, y1 - y0);
        cairo_fill(cr);
    }
    cairo_surface_destroy(s);
}

gboolean Visor::alDibujar(GtkWidget* w, cairo_t* cr, gpointer d) {
    auto* v = static_cast<Visor*>(d);
    const int vw = gtk_widget_get_allocated_width(w);
    const int vh = gtk_widget_get_allocated_height(w);
    cairo_set_source_rgb(cr, 0.40, 0.40, 0.42);
    cairo_paint(cr);
    if (v->y_.empty()) return TRUE;

    const double ox = gtk_adjustment_get_value(v->hadj_);
    const double oy = gtk_adjustment_get_value(v->vadj_);
    const int sf = gtk_widget_get_scale_factor(w);
    const int n = static_cast<int>(v->y_.size());

    for (int i = v->paginaEn(oy); i < n && v->y_[static_cast<std::size_t>(i)] < oy + vh; ++i) {
        const std::size_t ui = static_cast<std::size_t>(i);
        const double x = v->inicioX(ui) - ox;
        const double y = std::floor(v->y_[ui] - oy);
        cairo_set_source_rgba(cr, 0, 0, 0, 0.30); // sombra
        cairo_rectangle(cr, x + 2, y + 2, v->ancho_[ui], v->alto_[ui]);
        cairo_fill(cr);
        v->dibujarPagina(cr, i, x, y, vw, vh, sf);
    }
    return TRUE;
}

void Visor::alAsignar(GtkWidget*, GdkRectangle*, gpointer d) {
    auto* v = static_cast<Visor*>(d);
    if (v->ajuste_ != Ajuste::Ninguno) {
        const double zoomAntes = v->zoom_;
        const int pagina = v->actual_;
        const Ancla a = v->tomarAncla();
        v->calcularDisposicion();
        if (v->ajuste_ == Ajuste::Pagina && !v->y_.empty()) {
            const int i = std::min(pagina, static_cast<int>(v->y_.size()) - 1);
            gtk_adjustment_set_value(v->vadj_, v->y_[static_cast<std::size_t>(i)] - kMargen);
        } else {
            v->restaurarAncla(a);
        }
        if (v->zoom_ != zoomAntes) v->avisar();
    } else {
        v->actualizarAjustadores();
    }
}

gboolean Visor::alRueda(GtkWidget*, GdkEventScroll* ev, gpointer d) {
    auto* v = static_cast<Visor*>(d);
    double dx = 0.0;
    double dy = 0.0;
    switch (ev->direction) {
    case GDK_SCROLL_UP: dy = -1.0; break;
    case GDK_SCROLL_DOWN: dy = 1.0; break;
    case GDK_SCROLL_LEFT: dx = -1.0; break;
    case GDK_SCROLL_RIGHT: dx = 1.0; break;
    case GDK_SCROLL_SMOOTH: gdk_event_get_scroll_deltas(reinterpret_cast<GdkEvent*>(ev), &dx, &dy); break;
    default: break;
    }
    if (ev->state & GDK_CONTROL_MASK) {
        if (dy != 0.0) v->multiplicarZoom(std::pow(1.1, -dy));
        return TRUE;
    }
    if (ev->state & GDK_SHIFT_MASK) std::swap(dx, dy);
    gtk_adjustment_set_value(v->vadj_, gtk_adjustment_get_value(v->vadj_) + dy * kPasoRueda);
    gtk_adjustment_set_value(v->hadj_, gtk_adjustment_get_value(v->hadj_) + dx * kPasoRueda);
    return TRUE;
}

gboolean Visor::alTecla(GtkWidget*, GdkEventKey* ev, gpointer d) {
    auto* v = static_cast<Visor*>(d);
    const auto mover = [](GtkAdjustment* a, double delta) {
        gtk_adjustment_set_value(a, gtk_adjustment_get_value(a) + delta);
    };
    switch (ev->keyval) {
    case GDK_KEY_Escape:
        if (!v->colocando_) return FALSE;
        v->cancelarColocar();
        return TRUE;
    case GDK_KEY_Up: mover(v->vadj_, -kPasoTecla); return TRUE;
    case GDK_KEY_Down: mover(v->vadj_, kPasoTecla); return TRUE;
    case GDK_KEY_Left: mover(v->hadj_, -kPasoTecla); return TRUE;
    case GDK_KEY_Right: mover(v->hadj_, kPasoTecla); return TRUE;
    case GDK_KEY_Home: gtk_adjustment_set_value(v->vadj_, 0.0); return TRUE;
    case GDK_KEY_End: gtk_adjustment_set_value(v->vadj_, gtk_adjustment_get_upper(v->vadj_)); return TRUE;
    default: return FALSE;
    }
}

gboolean Visor::alPulsar(GtkWidget* w, GdkEventButton* ev, gpointer d) {
    auto* v = static_cast<Visor*>(d);
    gtk_widget_grab_focus(w);
    if (!v->colocando_ || ev->button != 1 || ev->type != GDK_BUTTON_PRESS) return FALSE;

    const double cx = ev->x + gtk_adjustment_get_value(v->hadj_);
    const double cy = ev->y + gtk_adjustment_get_value(v->vadj_);
    const int i = v->paginaEn(cy);
    if (v->y_.empty()) return TRUE;
    const std::size_t ui = static_cast<std::size_t>(i);
    const double x0 = v->inicioX(ui);
    const double px = cx - x0;
    const double py = cy - v->y_[ui];
    if (px < 0 || py < 0 || px >= v->ancho_[ui] || py >= v->alto_[ui]) return TRUE; // fuera de la página

    v->colocando_ = false;
    v->ponerCursor(nullptr);
    if (v->alColocar)
        v->alColocar(i, static_cast<int>(px), static_cast<int>(py), v->ancho_[ui], v->alto_[ui]);
    return TRUE;
}

double Visor::inicioX(std::size_t pagina) const {
    const double vw = gtk_widget_get_allocated_width(area_);
    return std::floor((std::max(totalAncho_, vw) - ancho_[pagina]) / 2.0);
}

void Visor::ponerCursor(const char* nombre) {
    GdkWindow* w = gtk_widget_get_window(area_);
    if (!w) return;
    if (!nombre) {
        gdk_window_set_cursor(w, nullptr);
        return;
    }
    GdkCursor* c = gdk_cursor_new_from_name(gdk_window_get_display(w), nombre);
    gdk_window_set_cursor(w, c);
    if (c) g_object_unref(c);
}

void Visor::empezarColocar() {
    if (y_.empty()) return;
    colocando_ = true;
    ponerCursor("crosshair");
    gtk_widget_grab_focus(area_);
}

void Visor::cancelarColocar() {
    if (!colocando_) return;
    colocando_ = false;
    ponerCursor(nullptr);
    if (alCancelarColocar) alCancelarColocar();
}

void Visor::alMover(GtkAdjustment*, gpointer d) {
    auto* v = static_cast<Visor*>(d);
    v->detectarPagina();
    gtk_widget_queue_draw(v->area_);
}

} // namespace ibz::gui
