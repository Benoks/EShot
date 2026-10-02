#!/usr/bin/env bash
set -euo pipefail

if (( $# == 0 )); then
  exit 0
fi

command -v gdbus >/dev/null 2>&1 || exit 2

if ! introspection="$(gdbus introspect --session \
    --dest org.freedesktop.PackageKit \
    --object-path /org/freedesktop/PackageKit 2>/dev/null)"; then
  exit 2
fi

if [[ "${introspection}" != *InstallPackageNames* ]]; then
  exit 2
fi

packages=""
for package in "$@"; do
  [[ "${package}" =~ ^[A-Za-z0-9@+_.:-]+$ ]] || {
    echo "Unsafe package name: ${package}" >&2
    exit 1
  }
  packages+="${packages:+,}'${package}'"
done

# The call returns only after the user confirms, authenticates and the
# download finishes; the default D-Bus timeout (about 25 s) is far too short.
exec gdbus call --session --timeout 3600 \
  --dest org.freedesktop.PackageKit \
  --object-path /org/freedesktop/PackageKit \
  --method org.freedesktop.PackageKit.Modify.InstallPackageNames \
  0 "[${packages}]" \
  'show-confirm-search,show-confirm-deps,show-confirm-install,show-progress,show-finished,show-warning'
