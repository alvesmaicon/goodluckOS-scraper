#!/usr/bin/env bash
# goodluckOS's scraper as an App Installer app: gl-scrape (scrape/), its screen (app/) and a curl of its own, with
# mbedTLS and the CA certificates in it, since goodluckOS has no curl.
#   ./build.sh                      the app for the console, in out/Scraper
#   ./build.sh app                  the App Installer's zip, out/Scraper-<version>.zip (APP_VERSION: default git describe)
#   ./build.sh native               the screen and gl-scrape for this machine, in out/native
#   ./build.sh shot <name> <keys>   the native screen in Xvfb on a made-up console, a screenshot at each "|" in out/shots
#                                   (keys: xdotool names, "_" waits a second; SHOT_LANG=pt_BR.UTF-8 for Portuguese)
#   ./build.sh real [folder...]     a real ScreenScraper run of the native gl-scrape on that console, no videos
#                                   (SS_USER and SS_PASSWORD: your account)
# ScreenScraper's developer credentials come from SS_DEVID and SS_DEVPASSWORD (.env, or a CI secret), never from git;
# without them gl-scrape says so. Runs in the goodluckos-scraper image (Dockerfile); downloads and build trees stay in
# the scraper-work volume.
set -euo pipefail

HERE=$(cd "$(dirname "$0")" && pwd)
IMAGE=goodluckos-scraper

if [ ! -f /.dockerenv ]; then
    docker image inspect $IMAGE >/dev/null 2>&1 || docker build -t $IMAGE "$HERE"
    if [ -f "$HERE/.env" ]; then set -a; . "$HERE/.env"; set +a; fi
    export SS_DEVID="${SS_DEVID:-}" SS_DEVPASSWORD="${SS_DEVPASSWORD:-}" SS_USER="${SS_USER:-}" SS_PASSWORD="${SS_PASSWORD:-}"
    export SHOT_LANG="${SHOT_LANG:-}"
    export APP_VERSION="${APP_VERSION:-$(git -C "$HERE" describe --tags --always 2>/dev/null || echo dev)}"
    exec docker run --rm -i -e SS_DEVID -e SS_DEVPASSWORD -e SS_USER -e SS_PASSWORD -e APP_VERSION -e SHOT_LANG \
        -v "$HERE:/repo" -v scraper-work:/work $IMAGE /repo/build.sh "$@"
fi

W=/work
SRC=$W/src
OUT=/repo/out
P=$OUT/Scraper
ARM="-O2 -march=armv7-a -mfpu=neon-vfpv4 -mfloat-abi=hard"
PCA="env PKG_CONFIG_LIBDIR=/usr/lib/arm-linux-gnueabihf/pkgconfig pkg-config"
IMGUI=1.92.9b
IMGUI_SHA=21d8a0a565e85dce943e375db00812c2f3f0ab21f3f0f7964e364a63422d7f99
CURL=8.21.0
CURL_SHA=aa1b66a70eace83dc624508745646c08ae561de512ab403adffb93ac87fc72e6
mkdir -p "$SRC" "$W/build" "$OUT"

# A source tarball with a pinned hash, unpacked once into $SRC/<name>
tarball() {
    local t=$SRC/$(basename "$2")
    if ! echo "$3  $t" | sha256sum -c --quiet >/dev/null 2>&1; then
        curl -fsSL -o "$t" "$2"
        echo "$3  $t" | sha256sum -c --quiet
    fi
    [ -d "$SRC/$1" ] || { mkdir "$SRC/$1"; tar -xf "$t" -C "$SRC/$1" --strip-components=1; }
}

# ImGui's core and its SDL2 backends as objects, for one compiler: <dir> <compiler> <flags...>
imgui() {
    local d=$1 cxx=$2 f
    shift 2
    tarball imgui https://github.com/ocornut/imgui/archive/v$IMGUI/imgui-v$IMGUI.tar.gz $IMGUI_SHA
    mkdir -p "$d"
    for f in imgui imgui_draw imgui_tables imgui_widgets backends/imgui_impl_sdl2 backends/imgui_impl_sdlrenderer2; do
        [ "$d/${f##*/}.o" -nt "$SRC/imgui/$f.cpp" ] || $cxx "$@" -I"$SRC/imgui" -c "$SRC/imgui/$f.cpp" -o "$d/${f##*/}.o"
    done
}

credentials() {
    mkdir -p "$W/inc"
    /repo/scrape/credentials.sh > "$W/inc/credentials.h"
}

# curl with mbedTLS and zlib linked in, so it needs nothing but the C library
curl_arm() {
    local b=$W/build/curl-arm s=$W/build/static-arm l
    [ -x "$b/src/curl" ] && return
    tarball curl https://curl.se/download/curl-$CURL.tar.xz $CURL_SHA
    mkdir -p "$s/lib" "$s/include"
    for l in mbedtls mbedx509 mbedcrypto z; do ln -sf /usr/lib/arm-linux-gnueabihf/lib$l.a "$s/lib/"; done
    ln -sfn /usr/include/mbedtls "$s/include/mbedtls"
    ln -sfn /usr/include/psa "$s/include/psa"
    rm -rf "$b"
    cp -r "$SRC/curl" "$b"
    (
        cd "$b"
        ./configure --host=arm-linux-gnueabihf CFLAGS="$ARM" LDFLAGS="-L$s/lib" \
            --disable-shared --enable-static --with-mbedtls="$s" --with-zlib \
            --with-ca-bundle=/etc/ssl/certs/ca-certificates.crt --without-ca-path \
            --without-libpsl --without-brotli --without-zstd --without-nghttp2 --without-libidn2 --without-librtmp \
            --without-libssh2 --disable-ldap --disable-ldaps --disable-rtsp --disable-dict --disable-telnet \
            --disable-tftp --disable-pop3 --disable-imap --disable-smb --disable-smtp --disable-gopher --disable-mqtt \
            --disable-ipfs --disable-websockets --disable-manual --disable-docs --disable-ntlm \
            --disable-kerberos-auth --disable-negotiate-auth --disable-aws >/dev/null
        make -j"$(nproc)" >/dev/null
    )
    arm-linux-gnueabihf-strip "$b/src/curl"
}

console() {
    credentials
    imgui "$W/build/imgui-arm" arm-linux-gnueabihf-g++ $ARM $($PCA --cflags sdl2)
    curl_arm
    rm -rf "$P"
    mkdir -p "$P/bin" "$P/lang" "$P/licenses"
    arm-linux-gnueabihf-g++ $ARM -std=c++17 -I"$W/inc" /repo/scrape/src/*.cpp -o "$P/bin/gl-scrape"
    arm-linux-gnueabihf-g++ $ARM -std=c++17 -I"$SRC/imgui" -I"$SRC/imgui/backends" $($PCA --cflags sdl2) \
        /repo/app/src/*.cpp "$W"/build/imgui-arm/*.o -o "$P/scraper" $($PCA --libs sdl2)
    arm-linux-gnueabihf-strip "$P/bin/gl-scrape" "$P/scraper"
    cp "$W/build/curl-arm/src/curl" "$P/bin/curl"
    cp /etc/ssl/certs/ca-certificates.crt "$P/cacert.pem"
    sed "s/@VERSION@/$APP_VERSION/" /repo/app/app.puppy > "$P/app.puppy"
    cp /repo/app/icon.png "$P/"
    cp /repo/app/lang/*.lang "$P/lang/"
    cp /repo/LICENSE "$P/licenses/goodluckOS-scraper.txt"
    cp "$SRC/curl/COPYING" "$P/licenses/curl.txt"
    cp "$SRC/imgui/LICENSE.txt" "$P/licenses/imgui.txt"
    cp /usr/share/doc/libmbedtls-dev/copyright "$P/licenses/mbedtls.txt" 2>/dev/null ||
        echo "Mbed TLS, Apache-2.0 OR GPL-2.0-or-later: https://github.com/Mbed-TLS/mbedtls" > "$P/licenses/mbedtls.txt"
    [ -n "$SS_DEVID" ] || echo "NO ScreenScraper developer credentials (SS_DEVID, SS_DEVPASSWORD)"
    local f
    for f in "$P/scraper" "$P/bin/gl-scrape" "$P/bin/curl"; do
        echo "$(basename "$f"): $(stat -c %s "$f") bytes, needs $(arm-linux-gnueabihf-readelf -d "$f" | grep -o 'NEEDED.*' |
            grep -o '\[.*\]' | tr -d '[]' | tr '\n' ' ')glibc $(arm-linux-gnueabihf-objdump -T "$f" | grep -oE 'GLIBC_[0-9.]+' | sort -Vu | tail -1)"
    done
}

app() {
    [ -x "$P/scraper" ] || console
    rm -f "$OUT"/Scraper-*.zip
    (cd "$OUT" && zip -qr "Scraper-$APP_VERSION.zip" Scraper)
    echo "out/Scraper-$APP_VERSION.zip: $(stat -c %s "$OUT/Scraper-$APP_VERSION.zip") bytes"
}

native() {
    local n=$OUT/native
    credentials
    imgui "$W/build/imgui-native" g++ -O2 $(pkg-config --cflags sdl2)
    mkdir -p "$n/bin" "$n/lang"
    g++ -O2 -std=c++17 -I"$W/inc" /repo/scrape/src/*.cpp -o "$n/bin/gl-scrape"
    g++ -O2 -std=c++17 -I"$SRC/imgui" -I"$SRC/imgui/backends" $(pkg-config --cflags sdl2) \
        /repo/app/src/*.cpp "$W"/build/imgui-native/*.o -o "$n/scraper" $(pkg-config --libs sdl2)
    cp /repo/app/lang/*.lang "$n/lang/"
}

# The container as a console: the launcher's tabs, a few games and an account, from tests/console
fake_console() {
    cp -r /repo/tests/console/. /
    (cd /home/player/roms && while read -r f; do mkdir -p "$(dirname "$f")"; head -c 1024 /dev/zero > "$f"; done < games.txt)
}

shot() {
    local name=$1 keys=$2 n=0 k
    native
    fake_console
    mkdir -p "$OUT/shots"
    Xvfb :99 -screen 0 640x480x24 -nolisten tcp >/dev/null 2>&1 &
    local X=$!
    sleep 1
    DISPLAY=:99 SDL_VIDEODRIVER=x11 LANG="${SHOT_LANG:-C}" "$OUT/native/scraper" >/tmp/app.log 2>&1 &
    local A=$!
    sleep 3
    DISPLAY=:99 xdotool mousemove 639 479
    for k in $keys; do
        if [ "$k" = "|" ]; then
            n=$((n + 1)); sleep 0.6
            DISPLAY=:99 xwd -root -silent | convert xwd:- "$OUT/shots/$name-$n.png"; echo "out/shots/$name-$n.png"
        elif [ "$k" = "_" ]; then sleep 1
        else DISPLAY=:99 xdotool key "$k"; sleep 0.3; fi
    done
    kill $A $X 2>/dev/null || true
}

real() {
    native
    fake_console
    "$OUT/native/bin/gl-scrape" --no-video --status /tmp/gl-scrape.status "$@" || true
    cat /tmp/gl-scrape.status 2>/dev/null | grep -E '^(state|found|notfound|failed|message)=' || true
    find /home/player/roms -path '*/icons/*' -type f | sort
    grep -ho '<image>[^<]*</image>' /home/player/roms/*/gamelist.xml 2>/dev/null || true
}

case "${1:-}" in
    "") console ;;
    app) app ;;
    native) native ;;
    shot) [ $# -eq 3 ] || { echo "usage: ./build.sh shot <name> \"<keys>\""; exit 2; }; shot "$2" "$3" ;;
    real) shift; real "$@" ;;
    *) sed -n '2,13p' "$0"; exit 2 ;;
esac
