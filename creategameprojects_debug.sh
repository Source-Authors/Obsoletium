#!/bin/bash

set -e
set -u
set -o pipefail

RED='\033[0;31m'
NC='\033[0m'

echo -e "${RED}Build bzip2${NC}"
pushd ./thirdparty/bzip2
make
popd

echo -e "${RED}Build libjpeg-turbo${NC}"
mkdir ./thirdparty/libjpeg-turbo/out
pushd ./thirdparty/libjpeg-turbo/out
cmake -DENABLE_SHARED=OFF -DENABLE_STATIC=ON -DWITH_CDJPEG=OFF -DWITH_TURBOJPEG=OFF ..
cmake --build . --config Release
popd

echo -e "${RED}Build zlib${NC}"
mkdir ./thirdparty/zlib/out
pushd ./thirdparty/zlib/out
cmake ..
cmake --build . --config Release
popd

ZLIB_SOURCE_DIR=$(pwd)/thirdparty/zlib

echo -e "${RED}Build libpng${NC}"
mkdir ./thirdparty/libpng/out
pushd ./thirdparty/libpng/out
cmake -DPNG_BUILD_ZLIB=ON -DPNG_SHARED=OFF -DPNG_STATIC=ON -DPNG_EXECUTABLES=OFF -DPNG_TESTS=OFF -DZLIB_INCLUDE_DIRS=$ZLIB_SOURCE_DIR;$ZLIB_SOURCE_DIR/out -DZLIB_LIBRARIES=$ZLIB_SOURCE_DIR/out/zlibstatic.a ..
cmake --build . --config Release
popd

echo -e "${RED}Build SDL${NC}"
mkdir ./thirdparty/SDL/out
pushd ./thirdparty/SDL/out
cmake -DSDL_TEST=OFF ..
cmake --build . --config Release
popd

echo -e "${RED}Build protobuf${NC}"
pushd ./thirdparty/protobuf
./autogen.sh
./configure
make
popd

echo -e "${RED}Build cryptopp${NC}"
pushd ./thirdparty/cryptopp
./TestScripts/cryptest-autotools.sh
popd

echo -e "${RED}Build vpc${NC}"
./external/vpc/devtools/bin/vpc ./devtools/bin/.
./devtools/bin/vpc_linux /2015 /define:WORKSHOP_IMPORT_DISABLE /define:SIXENSE_DISABLE /define:NO_X360_XDK /define:RAD_TELEMETRY_DISABLED /define:DISABLE_ETW /define:NO_STEAM /define:NO_ATI_COMPRESS /define:NO_NVTC /define:LTCG /no_ceg /nofpo /hl2 +game /mksln hl2.sln
