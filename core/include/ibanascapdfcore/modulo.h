/**
 * @file modulo.h
 * @brief Interfaz de los módulos (operaciones) y registro automático.
 *
 * Añadir una función = añadir un archivo en `core/src/modules/`:
 *
 * @code
 * class Rotar final : public ibz::Modulo {
 * public:
 *     const char* id() const override { return "rotar"; }
 *     const char* menu() const override { return "paginas"; }
 *     std::vector<ibz::Parametro> parametros() const override { ... }
 *     void ejecutar(ibz::Documento& doc, const ibz::Valores& v) const override { ... }
 * };
 * IBZ_REGISTRAR_MODULO(Rotar);
 * @endcode
 *
 * CMake recoge los archivos de `modules/` automáticamente y la interfaz crea
 * la entrada de menú `modulo.<id>.nombre` y el diálogo de parámetros.
 */
#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ibz {

class Documento;

/// Tipo de dato de un parámetro (decide el control del diálogo).
enum class TipoParametro {
    Entero,     ///< número entero entre minimo y maximo
    Texto,      ///< texto libre
    Opcion,     ///< una de @ref Parametro::opciones (claves de traducción)
    Paginas,     ///< rangos de páginas, ver rangos.h
    Contrasena,  ///< texto oculto
    ArchivosPdf, ///< lista ordenada de PDF; valor: rutas UTF-8 separadas por '\n'
    Carpeta,     ///< carpeta; valor: ruta UTF-8
    TextoLargo,  ///< texto de varias líneas
    Color,       ///< color; valor "#rrggbb"
    ArchivoImagen,     ///< imagen PNG; valor: ruta UTF-8
    ArchivoCertificado, ///< certificado: ruta UTF-8 de un .p12/.pfx (Android: o "keychain:<alias>")
    Dibujo             ///< firma dibujada con el ratón o el dedo; valor: formato de dibujo.h
};

/// Descripción de un parámetro de un módulo.
struct Parametro {
    std::string id;                    ///< nombre interno ("angulo")
    std::string clave;                 ///< clave de traducción de la etiqueta
    TipoParametro tipo = TipoParametro::Texto;
    /// Valor inicial. Comodines: "@actual" (página visible), "@carpeta" (carpeta del
    /// documento), "@nombre" (nombre del documento sin extensión).
    std::string defecto;
    std::vector<std::string> opciones; ///< para Opcion: ids; la etiqueta es "<clave>.<id>"
    int minimo = 0;                    ///< para Entero
    int maximo = 0;                    ///< para Entero; -1 = número de páginas del documento
    /**
     * @brief Condición para mostrar el control: "otro_id=valor1|valor2".
     *
     * Vacío = siempre visible. La interfaz oculta el control mientras el parámetro
     * @c otro_id no tenga uno de esos valores, y entonces no lo envía.
     */
    std::string visibleSi;
};

/**
 * @brief true si @p p debe mostrarse con los valores actuales de los otros parámetros.
 * @param valorDe función que devuelve el valor actual de un parámetro por su id.
 */
template <typename F>
bool parametroVisible(const Parametro& p, F&& valorDe) {
    if (p.visibleSi.empty()) return true;
    const std::size_t igual = p.visibleSi.find('=');
    if (igual == std::string::npos) return true;
    const std::string actual = valorDe(p.visibleSi.substr(0, igual));
    std::string_view resto = std::string_view(p.visibleSi).substr(igual + 1);
    while (true) {
        const std::size_t barra = resto.find('|');
        if (resto.substr(0, barra) == actual) return true;
        if (barra == std::string_view::npos) return false;
        resto = resto.substr(barra + 1);
    }
}

/**
 * @brief Valores elegidos por el usuario: id del parámetro → texto.
 *
 * La interfaz añade siempre "pagina_actual" (número de la página visible, desde 1).
 */
using Valores = std::unordered_map<std::string, std::string>;

/**
 * @brief Páginas (base 0) del valor @p id: rangos como "1-3,5"; vacío = página actual.
 * @throws ibz::Error("error.rangos") si el texto no es válido.
 */
std::vector<int> paginasElegidas(const Valores& valores, const std::string& id, int total);

/// Valor de @p id, o @p defecto si falta o está vacío.
std::string valor(const Valores& valores, const std::string& id, const std::string& defecto = {});

/// Valor real de @p id leído con punto decimal (independiente del locale), o @p defecto.
double valorReal(const Valores& valores, const std::string& id, double defecto);

/// Color "#rrggbb" → componentes de 0 a 1 (negro si no es válido).
void colorRgb(const std::string& texto, double& r, double& g, double& b);

/// Operación sobre un documento.
class Modulo {
public:
    virtual ~Modulo() = default;
    /// Identificador único ("rotar"); también forma las claves de traducción.
    virtual const char* id() const = 0;
    /// Menú donde aparece: "paginas", "documento" o "firma".
    virtual const char* menu() const = 0;
    /// Posición dentro del menú (menor = más arriba).
    virtual int orden() const { return 100; }
    /// Atajo de teclado en la sintaxis de GTK ("<Control>r", "Delete") o nullptr.
    virtual const char* atajo() const { return nullptr; }
    /// Parámetros que se piden al usuario antes de ejecutar.
    virtual std::vector<Parametro> parametros() const = 0;
    /// false si la operación no cambia el documento (por ejemplo, separar en archivos).
    virtual bool modificaDocumento() const { return true; }
    /**
     * @brief true si el usuario debe hacer clic en la página antes del diálogo.
     *
     * La interfaz añade entonces a los valores "pagina" (base 0) y "x", "y"
     * (punto pulsado, en puntos PDF del espacio de usuario de la página).
     */
    virtual bool requierePosicion() const { return false; }
    /**
     * @brief true si, con los valores elegidos, hace falta un clic en la página
     *        después del diálogo (por ejemplo, firma digital visible).
     *
     * La interfaz añade entonces "pagina", "x" e "y" como en requierePosicion().
     */
    virtual bool posicionTrasDialogo(const Valores&) const { return false; }
    /**
     * @brief Ejecuta la operación. Lanza ibz::Error si falla.
     * @return Mensaje ya traducido para el usuario, o cadena vacía.
     */
    virtual std::string ejecutar(Documento& doc, const Valores& valores) const = 0;
};

/// Registro global de módulos (se rellena antes de main()).
class Registro {
public:
    static Registro& instancia();
    /// Añade un módulo; devuelve true (para usarlo en inicializadores estáticos).
    bool agregar(std::unique_ptr<Modulo> modulo);
    /// Módulos ordenados por menú, orden e id.
    const std::vector<std::unique_ptr<Modulo>>& modulos() const noexcept { return modulos_; }
    /// Módulo con ese id o nullptr.
    const Modulo* buscar(std::string_view id) const noexcept;

private:
    std::vector<std::unique_ptr<Modulo>> modulos_;
};

} // namespace ibz

/// Registra un módulo. Uso: `IBZ_REGISTRAR_MODULO(MiClase);` al final del archivo.
#define IBZ_REGISTRAR_MODULO(Clase)                    \
    [[maybe_unused]] static const bool ibzRegistro_##Clase = \
        ::ibz::Registro::instancia().agregar(std::make_unique<Clase>())
