#!/usr/bin/env bash

set -euo pipefail

MODE="system"
if [[ "${1:-}" == "--system" ]]; then
  MODE="system"
elif [[ "${1:-}" == "--user" ]]; then
  MODE="user"
elif [[ "${1:-}" != "" ]]; then
  echo "Usage: $0 [--user|--system]"
  echo "  --system Install Telebit (fcitx5) into /usr (requires sudo) (default)"
  echo "  --user   Install Telebit (fcitx5) into \$HOME/.local"
  exit 1
fi

if [[ "$MODE" == "system" ]]; then
  PREFIX="/usr"
else
  PREFIX="${HOME}/.local"
fi

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "==> Building core library and tests (C++17, CMake) ..."
cmake -B "${ROOT_DIR}/build" "${ROOT_DIR}"
cmake --build "${ROOT_DIR}/build"

echo "==> Running C++ tests ..."
"${ROOT_DIR}/build/telebit_telex_tests"

echo "==> Building fcitx5 addon 'telebit-fcitx5' (mode: ${MODE}, prefix: ${PREFIX}) ..."
cd "${ROOT_DIR}/telebit-fcitx5"
cmake -B build -DCMAKE_INSTALL_PREFIX="${PREFIX}" .
cmake --build build

echo "==> Installing addon 'telebit-fcitx5' into ${PREFIX} ..."
if [[ "$MODE" == "system" ]]; then
  sudo cmake --install build
else
  cmake --install build
fi

ICON_HICOLOR="${PREFIX}/share/icons/hicolor"
if [[ -d "${ICON_HICOLOR}" ]] && command -v gtk-update-icon-cache >/dev/null 2>&1; then
  echo "==> Refreshing GTK icon cache (hicolor) ..."
  if [[ "$MODE" == "system" ]]; then
    sudo gtk-update-icon-cache -f "${ICON_HICOLOR}" 2>/dev/null || true
  else
    gtk-update-icon-cache -f "${ICON_HICOLOR}" 2>/dev/null || true
  fi
fi
# Qt often caches theme icons; stale IM menu art is usually fixed after this + fcitx5 -r
if [[ "$MODE" != "system" ]]; then
  rm -f "${HOME}/.cache/icon-cache.kcache" 2>/dev/null || true
fi

echo
echo "Done."
echo "- Add input method 'telebit-fcitx5' in fcitx5-configtool (Input Method -> Add -> Telebit / telebit-fcitx5)."
echo "- Then restart fcitx5: fcitx5 -r"

