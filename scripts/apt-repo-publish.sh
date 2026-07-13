#!/usr/bin/env bash
# Add .deb files to a flat APT repo checkout and (re)sign its indices.
#
# Usage:
#   apt-repo-publish.sh <repo_dir> <gpg_key_id> <suite> <deb_file>...
#
# Repo layout produced (served as-is over HTTPS, e.g. via GitHub Pages):
#   <repo_dir>/pool/<suite>/main/t/*.deb
#   <repo_dir>/dists/<suite>/main/binary-amd64/{Packages,Packages.gz}
#   <repo_dir>/dists/<suite>/{Release,Release.gpg,InRelease}
#   <repo_dir>/pubkey.gpg
#
# One "suite" per Ubuntu codename (jammy, noble, ...) keeps ABI-incompatible
# builds of telebit-fcitx5 from colliding: users pick the codename in their
# sources.list entry, so only the matching binaries are ever visible to apt.
set -euo pipefail

REPO_DIR="${1:?repo_dir required}"
GPG_KEY_ID="${2:?gpg_key_id required}"
SUITE="${3:?suite required (e.g. jammy, noble)}"
shift 3
DEB_FILES=("$@")
if [[ ${#DEB_FILES[@]} -eq 0 ]]; then
  echo "ERROR: at least one .deb file required" >&2
  exit 1
fi

POOL_DIR="${REPO_DIR}/pool/${SUITE}/main/t"
DIST_DIR="${REPO_DIR}/dists/${SUITE}"
BINARY_DIR="${DIST_DIR}/main/binary-amd64"
mkdir -p "${POOL_DIR}" "${BINARY_DIR}"

for f in "${DEB_FILES[@]}"; do
  cp -f "$f" "${POOL_DIR}/"
done

cd "${REPO_DIR}"

dpkg-scanpackages --arch amd64 "pool/${SUITE}" > "${BINARY_DIR}/Packages"
gzip -9 -c "${BINARY_DIR}/Packages" > "${BINARY_DIR}/Packages.gz"

apt-ftparchive \
  -o APT::FTPArchive::Release::Origin="Telebit" \
  -o APT::FTPArchive::Release::Label="Telebit" \
  -o APT::FTPArchive::Release::Suite="${SUITE}" \
  -o APT::FTPArchive::Release::Codename="${SUITE}" \
  -o APT::FTPArchive::Release::Architectures="amd64" \
  -o APT::FTPArchive::Release::Components="main" \
  -o APT::FTPArchive::Release::Description="Telebit Vietnamese Telex/VNI input method (fcitx5) APT repository" \
  release "dists/${SUITE}" > "dists/${SUITE}/Release"

gpg --batch --yes --pinentry-mode loopback \
  --default-key "${GPG_KEY_ID}" -abs -o "dists/${SUITE}/Release.gpg" "dists/${SUITE}/Release"
gpg --batch --yes --pinentry-mode loopback \
  --default-key "${GPG_KEY_ID}" --clearsign -o "dists/${SUITE}/InRelease" "dists/${SUITE}/Release"

gpg --batch --yes --armor --export "${GPG_KEY_ID}" > "${REPO_DIR}/pubkey.gpg"

if [[ ! -f "${REPO_DIR}/index.html" ]]; then
  cat > "${REPO_DIR}/index.html" <<'HTML'
<!doctype html>
<meta charset="utf-8">
<title>Telebit APT repository</title>
<p>APT repository for <a href="https://github.com/sonnam0904/telebit">Telebit</a>.
See the <a href="https://github.com/sonnam0904/telebit#3-c%C3%A0i-addon-telebit-fcitx5-cho-fcitx5">README</a>
for setup instructions.</p>
HTML
fi

echo "Published suite '${SUITE}' with:"
printf '  %s\n' "${DEB_FILES[@]##*/}"
