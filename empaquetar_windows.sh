#!/usr/bin/env bash
# Prepara dist/ para Windows con el ejecutable, sus DLL, los recursos de GTK y
# las licencias. Con --instalador genera además el instalador NSIS en instalador/.
#
# Ejecutar en MSYS2, terminal UCRT64:
#   ./empaquetar_windows.sh               # solo dist/
#   ./empaquetar_windows.sh --instalador  # dist/ + instalador
set -euo pipefail

raiz="$(cd "$(dirname "$0")" && pwd)"
cd "$raiz"

if [[ "${MSYSTEM:-}" != "UCRT64" ]]; then
    echo "Ejecute este script en la terminal UCRT64 de MSYS2." >&2
    exit 1
fi

instalador=0
[[ "${1:-}" == "--instalador" ]] && instalador=1

build="$raiz/build"
dist="$raiz/dist"
prefijo="/ucrt64"

echo "== Compilando"
cmake -S . -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build "$build"

echo "== Preparando dist/"
rm -rf "$dist"
mkdir -p "$dist/bin" "$dist/share" "$dist/lib" "$dist/licenses"

cp "$build/bin/ibanascapdf.exe" "$dist/bin/"
cp "$build/bin/pdfium.dll" "$dist/bin/"
cp -r "$build/share/ibanascapdf" "$dist/share/"
cp LICENSE THIRD_PARTY_NOTICES.md "$dist/"

# --- Recursos de GTK 3
# Cargadores de imágenes (PNG, SVG…) con su caché.
cp -r "$prefijo/lib/gdk-pixbuf-2.0" "$dist/lib/"
# Esquemas de GSettings (los usa el selector de archivos de GTK).
mkdir -p "$dist/share/glib-2.0/schemas"
cp "$prefijo"/share/glib-2.0/schemas/org.gtk.Settings.*.gschema.xml "$dist/share/glib-2.0/schemas/" 2>/dev/null || true
glib-compile-schemas "$dist/share/glib-2.0/schemas"
# Iconos del tema (flechas, carpetas, etc.).
mkdir -p "$dist/share/icons"
cp -r "$prefijo/share/icons/Adwaita" "$dist/share/icons/"
cp -r "$prefijo/share/icons/hicolor" "$dist/share/icons/"
# Traducciones internas de GTK (botones del selector, etc.) en español e italiano.
for l in es it; do
    mkdir -p "$dist/share/locale/$l/LC_MESSAGES"
    for mo in gtk30 gtk30-properties glib20; do
        f="$prefijo/share/locale/$l/LC_MESSAGES/$mo.mo"
        [[ -f "$f" ]] && cp "$f" "$dist/share/locale/$l/LC_MESSAGES/"
    done
done

# --- DLL: las del ejecutable, de PDFium y de los cargadores de imágenes.
# Se buscan de forma recursiva con objdump (viene con el compilador; no hace
# falta ldd). Solo se copian las de $prefijo/bin: las de Windows se ignoran.
echo "== Copiando DLL"
lista_dll="$(mktemp)"
pendientes=("$dist/bin/ibanascapdf.exe" "$dist/bin/pdfium.dll")
while IFS= read -r -d '' f; do pendientes+=("$f"); done \
    < <(find "$dist/lib/gdk-pixbuf-2.0" -name '*.dll' -print0)
declare -A visto=()
while ((${#pendientes[@]})); do
    actual="${pendientes[0]}"
    pendientes=("${pendientes[@]:1}")
    while read -r nombre; do
        clave="${nombre,,}"
        [[ -n "${visto[$clave]:-}" ]] && continue
        visto[$clave]=1
        origen="$prefijo/bin/$nombre"
        [[ -f "$origen" ]] || continue # DLL del sistema (KERNEL32, api-ms-…)
        echo "$origen" >> "$lista_dll"
        cp -n "$origen" "$dist/bin/"
        pendientes+=("$origen")
    done < <(objdump -p "$actual" | awk '/DLL Name:/ {print $3}' | tr -d '\r')
done
sort -u -o "$lista_dll" "$lista_dll"
echo "   $(wc -l < "$lista_dll") DLL copiadas"

# --- Licencias de los paquetes de MSYS2 incluidos.
echo "== Copiando licencias"
lista="$dist/licenses/LISTA.txt"
printf 'Paquete\tVersión\tLicencia\n' > "$lista"
{
    cat "$lista_dll"
    find "$dist/lib/gdk-pixbuf-2.0" -name '*.dll' | sed "s|^$dist/lib|$prefijo/lib|"
    echo "$prefijo/share/icons/Adwaita/index.theme"
} | while read -r f; do
    pacman -Qoq "$f" 2>/dev/null || true
done | sort -u | while read -r paquete; do
    base="${paquete#mingw-w64-ucrt-x86_64-}"
    version="$(pacman -Q "$paquete" | awk '{print $2}')"
    licencia="$(pacman -Qi "$paquete" | awk -F': ' '/^Licenses/ {print $2}')"
    printf '%s\t%s\t%s\n' "$paquete" "$version" "$licencia" >> "$lista"
    if [[ -d "$prefijo/share/licenses/$base" ]]; then
        cp -r "$prefijo/share/licenses/$base" "$dist/licenses/"
    fi
done
rm -f "$lista_dll"

# PDFium (binarios de bblanchon/pdfium-binaries) y sus componentes.
mkdir -p "$dist/licenses/pdfium"
cp third_party/pdfium/LICENSE* "$dist/licenses/pdfium/" 2>/dev/null || true
[[ -d third_party/pdfium/licenses ]] && cp -r third_party/pdfium/licenses/. "$dist/licenses/pdfium/"
pdfium_ver="$(head -n 4 third_party/pdfium/VERSION 2>/dev/null | tr '\n' ' ')"
printf 'pdfium (bblanchon/pdfium-binaries)\t%s\tBSD-3-Clause / Apache-2.0 (ver licenses/pdfium)\n' "$pdfium_ver" >> "$lista"

echo "== dist/ listo: $(du -sh "$dist" | cut -f1)"
echo "   Revise la lista de licencias: dist/licenses/LISTA.txt"

if [[ $instalador -eq 1 ]]; then
    echo "== Generando instalador NSIS"
    mkdir -p "$raiz/instalador"
    (cd "$build" && cpack -G NSIS -B "$raiz/instalador")
    rm -rf "$raiz/instalador/_CPack_Packages"
    ls -1 "$raiz"/instalador/*.exe
fi
