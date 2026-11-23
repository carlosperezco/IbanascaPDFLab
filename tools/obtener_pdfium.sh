#!/usr/bin/env bash
# Descarga PDFium precompilado (https://github.com/bblanchon/pdfium-binaries)
# en third_party/pdfium. Funciona en Linux y en MSYS2 (terminal UCRT64).
#
# Uso:
#   ./tools/obtener_pdfium.sh            # última versión publicada
#   PDFIUM_VERSION=7350 ./tools/obtener_pdfium.sh   # versión fija (chromium/7350)
#   PDFIUM_ARCHIVO=~/Downloads/pdfium-win-x64.tgz ./tools/obtener_pdfium.sh   # ya descargado
#
# Para publicar una versión del programa conviene fijar PDFIUM_VERSION y
# anotarla en CHANGELOG.md.
set -euo pipefail

raiz="$(cd "$(dirname "$0")/.." && pwd)"
destino="$raiz/third_party/pdfium"
version="${PDFIUM_VERSION:-latest}"

case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*) archivo="pdfium-win-x64.tgz" ;;
    Linux)
        case "$(uname -m)" in
            x86_64) archivo="pdfium-linux-x64.tgz" ;;
            aarch64|arm64) archivo="pdfium-linux-arm64.tgz" ;;
            *) echo "Arquitectura no prevista: $(uname -m)" >&2; exit 1 ;;
        esac ;;
    *) echo "Sistema no previsto: $(uname -s)" >&2; exit 1 ;;
esac

if [[ "$version" == "latest" ]]; then
    url="https://github.com/bblanchon/pdfium-binaries/releases/latest/download/$archivo"
else
    url="https://github.com/bblanchon/pdfium-binaries/releases/download/chromium%2F$version/$archivo"
fi

temporal="$(mktemp -d)"
trap 'rm -rf "$temporal"' EXIT

if [[ -n "${PDFIUM_ARCHIVO:-}" ]]; then
    # Archivo ya descargado a mano (por ejemplo con el navegador).
    echo "Usando $PDFIUM_ARCHIVO"
    cp "$PDFIUM_ARCHIVO" "$temporal/$archivo"
else
    echo "Descargando $url"
    if ! curl -fL --retry 3 -o "$temporal/$archivo" "$url"; then
        # En Windows, el curl de Windows usa los certificados del sistema
        # (incluidos los de un antivirus o un proxy de empresa).
        curl_windows="/c/Windows/System32/curl.exe"
        if [[ -x "$curl_windows" ]]; then
            echo "Reintentando con el curl de Windows"
            "$curl_windows" -fL --retry 3 -o "$temporal/$archivo" "$url"
        else
            exit 1
        fi
    fi
fi

rm -rf "$destino"
mkdir -p "$destino"
tar -xzf "$temporal/$archivo" -C "$destino"

echo "PDFium instalado en $destino"
[[ -f "$destino/VERSION" ]] && cat "$destino/VERSION"
