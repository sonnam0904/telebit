#!/usr/bin/env bash
# Build the telebit-fcitx5 .rpms for GitHub Releases, one per supported Fedora
# release, using a semantic-release version. Each is built inside its own
# fedora:<N> container so the .rpm links against that release's own
# libstdc++/fcitx5 — a single "Fedora" rpm cannot satisfy both.
# Usage: build-release-rpm.sh <semver>
set -euo pipefail

VERSION="${1:?semver required (e.g. 1.2.4)}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="${ROOT}/release-rpms"
BUILD_DIR="${ROOT}/telebit-fcitx5/build-rpm"
mkdir -p "${OUT}"
rm -f "${OUT}"/*.rpm

# Fedora releases to ship, as "<suffix>=<image>" — add a line here for another.
FEDORA_TARGETS=(
  "fedora43=fedora:43"
  "fedora44=fedora:44"
)

# Build inside Fedora to match target environment.
if ! command -v docker >/dev/null 2>&1; then
  echo "ERROR: docker required to build release RPM" >&2
  exit 1
fi

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

# build_in_docker <suffix> <image>
# Builds inside <image> so the .rpm links against that Fedora release's own
# libraries, then moves the result out before the next target reuses the
# build dir (a CMake cache from a different container would be stale).
build_in_docker() {
  local suffix="$1" image="$2"
  echo "==> Building ${suffix} .rpm in ${image} ..."
  rm -rf "${BUILD_DIR}"
  docker run --rm \
    -v "${ROOT}:/workspace" \
    -w /workspace \
    -e "TELEBIT_PACKAGE_VERSION=${VERSION}" \
    -e "TELEBIT_RPM_PACKAGE_SUFFIX=${suffix}" \
    -e "HOST_UID=$(id -u)" \
    -e "HOST_GID=$(id -g)" \
    "${image}" \
    bash -lc '
      set -euo pipefail
      # /workspace is the host repo and this container is root: everything the
      # build writes lands on the host owned by root. Hand the build dir back on
      # the way out, or the next target gets "Permission denied" on its rm -rf.
      trap "chown -R ${HOST_UID}:${HOST_GID} telebit-fcitx5/build-rpm 2>/dev/null || true" EXIT
      dnf install -y cmake gcc-c++ make fcitx5-devel extra-cmake-modules git rpm-build libcurl-devel
      git config --global --add safe.directory /workspace
      bash scripts/build-rpm.sh
    '
  move_rpm_to_release
}

for entry in "${FEDORA_TARGETS[@]}"; do
  build_in_docker "${entry%%=*}" "${entry#*=}"
done

echo "Release .rpm artifacts:"
ls -la "${OUT}"

