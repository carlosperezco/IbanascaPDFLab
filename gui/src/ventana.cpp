/**
 * @file ventana.cpp
 * @brief Ventana principal: menús, bienvenida, visor, miniaturas y barra de estado.
 *
 * La ventana no contiene lógica de PDF: abre y guarda con ibz::Documento y
 * ejecuta los módulos del registro (ibz::Registro).
 */
#include "ibanascapdfgui/interfaz.h"

#include "dialogos.h"
#include "miniaturas.h"
#include "util.h"
#include "version.h"
#include "visor.h"

#include <ibanascapdfcore/claves.h>
#include <ibanascapdfcore/documento.h>
#include <ibanascapdfcore/error.h>
#include <ibanascapdfcore/idioma.h>
#include <ibanascapdfcore/modulo.h>
#include <ibanascapdfcore/operaciones.h>
#include <ibanascapdfcore/texto.h>
#include <ibanascapdfcore/rutas.h>

#include <gtk/gtk.h>
#include <libintl.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace ibz::gui {

namespace {

constexpr double kPasoZoom = 1.2;

/**
 * @brief Cambia también el idioma de los textos propios de GTK (botones de «Acerca de», etc.).
 *
 * GTK traduce con gettext según LANGUAGE. gettext guarda en caché las traducciones
 * ya usadas; volver a fijar el dominio con textdomain() es la forma documentada de
 * invalidar esa caché, de modo que los diálogos nuevos salen en el idioma elegido.
 */
void aplicarIdiomaGtk(const std::string& codigo) {
    g_setenv("LANGUAGE", codigo.c_str(), TRUE);
    const char* dominio = textdomain(nullptr);
    const std::string actual = dominio ? dominio : "messages";
    textdomain(actual.c_str());
}
/// Menús propios de los módulos (las operaciones de "paginas" se repiten en Editar).
const char* const kMenusModulos[] = {"paginas", "documento", "firma"};

/// Ventana principal del programa.
class Ventana {
public:
    Ventana(const Claves& config, Claves& ajustes);
    ~Ventana();
    Ventana(const Ventana&) = delete;
    Ventana& operator=(const Ventana&) = delete;

    void mostrar();
    void abrir(const fs::path& ruta);

private:
    struct Accion {
        std::function<void()> f;
    };

    // Construcción
    void construirMenus();
    GtkWidget* menu(GtkWidget* barra, const char* clave);
    GtkWidget* item(GtkWidget* menu, const char* clave, std::function<void()> f, guint tecla = 0,
                    GdkModifierType mods = GdkModifierType(0), bool requiereDoc = false);
    void construirBienvenida();
    void textosBienvenida();

    // Acciones
    void accionNuevo();
    void accionAbrir();
    bool accionGuardarComo();
    void accionCerrar();
    void accionSalir();
    void accionDeshacer();
    void accionRehacer();
    void accionIrAPagina();
    void agregarModulos(GtkWidget* menu, const char* grupo, bool conAtajos);
    void ejecutarModulo(const Modulo& m);
    void ejecutarConValores(const Modulo& m, Valores v);
    void ejecutarDirecto(const Modulo& m, Valores v);
    void colocar(int pagina, int x, int y, int ancho, int alto);
    void moverPorArrastre(int origen, int destino);
    void cambiarIdioma(const std::string& codigo);

    bool confirmarDescartar();
    void documentoCambiado(bool conservarPosicion);
    void actualizarEstado();
    void guardarAjustes();
    GtkWindow* ventana() const noexcept { return GTK_WINDOW(ventana_); }

    static void alActivar(GtkMenuItem*, gpointer d);
    static void alCambiarCampoPagina(GtkSpinButton* s, gpointer d);
    static void alActivarCampoPagina(GtkEntry*, gpointer d);
    static gboolean alTeclaVentana(GtkWidget* w, GdkEventKey* ev, gpointer d);
    static void alAlternar(GtkCheckMenuItem* c, gpointer d);
    static gboolean alCerrarVentana(GtkWidget*, GdkEvent*, gpointer d);
    static void alSoltar(GtkWidget*, GdkDragContext*, gint, gint, GtkSelectionData* datos, guint, guint, gpointer d);

    const Claves& config_;
    Claves& ajustes_;
    Documento doc_;

    GtkWidget* ventana_ = nullptr;
    GtkWidget* caja_ = nullptr;
    GtkWidget* barra_ = nullptr;
    GtkAccelGroup* atajos_ = nullptr;
    GtkWidget* pila_ = nullptr;
    GtkWidget* estado_ = nullptr;      ///< texto de la barra inferior (o «Sin documento»)
    GtkWidget* cajaPagina_ = nullptr;  ///< «Página [n] de N»
    GtkWidget* etiquetaPagina_ = nullptr;
    GtkWidget* campoPagina_ = nullptr;
    bool silencioPagina_ = false;
    GtkWidget* bienvenidaTitulo_ = nullptr;
    GtkWidget* bienvenidaTexto_ = nullptr;

    std::unique_ptr<Visor> visor_;
    std::unique_ptr<Miniaturas> miniaturas_;

    std::vector<std::unique_ptr<Accion>> acciones_;
    std::vector<GtkWidget*> itemsDoc_;
    GtkWidget* itemDeshacer_ = nullptr;
    GtkWidget* itemRehacer_ = nullptr;
    std::string idiomaPendiente_;
    const Modulo* moduloPendiente_ = nullptr; ///< espera el clic en la página (modo colocar)
    Valores valoresPendientes_;               ///< valores ya elegidos (clic después del diálogo)
    bool dialogoHecho_ = false;               ///< el clic completa un diálogo ya respondido
    fs::path rutaSoltada_;
};

Ventana::Ventana(const Claves& config, Claves& ajustes) : config_(config), ajustes_(ajustes) {
    ventana_ = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(ventana(), ajustes_.obtenerEntero("ventana.ancho", 1100),
                                ajustes_.obtenerEntero("ventana.alto", 760));
    if (ajustes_.obtener("ventana.maximizada") == "1") gtk_window_maximize(ventana());

    atajos_ = gtk_accel_group_new();
    gtk_window_add_accel_group(ventana(), atajos_);

    caja_ = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(ventana_), caja_);

    visor_ = std::make_unique<Visor>(doc_);
    miniaturas_ = std::make_unique<Miniaturas>(doc_);

    construirMenus();

    pila_ = gtk_stack_new();
    construirBienvenida();
    GtkWidget* panel = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_paned_pack1(GTK_PANED(panel), miniaturas_->widget(), FALSE, FALSE);
    gtk_paned_pack2(GTK_PANED(panel), visor_->widget(), TRUE, FALSE);
    gtk_stack_add_named(GTK_STACK(pila_), panel, "documento");
    gtk_box_pack_start(GTK_BOX(caja_), pila_, TRUE, TRUE, 0);

    gtk_box_pack_start(GTK_BOX(caja_), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 0);
    // Barra inferior: «Página [n] de N · zoom» o «Sin documento».
    GtkWidget* barraEstado = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_margin_start(barraEstado, 8);
    gtk_widget_set_margin_end(barraEstado, 8);
    gtk_widget_set_margin_top(barraEstado, 2);
    gtk_widget_set_margin_bottom(barraEstado, 2);
    cajaPagina_ = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_no_show_all(cajaPagina_, TRUE);
    etiquetaPagina_ = gtk_label_new(nullptr);
    campoPagina_ = gtk_spin_button_new_with_range(1, 1, 1);
    gtk_entry_set_width_chars(GTK_ENTRY(campoPagina_), 5);
    gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(campoPagina_), TRUE);
    gtk_box_pack_start(GTK_BOX(cajaPagina_), etiquetaPagina_, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(cajaPagina_), campoPagina_, FALSE, FALSE, 0);
    gtk_widget_show(etiquetaPagina_);
    gtk_widget_show(campoPagina_);
    estado_ = gtk_label_new(nullptr);
    gtk_label_set_xalign(GTK_LABEL(estado_), 0.0f);
    gtk_box_pack_start(GTK_BOX(barraEstado), cajaPagina_, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(barraEstado), estado_, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(caja_), barraEstado, FALSE, FALSE, 0);
    g_signal_connect(campoPagina_, "value-changed", G_CALLBACK(alCambiarCampoPagina), this);
    g_signal_connect(campoPagina_, "activate", G_CALLBACK(alActivarCampoPagina), this);
    // Las teclas sin Ctrl van primero al campo de texto (Supr, Inicio… no disparan menús).
    g_signal_connect(ventana_, "key-press-event", G_CALLBACK(alTeclaVentana), this);

    visor_->alCambiar = [this](int pagina) {
        miniaturas_->seleccionar(pagina);
        actualizarEstado();
    };
    miniaturas_->alElegir = [this](int pagina) { visor_->irAPagina(pagina); };
    visor_->alColocar = [this](int pagina, int x, int y, int ancho, int alto) { colocar(pagina, x, y, ancho, alto); };
    visor_->alCancelarColocar = [this] {
        moduloPendiente_ = nullptr;
        dialogoHecho_ = false;
        valoresPendientes_.clear();
        actualizarEstado();
    };
    miniaturas_->alMover = [this](int origen, int destino) { moverPorArrastre(origen, destino); };

    g_signal_connect(ventana_, "delete-event", G_CALLBACK(alCerrarVentana), this);

    static GtkTargetEntry destino[] = {{const_cast<gchar*>("text/uri-list"), 0, 0}};
    gtk_drag_dest_set(ventana_, GTK_DEST_DEFAULT_ALL, destino, 1, GDK_ACTION_COPY);
    g_signal_connect(ventana_, "drag-data-received", G_CALLBACK(alSoltar), this);
}

Ventana::~Ventana() {
    if (ventana_) gtk_widget_destroy(ventana_);
    if (atajos_) g_object_unref(atajos_);
}

void Ventana::mostrar() {
    gtk_widget_show_all(ventana_);
    documentoCambiado(false);
}

// ---------------------------------------------------------------- menús

GtkWidget* Ventana::menu(GtkWidget* barra, const char* clave) {
    GtkWidget* raiz = gtk_menu_item_new_with_mnemonic(tr(clave).c_str());
    GtkWidget* m = gtk_menu_new();
    gtk_menu_set_accel_group(GTK_MENU(m), atajos_);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(raiz), m);
    gtk_menu_shell_append(GTK_MENU_SHELL(barra), raiz);
    return m;
}

GtkWidget* Ventana::item(GtkWidget* menu, const char* clave, std::function<void()> f, guint tecla,
                         GdkModifierType mods, bool requiereDoc) {
    GtkWidget* it = gtk_menu_item_new_with_mnemonic(tr(clave).c_str());
    acciones_.push_back(std::make_unique<Accion>(Accion{std::move(f)}));
    g_signal_connect(it, "activate", G_CALLBACK(alActivar), acciones_.back().get());
    if (tecla) gtk_widget_add_accelerator(it, "activate", atajos_, tecla, mods, GTK_ACCEL_VISIBLE);
    if (requiereDoc) itemsDoc_.push_back(it);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), it);
    return it;
}

void Ventana::construirMenus() {
    if (barra_) gtk_widget_destroy(barra_);
    acciones_.clear();
    itemsDoc_.clear();
    const auto ctrl = GDK_CONTROL_MASK;
    const auto ninguna = GdkModifierType(0);

    barra_ = gtk_menu_bar_new();

    GtkWidget* m = menu(barra_, "menu.archivo");
    item(m, "accion.nuevo", [this] { accionNuevo(); }, GDK_KEY_n, ctrl);
    item(m, "accion.abrir", [this] { accionAbrir(); }, GDK_KEY_o, ctrl);
    item(m, "accion.guardar_como", [this] { accionGuardarComo(); }, GDK_KEY_s,
         GdkModifierType(GDK_CONTROL_MASK | GDK_SHIFT_MASK), true);
    item(m, "accion.cerrar", [this] { accionCerrar(); }, GDK_KEY_w, ctrl, true);
    gtk_menu_shell_append(GTK_MENU_SHELL(m), gtk_separator_menu_item_new());
    item(m, "accion.salir", [this] { accionSalir(); }, GDK_KEY_q, ctrl);

    m = menu(barra_, "menu.editar");
    itemDeshacer_ = item(m, "accion.deshacer", [this] { accionDeshacer(); }, GDK_KEY_z, ctrl);
    itemRehacer_ = item(m, "accion.rehacer", [this] { accionRehacer(); }, GDK_KEY_y, ctrl);
    gtk_widget_add_accelerator(itemRehacer_, "activate", atajos_, GDK_KEY_z,
                               GdkModifierType(GDK_CONTROL_MASK | GDK_SHIFT_MASK), GtkAccelFlags(0));
    agregarModulos(m, "paginas", false); // copia del menú Páginas

    m = menu(barra_, "menu.ver");
    GtkWidget* it = item(m, "accion.acercar", [this] { visor_->multiplicarZoom(kPasoZoom); }, GDK_KEY_plus, ctrl, true);
    gtk_widget_add_accelerator(it, "activate", atajos_, GDK_KEY_equal, ctrl, GtkAccelFlags(0));
    gtk_widget_add_accelerator(it, "activate", atajos_, GDK_KEY_KP_Add, ctrl, GtkAccelFlags(0));
    it = item(m, "accion.alejar", [this] { visor_->multiplicarZoom(1.0 / kPasoZoom); }, GDK_KEY_minus, ctrl, true);
    gtk_widget_add_accelerator(it, "activate", atajos_, GDK_KEY_KP_Subtract, ctrl, GtkAccelFlags(0));
    item(m, "accion.tamano_real", [this] { visor_->ponerZoom(1.0); }, GDK_KEY_0, ctrl, true);
    item(m, "accion.ajustar_ancho", [this] { visor_->ajustarAncho(); }, GDK_KEY_1, ctrl, true);
    item(m, "accion.ajustar_pagina", [this] { visor_->ajustarPagina(); }, GDK_KEY_2, ctrl, true);
    gtk_menu_shell_append(GTK_MENU_SHELL(m), gtk_separator_menu_item_new());
    item(m, "accion.pagina_anterior", [this] { visor_->irAPagina(visor_->paginaActual() - 1); }, GDK_KEY_Page_Up,
         ninguna, true);
    item(m, "accion.pagina_siguiente", [this] { visor_->irAPagina(visor_->paginaActual() + 1); }, GDK_KEY_Page_Down,
         ninguna, true);
    item(m, "accion.primera_pagina", [this] { visor_->irAPagina(0); }, GDK_KEY_Home, ctrl, true);
    item(m, "accion.ultima_pagina", [this] { visor_->irAPagina(doc_.paginas() - 1); }, GDK_KEY_End, ctrl, true);
    item(m, "accion.ir_a_pagina", [this] { accionIrAPagina(); }, GDK_KEY_g, ctrl, true);

    // Menús de los módulos registrados (solo los que tienen alguno).
    for (const char* nombreMenu : kMenusModulos) {
        const bool hay = std::any_of(Registro::instancia().modulos().begin(), Registro::instancia().modulos().end(),
                                     [&](const auto& mod) { return std::strcmp(mod->menu(), nombreMenu) == 0; });
        if (hay) agregarModulos(menu(barra_, ("menu." + std::string(nombreMenu)).c_str()), nombreMenu, true);
    }

    // Idioma: menú propio en la barra principal, antes de Ayuda.
    GtkWidget* subIdioma = menu(barra_, "menu.idioma");
    GSList* grupo = nullptr;
    for (const auto& [codigo, nombre] : Idioma::disponibles()) {
        GtkWidget* r = gtk_radio_menu_item_new_with_label(grupo, nombre.c_str());
        grupo = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(r));
        if (codigo == Idioma::global().codigo()) gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(r), TRUE);
        const std::string c = codigo;
        acciones_.push_back(std::make_unique<Accion>(Accion{[this, c] { cambiarIdioma(c); }}));
        g_signal_connect(r, "toggled", G_CALLBACK(alAlternar), acciones_.back().get());
        gtk_menu_shell_append(GTK_MENU_SHELL(subIdioma), r);
    }

    m = menu(barra_, "menu.ayuda");
    item(m, "accion.acerca_de", [this] { acercaDe(ventana(), config_); });

    gtk_box_pack_start(GTK_BOX(caja_), barra_, FALSE, FALSE, 0);
    gtk_box_reorder_child(GTK_BOX(caja_), barra_, 0);
    gtk_widget_show_all(barra_);
}

/**
 * @param conAtajos true: el elemento recibe el atajo. false: solo lo muestra
 *                  (copia en otro menú; una tecla no puede activar dos elementos).
 */
void Ventana::agregarModulos(GtkWidget* m, const char* grupo, bool conAtajos) {
    bool primero = true;
    for (const auto& mod : Registro::instancia().modulos()) {
        if (std::strcmp(mod->menu(), grupo) != 0) continue;
        // En un menú con otras entradas (Editar), los módulos van tras un separador.
        if (primero) {
            GList* hijos = gtk_container_get_children(GTK_CONTAINER(m));
            if (hijos) gtk_menu_shell_append(GTK_MENU_SHELL(m), gtk_separator_menu_item_new());
            g_list_free(hijos);
        }
        primero = false;
        const Modulo* p = mod.get();
        const std::string clave = "modulo." + std::string(p->id()) + ".nombre";
        guint tecla = 0;
        GdkModifierType mods = GdkModifierType(0);
        if (p->atajo()) gtk_accelerator_parse(p->atajo(), &tecla, &mods);
        GtkWidget* it = item(m, clave.c_str(), [this, p] { ejecutarModulo(*p); }, conAtajos ? tecla : 0, mods, true);
        if (!conAtajos && tecla) {
            GtkWidget* etiqueta = gtk_bin_get_child(GTK_BIN(it));
            if (GTK_IS_ACCEL_LABEL(etiqueta)) gtk_accel_label_set_accel(GTK_ACCEL_LABEL(etiqueta), tecla, mods);
        }
    }
}

// ---------------------------------------------------------------- bienvenida

void Ventana::construirBienvenida() {
    GtkWidget* caja = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_halign(caja, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(caja, GTK_ALIGN_CENTER);

    const fs::path logo = carpetaDatos() / desdeUtf8(config_.obtener("logo", "assets/logo.png"));
    if (GdkPixbuf* pb = gdk_pixbuf_new_from_file_at_size(aGlib(logo).c_str(), 128, 128, nullptr)) {
        gtk_box_pack_start(GTK_BOX(caja), gtk_image_new_from_pixbuf(pb), FALSE, FALSE, 0);
        g_object_unref(pb);
    }
    bienvenidaTitulo_ = gtk_label_new(nullptr);
    bienvenidaTexto_ = gtk_label_new(nullptr);
    gtk_label_set_line_wrap(GTK_LABEL(bienvenidaTexto_), TRUE);
    gtk_label_set_justify(GTK_LABEL(bienvenidaTexto_), GTK_JUSTIFY_CENTER);
    gtk_label_set_max_width_chars(GTK_LABEL(bienvenidaTexto_), 60);
    gtk_box_pack_start(GTK_BOX(caja), bienvenidaTitulo_, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(caja), bienvenidaTexto_, FALSE, FALSE, 0);
    gtk_stack_add_named(GTK_STACK(pila_), caja, "bienvenida");
    textosBienvenida();
}

void Ventana::textosBienvenida() {
    const std::string titulo = config_.obtener("titulo", IBZ_NOMBRE);
    gchar* m = g_markup_printf_escaped("<span size='xx-large' weight='bold'>%s</span>", titulo.c_str());
    gtk_label_set_markup(GTK_LABEL(bienvenidaTitulo_), m);
    g_free(m);
    std::string texto = config_.obtener("mensaje_inicial." + Idioma::global().codigo());
    if (texto.empty()) texto = tr("bienvenida.mensaje");
    gtk_label_set_text(GTK_LABEL(bienvenidaTexto_), texto.c_str());
}

// ---------------------------------------------------------------- acciones

void Ventana::abrir(const fs::path& ruta) {
    if (!confirmarDescartar()) return;
    std::string contrasena;
    for (int intento = 0;; ++intento) {
        try {
            doc_.abrir(ruta, contrasena);
            break;
        } catch (const Error& e) {
            if (e.clave() == "error.contrasena" && intento < 3) {
                if (!pedirContrasena(ventana(), intento > 0, contrasena)) return;
                continue;
            }
            mostrarError(ventana(), e);
            return;
        } catch (const std::exception& e) {
            mostrarErrorTexto(ventana(), tr("error.desconocido"), e.what());
            return;
        }
    }
    ajustes_.poner("ultima_carpeta", utf8(ruta.parent_path()));
    documentoCambiado(false);
}

/// Parámetros del diálogo «Nuevo PDF» (usa el diálogo genérico de los módulos).
class NuevoPdf final : public Modulo {
public:
    const char* id() const override { return "nuevo"; }
    const char* menu() const override { return "archivo"; }
    std::vector<Parametro> parametros() const override {
        Parametro tamano;
        tamano.id = "tamano";
        tamano.clave = "param.tamano_nuevo";
        tamano.tipo = TipoParametro::Opcion;
        tamano.opciones = {"a4", "carta", "oficio", "a5", "a3"};
        tamano.defecto = "a4";

        Parametro orientacion;
        orientacion.id = "orientacion";
        orientacion.clave = "param.orientacion";
        orientacion.tipo = TipoParametro::Opcion;
        orientacion.opciones = {"vertical", "horizontal"};
        orientacion.defecto = "vertical";
        return {tamano, orientacion};
    }
    std::string ejecutar(Documento&, const Valores&) const override { return {}; }
};

void Ventana::accionNuevo() {
    if (!confirmarDescartar()) return;
    static const NuevoPdf dialogo;
    Valores v;
    ContextoDialogo ctx;
    ctx.recordados = &ajustes_;
    if (!pedirParametros(ventana(), dialogo, v, ctx)) return;

    // Tamaños en puntos (1/72"): A4, Carta, Oficio (Legal), A5, A3.
    const std::string t = v["tamano"];
    double ancho = 595.276;
    double alto = 841.89;
    if (t == "carta") { ancho = 612.0; alto = 792.0; }
    else if (t == "oficio") { ancho = 612.0; alto = 1008.0; }
    else if (t == "a5") { ancho = 419.528; alto = 595.276; }
    else if (t == "a3") { ancho = 841.89; alto = 1190.551; }
    if (v["orientacion"] == "horizontal") std::swap(ancho, alto);
    try {
        doc_.nuevo(crearPdfVacio(ancho, alto), tr("archivo.sin_titulo") + ".pdf");
    } catch (const Error& e) {
        mostrarError(ventana(), e);
        return;
    }
    documentoCambiado(false);
}

void Ventana::accionAbrir() {
    if (auto r = elegirPdfAbrir(ventana(), ajustes_.obtener("ultima_carpeta"))) abrir(*r);
}

bool Ventana::accionGuardarComo() {
    if (!doc_.abierto()) return false;
    const bool nuevo = doc_.esNuevo();
    const std::string nombre = utf8(doc_.ruta().stem()) + (nuevo ? "" : tr("archivo.sufijo")) + ".pdf";
    const std::string carpeta = nuevo ? ajustes_.obtener("ultima_carpeta") : utf8(doc_.ruta().parent_path());
    const auto r = elegirPdfGuardar(ventana(), carpeta, nombre);
    if (!r) return false;
    try {
        doc_.guardarComo(*r);
    } catch (const Error& e) {
        mostrarError(ventana(), e);
        return false;
    }
    ajustes_.poner("ultima_carpeta", utf8(r->parent_path()));
    actualizarEstado();
    return true;
}

void Ventana::accionCerrar() {
    if (!confirmarDescartar()) return;
    doc_.cerrar();
    documentoCambiado(false);
}

void Ventana::accionSalir() {
    if (!confirmarDescartar()) return;
    guardarAjustes();
    gtk_main_quit();
}

void Ventana::accionDeshacer() {
    try {
        doc_.deshacer();
    } catch (const Error& e) {
        mostrarError(ventana(), e);
    }
    documentoCambiado(true);
}

void Ventana::accionRehacer() {
    try {
        doc_.rehacer();
    } catch (const Error& e) {
        mostrarError(ventana(), e);
    }
    documentoCambiado(true);
}

void Ventana::accionIrAPagina() {
    if (!doc_.abierto()) return;
    gtk_widget_grab_focus(campoPagina_);
    gtk_editable_select_region(GTK_EDITABLE(campoPagina_), 0, -1);
}

void Ventana::ejecutarModulo(const Modulo& m) {
    if (!doc_.abierto()) return;
    if (m.requierePosicion()) {
        // Primero el clic en la página; el diálogo se abre después (colocar()).
        moduloPendiente_ = &m;
        visor_->empezarColocar();
        actualizarEstado();
        return;
    }
    ejecutarConValores(m, {});
}

void Ventana::colocar(int pagina, int x, int y, int ancho, int alto) {
    const Modulo* m = moduloPendiente_;
    const bool dialogoHecho = dialogoHecho_;
    Valores v = std::move(valoresPendientes_);
    moduloPendiente_ = nullptr;
    dialogoHecho_ = false;
    valoresPendientes_.clear();
    actualizarEstado();
    if (!m) return;
    double px = 0.0;
    double py = 0.0;
    if (!doc_.dispositivoAPagina(pagina, ancho, alto, x, y, px, py)) return;
    v["pagina"] = std::to_string(pagina);
    v["x"] = numeroPdf(px);
    v["y"] = numeroPdf(py);
    if (dialogoHecho) {
        ejecutarDirecto(*m, std::move(v));
    } else {
        ejecutarConValores(*m, std::move(v));
    }
}

void Ventana::ejecutarConValores(const Modulo& m, Valores v) {
    ContextoDialogo ctx;
    ctx.paginaActual = visor_->paginaActual() + 1;
    ctx.totalPaginas = doc_.paginas();
    ctx.carpeta = utf8(doc_.ruta().parent_path());
    ctx.nombre = utf8(doc_.ruta().stem());
    ctx.recordados = &ajustes_;
    if (!pedirParametros(ventana(), m, v, ctx)) return;
    v["pagina_actual"] = std::to_string(ctx.paginaActual);
    if (!v.count("pagina") && m.posicionTrasDialogo(v)) {
        // Diálogo respondido; falta el clic en la página (colocar() termina).
        moduloPendiente_ = &m;
        valoresPendientes_ = std::move(v);
        dialogoHecho_ = true;
        visor_->empezarColocar();
        actualizarEstado();
        return;
    }
    ejecutarDirecto(m, std::move(v));
}

void Ventana::ejecutarDirecto(const Modulo& m, Valores v) {
    if (v.count("pagina_actual") == 0) v["pagina_actual"] = std::to_string(visor_->paginaActual() + 1);
    std::string mensaje;
    try {
        mensaje = m.ejecutar(doc_, v);
    } catch (const Error& e) {
        mostrarError(ventana(), e);
        return;
    } catch (const std::exception& e) {
        mostrarErrorTexto(ventana(), tr("error.desconocido"), e.what());
        return;
    }
    if (m.modificaDocumento()) documentoCambiado(true);
    if (!mensaje.empty()) mostrarInfo(ventana(), mensaje);
}

void Ventana::moverPorArrastre(int origen, int destino) {
    if (!doc_.abierto() || origen == destino) return;
    try {
        moverPaginas(doc_, {origen}, destino);
    } catch (const Error& e) {
        mostrarError(ventana(), e);
        return;
    }
    documentoCambiado(true);
    visor_->irAPagina(destino);
}

void Ventana::cambiarIdioma(const std::string& codigo) {
    if (codigo == Idioma::global().codigo()) return;
    idiomaPendiente_ = codigo;
    // Se reconstruyen los menús fuera de la señal del propio elemento de menú.
    g_idle_add(
        [](gpointer d) -> gboolean {
            auto* v = static_cast<Ventana*>(d);
            if (Idioma::global().cargar(v->idiomaPendiente_)) {
                v->ajustes_.poner("idioma", v->idiomaPendiente_);
                v->guardarAjustes();
                aplicarIdiomaGtk(Idioma::global().codigo());
            }
            v->construirMenus();
            v->textosBienvenida();
            v->actualizarEstado();
            return G_SOURCE_REMOVE;
        },
        this);
}

bool Ventana::confirmarDescartar() {
    if (!doc_.abierto() || !doc_.modificado()) return true;
    switch (preguntarDescartar(ventana())) {
    case Respuesta::Descartar: return true;
    case Respuesta::Guardar: return accionGuardarComo();
    case Respuesta::Cancelar: return false;
    }
    return false;
}

void Ventana::documentoCambiado(bool conservarPosicion) {
    visor_->recargar(conservarPosicion);
    miniaturas_->recargar();
    miniaturas_->seleccionar(visor_->paginaActual());
    gtk_stack_set_visible_child_name(GTK_STACK(pila_), doc_.abierto() ? "documento" : "bienvenida");
    actualizarEstado();
}

void Ventana::actualizarEstado() {
    const bool hay = doc_.abierto();
    for (GtkWidget* w : itemsDoc_) gtk_widget_set_sensitive(w, hay);
    if (itemDeshacer_) gtk_widget_set_sensitive(itemDeshacer_, doc_.puedeDeshacer());
    if (itemRehacer_) gtk_widget_set_sensitive(itemRehacer_, doc_.puedeRehacer());

    std::string titulo = config_.obtener("titulo", IBZ_NOMBRE);
    if (hay) {
        std::string nombre = utf8(doc_.ruta().filename());
        if (doc_.modificado()) nombre = "*" + nombre;
        titulo = nombre + " \xE2\x80\x94 " + titulo;
        silencioPagina_ = true;
        gtk_spin_button_set_range(GTK_SPIN_BUTTON(campoPagina_), 1, std::max(1, doc_.paginas()));
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(campoPagina_), visor_->paginaActual() + 1);
        silencioPagina_ = false;
        gtk_label_set_text(GTK_LABEL(etiquetaPagina_), tr("estado.pagina_antes").c_str());
        const std::string estado =
            formato(tr("estado.de_total"), {std::to_string(doc_.paginas())}) + "   \xC2\xB7   " +
            formato(tr("estado.zoom"), {std::to_string(std::lround(visor_->zoom() * 100.0))});
        gtk_label_set_text(GTK_LABEL(estado_), estado.c_str());
        gtk_widget_show(cajaPagina_);
        if (visor_->colocando()) gtk_label_set_text(GTK_LABEL(estado_), tr("estado.colocar").c_str());
    } else {
        gtk_widget_hide(cajaPagina_);
        gtk_label_set_text(GTK_LABEL(estado_), tr("estado.sin_documento").c_str());
    }
    gtk_window_set_title(ventana(), titulo.c_str());
}

void Ventana::guardarAjustes() {
    const bool max = gtk_window_is_maximized(ventana());
    ajustes_.poner("ventana.maximizada", max ? "1" : "0");
    if (!max) {
        int w = 0;
        int h = 0;
        gtk_window_get_size(ventana(), &w, &h);
        ajustes_.poner("ventana.ancho", std::to_string(w));
        ajustes_.poner("ventana.alto", std::to_string(h));
    }
    ajustes_.guardar(rutaAjustes());
}

// ---------------------------------------------------------------- señales

void Ventana::alActivar(GtkMenuItem*, gpointer d) {
    static_cast<Accion*>(d)->f();
}

void Ventana::alCambiarCampoPagina(GtkSpinButton* s, gpointer d) {
    auto* v = static_cast<Ventana*>(d);
    if (v->silencioPagina_ || !v->doc_.abierto()) return;
    v->visor_->irAPagina(gtk_spin_button_get_value_as_int(s) - 1);
}

void Ventana::alActivarCampoPagina(GtkEntry*, gpointer d) {
    // Intro: se aplica el número escrito (value-changed) y se vuelve a la vista.
    auto* v = static_cast<Ventana*>(d);
    gtk_spin_button_update(GTK_SPIN_BUTTON(v->campoPagina_));
    v->visor_->enfocar();
}

gboolean Ventana::alTeclaVentana(GtkWidget* w, GdkEventKey* ev, gpointer) {
    GtkWidget* foco = gtk_window_get_focus(GTK_WINDOW(w));
    if (foco && GTK_IS_EDITABLE(foco) && !(ev->state & GDK_CONTROL_MASK))
        return gtk_window_propagate_key_event(GTK_WINDOW(w), ev);
    return FALSE;
}

void Ventana::alAlternar(GtkCheckMenuItem* c, gpointer d) {
    if (gtk_check_menu_item_get_active(c)) static_cast<Accion*>(d)->f();
}

gboolean Ventana::alCerrarVentana(GtkWidget*, GdkEvent*, gpointer d) {
    static_cast<Ventana*>(d)->accionSalir();
    return TRUE; // la ventana se destruye en ~Ventana
}

void Ventana::alSoltar(GtkWidget*, GdkDragContext*, gint, gint, GtkSelectionData* datos, guint, guint, gpointer d) {
    auto* v = static_cast<Ventana*>(d);
    gchar** uris = gtk_selection_data_get_uris(datos); // GTK termina el arrastre (GTK_DEST_DEFAULT_ALL)
    if (!uris) return;
    if (uris[0]) {
        if (gchar* nombre = g_filename_from_uri(uris[0], nullptr, nullptr)) {
            v->rutaSoltada_ = rutaDesdeGlib(nombre);
            g_free(nombre);
            // Se abre fuera de la señal del arrastre (puede mostrar diálogos).
            g_idle_add(
                [](gpointer p) -> gboolean {
                    auto* vv = static_cast<Ventana*>(p);
                    vv->abrir(vv->rutaSoltada_);
                    return G_SOURCE_REMOVE;
                },
                v);
        }
    }
    g_strfreev(uris);
}

} // namespace

int ejecutar(const Claves& config, Claves& ajustes, const std::string& archivo) {
    gtk_init(nullptr, nullptr);

    const fs::path icono = carpetaDatos() / desdeUtf8(config.obtener("icono", "assets/ibanascapdf.png"));
    gtk_window_set_default_icon_from_file(aGlib(icono).c_str(), nullptr);

    Ventana v(config, ajustes);
    v.mostrar();
    if (!archivo.empty()) v.abrir(desdeUtf8(archivo));
    gtk_main();
    return 0;
}

} // namespace ibz::gui
