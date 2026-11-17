# The build image: Debian with the armhf cross compiler and the libraries goodluckOS has (SDL2, zlib), mbedTLS for a
# curl of the app's own, and Xvfb for screenshots of the native build. Native on amd64 and arm64 hosts (Apple Silicon
# under Colima).
FROM debian:trixie

RUN dpkg --add-architecture armhf && apt-get update && apt-get install -y --no-install-recommends \
    build-essential crossbuild-essential-armhf pkg-config ca-certificates curl file xz-utils zip unzip \
    libsdl2-dev libsdl2-dev:armhf zlib1g-dev:armhf libmbedtls-dev:armhf \
    xvfb xdotool imagemagick x11-apps fonts-noto-core \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /repo
