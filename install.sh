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

# The doctor CLI ships in the same package, so its suite runs here rather than
# with the engine tests above — a broken verdict layer must not reach an install.
echo "==> Running doctor CLI tests ..."
./build/cli/telebit_doctor_tests

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

if [[ "$MODE" == "system" ]]; then
  ENVD_FILE="${PREFIX}/lib/environment.d/60-telebit-fcitx5.conf"
else
  ENVD_FILE="${HOME}/.config/environment.d/60-telebit-fcitx5.conf"
fi

echo
echo "Done."
echo "- Add input method 'telebit-fcitx5' in fcitx5-configtool (Input Method -> Add -> Telebit / telebit-fcitx5)."
echo "- Then restart fcitx5: fcitx5 -r"
echo
echo "- Check the whole input-method path, sandboxed apps included: telebit doctor"
echo
echo "Installed ${ENVD_FILE}: it points GTK_IM_MODULE / QT_IM_MODULE / XMODIFIERS at"
echo "fcitx5 for the whole graphical session, Flatpak apps included."
echo "- Log out and back in for it to take effect (systemd reads environment.d at session start)."
echo "- To opt out: sudo ln -s /dev/null /etc/environment.d/60-telebit-fcitx5.conf"

