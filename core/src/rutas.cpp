/**
 * @file rutas.cpp
 * @brief Implementación de rutas y conversión UTF-8.
 */
#include "ibanascapdfcore/rutas.h"

#include <cstdlib>
#include <system_error>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

namespace fs = std::filesystem;

namespace ibz {

#ifdef _WIN32
namespace {
/// UTF-16 → UTF-8.
std::string anchoAUtf8(const wchar_t* w) {
    if (!w || !*w) return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    if (n <= 1) return {};
    std::string s(static_cast<size_t>(n - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w, -1, s.data(), n, nullptr, nullptr);
    return s;
}
/// UTF-8 → UTF-16.
std::wstring utf8AAncho(const char* s) {
    if (!s || !*s) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, nullptr, 0);
    if (n <= 1) return {};
    std::wstring w(static_cast<size_t>(n - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s, -1, w.data(), n);
    return w;
}
} // namespace
#endif

// Válido en C++17 y C++20: en C++20 u8string() devuelve std::u8string y
// u8path() está obsoleto (QPDF 12 obliga a compilar con -std=c++20).
std::string utf8(const fs::path& ruta) {
    const auto s = ruta.u8string();
    return std::string(s.begin(), s.end());
}

fs::path desdeUtf8(const std::string& texto) {
#if defined(__cpp_char8_t)
    return fs::path(std::u8string(texto.begin(), texto.end()));
#else
    return fs::u8path(texto);
#endif
}

std::string variableEntorno(const char* nombre) {
#ifdef _WIN32
    const std::wstring n = utf8AAncho(nombre);
    return anchoAUtf8(_wgetenv(n.c_str()));
#else
    const char* v = std::getenv(nombre);
    return v ? std::string(v) : std::string();
#endif
}

fs::path carpetaEjecutable() {
#ifdef _WIN32
    std::wstring b(MAX_PATH, L'\0');
    for (;;) {
        const DWORD n = GetModuleFileNameW(nullptr, b.data(), static_cast<DWORD>(b.size()));
        if (n == 0) return fs::current_path();
        if (n < b.size()) {
            b.resize(n);
            break;
        }
        b.resize(b.size() * 2);
    }
    return fs::path(b).parent_path();
#else
    std::error_code ec;
    const fs::path p = fs::read_symlink("/proc/self/exe", ec);
    return ec ? fs::current_path() : p.parent_path();
#endif
}

namespace {

/// Carpetas fijadas por la plataforma (Android); vacías = se calculan.
fs::path& datosFijada() {
    static fs::path p;
    return p;
}

fs::path& usuarioFijada() {
    static fs::path p;
    return p;
}

} // namespace

void establecerCarpetas(const fs::path& datos, const fs::path& usuario) {
    datosFijada() = datos;
    usuarioFijada() = usuario;
}

const fs::path& carpetaDatos() {
    if (!datosFijada().empty()) return datosFijada();
    static const fs::path carpeta = [] {
        const std::string forzada = variableEntorno("IBANASCAPDF_DATOS");
        if (!forzada.empty()) return desdeUtf8(forzada);
        const fs::path exe = carpetaEjecutable();
        const fs::path share = exe.parent_path() / "share" / "ibanascapdf";
        std::error_code ec;
        if (fs::is_directory(share, ec)) return share;
        return exe;
    }();
    return carpeta;
}

fs::path carpetaUsuario() {
    fs::path base;
    if (!usuarioFijada().empty()) {
        std::error_code ec;
        fs::create_directories(usuarioFijada(), ec);
        return usuarioFijada();
    }
#ifdef _WIN32
    const std::string appdata = variableEntorno("APPDATA");
    base = (appdata.empty() ? carpetaEjecutable() : desdeUtf8(appdata)) / "DonatellaTech" / "IbanascaPDFLab";
#else
    const std::string xdg = variableEntorno("XDG_CONFIG_HOME");
    if (!xdg.empty()) {
        base = desdeUtf8(xdg);
    } else {
        const std::string home = variableEntorno("HOME");
        base = (home.empty() ? fs::current_path() : desdeUtf8(home)) / ".config";
    }
    base /= fs::path("donatellatech") / "ibanascapdf";
#endif
    std::error_code ec;
    fs::create_directories(base, ec);
    return base;
}

fs::path rutaAjustes() {
    return carpetaUsuario() / "ajustes.conf";
}

std::vector<std::string> argumentosUtf8(int argc, char** argv) {
    std::vector<std::string> args;
#ifdef _WIN32
    (void)argc;
    (void)argv;
    int n = 0;
    LPWSTR* w = CommandLineToArgvW(GetCommandLineW(), &n);
    if (w) {
        args.reserve(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i) args.push_back(anchoAUtf8(w[i]));
        LocalFree(w);
    }
#else
    args.reserve(static_cast<size_t>(argc));
    for (int i = 0; i < argc; ++i) args.emplace_back(argv[i]);
#endif
    return args;
}

} // namespace ibz
