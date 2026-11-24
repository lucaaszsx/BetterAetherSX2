#!/usr/bin/env bash
# Cross-compiles the libraries PCSX2 needs for Android arm64-v8a and installs them into a prefix.
# Versions and checksums follow upstream PCSX2's .github/workflows/scripts/linux/build-dependencies-qt.sh for the
# vendored release, so both builds use the same code. Qt, FFmpeg and libbacktrace are left out: the app has no Qt
# UI, FFmpeg is loaded at runtime only for video capture, and Android has its own crash reporting.
#
# Usage: build-deps.sh <ndk-dir> <install-prefix>

set -euo pipefail

if [ "$#" -ne 2 ]; then
    echo 'usage: build-deps.sh <ndk-dir> <install-prefix>' >&2
    exit 2
fi

NDK="$(realpath "$1")"
PREFIX="$(realpath -m "$2")"
SCRIPT_DIR="$(realpath "$(dirname "${BASH_SOURCE[0]}")")"
UPSTREAM="$SCRIPT_DIR/../../upstream/pcsx2"
ANDROID_PLATFORM=29
JOBS="$(getconf _NPROCESSORS_ONLN)"

LIBJPEGTURBO=3.2.0
LIBPNG=1.6.58
LIBWEBP=1.6.0
SDL=SDL3-3.4.12
LZ4=1.10.0
ZSTD=1.5.7
FREETYPE=2.14.1
PLUTOVG=1.3.2
PLUTOSVG=0.0.7
RAPIDYAML=0.12.1
SHADERC=2026.2
SHADERC_GLSLANG=275822a6261ee689aadb1da5f09a0ec2f058685c
SHADERC_SPIRVHEADERS=58006c901d1d5c37dece6b6610e9af87fa951375
SHADERC_SPIRVTOOLS=6337eb62cadd7d124ac6789bf39c0f71148f0a73

mkdir -p "$PREFIX" deps-src
cd deps-src

cat > SHASUMS <<EOF
6f30092cef9fb839779646608f4ee14ae3cbac989c47fa05e841b0841f09878e  libjpeg-turbo-$LIBJPEGTURBO.tar.gz
28eb403f51f0f7405249132cecfe82ea5c0ef97f1b32c5a65828814ae0d34775  libpng-$LIBPNG.tar.xz
eee7dea22ed502868017971c86c63c4ed1e6085de0baebfdcc3d3322f00f3eb0  libpng-$LIBPNG-apng.patch.gz
e4ab7009bf0629fd11982d4c2aa83964cf244cffba7347ecd39019a9e38c4564  libwebp-$LIBWEBP.tar.gz
f07b958a9ac5020fb7a44cadb957f658b2149c3c8abb4f63145fac9303249db7  $SDL.tar.gz
537512904744b35e232912055ccf8ec66d768639ff3abe5788d90d792ec5f48b  lz4-$LZ4.tar.gz
eb33e51f49a15e023950cd7825ca74a4a2b43db8354825ac24fc1b7ee09e6fa3  zstd-$ZSTD.tar.gz
32427e8c471ac095853212a37aef816c60b42052d4d9e48230bab3bdf2936ccc  freetype-$FREETYPE.tar.xz
7bd4e79ce18b1d47517e7e91fbb7cf19d4f01942804a519bc7c0bf32b6325dd5  plutovg-$PLUTOVG.tar.gz
78561b571ac224030cdc450ca2986b4de915c2ba7616004a6d71a379bffd15f3  plutosvg-$PLUTOSVG.tar.gz
e9efcdd17f86287748793cf21d106e461fcad8d103a3e5a23632afe93828660d  rapidyaml-$RAPIDYAML-src.tgz
f924178e75e3293082481b25ed64d5e48a795b479dac3bd3c83d23070855df42  shaderc-$SHADERC.tar.gz
971848a1cc639ce8dc244e778b17efe0f690e32ac398a75e31d1c67ad06d3e0a  shaderc-glslang-$SHADERC_GLSLANG.tar.gz
a6cb1b300bb8171795e116457e858e555334749f9cacaed8068ae0ef8681110c  shaderc-spirv-headers-$SHADERC_SPIRVHEADERS.tar.gz
e156be0bd81c8812f1bff8e520422bfa9df61b3045587b9eb483185f1074a7b2  shaderc-spirv-tools-$SHADERC_SPIRVTOOLS.tar.gz
EOF

if ! sha256sum --quiet --check SHASUMS 2> /dev/null; then
    curl --fail --location --remote-name-all \
        "https://github.com/libjpeg-turbo/libjpeg-turbo/releases/download/$LIBJPEGTURBO/libjpeg-turbo-$LIBJPEGTURBO.tar.gz" \
        "https://downloads.sourceforge.net/project/libpng/libpng16/$LIBPNG/libpng-$LIBPNG.tar.xz" \
        "https://download.sourceforge.net/libpng-apng/libpng-$LIBPNG-apng.patch.gz" \
        "https://storage.googleapis.com/downloads.webmproject.org/releases/webp/libwebp-$LIBWEBP.tar.gz" \
        "https://libsdl.org/release/$SDL.tar.gz" \
        "https://github.com/lz4/lz4/releases/download/v$LZ4/lz4-$LZ4.tar.gz" \
        "https://github.com/facebook/zstd/releases/download/v$ZSTD/zstd-$ZSTD.tar.gz" \
        "https://downloads.sourceforge.net/project/freetype/freetype2/$FREETYPE/freetype-$FREETYPE.tar.xz" \
        "https://github.com/sammycage/plutovg/archive/v$PLUTOVG/plutovg-$PLUTOVG.tar.gz" \
        "https://github.com/sammycage/plutosvg/archive/v$PLUTOSVG/plutosvg-$PLUTOSVG.tar.gz" \
        "https://github.com/biojppm/rapidyaml/releases/download/v$RAPIDYAML/rapidyaml-$RAPIDYAML-src.tgz" \
        "https://github.com/google/shaderc/archive/v$SHADERC/shaderc-$SHADERC.tar.gz" \
        "https://github.com/KhronosGroup/glslang/archive/$SHADERC_GLSLANG/shaderc-glslang-$SHADERC_GLSLANG.tar.gz" \
        "https://github.com/KhronosGroup/SPIRV-Headers/archive/$SHADERC_SPIRVHEADERS/shaderc-spirv-headers-$SHADERC_SPIRVHEADERS.tar.gz" \
        "https://github.com/KhronosGroup/SPIRV-Tools/archive/$SHADERC_SPIRVTOOLS/shaderc-spirv-tools-$SHADERC_SPIRVTOOLS.tar.gz"
fi
sha256sum --check --strict SHASUMS

# Configures, builds and installs one CMake project for the Android target. Extra arguments go to the configure step.
cmake_install() {
    local source_dir="$1"
    shift
    cmake -S "$source_dir" -B "$source_dir/build-android" -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" \
        -DANDROID_ABI=arm64-v8a \
        -DANDROID_PLATFORM="$ANDROID_PLATFORM" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_PREFIX_PATH="$PREFIX" \
        -DCMAKE_FIND_ROOT_PATH="$PREFIX" \
        -DCMAKE_INSTALL_PREFIX="$PREFIX" \
        -DBUILD_SHARED_LIBS=ON \
        "$@"
    cmake --build "$source_dir/build-android" --parallel "$JOBS"
    cmake --install "$source_dir/build-android"
}

# Unpacks an archive after removing what a previous run left in its directory.
unpack() {
    local archive="$1"
    local directory="$2"
    rm -rf "$directory"
    tar xf "$archive"
}

echo 'Building libjpeg-turbo'
unpack "libjpeg-turbo-$LIBJPEGTURBO.tar.gz" "libjpeg-turbo-$LIBJPEGTURBO"
cmake_install "libjpeg-turbo-$LIBJPEGTURBO" -DENABLE_STATIC=OFF -DENABLE_SHARED=ON -DCMAKE_INSTALL_DEFAULT_LIBDIR=lib

echo 'Building libpng with APNG'
unpack "libpng-$LIBPNG.tar.xz" "libpng-$LIBPNG"
gzip --keep --decompress --force "libpng-$LIBPNG-apng.patch.gz"
patch -d "libpng-$LIBPNG" -p1 < "libpng-$LIBPNG-apng.patch"
cmake_install "libpng-$LIBPNG" -DPNG_TESTS=OFF -DPNG_STATIC=OFF -DPNG_SHARED=ON -DPNG_TOOLS=OFF

echo 'Building LZ4'
unpack "lz4-$LZ4.tar.gz" "lz4-$LZ4"
cmake_install "lz4-$LZ4/build/cmake" -DLZ4_BUILD_CLI=OFF -DLZ4_BUILD_LEGACY_LZ4C=OFF

echo 'Building Zstandard'
unpack "zstd-$ZSTD.tar.gz" "zstd-$ZSTD"
cmake_install "zstd-$ZSTD/build/cmake" -DZSTD_BUILD_SHARED=ON -DZSTD_BUILD_STATIC=OFF -DZSTD_BUILD_PROGRAMS=OFF

echo 'Building WebP'
unpack "libwebp-$LIBWEBP.tar.gz" "libwebp-$LIBWEBP"
cmake_install "libwebp-$LIBWEBP" \
    -DWEBP_BUILD_ANIM_UTILS=OFF -DWEBP_BUILD_CWEBP=OFF -DWEBP_BUILD_DWEBP=OFF -DWEBP_BUILD_GIF2WEBP=OFF \
    -DWEBP_BUILD_IMG2WEBP=OFF -DWEBP_BUILD_VWEBP=OFF -DWEBP_BUILD_WEBPINFO=OFF -DWEBP_BUILD_WEBPMUX=OFF \
    -DWEBP_BUILD_EXTRAS=OFF

echo 'Building FreeType'
unpack "freetype-$FREETYPE.tar.xz" "freetype-$FREETYPE"
# Only the outline rasterizer is needed; the optional codecs would pull in more libraries.
cmake_install "freetype-$FREETYPE" \
    -DFT_DISABLE_BZIP2=ON -DFT_DISABLE_BROTLI=ON -DFT_DISABLE_HARFBUZZ=ON -DFT_DISABLE_PNG=ON -DFT_REQUIRE_ZLIB=ON

echo 'Building SDL'
unpack "$SDL.tar.gz" "$SDL"
# Subsystems stay at their defaults: SDL's Android glue references the video and sensor code, so turning them off
# as the desktop build does fails to link.
cmake_install "$SDL" -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TEST_LIBRARY=OFF

echo 'Building PlutoVG'
unpack "plutovg-$PLUTOVG.tar.gz" "plutovg-$PLUTOVG"
cmake_install "plutovg-$PLUTOVG" -DPLUTOVG_BUILD_EXAMPLES=OFF

echo 'Building PlutoSVG'
unpack "plutosvg-$PLUTOSVG.tar.gz" "plutosvg-$PLUTOSVG"
cmake_install "plutosvg-$PLUTOSVG" -DPLUTOSVG_ENABLE_FREETYPE=OFF -DPLUTOSVG_BUILD_EXAMPLES=OFF

echo 'Building RapidYAML'
unpack "rapidyaml-$RAPIDYAML-src.tgz" "rapidyaml-$RAPIDYAML-src"
cmake_install "rapidyaml-$RAPIDYAML-src"

echo 'Building shaderc'
unpack "shaderc-$SHADERC.tar.gz" "shaderc-$SHADERC"
tar xf "shaderc-glslang-$SHADERC_GLSLANG.tar.gz" -C "shaderc-$SHADERC/third_party"
mv "shaderc-$SHADERC/third_party/glslang-$SHADERC_GLSLANG" "shaderc-$SHADERC/third_party/glslang"
tar xf "shaderc-spirv-headers-$SHADERC_SPIRVHEADERS.tar.gz" -C "shaderc-$SHADERC/third_party"
mv "shaderc-$SHADERC/third_party/SPIRV-Headers-$SHADERC_SPIRVHEADERS" "shaderc-$SHADERC/third_party/spirv-headers"
tar xf "shaderc-spirv-tools-$SHADERC_SPIRVTOOLS.tar.gz" -C "shaderc-$SHADERC/third_party"
mv "shaderc-$SHADERC/third_party/SPIRV-Tools-$SHADERC_SPIRVTOOLS" "shaderc-$SHADERC/third_party/spirv-tools"
patch -d "shaderc-$SHADERC" -p1 < "$UPSTREAM/.github/workflows/scripts/common/shaderc-changes.patch"
cmake_install "shaderc-$SHADERC" -DSHADERC_SKIP_TESTS=ON -DSHADERC_SKIP_EXAMPLES=ON -DSHADERC_SKIP_COPYRIGHT_CHECK=ON

echo "Dependencies installed in $PREFIX"
