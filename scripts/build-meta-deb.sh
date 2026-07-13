#!/usr/bin/env bash
# Build the "telebit" convenience metapackage: no files of its own, just
# Depends: telebit-fcitx5, so users can `apt install telebit` without us
# renaming (and breaking upgrades for) the real telebit-fcitx5 package.
# Usage: build-meta-deb.sh <version> [output-dir]
set -euo pipefail

VERSION="${1:?version required (e.g. 1.2.3 or 1.2.3+jammy)}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="${2:-${ROOT}/release-debs}"
mkdir -p "${OUT}"

WORK="$(mktemp -d)"
trap 'rm -rf "${WORK}"' EXIT

PKGROOT="${WORK}/pkg"
mkdir -p "${PKGROOT}/DEBIAN"

cat > "${PKGROOT}/DEBIAN/control" <<EOF
Package: telebit
Version: ${VERSION}
Section: utils
Priority: optional
Architecture: all
Depends: telebit-fcitx5
Maintainer: telebit-fcitx5 <https://github.com/sonnam0904/telebit>
Description: Vietnamese Telex/VNI input method for fcitx5 (metapackage)
 Convenience metapackage so 'sudo apt install telebit' works. It ships no
 files of its own; it only depends on telebit-fcitx5, the real addon package.
EOF

OUT_DEB="${OUT}/telebit_${VERSION}_all.deb"
dpkg-deb --build --root-owner-group "${PKGROOT}" "${OUT_DEB}"

echo "Built: ${OUT_DEB}"
