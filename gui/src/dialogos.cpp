/**
 * @file dialogos.cpp
 * @brief Diálogos de la interfaz.
 */
#include "dialogos.h"

#include "util.h"
#include "version.h"

#include <ibanascapdfcore/claves.h>
#include <ibanascapdfcore/error.h>
#include <ibanascapdfcore/idioma.h>
#include <ibanascapdfcore/rutas.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace fs = std::filesystem;

namespace ibz::gui {

namespace {

void filtroPdf(GtkFileChooser* fc) {
    GtkFileFilter* f = gtk_file_filter_new();
    gtk_file_filter_set_name(f, tr("filtro.pdf").c_str());
    gtk_file_filter_add_pattern(f, "*.pdf");
    gtk_file_filter_add_pattern(f, "*.PDF");
    gtk_file_chooser_add_filter(fc, f);
}

std::optional<fs::path> ejecutarSelector(GtkFileChooserNative* d) {
    std::optional<fs::path> r;
    if (gtk_native_dialog_run(GTK_NATIVE_DIALOG(d)) == GTK_RESPONSE_ACCEPT) {
        if (gchar* n = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(d))) {
            r = rutaDesdeGlib(n);
            g_free(n);
        }
    }
    g_object_unref(d);
    return r;
}

// ------------------------------------------------ lista de PDF (TipoParametro::ArchivosPdf)

constexpr const char* kRuta = "ibz-ruta";     ///< ruta UTF-8 guardada en cada fila
constexpr const char* kPadre = "ibz-padre";   ///< ventana padre del diálogo
constexpr const char* kLista = "ibz-lista";   ///< GtkListBox dentro del control
constexpr const char* kCarpeta = "ibz-carpeta";

void anadirFila(GtkWidget* lista, const fs::path& ruta) {
    GtkWidget* etiqueta = gtk_label_new(utf8(ruta.filename()).c_str());
    gtk_label_set_xalign(GTK_LABEL(etiqueta), 0.0f);
    gtk_label_set_ellipsize(GTK_LABEL(etiqueta), PANGO_ELLIPSIZE_MIDDLE);
    gtk_widget_set_margin_start(etiqueta, 6);
    gtk_widget_set_margin_end(etiqueta, 6);
    gtk_widget_set_tooltip_text(etiqueta, utf8(ruta).c_str());
    gtk_list_box_insert(GTK_LIST_BOX(lista), etiqueta, -1);
    GtkWidget* fila = gtk_widget_get_parent(etiqueta);
    g_object_set_data_full(G_OBJECT(fila), kRuta, g_strdup(utf8(ruta).c_str()), g_free);
    gtk_widget_show_all(fila);
}

void alAnadirPdf(GtkButton*, gpointer d) {
    GtkWidget* lista = GTK_WIDGET(d);
    auto* padre = static_cast<GtkWindow*>(g_object_get_data(G_OBJECT(lista), kPadre));
    GtkFileChooserNative* fc = gtk_file_chooser_native_new(sinMnemonico(tr("dialogo.abrir")).c_str(), padre,
                                                           GTK_FILE_CHOOSER_ACTION_OPEN, tr("boton.abrir").c_str(),
                                                           tr("boton.cancelar").c_str());
    gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(fc), TRUE);
    filtroPdf(GTK_FILE_CHOOSER(fc));
    if (const char* c = static_cast<const char*>(g_object_get_data(G_OBJECT(lista), kCarpeta)); c && *c)
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(fc), aGlib(desdeUtf8(c)).c_str());
    if (gtk_native_dialog_run(GTK_NATIVE_DIALOG(fc)) == GTK_RESPONSE_ACCEPT) {
        GSList* nombres = gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(fc));
        for (GSList* n = nombres; n; n = n->next) anadirFila(lista, rutaDesdeGlib(static_cast<const char*>(n->data)));
        g_slist_free_full(nombres, g_free);
    }
    g_object_unref(fc);
}

void alQuitarPdf(GtkButton*, gpointer d) {
    if (GtkListBoxRow* fila = gtk_list_box_get_selected_row(GTK_LIST_BOX(d))) gtk_widget_destroy(GTK_WIDGET(fila));
}

void moverFila(GtkWidget* lista, int delta) {
    GtkListBoxRow* fila = gtk_list_box_get_selected_row(GTK_LIST_BOX(lista));
    if (!fila) return;
    const int i = gtk_list_box_row_get_index(fila) + delta;
    if (i < 0 || !gtk_list_box_get_row_at_index(GTK_LIST_BOX(lista), i)) return;
    g_object_ref(fila);
    gtk_container_remove(GTK_CONTAINER(lista), GTK_WIDGET(fila));
    gtk_list_box_insert(GTK_LIST_BOX(lista), GTK_WIDGET(fila), i);
    gtk_list_box_select_row(GTK_LIST_BOX(lista), fila);
    g_object_unref(fila);
}

void alSubirPdf(GtkButton*, gpointer d) { moverFila(GTK_WIDGET(d), -1); }
void alBajarPdf(GtkButton*, gpointer d) { moverFila(GTK_WIDGET(d), 1); }

GtkWidget* boton(const char* clave, GCallback cb, gpointer datos) {
    GtkWidget* b = gtk_button_new_with_mnemonic(tr(clave).c_str());
    g_signal_connect(b, "clicked", cb, datos);
    return b;
}

/// Control con la lista ordenada de PDF y los botones Añadir, Quitar, Subir y Bajar.
GtkWidget* crearListaPdf(GtkWindow* padre, const std::string& carpeta) {
    GtkWidget* lista = gtk_list_box_new();
    g_object_set_data(G_OBJECT(lista), kPadre, padre);
    g_object_set_data_full(G_OBJECT(lista), kCarpeta, g_strdup(carpeta.c_str()), g_free);

    GtkWidget* desplazar = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(desplazar), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(desplazar), GTK_SHADOW_IN);
    gtk_widget_set_size_request(desplazar, 380, 150);
    gtk_container_add(GTK_CONTAINER(desplazar), lista);

    GtkWidget* botones = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_pack_start(GTK_BOX(botones), boton("boton.anadir", G_CALLBACK(alAnadirPdf), lista), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(botones), boton("boton.quitar", G_CALLBACK(alQuitarPdf), lista), FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(botones), boton("boton.bajar", G_CALLBACK(alBajarPdf), lista), FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(botones), boton("boton.subir", G_CALLBACK(alSubirPdf), lista), FALSE, FALSE, 0);

    GtkWidget* caja = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_hexpand(caja, TRUE);
    gtk_box_pack_start(GTK_BOX(caja), desplazar, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(caja), botones, FALSE, FALSE, 0);
    g_object_set_data(G_OBJECT(caja), kLista, lista);
    return caja;
}

/// Rutas de la lista, en orden, separadas por '\n'.
std::string leerListaPdf(GtkWidget* caja) {
    auto* lista = static_cast<GtkWidget*>(g_object_get_data(G_OBJECT(caja), kLista));
    std::string r;
    for (int i = 0;; ++i) {
        GtkListBoxRow* fila = gtk_list_box_get_row_at_index(GTK_LIST_BOX(lista), i);
        if (!fila) break;
        if (const char* ruta = static_cast<const char*>(g_object_get_data(G_OBJECT(fila), kRuta))) {
            if (!r.empty()) r += '\n';
            r += ruta;
        }
    }
    return r;
}

// ------------------------------------------------ carpeta (TipoParametro::Carpeta)

void alElegirCarpeta(GtkButton*, gpointer d) {
    GtkWidget* entrada = GTK_WIDGET(d);
    auto* padre = static_cast<GtkWindow*>(g_object_get_data(G_OBJECT(entrada), kPadre));
    GtkFileChooserNative* fc = gtk_file_chooser_native_new(sinMnemonico(tr("boton.elegir")).c_str(), padre,
                                                           GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                                           tr("boton.aceptar").c_str(), tr("boton.cancelar").c_str());
    const std::string actual = gtk_entry_get_text(GTK_ENTRY(entrada));
    if (!actual.empty()) gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(fc), aGlib(desdeUtf8(actual)).c_str());
    if (gtk_native_dialog_run(GTK_NATIVE_DIALOG(fc)) == GTK_RESPONSE_ACCEPT) {
        if (gchar* n = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(fc))) {
            gtk_entry_set_text(GTK_ENTRY(entrada), utf8(rutaDesdeGlib(n)).c_str());
            g_free(n);
        }
    }
    g_object_unref(fc);
}

void alElegirImagen(GtkButton*, gpointer d) {
    GtkWidget* entrada = GTK_WIDGET(d);
    auto* padre = static_cast<GtkWindow*>(g_object_get_data(G_OBJECT(entrada), kPadre));
    GtkFileChooserNative* fc = gtk_file_chooser_native_new(sinMnemonico(tr("boton.elegir")).c_str(), padre,
                                                           GTK_FILE_CHOOSER_ACTION_OPEN, tr("boton.abrir").c_str(),
                                                           tr("boton.cancelar").c_str());
    GtkFileFilter* f = gtk_file_filter_new();
    gtk_file_filter_set_name(f, tr("filtro.png").c_str());
    gtk_file_filter_add_pattern(f, "*.png");
    gtk_file_filter_add_pattern(f, "*.PNG");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(fc), f);
    const std::string actual = gtk_entry_get_text(GTK_ENTRY(entrada));
    if (!actual.empty())
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(fc), aGlib(desdeUtf8(actual).parent_path()).c_str());
    if (gtk_native_dialog_run(GTK_NATIVE_DIALOG(fc)) == GTK_RESPONSE_ACCEPT) {
        if (gchar* n = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(fc))) {
            gtk_entry_set_text(GTK_ENTRY(entrada), utf8(rutaDesdeGlib(n)).c_str());
            g_free(n);
        }
    }
    g_object_unref(fc);
}

/// Campo con la ruta de una imagen PNG y botón «Elegir…» (TipoParametro::ArchivoImagen).
GtkWidget* crearImagen(GtkWindow* padre, const std::string& ruta) {
    GtkWidget* entrada = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entrada), ruta.c_str());
    gtk_entry_set_width_chars(GTK_ENTRY(entrada), 36);
    gtk_widget_set_hexpand(entrada, TRUE);
    g_object_set_data(G_OBJECT(entrada), kPadre, padre);
    GtkWidget* caja = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_pack_start(GTK_BOX(caja), entrada, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(caja), boton("boton.elegir", G_CALLBACK(alElegirImagen), entrada), FALSE, FALSE, 0);
    g_object_set_data(G_OBJECT(caja), kLista, entrada);
    return caja;
}

/// Campo de texto con la carpeta y botón «Elegir…».
GtkWidget* crearCarpeta(GtkWindow* padre, const std::string& carpeta) {
    GtkWidget* entrada = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entrada), carpeta.c_str());
    gtk_entry_set_width_chars(GTK_ENTRY(entrada), 36);
    gtk_widget_set_hexpand(entrada, TRUE);
    g_object_set_data(G_OBJECT(entrada), kPadre, padre);
    GtkWidget* caja = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_pack_start(GTK_BOX(caja), entrada, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(caja), boton("boton.elegir", G_CALLBACK(alElegirCarpeta), entrada), FALSE, FALSE, 0);
    g_object_set_data(G_OBJECT(caja), kLista, entrada);
    return caja;
}

/// Texto de varias líneas (TipoParametro::TextoLargo).
GtkWidget* crearTextoLargo(const std::string& defecto) {
    GtkWidget* vista = gtk_text_view_new();
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(vista), GTK_WRAP_WORD_CHAR);
    gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(vista)), defecto.c_str(), -1);
    GtkWidget* desplazar = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(desplazar), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(desplazar), GTK_SHADOW_IN);
    gtk_widget_set_size_request(desplazar, 380, 110);
    gtk_widget_set_hexpand(desplazar, TRUE);
    gtk_container_add(GTK_CONTAINER(desplazar), vista);
    g_object_set_data(G_OBJECT(desplazar), kLista, vista);
    return desplazar;
}

std::string leerTextoLargo(GtkWidget* c) {
    auto* vista = static_cast<GtkWidget*>(g_object_get_data(G_OBJECT(c), kLista));
    GtkTextBuffer* b = gtk_text_view_get_buffer(GTK_TEXT_VIEW(vista));
    GtkTextIter ini;
    GtkTextIter fin;
    gtk_text_buffer_get_bounds(b, &ini, &fin);
    gchar* t = gtk_text_buffer_get_text(b, &ini, &fin, FALSE);
    std::string r = t ? t : "";
    g_free(t);
    return r;
}

/// Colores ofrecidos para el texto (TipoParametro::Color). Lista dentro del propio
/// diálogo: sin ventana de color aparte (en Windows podía quedar detrás del diálogo).
struct ColorNombrado {
    const char* hex;
    const char* clave;
    guint32 rgba;
};
constexpr ColorNombrado kColores[] = {
    {"#000000", "color.negro", 0x000000FF},    {"#1f3a93", "color.azul_oscuro", 0x1F3A93FF},
    {"#1e66f5", "color.azul", 0x1E66F5FF},     {"#c62828", "color.rojo", 0xC62828FF},
    {"#2e7d32", "color.verde", 0x2E7D32FF},    {"#ef6c00", "color.naranja", 0xEF6C00FF},
    {"#6a1b9a", "color.morado", 0x6A1B9AFF},   {"#616161", "color.gris", 0x616161FF},
};

GtkWidget* crearColor(const std::string& defecto) {
    GtkListStore* modelo = gtk_list_store_new(3, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING);
    for (const ColorNombrado& c : kColores) {
        GdkPixbuf* muestra = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, 16, 16);
        gdk_pixbuf_fill(muestra, c.rgba);
        gtk_list_store_insert_with_values(modelo, nullptr, -1, 0, muestra, 1, tr(c.clave).c_str(), 2, c.hex, -1);
        g_object_unref(muestra);
    }
    GtkWidget* combo = gtk_combo_box_new_with_model(GTK_TREE_MODEL(modelo));
    g_object_unref(modelo);
    gtk_combo_box_set_id_column(GTK_COMBO_BOX(combo), 2);
    GtkCellRenderer* rMuestra = gtk_cell_renderer_pixbuf_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combo), rMuestra, FALSE);
    gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(combo), rMuestra, "pixbuf", 0);
    GtkCellRenderer* rTexto = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combo), rTexto, TRUE);
    gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(combo), rTexto, "text", 1);
    if (!gtk_combo_box_set_active_id(GTK_COMBO_BOX(combo), defecto.c_str())) gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);
    return combo;
}

std::string leerColor(GtkWidget* c) {
    const gchar* id = gtk_combo_box_get_active_id(GTK_COMBO_BOX(c));
    return id ? id : "#000000";
}

/// Tipos cuyo último valor se recuerda entre usos.
bool seRecuerda(TipoParametro t) {
    return t == TipoParametro::Opcion || t == TipoParametro::Entero || t == TipoParametro::Color ||
           t == TipoParametro::ArchivoImagen;
}

GdkPixbuf* cargarLogo(const Claves& config, int lado) {
    const fs::path ruta = carpetaDatos() / desdeUtf8(config.obtener("logo", "assets/logo.png"));
    return gdk_pixbuf_new_from_file_at_size(aGlib(ruta).c_str(), lado, lado, nullptr);
}

} // namespace

std::optional<fs::path> elegirPdfAbrir(GtkWindow* padre, const std::string& carpeta) {
    GtkFileChooserNative* d = gtk_file_chooser_native_new(sinMnemonico(tr("dialogo.abrir")).c_str(), padre,
                                                          GTK_FILE_CHOOSER_ACTION_OPEN, tr("boton.abrir").c_str(),
                                                          tr("boton.cancelar").c_str());
    filtroPdf(GTK_FILE_CHOOSER(d));
    if (!carpeta.empty()) gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(d), aGlib(desdeUtf8(carpeta)).c_str());
    return ejecutarSelector(d);
}

std::optional<fs::path> elegirPdfGuardar(GtkWindow* padre, const std::string& carpeta, const std::string& nombre) {
    GtkFileChooserNative* d = gtk_file_chooser_native_new(sinMnemonico(tr("dialogo.guardar_como")).c_str(), padre,
                                                          GTK_FILE_CHOOSER_ACTION_SAVE, tr("boton.guardar").c_str(),
                                                          tr("boton.cancelar").c_str());
    GtkFileChooser* fc = GTK_FILE_CHOOSER(d);
    gtk_file_chooser_set_do_overwrite_confirmation(fc, TRUE);
    filtroPdf(fc);
    if (!carpeta.empty()) gtk_file_chooser_set_current_folder(fc, aGlib(desdeUtf8(carpeta)).c_str());
    gtk_file_chooser_set_current_name(fc, nombre.c_str()); // siempre UTF-8
    auto r = ejecutarSelector(d);
    if (r && r->extension() != ".pdf" && r->extension() != ".PDF") *r += ".pdf";
    return r;
}

void mostrarInfo(GtkWindow* padre, const std::string& texto) {
    GtkWidget* d = gtk_message_dialog_new(padre, GtkDialogFlags(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
                                          GTK_MESSAGE_INFO, GTK_BUTTONS_NONE, "%s", texto.c_str());
    gtk_dialog_add_button(GTK_DIALOG(d), tr("boton.cerrar").c_str(), GTK_RESPONSE_CLOSE);
    gtk_dialog_run(GTK_DIALOG(d));
    gtk_widget_destroy(d);
}

bool pedirContrasena(GtkWindow* padre, bool incorrecta, std::string& contrasena) {
    GtkWidget* d = gtk_dialog_new_with_buttons(
        tr("dialogo.contrasena.titulo").c_str(), padre, GtkDialogFlags(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
        tr("boton.cancelar").c_str(), GTK_RESPONSE_CANCEL, tr("boton.abrir").c_str(), GTK_RESPONSE_ACCEPT, nullptr);
    gtk_dialog_set_default_response(GTK_DIALOG(d), GTK_RESPONSE_ACCEPT);

    GtkWidget* caja = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(caja), 12);
    GtkWidget* texto =
        gtk_label_new(tr(incorrecta ? "dialogo.contrasena.incorrecta" : "dialogo.contrasena.texto").c_str());
    gtk_label_set_xalign(GTK_LABEL(texto), 0.0f);
    gtk_label_set_line_wrap(GTK_LABEL(texto), TRUE);
    GtkWidget* entrada = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(entrada), FALSE);
    gtk_entry_set_activates_default(GTK_ENTRY(entrada), TRUE);
    gtk_box_pack_start(GTK_BOX(caja), texto, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(caja), entrada, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(d))), caja);
    gtk_widget_show_all(d);

    const bool ok = gtk_dialog_run(GTK_DIALOG(d)) == GTK_RESPONSE_ACCEPT;
    if (ok) contrasena = gtk_entry_get_text(GTK_ENTRY(entrada));
    gtk_widget_destroy(d);
    return ok;
}

void mostrarErrorTexto(GtkWindow* padre, const std::string& principal, const std::string& detalle) {
    GtkWidget* d = gtk_message_dialog_new(padre, GtkDialogFlags(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
                                          GTK_MESSAGE_ERROR, GTK_BUTTONS_NONE, "%s", principal.c_str());
    if (!detalle.empty()) gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(d), "%s", detalle.c_str());
    gtk_dialog_add_button(GTK_DIALOG(d), tr("boton.cerrar").c_str(), GTK_RESPONSE_CLOSE);
    gtk_window_set_title(GTK_WINDOW(d), tr("error.titulo").c_str());
    gtk_dialog_run(GTK_DIALOG(d));
    gtk_widget_destroy(d);
}

void mostrarError(GtkWindow* padre, const Error& e) {
    mostrarErrorTexto(padre, tr(e.clave().c_str()), e.detalle());
}

Respuesta preguntarDescartar(GtkWindow* padre) {
    GtkWidget* d = gtk_message_dialog_new(padre, GtkDialogFlags(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
                                          GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE, "%s",
                                          tr("dialogo.descartar.titulo").c_str());
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(d), "%s", tr("dialogo.descartar.texto").c_str());
    gtk_dialog_add_buttons(GTK_DIALOG(d), tr("boton.descartar").c_str(), 1, tr("boton.cancelar").c_str(),
                           GTK_RESPONSE_CANCEL, tr("boton.guardar_como").c_str(), 2, nullptr);
    gtk_dialog_set_default_response(GTK_DIALOG(d), 2);
    const int r = gtk_dialog_run(GTK_DIALOG(d));
    gtk_widget_destroy(d);
    if (r == 1) return Respuesta::Descartar;
    if (r == 2) return Respuesta::Guardar;
    return Respuesta::Cancelar;
}

void acercaDe(GtkWindow* padre, const Claves& config) {
    static const gchar* autores[] = {IBZ_AUTOR " <" IBZ_CONTACTO ">", nullptr};
    const std::string titulo = config.obtener("titulo", IBZ_NOMBRE);
    GdkPixbuf* logo = cargarLogo(config, 128);
    gtk_show_about_dialog(padre, "program-name", titulo.c_str(), "version", IBZ_VERSION, "comments",
                          tr("acerca.descripcion").c_str(), "copyright",
                          "Copyright \xC2\xA9 " IBZ_ANIO " " IBZ_AUTOR " (" IBZ_ORGANIZACION ")", "license-type",
                          GTK_LICENSE_MIT_X11, "website", IBZ_WEB, "website-label", "donatellatech.com", "authors",
                          autores, "logo", logo, nullptr);
    if (logo) g_object_unref(logo);
}

bool pedirParametros(GtkWindow* padre, const Modulo& modulo, Valores& valores, const ContextoDialogo& ctx) {
    const std::vector<Parametro> ps = modulo.parametros();
    if (ps.empty()) return true;

    const std::string clave = "modulo." + std::string(modulo.id()) + ".nombre";
    GtkWidget* d = gtk_dialog_new_with_buttons(
        sinMnemonico(tr(clave.c_str())).c_str(), padre, GtkDialogFlags(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
        tr("boton.cancelar").c_str(), GTK_RESPONSE_CANCEL, tr("boton.aceptar").c_str(), GTK_RESPONSE_ACCEPT, nullptr);
    gtk_dialog_set_default_response(GTK_DIALOG(d), GTK_RESPONSE_ACCEPT);

    GtkWidget* rejilla = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(rejilla), 8);
    gtk_grid_set_column_spacing(GTK_GRID(rejilla), 12);
    gtk_container_set_border_width(GTK_CONTAINER(rejilla), 12);

    std::vector<GtkWidget*> controles;
    controles.reserve(ps.size());
    int fila = 0;
    for (const Parametro& p : ps) {
        GtkWidget* etiqueta = gtk_label_new(tr(p.clave.c_str()).c_str());
        gtk_label_set_xalign(GTK_LABEL(etiqueta), 1.0f);
        GtkWidget* c = nullptr;
        std::string defecto = p.defecto;
        const std::string claveRecuerdo = "recordar." + std::string(modulo.id()) + "." + p.id;
        if (ctx.recordados && seRecuerda(p.tipo))
            if (const std::string* r = ctx.recordados->buscar(claveRecuerdo)) defecto = *r;
        if (defecto == "@actual") defecto = std::to_string(ctx.paginaActual);
        else if (defecto == "@carpeta") defecto = ctx.carpeta;
        else if (defecto == "@nombre") defecto = ctx.nombre;
        switch (p.tipo) {
        case TipoParametro::Entero: {
            const int maximo = p.maximo < 0 ? std::max(p.minimo, ctx.totalPaginas) : p.maximo;
            c = gtk_spin_button_new_with_range(p.minimo, maximo, 1);
            gtk_spin_button_set_value(GTK_SPIN_BUTTON(c), g_ascii_strtod(defecto.c_str(), nullptr));
            gtk_entry_set_activates_default(GTK_ENTRY(c), TRUE);
            break;
        }
        case TipoParametro::Opcion: {
            c = gtk_combo_box_text_new();
            for (const std::string& o : p.opciones) {
                const std::string k = p.clave + "." + o;
                gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(c), o.c_str(), tr(k.c_str()).c_str());
            }
            if (!gtk_combo_box_set_active_id(GTK_COMBO_BOX(c), defecto.c_str()))
                gtk_combo_box_set_active(GTK_COMBO_BOX(c), 0);
            break;
        }
        case TipoParametro::Texto:
        case TipoParametro::Paginas:
        case TipoParametro::Contrasena: {
            c = gtk_entry_new();
            gtk_entry_set_text(GTK_ENTRY(c), defecto.c_str());
            gtk_entry_set_activates_default(GTK_ENTRY(c), TRUE);
            gtk_widget_set_hexpand(c, TRUE);
            if (p.tipo == TipoParametro::Contrasena) gtk_entry_set_visibility(GTK_ENTRY(c), FALSE);
            if (p.tipo == TipoParametro::Paginas) gtk_entry_set_placeholder_text(GTK_ENTRY(c), "1-3,5,8-");
            break;
        }
        case TipoParametro::ArchivosPdf:
            c = crearListaPdf(padre, ctx.carpeta);
            gtk_label_set_yalign(GTK_LABEL(etiqueta), 0.0f);
            break;
        case TipoParametro::Carpeta:
            c = crearCarpeta(padre, defecto);
            break;
        case TipoParametro::TextoLargo:
            c = crearTextoLargo(defecto);
            gtk_label_set_yalign(GTK_LABEL(etiqueta), 0.0f);
            break;
        case TipoParametro::Color:
            c = crearColor(defecto);
            gtk_widget_set_halign(c, GTK_ALIGN_START);
            break;
        case TipoParametro::ArchivoImagen:
            c = crearImagen(padre, defecto);
            break;
        }
        gtk_grid_attach(GTK_GRID(rejilla), etiqueta, 0, fila, 1, 1);
        gtk_grid_attach(GTK_GRID(rejilla), c, 1, fila, 1, 1);
        controles.push_back(c);
        ++fila;
    }
    gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(d))), rejilla);
    gtk_widget_show_all(d);
    // El cursor empieza en el texto a escribir, si lo hay.
    for (std::size_t i = 0; i < ps.size(); ++i) {
        if (ps[i].tipo == TipoParametro::TextoLargo) {
            gtk_widget_grab_focus(static_cast<GtkWidget*>(g_object_get_data(G_OBJECT(controles[i]), kLista)));
            break;
        }
    }

    const bool ok = gtk_dialog_run(GTK_DIALOG(d)) == GTK_RESPONSE_ACCEPT;
    if (ok) {
        for (std::size_t i = 0; i < ps.size(); ++i) {
            GtkWidget* c = controles[i];
            switch (ps[i].tipo) {
            case TipoParametro::Entero:
                valores[ps[i].id] = std::to_string(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(c)));
                break;
            case TipoParametro::Opcion: {
                const gchar* id = gtk_combo_box_get_active_id(GTK_COMBO_BOX(c));
                valores[ps[i].id] = id ? id : "";
                break;
            }
            case TipoParametro::Texto:
            case TipoParametro::Paginas:
            case TipoParametro::Contrasena:
                valores[ps[i].id] = gtk_entry_get_text(GTK_ENTRY(c));
                break;
            case TipoParametro::ArchivosPdf:
                valores[ps[i].id] = leerListaPdf(c);
                break;
            case TipoParametro::Carpeta:
                valores[ps[i].id] =
                    gtk_entry_get_text(GTK_ENTRY(g_object_get_data(G_OBJECT(c), kLista)));
                break;
            case TipoParametro::TextoLargo:
                valores[ps[i].id] = leerTextoLargo(c);
                break;
            case TipoParametro::Color:
                valores[ps[i].id] = leerColor(c);
                break;
            case TipoParametro::ArchivoImagen:
                valores[ps[i].id] = gtk_entry_get_text(GTK_ENTRY(g_object_get_data(G_OBJECT(c), kLista)));
                break;
            }
            if (ctx.recordados && seRecuerda(ps[i].tipo))
                ctx.recordados->poner("recordar." + std::string(modulo.id()) + "." + ps[i].id, valores[ps[i].id]);
        }
    }
    gtk_widget_destroy(d);
    return ok;
}

} // namespace ibz::gui
