#!/usr/bin/env bash
# Build telebit-fcitx5 .rpm for GitHub Releases, using a semantic-release version.
# Usage: build-release-rpm.sh <semver>
set -euo pipefail

VERSION="${1:?semver required (e.g. 1.2.4)}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="${ROOT}/release-rpms"
mkdir -p "${OUT}"
rm -f "${OUT}"/*.rpm

# Build inside Fedora to match target environment.
if ! command -v docker >/dev/null 2>&1; then
  echo "ERROR: docker required to build release RPM" >&2
  exit 1
fi

docker run --rm \
  -v "${ROOT}:/workspace" \
  -w /workspace \
  -e "TELEBIT_PACKAGE_VERSION=${VERSION}" \
  -e "TELEBIT_RPM_PACKAGE_SUFFIX=fedora43" \
  fedora:43 \
  bash -lc '
    set -euo pipefail
    dnf install -y cmake gcc-c++ make fcitx5-devel extra-cmake-modules git rpm-build libcurl-devel
    git config --global --add safe.directory /workspace
    bash scripts/build-rpm.sh
  '

# Docker runs as root; fix ownership and move the rpm(s) into release-rpms.
move_rpm_to_release() {
  local f dest
  shopt -s nullglob
  for f in "${ROOT}/telebit-fcitx5/build-rpm"/*.rpm; do
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

move_rpm_to_release

echo "Release .rpm artifacts:"
ls -la "${OUT}"

