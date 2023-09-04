#!/usr/bin/env zsh

mkdir -p build
cd build

# Missed Qt6Compact5...
    # -DCMAKE_PREFIX_PATH=$QT_DIR \
cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DOPENSSL_ROOT_DIR=/opt/homebrew/opt/openssl@1.1 \
    -DUSE_PRECOMPILED_HEADERS=ON \
    -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCHATTERINO_LTO=ON \
    -DCHATTERINO_PLUGINS=OFF \
    -DBUILD_WITH_QT6=ON \
    ..

cmake --build . --config Release --parallel

rm -rf /Applications/Chatterino.app
mv bin/Chatterino.app /Applications/.
cd ..
rm -rf build
