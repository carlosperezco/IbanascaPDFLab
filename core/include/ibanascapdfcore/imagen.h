/**
 * @file imagen.h
 * @brief Imágenes PNG para la firma visual: lectura propia (con zlib) e inserción con QPDF.
 *
 * El lector cubre los PNG habituales: gris, gris con alfa, RGB, RGBA y paleta,
 * de 1 a 16 bits, con transparencia (tRNS). No admite PNG entrelazados (Adam7).
 */
#pragma once

#include <cstdint>
#include <filesystem>
#include <string_view>
#include <vector>

namespace ibz {

class Documento;

/// Imagen de 8 bits por canal, filas de arriba abajo.
struct ImagenRaster {
    int ancho = 0;
    int alto = 0;
    int canales = 3;                ///< 1 (gris) o 3 (RGB)
    std::vector<std::uint8_t> color; ///< ancho × alto × canales
    std::vector<std::uint8_t> alfa;  ///< ancho × alto, vacío si es opaca
};

/**
 * @brief Lee un PNG desde memoria.
 * @throws ibz::Error("error.png") si no es un PNG válido, ("error.png_entrelazado").
 */
ImagenRaster leerPng(std::string_view datos);

/// Lee un PNG desde archivo. @throws ibz::Error("error.leer"), ("error.png"), …
ImagenRaster leerPng(const std::filesystem::path& ruta);

/**
 * @brief Vuelve transparente el fondo blanco (papel escaneado o fotografiado).
 *
 * Los píxeles muy claros pasan a transparentes con un borde suave, de modo que
 * el trazo queda limpio sobre cualquier fondo.
 */
void quitarFondoBlanco(ImagenRaster& img);

/**
 * @brief Inserta la imagen en una página (se puede deshacer).
 *
 * @p x, @p y (puntos PDF, espacio de usuario) son la esquina superior izquierda
 * tal como se ve; la imagen queda derecha aunque la página tenga /Rotate.
 * @param anchoPt Ancho en puntos; el alto se calcula para conservar la proporción.
 */
void insertarImagen(Documento& doc, int pagina, double x, double y, double anchoPt, const ImagenRaster& img);

} // namespace ibz
