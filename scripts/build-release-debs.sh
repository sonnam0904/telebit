#!/usr/bin/env bash
# Build two release .debs for GitHub Releases: jammy (22.04 libc/libstdc++) and noble (24.04).
# Run on ubuntu-22.04 CI with Docker available. Usage: build-release-debs.sh <semver>
set -euo pipefail

VERSION="${1:?semver required (e.g. 1.2.3)}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="${ROOT}/release-debs"
mkdir -p "${OUT}"
rm -f "${OUT}"/*.deb

build_native_jammy() {
  export TELEBIT_DEB_PACKAGE_VERSION="${VERSION}+jammy"
  unset TELEBIT_DEB_PACKAGE_SUFFIX || true
  rm -rf "${ROOT}/telebit-fcitx5/build-deb"
  bash "${ROOT}/scripts/build-deb.sh"
  shopt -s nullglob
  for f in "${ROOT}/telebit-fcitx5/build-deb"/*.deb; do
    mv "$f" "${OUT}/"
  done
  shopt -u nullglob
}

build_native_jammy

if ! command -v docker >/dev/null 2>&1; then
  echo "ERROR: docker required to build noble .deb" >&2
  exit 1
fi

rm -rf "${ROOT}/telebit-fcitx5/build-deb"
docker run --rm \
  -v "${ROOT}:/src" \
  -w /src \
  -e "TELEBIT_DEB_PACKAGE_VERSION=${VERSION}+noble" \
  ubuntu:24.04 \
  bash -lc '
    set -euo pipefail
    apt-get update -qq
    DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
      git ca-certificates build-essential cmake pkg-config libfcitx5core-dev libcurl4-openssl-dev
    bash scripts/build-deb.sh
  '

# Docker runs as root; .deb files are root-owned on the host — plain mv fails for CI user (e.g. runner).
move_deb_to_release() {
  local f dest
  shopt -s nullglob
  for f in "${ROOT}/telebit-fcitx5/build-deb"/*.deb; do
    dest="${OUT}/$(basename "$f")"
    if [[ -O "$f" ]]; then
      mv "$f" "$dest"
    elif command -v sudo >/dev/null 2>&1; then
      sudo mv "$f" "$dest"
      sudo chown "$(id -un):$(id -gn)" "$dest"
    else
      echo "ERROR: cannot move root-owned file (install sudo or run as root): $f" >&2
      exit 1
    fi
  done
  shopt -u nullglob
}

move_deb_to_release

echo "Release .deb artifacts:"
ls -la "${OUT}"
