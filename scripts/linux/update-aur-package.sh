#!/usr/bin/env bash
set -euo pipefail

version="${1:-}"
checksum="${2:-}"
package_dir="${3:-}"
pkgbuild="${package_dir}/PKGBUILD"

[[ "${version}" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || {
    echo "Version must use MAJOR.MINOR.PATCH." >&2
    exit 1
}
[[ "${checksum}" =~ ^[0-9a-f]{64}$ ]] || {
    echo "Checksum must be a lowercase SHA-256 value." >&2
    exit 1
}
[[ -f "${pkgbuild}" ]] || {
    echo "PKGBUILD not found: ${pkgbuild}" >&2
    exit 1
}

sed -i -E "s/^pkgver=.*/pkgver=${version}/; s/^pkgrel=.*/pkgrel=1/" "${pkgbuild}"
sed -i -E "/^sha256sums=\(/s/'[0-9a-f]{64}'/'${checksum}'/" "${pkgbuild}"
