#!/bin/bash
set -e

npm install -g @napi-rs/cli

rm -rf build
rm -rf target

cargo build --release

mkdir -p build && cd build
cmake ..
make