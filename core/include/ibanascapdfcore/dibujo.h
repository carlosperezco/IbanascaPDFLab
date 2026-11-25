/**
 * @file dibujo.h
 * @brief Firma dibujada a mano (ratón o dedo) guardada como trazos vectoriales.
 *
 * La interfaz recoge los puntos de cada trazo en su lienzo y los pasa como texto
 * (un valor más de @ref ibz::Valores). El núcleo los recorta al contorno, los
 * suaviza con curvas de Bézier y los escribe en el PDF como trazado vectorial:
 * sin imágenes ni bibliotecas, nítido a cualquier zoom y de pocos bytes.
 */
#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ibz {

class Documento;

/// Trazos dibujados en un lienzo (coordenadas del lienzo: origen arriba a la izquierda).
struct Dibujo {
    double ancho = 0;   ///< ancho del lienzo
    double alto = 0;    ///< alto del lienzo
    double grosor = 3;  ///< grosor del trazo, en unidades del lienzo
    std::vector<std::vector<std::pair<float, float>>> trazos;

    bool vacio() const noexcept;
};

/**
 * @brief Lee el formato de texto: "ibzd1;ancho,alto,grosor;x,y,x,y,…;x,y,…".
 *
 * Cada grupo separado por ';' tras la cabecera es un trazo. Los números usan punto
 * decimal. Texto vacío = dibujo vacío.
 * @throws ibz::Error("error.dibujo") si el texto no es válido.
 */
Dibujo leerDibujo(std::string_view texto);

/// Escribe el formato de texto de leerDibujo().
std::string escribirDibujo(const Dibujo& dibujo);

/// Proporción alto/ancho del dibujo recortado a sus trazos (0 si está vacío).
double proporcionDibujo(const Dibujo& dibujo);

/**
 * @brief Operadores PDF que dibujan los trazos recortados en un recuadro de ancho
 *        @p ancho (y alto ancho·proporción) con el origen abajo a la izquierda.
 */
std::string contenidoDibujo(const Dibujo& dibujo, double ancho, double r, double g, double b);

/**
 * @brief Añade el dibujo a una página como trazado vectorial. Se puede deshacer.
 * @param pagina  página (base 0)
 * @param x,y     esquina superior izquierda tal como se ve (puntos PDF)
 * @param anchoPt ancho del dibujo en puntos; el alto conserva la proporción
 * @throws ibz::Error("error.dibujo_vacio"), ("error.rangos"), ("error.qpdf")
 */
void insertarDibujo(Documento& doc, int pagina, double x, double y, double anchoPt, const Dibujo& dibujo, double r,
                    double g, double b);

} // namespace ibz
