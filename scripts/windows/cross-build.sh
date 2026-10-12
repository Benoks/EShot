#!/usr/bin/env bash
# Cross-compiles EShot for Windows x64 on Linux and packages a portable zip
# for testing, without GitHub Actions:
#   dist-windows/EShot-<version>-test-win64.zip
#
# Needs:
#   - llvm-mingw (https://github.com/mstorsjo/llvm-mingw) on PATH or in
#     ~/.local/opt/llvm-mingw
#   - Qt 6.8.2 for win64_llvm_mingw plus the matching Linux host Qt, e.g.:
#       python3 -m venv ~/.local/opt/qt-cross/venv
#       ~/.local/opt/qt-cross/venv/bin/pip install aqtinstall
#       cd ~/.local/opt/qt-cross
#       venv/bin/aqt install-qt windows desktop 6.8.2 win64_llvm_mingw -O qt
#       venv/bin/aqt install-qt linux desktop 6.8.2 linux_gcc_64 -O qt
#
#   scripts/windows/cross-build.sh          build and package
#   scripts/windows/cross-build.sh --tests  also run the test executables in Wine
set -euo pipefail
cd "$(dirname "$0")/../.."

llvm_mingw="${LLVM_MINGW:-$HOME/.local/opt/llvm-mingw}"
qt_root="${QT_CROSS_ROOT:-$HOME/.local/opt/qt-cross/qt/6.8.2}"
qt_win="${qt_root}/llvm-mingw_64"
qt_host="${qt_root}/gcc_64"
build_dir=build-win64
out_dir=dist-windows

for path in "${llvm_mingw}/bin/x86_64-w64-mingw32-clang++" "${qt_win}/bin/Qt6Core.dll" "${qt_host}/bin/qmake"; do
  [[ -e "${path}" ]] || { echo "missing: ${path} (see the header of this script)" >&2; exit 1; }
done

toolchain="$(mktemp)"
trap 'rm -f "${toolchain}"' EXIT
cat >"${toolchain}" <<EOF
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_C_COMPILER "${llvm_mingw}/bin/x86_64-w64-mingw32-clang")
set(CMAKE_CXX_COMPILER "${llvm_mingw}/bin/x86_64-w64-mingw32-clang++")
set(CMAKE_RC_COMPILER "${llvm_mingw}/bin/x86_64-w64-mingw32-windres")
set(CMAKE_FIND_ROOT_PATH "${qt_win}" "${llvm_mingw}/x86_64-w64-mingw32")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)
set(QT_HOST_PATH "${qt_host}" CACHE PATH "")
EOF

cmake -S . -B "${build_dir}" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="${toolchain}" -DCMAKE_PREFIX_PATH="${qt_win}" -DBUILD_TESTING=ON >/dev/null
cmake --build "${build_dir}"

version="$(sed -n 's/^project(EShot VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt | head -n1)"
stage="$(mktemp -d)"
package="${stage}/EShot"
mkdir -p "${package}"/{platforms,styles,imageformats,iconengines,tls,ffmpeg}
cp "${build_dir}/bin/EShot.exe" "${package}/"
for dll in Qt6Core Qt6Gui Qt6Widgets Qt6Network Qt6Svg libc++ libunwind; do
  cp "${qt_win}/bin/${dll}.dll" "${package}/"
done
cp "${qt_win}/plugins/platforms/qwindows.dll" "${package}/platforms/"
cp "${qt_win}/plugins/styles/qmodernwindowsstyle.dll" "${package}/styles/"
cp "${qt_win}/plugins/imageformats/"{qsvg,qico,qjpeg,qgif}.dll "${package}/imageformats/"
cp "${qt_win}/plugins/iconengines/qsvgicon.dll" "${package}/iconengines/"
cp "${qt_win}/plugins/tls/"{qschannelbackend,qcertonlybackend}.dll "${package}/tls/"

# The same pinned, checksummed FFmpeg build as CI (.github/workflows/build.yml).
ffmpeg_url="$(sed -n "s/.*ffmpeg_url: '\(.*win64-gpl.*\.zip\)'.*/\1/p" .github/workflows/build.yml | head -n1)"
ffmpeg_sha="$(grep -A1 -F "${ffmpeg_url}" .github/workflows/build.yml | sed -n "s/.*ffmpeg_sha256: '\([0-9a-f]*\)'.*/\1/p" | head -n1)"
ffmpeg_cache="${build_dir}/ffmpeg-win64.zip"
if [[ ! -f "${ffmpeg_cache}" ]] || ! printf '%s  %s\n' "${ffmpeg_sha}" "${ffmpeg_cache}" | sha256sum -c - >/dev/null 2>&1; then
  curl -L --fail --retry 3 -o "${ffmpeg_cache}" "${ffmpeg_url}"
  printf '%s  %s\n' "${ffmpeg_sha}" "${ffmpeg_cache}" | sha256sum -c -
fi
extract="$(mktemp -d)"
python3 -I -m zipfile -e "${ffmpeg_cache}" "${extract}" >/dev/null
cp "${extract}"/*/bin/ffmpeg.exe "${package}/ffmpeg/"
rm -rf "${extract}"

mkdir -p "${out_dir}"
zip="$PWD/${out_dir}/EShot-${version}-test-win64.zip"
rm -f "${zip}"
(cd "${stage}" && python3 -I -m zipfile -c "${zip}" EShot)
rm -rf "${stage}"
echo "${zip}"

if [[ "${1:-}" == "--tests" ]]; then
  command -v wine >/dev/null || { echo "wine is needed for --tests" >&2; exit 1; }
  export WINEDEBUG=-all
  export WINEPATH="$(winepath -w "${qt_win}/bin");$(winepath -w "${llvm_mingw}/x86_64-w64-mingw32/bin")"
  failed=0
  for test in "${build_dir}"/eshot_*_tests.exe; do
    if ! timeout 120 wine "${test}" -silent >/dev/null 2>&1; then
      echo "FAILED: ${test}"
      failed=1
    fi
  done
  # Linux-only behaviour (POSIX paths, /bin/sh) fails in Wine by design:
  # linux_dependency_selection, linux_update_script, recording_messages (file URLs).
  exit "${failed}"
fi
