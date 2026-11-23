/**
 * @file dialogos.h
 * @brief Diálogos de la interfaz (archivos, contraseña, errores, parámetros, acerca de).
 */
#pragma once

#include <gtk/gtk.h>

#include <ibanascapdfcore/modulo.h>

#include <filesystem>
#include <optional>
#include <string>

namespace ibz {
class Claves;
class Error;
} // namespace ibz

namespace ibz::gui {

/// Elegir un PDF para abrir (diálogo nativo del sistema).
std::optional<std::filesystem::path> elegirPdfAbrir(GtkWindow* padre, const std::string& carpeta);

/// Elegir dónde guardar un PDF; pide confirmación si el archivo ya existe.
std::optional<std::filesystem::path> elegirPdfGuardar(GtkWindow* padre, const std::string& carpeta,
                                                      const std::string& nombre);

/// Pide la contraseña de un PDF protegido. Devuelve false si se cancela.
bool pedirContrasena(GtkWindow* padre, bool incorrecta, std::string& contrasena);

/// Muestra un ibz::Error traducido.
void mostrarError(GtkWindow* padre, const Error& e);

/// Muestra un error con texto ya traducido.
void mostrarErrorTexto(GtkWindow* padre, const std::string& principal, const std::string& detalle = {});

/// Respuesta a «hay cambios sin guardar».
enum class Respuesta { Guardar, Descartar, Cancelar };

/// Pregunta qué hacer con los cambios sin guardar.
Respuesta preguntarDescartar(GtkWindow* padre);

/// Ventana «Acerca de».
void acercaDe(GtkWindow* padre, const Claves& config);

/// Datos del documento que usa el diálogo de parámetros (comodines de Parametro::defecto).
struct ContextoDialogo {
    int paginaActual = 1;   ///< "@actual" (desde 1)
    int totalPaginas = 1;   ///< máximo de los enteros con maximo = -1
    std::string carpeta;    ///< "@carpeta" (UTF-8); también carpeta inicial al elegir archivos
    std::string nombre;     ///< "@nombre" (UTF-8, sin extensión)
    /// Últimos valores elegidos (fuente, tamaño, color, opciones…); se leen y se actualizan.
    Claves* recordados = nullptr;
};

/**
 * @brief Diálogo genérico con los parámetros de un módulo.
 * @return false si el usuario cancela. Si el módulo no tiene parámetros devuelve true sin mostrar nada.
 */
bool pedirParametros(GtkWindow* padre, const Modulo& modulo, Valores& valores, const ContextoDialogo& ctx);

/// Mensaje informativo (resultado de una operación).
void mostrarInfo(GtkWindow* padre, const std::string& texto);

} // namespace ibz::gui
