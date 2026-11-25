/**
 * @file sha256.h
 * @brief SHA-256 (FIPS 180-4) propio del núcleo: así la firma no depende de una
 *        biblioteca criptográfica concreta (GnuTLS en escritorio, Java en Android).
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace ibz::interno {

/// Resumen SHA-256 incremental.
class Sha256 {
public:
    Sha256() noexcept;
    void agregar(const unsigned char* datos, std::size_t n) noexcept;
    std::array<unsigned char, 32> terminar() noexcept;

    /// Resumen de un bloque completo.
    static std::array<unsigned char, 32> de(const unsigned char* datos, std::size_t n) noexcept;

private:
    void bloque(const unsigned char* p) noexcept;
    std::uint32_t h_[8];
    unsigned char buf_[64];
    std::size_t lleno_ = 0;
    std::uint64_t total_ = 0;
};

} // namespace ibz::interno
