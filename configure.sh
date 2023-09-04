#!/usr/bin/env zsh

mkdir -p build
cd build

# Missed Qt6Compact5...
    # -DCMAKE_PREFIX_PATH=$QT_DIR \
cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=10.15 \
  -DOPENSSL_ROOT_DIR=/usr/local/opt/openssl \
  -DUSE_PRECOMPILED_HEADERS=OFF \
  -DCHATTERINO_LTO=ON \
  -DFORCE_JSON_GENERATION=Off \
  ..

make -j"$(sysctl -n hw.logicalcpu)"

rm -rf /Applications/Chatterino.app
mv bin/Chatterino.app /Applications/.
cd ..
rm -rf build
