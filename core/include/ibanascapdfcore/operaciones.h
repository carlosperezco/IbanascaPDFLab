/**
 * @file operaciones.h
 * @brief Operaciones sobre páginas hechas con QPDF.
 *
 * Todas reciben índices de página base 0, generan un PDF nuevo y lo entregan
 * con Documento::reemplazar() (se pueden deshacer). Las usan los módulos y la
 * interfaz (por ejemplo, al arrastrar miniaturas).
 */
#pragma once

#include <filesystem>
#include <vector>

namespace ibz {

class Documento;

/**
 * @brief Gira páginas.
 * @param angulo Múltiplo de 90; positivo = sentido horario.
 * @throws ibz::Error("error.qpdf") si QPDF no puede procesar el documento.
 */
void rotarPaginas(Documento& doc, const std::vector<int>& paginas, int angulo);

/**
 * @brief Elimina páginas (los índices repetidos se ignoran).
 * @throws ibz::Error("error.eliminar_todas") si se pidieran eliminar todas.
 */
void eliminarPaginas(Documento& doc, const std::vector<int>& paginas);

/**
 * @brief Mueve páginas a otra posición.
 *
 * Las páginas indicadas (en ese orden) quedan juntas y la primera de ellas
 * pasa a ocupar la posición @p posicion (base 0) del documento resultante.
 */
void moverPaginas(Documento& doc, const std::vector<int>& paginas, int posicion);

/**
 * @brief Aplica un orden completo: @p orden contiene cada página exactamente una vez.
 * @throws ibz::Error("error.orden") si no es una permutación de todas las páginas.
 */
void reordenarPaginas(Documento& doc, const std::vector<int>& orden);

/// PDF nuevo con una página en blanco de @p ancho × @p alto puntos.
std::vector<unsigned char> crearPdfVacio(double ancho, double alto);

/**
 * @brief Inserta una página en blanco.
 * @param posicion Índice (base 0) que ocupará la página nueva (0 … doc.paginas()).
 * @param ancho, alto Tamaño en puntos PDF.
 */
void insertarPaginaBlanco(Documento& doc, int posicion, double ancho, double alto);

/**
 * @brief Duplica páginas: cada copia queda justo después de su original.
 *
 * Las copias comparten el contenido con el original (sin aumentar el tamaño del
 * archivo); las anotaciones (enlaces, campos) no se copian.
 * @param copias Número de copias de cada página (1 o más).
 */
void duplicarPaginas(Documento& doc, const std::vector<int>& paginas, int copias);

/**
 * @brief Inserta en el documento todas las páginas de otros PDF, en el orden dado.
 * @param posicion Índice (base 0) donde queda la primera página insertada:
 *                 0 = al principio, doc.paginas() = al final.
 * @throws ibz::Error("error.sin_archivos"), ("error.leer"), ("error.formato"),
 *         ("error.contrasena_archivo") con la ruta del archivo en el detalle.
 */
void fusionarArchivos(Documento& doc, const std::vector<std::filesystem::path>& archivos, int posicion);

/**
 * @brief Escribe cada grupo de páginas en un PDF nuevo. No modifica el documento.
 *
 * Antes de escribir nada comprueba que ningún destino exista (nunca se
 * sobrescriben archivos) y que no haya destinos repetidos.
 * @throws ibz::Error("error.existe") con la ruta, ("error.escribir"), ("error.rangos").
 */
void escribirGrupos(const Documento& doc, const std::vector<std::vector<int>>& grupos,
                    const std::vector<std::filesystem::path>& destinos);

} // namespace ibz
