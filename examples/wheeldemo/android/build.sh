#!/usr/bin/env bash
# Builds wheeldemo.apk for Android arm64 (Meta Quest) without Gradle.
#   SDL3=/path/to/SDL (an unmodified SDL3 checkout, e.g. release-3.4.x) ./build.sh
# Needs the Android SDK (build-tools, platform android-34+), NDK, JDK 17, cmake and ninja.
# Signs with a debug key it creates in ./debug.keystore.
set -euo pipefail
cd "$(dirname "$0")"
: "${SDL3:?set SDL3 to an SDL3 source checkout}"
SDK="${ANDROID_HOME:-$HOME/Library/Android/sdk}"
NDK="${ANDROID_NDK:-$(ls -d "$SDK"/ndk/* | sort -V | tail -1)}"
BT="${BUILD_TOOLS:-$(ls -d "$SDK"/build-tools/* | sort -V | tail -1)}"
JAR="${ANDROID_JAR:-$(ls -d "$SDK"/platforms/android-* | sort -V | tail -1)/android.jar}"
HOST="$(ls "$NDK/toolchains/llvm/prebuilt" | head -1)"
LLVM="$NDK/toolchains/llvm/prebuilt/$HOST/bin"
ABI=arm64-v8a
OUT=build
SDLB="$OUT/sdl3"

if [ ! -f "$SDLB/build.ninja" ]; then
  cmake -S "$SDL3" -B "$SDLB" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" -DANDROID_ABI=$ABI \
    -DANDROID_PLATFORM=android-29 -DCMAKE_BUILD_TYPE=Release -DSDL_SHARED=ON -DSDL_STATIC=OFF \
    -DSDL_TEST_LIBRARY=OFF -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF >/dev/null
fi
cmake --build "$SDLB" >/dev/null

rm -rf "$OUT/apk"; mkdir -p "$OUT/apk/lib/$ABI" "$OUT/apk/classes" "$OUT/apk/dex" "$OUT/apk/res"
"$LLVM/aarch64-linux-android29-clang" -shared -fPIC -O2 -Wall -Wextra -DLG4FF_SDL3 \
  -o "$OUT/apk/lib/$ABI/libmain.so" ../main.c ../../../lg4ff_hid.c \
  -I../../.. -I"$SDL3/include" -L"$SDLB" -lSDL3 -llog -landroid
cp "$SDLB/libSDL3.so" "$OUT/apk/lib/$ABI/"
"$LLVM/llvm-strip" --strip-unneeded "$OUT/apk/lib/$ABI/"*.so

"$BT/aapt2" compile --dir res -o "$OUT/apk/res/res.zip"
"$BT/aapt2" link -I "$JAR" --manifest AndroidManifest.xml -o "$OUT/apk/unsigned.apk" "$OUT/apk/res/res.zip"
find "$SDL3/android-project/app/src/main/java" -name '*.java' | sed 's/.*/"&"/' > "$OUT/apk/sources.txt"
javac -source 17 -target 17 -nowarn -Xlint:none -classpath "$JAR" -d "$OUT/apk/classes" @"$OUT/apk/sources.txt" 2>&1 | grep -v "^Note:" || true
find "$OUT/apk/classes" -name "*.class" > "$OUT/apk/classes.txt"
"$BT/d8" --min-api 29 --lib "$JAR" --output "$OUT/apk/dex" @"$OUT/apk/classes.txt"
(cd "$OUT/apk/dex" && zip -q ../unsigned.apk classes.dex)
(cd "$OUT/apk" && zip -q -r unsigned.apk lib)
"$BT/zipalign" -f -P 16 4 "$OUT/apk/unsigned.apk" "$OUT/apk/aligned.apk"

KS=debug.keystore
if [ ! -f "$KS" ]; then
  keytool -genkeypair -keystore "$KS" -storepass android -keypass android -alias debug \
    -keyalg RSA -keysize 2048 -validity 10000 -dname "CN=lg4ff-hid debug" >/dev/null 2>&1
fi
"$BT/apksigner" sign --ks "$KS" --ks-pass pass:android --key-pass pass:android --out wheeldemo.apk "$OUT/apk/aligned.apk"
rm -f wheeldemo.apk.idsig
echo "OK -> $(pwd)/wheeldemo.apk"
